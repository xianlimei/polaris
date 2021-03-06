/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)util.c	1.1	06/06/20 SMI"

#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <utility.h>
#include <fcntl.h>
#include <syslog.h>

#include "local_types.h"
#include "target.h"
#include "utility.h"
#include "errcode.h"
#include "xml.h"
#include <sys/scsi/generic/commands.h>

#define	CRC32_STR	"CRC32C"
#define	NONE_STR	"None"

static thick_provo_t	*thick_head,
			*thick_tail;
pthread_mutex_t		thick_mutex;

static Boolean_t connection_parameters_get(iscsi_conn_t *c, char *targ_name);

void
util_init()
{
	(void) pthread_mutex_init(&thick_mutex, NULL);
}

/*
 * []----
 * | read_retry -- read data into buffer
 * |
 * | There have been problems on Solaris when reads from a socket
 * | fail to return the expected amount of data. It seems to occur
 * | around 2KB, but nothing reproduciable. By issuing multiple reads
 * | and adjusting the buffer pointer we can get around this problem.
 * | The suspection at this point is an interaction with threads, sockets,
 * | and large iSCSI transfers.
 * []----
 */
int
read_retry(int fd, char *buf, int count)
{
#define	SMALLER_READS
#ifdef SMALLER_READS
	int	cc,
		min,
		total = 0;

	while (count) {
		min = MIN(count, 512);
		cc = read(fd, buf, min);
		if (cc == -1) {
			if ((errno == EAGAIN) || (errno == 0))
				continue;
			else
				return (-1);
		}
		if (cc == 0)
			break;
		buf += cc;
		count -= cc;
		total += cc;
	}
	return (total);
#else
	return (read(fd, buf, count));
#endif
}

/*
 * []----
 * | check_access -- see if the requesting initiator is in the ACL
 * |
 * | Optionally will also check to see if this initiator requires
 * | authentication.
 * []----
 */
Boolean_t
check_access(xml_node_t *targ, char *initiator_name, Boolean_t req_chap)
{
	xml_node_t	*acl,
			*inode		= NULL,
			*tgt_initiator	= NULL;
	char		*dummy;
	Boolean_t	valid		= False,
			found_chap	= False;

	/*
	 * If there's no ACL for this target everyone has access.
	 */
	if ((acl = xml_node_next(targ, XML_ELEMENT_ACLLIST, NULL)) == NULL)
		return (True);

	/*
	 * Find the local initiator name and also save the knowledge
	 * if the initiator had a CHAP secret.
	 */
	while ((inode = xml_node_next(main_config, XML_ELEMENT_INIT,
	    inode)) != NULL) {
		if (xml_find_value_str(inode, XML_ELEMENT_INAME, &dummy) ==
		    True) {
			if (strcmp(dummy, initiator_name) == 0) {
				free(dummy);
				if (xml_find_value_str(inode,
				    XML_ELEMENT_CHAPSECRET, &dummy) == True) {
					free(dummy);
					found_chap = True;
				}
				break;
			} else {
				free(dummy);
			}
		}
	}

	if ((acl != NULL) && (inode == NULL))
		return (False);

	while ((tgt_initiator = xml_node_next(acl, XML_ELEMENT_INIT,
	    tgt_initiator)) != NULL) {

		if (strcmp(inode->x_value, tgt_initiator->x_value) == 0) {
			valid = True;
			break;
		}
	}

	if (valid == True) {

		/*
		 * If req_chap is True it means the login code hasn't gone
		 * through the authentication phase and it's trying to
		 * determine if the initiator should have done so. If
		 * we find a CHAP-secret then this routine will fail.
		 * No CHAP-secret for an initiator just means that a
		 * simple ACL list is used. This can be spoofed easily
		 * enough and is mainly used to limit the number of
		 * targets an initiator would see.
		 */
		if ((req_chap == True) && (found_chap == True))
			valid = False;
	}

	return (valid);
}

/*
 * []----
 * | convert_local_tpgt -- Convert a local tpgt name to real addresses
 * |
 * | To simplify the configuration files targets only have a target portal
 * | group tag string(s) associated. In the main configuration file there's
 * | a tpgt element which has one or more ip-address elements. So the tag
 * | is located and the actual data is inserted into the outgoing stream.
 * []----
 */
static Boolean_t
convert_local_tpgt(char **text, int *text_length, char *local_tpgt)
{
	xml_node_t	*tpgt	= NULL,
	*x;
	char		buf[80];
	char		ipaddr[4];

	while ((tpgt = xml_node_next(main_config, XML_ELEMENT_TPGT,
	    tpgt)) != NULL) {
		if (strcmp(tpgt->x_value, local_tpgt) == 0) {

			/*
			 * The only children of the tpgt element are
			 * ip-address elements. The value of each element is
			 * the string we need to use. So, we don't need to
			 * check the node's name to see if this is correct or
			 * not.
			 */
			if (tpgt->x_child == NULL) {
				return (False);
			}
			for (x = tpgt->x_child; x; x = x->x_sibling) {
				if (inet_pton(AF_INET, x->x_value, &ipaddr)
				    == 1) {
					/*
					 * Valid IPv4 address
					 */
					(void) snprintf(buf, sizeof (buf),
					    "%s,%s", x->x_value, local_tpgt);
				} else {
					/*
					 * Invalid IPv4 address
					 * try with brackets (RFC2732)
					 */
					(void) snprintf(buf, sizeof (buf),
					    "[%s],%s", x->x_value, local_tpgt);
				}
				(void) add_text(text, text_length,
				    "TargetAddress", buf);
			}
			break;
		}
	}

	return (True);
}

/*
 * []----
 * | add_target_address -- find and add any target address information
 * []----
 */
static void
add_target_address(iscsi_conn_t *c, char **text, int *text_length,
    xml_node_t *targ)
{
	xml_node_t	*tpgt_list,
			*tpgt		= NULL;
	struct sockaddr_in	*sp4;
	struct sockaddr_in6	*sp6;
	/*
	 * 7 is enough room for the largest TPGT of "65536", the ',' and a NULL
	 */
	char	buf[INET6_ADDRSTRLEN + 7],
		net_buf[INET6_ADDRSTRLEN];

	if ((tpgt_list = xml_node_next(targ, XML_ELEMENT_TPGTLIST,
	    NULL)) == NULL) {
		if_target_address(text, text_length,
		    (struct sockaddr *)&c->c_target_sockaddr);
		return;
	}

	while ((tpgt = xml_node_next(tpgt_list, XML_ELEMENT_TPGT,
	    tpgt)) != NULL) {
		if (convert_local_tpgt(text, text_length, tpgt->x_value) ==
		    False) {
			if (c->c_target_sockaddr.ss_family == AF_INET) {
				/*CSTYLED*/
				sp4 = (struct sockaddr_in *)&c->c_target_sockaddr;
				(void) snprintf(buf, sizeof (buf), "%s,%s",
				    inet_ntop(sp4->sin_family,
					(void *)&sp4->sin_addr,
					net_buf, sizeof (net_buf)),
				    tpgt->x_value);
			} else {
				/*CSTYLED*/
				sp6 = (struct sockaddr_in6 *)&c->c_target_sockaddr;
				(void) snprintf(buf, sizeof (buf), "%s,%s",
				    inet_ntop(sp6->sin6_family,
					(void *)&sp6->sin6_addr,
					net_buf, sizeof (net_buf)),
				    tpgt->x_value);
			}
			(void) add_text(text, text_length, "TargetAddress",
			    buf);
		}
	}
}

#ifdef notused
/*
 * []----
 * | create_tpgt_list -- create XML list of tpgt's for target
 * |
 * | Caller must free the data returned. The incoming tname is the
 * | iSCSI node name (we normally use the IQN form).
 * []----
 */
char *
create_tpgt_list(char *tname)
{
	char		*buf		= NULL,
			*p;
	xml_node_t	*tnode		= NULL,
			*tpgtlist	= NULL,
			*tpgt		= NULL;

	while ((tnode = xml_node_next(targets_config, XML_ELEMENT_TARG,
	    tnode)) != NULL) {
		if (xml_find_value_str(tnode, XML_ELEMENT_INAME, &p) ==
		    False) {
			continue;
		}
		if (strcmp(tname, p) != 0) {
			free(p);
			continue;
		} else
			free(p);
		if ((tpgtlist = xml_node_next(tnode, XML_ELEMENT_TPGTLIST,
		    NULL)) == NULL) {

			/*
			 * No TPGT list available so just return a NULL.
			 * This is not an error.
			 */
			return (NULL);
		}

		buf_add_tag(&buf, XML_ELEMENT_TPGTLIST, Tag_Start);
		while ((tpgt = xml_node_next(tpgtlist, XML_ELEMENT_TPGT,
		    tpgt)) != NULL) {
			xml_add_tag(&buf, XML_ELEMENT_TPGT, tpgt->x_value);
		}
		buf_add_tag(&buf, XML_ELEMENT_TPGTLIST, Tag_End);
		return (buf);
	}
	return (NULL);
}
#endif

/*
 * []----
 * | add_targets -- add TargetName and TargetAddress to text argument
 * |
 * | Add targets which this initiator is allowed to see based on
 * | the access_list associated with a target. If a target doesn't
 * | have an access list then let everyone see it.
 * []----
 */
static Boolean_t
add_targets(iscsi_conn_t *c, char **text, int *text_length)
{
	xml_node_t	*targ		= NULL;
	Boolean_t	rval		= True;
	char		*targ_name	= NULL;

	while ((rval == True) && ((targ = xml_node_next(targets_config,
	    XML_ELEMENT_TARG, targ)) != NULL)) {

		if (check_access(targ, c->c_sess->s_i_name, False) == True) {

			if (xml_find_value_str(targ, XML_ELEMENT_INAME,
			    &targ_name) == False) {
				rval = False;
				break;
			}
			queue_prt(c->c_mgmtq, Q_CONN_LOGIN,
			    "CON%x    %24s = %s", c->c_num, "TargetName",
			    targ_name);

			(void) add_text(text, text_length, "TargetName",
			    targ_name);
			free(targ_name);
			add_target_address(c, text, text_length, targ);
		}
	}
	return (rval);
}

#ifdef notused
/*
 * []----
 * | add_target_alias -- Add TargetAlias property if available.
 * []----
 */
Boolean_t
add_target_alias(iscsi_conn_t *c, char **text, int *text_length)
{
	xml_node_t	*targ		= NULL;
	char		*targ_name	= NULL,
			*alias_name	= NULL;
	Boolean_t	rval		= True;

	/*
	 * Most discovery sessions don't have a target name which is
	 * what they're looking for in the first place.
	 */
	if (c->c_sess->s_type == SessionDiscovery)
		return (True);

	while ((targ = xml_node_next(targets_config, XML_ELEMENT_TARG,
	    targ)) != NULL) {

		/*
		 * This is a hard error. Since we use node-name quite often
		 * up to this point in time if this fails either we've run
		 * out of memory or there's a memory corruption problem.
		 */
		if (xml_find_value_str(targ, XML_ELEMENT_INAME, &targ_name) ==
		    False)
			return (False);

		if (strcmp(targ_name, c->c_sess->s_t_name) == 0) {
			if (xml_find_value_str(targ, XML_ELEMENT_ALIAS,
			    &alias_name) == True) {

				/*
				 * Target name matches and we've got an alias.
				 */
				rval = add_text(text, text_length,
				    ISCSI_TARGET_ALIAS, alias_name);
				queue_prt(c->c_mgmtq, Q_CONN_LOGIN,
				    "CON%x    %-24s = %s", c->c_num,
				    ISCSI_TARGET_ALIAS, alias_name);
				break;
			} else if (xml_find_value_str(targ, XML_ELEMENT_TARG,
			    &alias_name) == True) {

				/*
				 * If we don't have a user settable alias
				 * then use the local name the administrator
				 * setup.
				 */
				rval = add_text(text, text_length,
				    ISCSI_TARGET_ALIAS, alias_name);
				queue_prt(c->c_mgmtq, Q_CONN_LOGIN,
				    "CON%x    %-24s = %s", c->c_num,
				    ISCSI_TARGET_ALIAS, alias_name);
				break;
			}
		}

		/*
		 * We don't have to worry about freeing alias_name because
		 * it will only be set if we found a match and had an alias
		 * in which case we'd never get here.
		 */
		free(targ_name);
		targ_name = NULL;
	}

	if (targ_name)
		free(targ_name);
	if (alias_name)
		free(alias_name);

	return (rval);
}
#endif

/*
 * []----
 * | add_text -- Add new name/value pair to possibly existing string
 * []----
 */
Boolean_t
add_text(char **text, int *current_length, char *name, char *val)
{
	int	dlen = *current_length,
		plen;
	char	*p;

	/*
	 * Length is 'name' + separator + 'value' + NULL
	 */
	plen = strlen(name) + 1 + strlen(val) + 1;

	if (dlen) {
		if ((p = (char *)realloc(*text, dlen + plen)) == NULL)
			return (False);
	} else {
		if ((p = (char *)malloc(plen)) == NULL)
			return (False);
	}

	*text = p;
	p = *text + dlen;

	(void) snprintf(p, plen, "%s%c%s", name, ISCSI_TEXT_SEPARATOR, val);
	*current_length = dlen + plen;

	return (True);
}

static void
send_named_msg(iscsi_conn_t *c, msg_type_t t, char *name)
{
	target_queue_t	*q = queue_alloc();
	msg_t		*m;
	name_request_t	n;

	n.nr_q		= q;
	n.nr_name	= name;

	queue_message_set(c->c_sessq, 0, t, &n);
	m = queue_message_get(q);
	queue_message_free(m);
	queue_free(q, NULL);
}

static Boolean_t
parse_digest_vals(Boolean_t *bp, char *name, char *val, char **text, int *len)
{
	Boolean_t	rval;

	/*
	 * It's the initiators data so we'll allow them
	 * to determine if CRC checks should be enabled
	 * or not. So, look at the first token, which
	 * declares their preference, and use that.
	 */
	if (strncmp(val, CRC32_STR, strlen(CRC32_STR)) == 0) {
		*bp = True;
		rval = add_text(text, len, name, CRC32_STR);
	} else if (strncmp(val, NONE_STR, strlen(NONE_STR)) == 0) {
		*bp = False;
		rval = add_text(text, len, name, NONE_STR);
	} else {
		*bp = False;
		rval = add_text(text, len, name, "Reject");
	}

	return (rval);
}

/*
 * []----
 * | parse_text -- receive text information from initiator and parse
 * |
 * | Read in the current data based on the amount which the login PDU says
 * | should be available. Add it to the end of previous data if it exists.
 * | Previous data would be from a PDU which had the 'C' bit set and was
 * | stored in the connection.
 * |
 * | Once values for parameter name has been selected store outgoing string
 * | in text message for response.
 * |
 * | If errcode is non-NULL the appropriate login error code will be
 * | stored.
 * []----
 */
Boolean_t
parse_text(iscsi_conn_t *c, int dlen, char **text, int *text_length,
    int *errcode)
{
	char		*p		= NULL,
			*n,
			*cur_pair,
			param_rsp[32];
	int		plen;		/* pair length */
	Boolean_t	rval		= True;
	char		param_buf[16];

	if ((p = (char *)malloc(dlen)) == NULL)
		return (False);

	/*
	 * Read in data to buffer.
	 */
	if (read(c->c_fd, p, dlen) != dlen) {
		free(p);
		return (False);
	}

	queue_prt(c->c_mgmtq, Q_CONN_NONIO, "CON%x  Available text size %d",
	    c->c_num, dlen);

	/*
	 * Read in and toss any pad data
	 */
	if (dlen % ISCSI_PAD_WORD_LEN) {
		char junk[ISCSI_PAD_WORD_LEN];
		int pad_len = ISCSI_PAD_WORD_LEN - (dlen % ISCSI_PAD_WORD_LEN);

		if (read(c->c_fd, junk, pad_len) != pad_len) {
			free(p);
			return (False);
		}
	}

	if (c->c_text_area != NULL) {
		if ((n = (char *)realloc(c->c_text_area,
		    c->c_text_len + dlen)) == NULL) {
			free(p);
			return (False);
		}
		bcopy(p, n + c->c_text_len, dlen);

		/*
		 * No longer need the space allocated to 'p' since it
		 * will point to the aggregated area of all data.
		 */
		free(p);

		/*
		 * Point 'p' to this new area for parsing and save the
		 * combined length in dlen.
		 */
		p = n;
		dlen += c->c_text_len;

		/*
		 * Clear the indication that space has been allocated
		 */
		c->c_text_area = NULL;
		c->c_text_len = 0;
	}

	/*
	 * At this point 'p' points to the name/value parameters. Need
	 * to cycle through each pair.
	 */
	n = p;
	while (dlen > 0) {
		cur_pair = n;

		plen = strlen(n);
		if ((n = strchr(cur_pair, ISCSI_TEXT_SEPARATOR)) == NULL) {
			if (errcode != NULL)
				*errcode =
				    (ISCSI_STATUS_CLASS_INITIATOR_ERR << 8) |
				    ISCSI_LOGIN_STATUS_INIT_ERR;
			rval = False;
			break;
		} else
			*n++ = '\0';

		queue_prt(c->c_mgmtq, Q_CONN_LOGIN, "CON%x    %-24s = %s",
		    c->c_num, cur_pair, n);

		/*
		 * At this point, 'cur_pair' points at the name and 'n'
		 * points at the value.
		 */
		if (strcmp("HeaderDigest", cur_pair) == 0) {

			rval = parse_digest_vals(&c->c_header_digest,
			    cur_pair, n, text, text_length);

		} else if (strcmp("DataDigest", cur_pair) == 0) {

			rval = parse_digest_vals(&c->c_data_digest, cur_pair,
			    n, text, text_length);

		} else if (strcmp("InitiatorName", cur_pair) == 0) {

			send_named_msg(c, msg_initiator_name, n);

		} else if (strcmp("InitiatorAlias", cur_pair) == 0) {

			send_named_msg(c, msg_initiator_alias, n);

		} else if (strcmp("TargetName", cur_pair) == 0) {

			send_named_msg(c, msg_target_name, n);

			/*
			 * Had to wait until now before loading any parameters
			 * because they are based on the TargetName which
			 * hasn't been known until now. This might fail
			 * because the target doesn't exist or the initiator
			 * doesn't have permission to access this target.
			 */
			if ((rval = connection_parameters_get(c, n)) == False) {
				*errcode =
				    (ISCSI_STATUS_CLASS_INITIATOR_ERR << 8) |
					ISCSI_LOGIN_STATUS_TGT_FORBIDDEN;
			} else if ((rval = add_text(text, text_length,
			    "TargetAlias", c->c_targ_alias)) == True) {

				/*
				 * Add TPGT now
				 */
				(void) snprintf(param_buf, sizeof (param_buf),
				    "%d", c->c_tpgt);
				rval = add_text(text, text_length,
				    "TargetPortalGroupTag", param_buf);
			}

		} else if (strcmp("SessionType", cur_pair) == 0) {

			c->c_sess->s_type = strcmp(n, "Discovery") == 0 ?
			    SessionDiscovery : SessionNormal;

		} else if (strcmp("SendTargets", cur_pair) == 0) {

			if ((c->c_sess->s_type != SessionDiscovery) &&
			    (strcmp("All", n) == 0)) {
				rval = add_text(text, text_length, cur_pair,
				    "Irrelevant");
			} else {
				rval = add_targets(c, text, text_length);
			}

		} else if (strcmp("MaxRecvDataSegmentLength", cur_pair) == 0) {

			c->c_max_recv_data = strtol(n, NULL, 0);
			rval = add_text(text, text_length, cur_pair, n);

		} else if (strcmp("DefaultTime2Wait", cur_pair) == 0) {

			c->c_default_time_2_wait = strtol(n, NULL, 0);

		} else if (strcmp("DefaultTime2Retain", cur_pair) == 0) {

			c->c_default_time_2_retain = strtol(n, NULL, 0);

		} else if (strcmp("ErrorRecoveryLevel", cur_pair) == 0) {

			c->c_erl = 0;
			(void) snprintf(param_rsp, sizeof (param_rsp),
			    "%d", c->c_erl);
			rval = add_text(text, text_length,
			    cur_pair, param_rsp);

		} else if (strcmp("IFMarker", cur_pair) == 0) {

			c->c_ifmarker = False;
			rval = add_text(text, text_length, cur_pair, "No");

		} else if (strcmp("OFMarker", cur_pair) == 0) {

			c->c_ofmarker = False;
			rval = add_text(text, text_length, cur_pair, "No");

		} else if (strcmp("InitialR2T", cur_pair) == 0) {

			c->c_initialR2T = True;
			rval = add_text(text, text_length, cur_pair, "Yes");

		} else if (strcmp("ImmediateData", cur_pair) == 0) {

			/*
			 * Since we can handle immediate data without
			 * a problem just echo back what the initiator
			 * sends. If the initiator decides to violate
			 * the spec by sending immediate data even though
			 * they've disabled it, it's there problem and
			 * we'll deal with the data.
			 */
			c->c_immediate_data = strcmp(n, "No") ? True : False;
			rval = add_text(text, text_length, cur_pair, n);

		} else if (strcmp("MaxBurstLength", cur_pair) == 0) {

			c->c_max_burst_len = strtol(n, NULL, 0);

		} else if (strcmp("FirstBurstLength", cur_pair) == 0) {

			/*
			 * We can handle anything the initiator wishes
			 * to shove in our direction. So, store the value
			 * in case we ever wish to validate input data,
			 * but there's no real need to do so.
			 */
			c->c_first_burst_len = strtol(n, NULL, 0);

		} else if (strcmp("MaxOutstandingR2T", cur_pair) == 0) {

			/*
			 * Save the value, but at most we'll toss out
			 * one R2T packet.
			 */
			c->c_max_outstanding_r2t = strtol(n, NULL, 0);

		} else if (strcmp("MaxConnections", cur_pair) == 0) {

			/* ---- To be fixed ---- */
			c->c_max_connections = 1;
			(void) snprintf(param_rsp, sizeof (param_rsp),
			    "%d", c->c_max_connections);
			rval = add_text(text, text_length,
			    cur_pair, param_rsp);

		} else if (strcmp("DataPDUInOrder", cur_pair) == 0) {

			/*
			 * We can handle DataPDU's out of order and
			 * currently we'll only send them in order. We're
			 * to far removed from the hardware to see data
			 * coming off of the platters out of order so
			 * it's unlikely we'd ever implement this feature.
			 * Store the parameter and echo back the initiators
			 * request.
			 */
			c->c_data_pdu_in_order = strcmp(n, "Yes") == 0 ?
			    True : False;
			rval = add_text(text, text_length, cur_pair, n);

		} else if (strcmp("DataSequenceInOrder", cur_pair) == 0) {

			/*
			 * Currently we're set up to look at and require
			 * PDU sequence numbers be in order. The check
			 * now is only done as a prelude to supporting
			 * MC/S and guaranteeing the order of incoming
			 * packetss on different connections.
			 */
			c->c_data_sequence_in_order = True;
			rval = add_text(text, text_length, cur_pair, "Yes");

		} else if ((strcmp("AuthMethod", cur_pair) == 0) ||
		    (strcmp("CHAP_A", cur_pair) == 0) ||
		    (strcmp("CHAP_I", cur_pair) == 0) ||
		    (strcmp("CHAP_C", cur_pair) == 0) ||
		    (strcmp("CHAP_N", cur_pair) == 0) ||
		    (strcmp("CHAP_R", cur_pair) == 0)) {

			rval = add_text(&(c->auth_text), &(c->auth_text_length),
					    cur_pair, n);

		} else {

			/*
			 * It's perfectly legitimate for an initiator to
			 * send us a parameter we don't currently understand.
			 * For example, an initiator that supports iSER will
			 * send an RDMA options parameter. If we respond with
			 * a valid return value it knows to switch to iSER
			 * for future processing.
			 */
			rval = add_text(text, text_length,
					cur_pair, "NotUnderstood");

			/*
			 * Go ahead a log this information in case we see
			 * something unexpected.
			 */
			queue_prt(c->c_mgmtq, Q_CONN_ERRS,
			    "CON%x  Unknown parameter %s=%s",
			    c->c_num, cur_pair, n);
		}

		if (rval == False) {
			/*
			 * Make sure the caller wants error status and that it
			 * hasn't already been set.
			 */
			if ((errcode != NULL) && (*errcode == 0))
				*errcode =
				    (ISCSI_STATUS_CLASS_TARGET_ERR << 8) |
				    ISCSI_LOGIN_STATUS_TARGET_ERROR;
			break;
		}

		/*
		 * next pair of parameters. 1 is added to include the NULL
		 * byte and the end of each string.
		 */
		n = cur_pair + plen + 1;
		dlen -= (plen + 1);
	}

	if (p != NULL)
		free(p);

	return (rval);
}

void
connection_parameters_default(iscsi_conn_t *c)
{
	c->c_tpgt = 1;
}

/*
 * []----
 * | find_main_tpgt -- Looks up the IP address and finds a match TPGT
 * |
 * | If no, TPGT for this address exists the routine returns 0 which
 * | is an illegal TPGT value.
 * []----
 */
int
find_main_tpgt(struct sockaddr_storage *pst)
{
	char		ip_addr[16];
	xml_node_t	*tpgt				= NULL,
			*ip_node			= NULL;
	struct in_addr	addr;
	struct in6_addr	addr6;

	/*
	 * Hardly can you believe that such struct-to-struct
	 * assignment IS valid.
	 */
	addr = ((struct sockaddr_in *)pst)->sin_addr;
	addr6 = ((struct sockaddr_in6 *)pst)->sin6_addr;

	while ((tpgt = xml_node_next(main_config, XML_ELEMENT_TPGT,
	    tpgt)) != NULL) {

		ip_node = NULL;
		while ((ip_node = xml_node_next(tpgt, XML_ELEMENT_IPADDR,
		    ip_node)) != NULL) {

			if (pst->ss_family == AF_INET) {

				if (inet_pton(AF_INET, ip_node->x_value,
					ip_addr) != 1) {
					continue;
				}
				if (bcmp(ip_addr, &addr,
					sizeof (struct in_addr)) == 0) {
					return (atoi(tpgt->x_value));
				}
			} else if (pst->ss_family == AF_INET6) {

				if (inet_pton(AF_INET6, ip_node->x_value,
					ip_addr) != 1) {
					continue;
				}
				if (bcmp(ip_addr, &addr6,
					sizeof (struct in6_addr)) == 0) {
					return (atoi(tpgt->x_value));
				}
			}
		}
	}

	return (1);
}

static int
convert_to_tpgt(iscsi_conn_t *c, xml_node_t *targ)
{
	xml_node_t	*list,
			*tpgt		= NULL;
	int		addr_tpgt,
			pos_tpgt;

	/*
	 * If we don't find our IP in the general configuration list
	 * we'll use the default value which is 1 according to RFC3720.
	 */
	addr_tpgt = find_main_tpgt(&(c->c_target_sockaddr));

	/*
	 * If this target doesn't have a list of target portal group tags
	 * just return the default which is 1.
	 */
	list = xml_node_next(targ, XML_ELEMENT_TPGTLIST, NULL);
	if (list == NULL)
		return (addr_tpgt);

	while ((tpgt = xml_node_next(list, XML_ELEMENT_TPGT, tpgt)) != NULL) {
		(void) xml_find_value_int(tpgt, XML_ELEMENT_TPGT, &pos_tpgt);
		if (pos_tpgt == addr_tpgt) {
			return (addr_tpgt);
		}
	}

	return (0);
}

/*
 * []----
 * | find_target_node -- given a target IQN name, return the XML node
 * []----
 */
xml_node_t *
find_target_node(char *targ_name)
{
	xml_node_t	*tnode	= NULL;
	char		*iname;

	while ((tnode = xml_node_next(targets_config, XML_ELEMENT_TARG,
	    tnode)) != NULL) {
		if (xml_find_value_str(tnode, XML_ELEMENT_INAME, &iname) ==
		    True) {
			if (strcmp(iname, targ_name) == 0) {
				free(iname);
				return (tnode);
			} else
				free(iname);
		}
	}

	return (NULL);
}

static Boolean_t
connection_parameters_get(iscsi_conn_t *c, char *targ_name)
{
	xml_node_t	*targ,
			*alias;
	Boolean_t	rval	= False;

	if ((targ = find_target_node(targ_name)) != NULL) {

		if (check_access(targ, c->c_sess->s_i_name, False) == False)
			return (False);

		/*
		 * Have a valid node for our target. Start looking
		 * for connection oriented parameters.
		 */
		if ((c->c_tpgt = convert_to_tpgt(c, targ)) == 0)
			return (False);
		if ((alias = xml_node_next(targ, XML_ELEMENT_ALIAS, NULL)) ==
		    NULL) {
			(void) xml_find_value_str(targ, XML_ELEMENT_TARG,
			    &c->c_targ_alias);
		} else {
			(void) xml_find_value_str(alias, XML_ELEMENT_ALIAS,
			    &c->c_targ_alias);
		}

		(void) xml_find_value_int(targ, XML_ELEMENT_MAXCMDS,
		    &c->c_maxcmdsn);
		rval = True;
	}

	return (rval);
}

Boolean_t
validate_version(xml_node_t *node, int *maj_p, int *min_p)
{
	char	*vers_str	= NULL,
		*minor_part;
	int	maj,
		min;

	if ((xml_find_attr_str(node, XML_ELEMENT_VERS, &vers_str) == False) ||
	    (vers_str == NULL))
		return (False);

	maj = strtol(vers_str, &minor_part, 0);
	if ((maj > *maj_p) || (minor_part == NULL) || (*minor_part != '.')) {
		free(vers_str);
		return (False);
	}

	min	= strtol(&minor_part[1], NULL, 0);
	*maj_p	= maj;
	*min_p	= min;
	free(vers_str);

	return (True);
}

/*
 * []----
 * | sna_lt -- Serial Number Arithmetic, 32 bits, less than, RFC1982
 * []----
 */
int
sna_lt(uint32_t n1, uint32_t n2)
{
	return ((n1 != n2) &&
		(((n1 < n2) && ((n2 - n1) < SNA32_CHECK)) ||
		    ((n1 > n2) && ((n2 - n1) < SNA32_CHECK))));
}

/*
 * []----
 * sna_lte -- Serial Number Arithmetic, 32 bits, less than, RFC1982
 * []----
 */
int
sna_lte(uint32_t n1, uint32_t n2)
{
	return ((n1 == n2) ||
		(((n1 < n2) && ((n2 - n1) < SNA32_CHECK)) ||
		    ((n1 > n2) && ((n2 - n1) < SNA32_CHECK))));
}

Boolean_t
update_config_main(char **msg)
{
	if (xml_dump2file(main_config, config_file) == False) {
		xml_rtn_msg(msg, ERR_UPDATE_MAINCFG_FAILED);
		return (False);
	} else
		return (True);
}

Boolean_t
update_config_targets(char **msg)
{
	char	path[MAXPATHLEN];

	(void) snprintf(path, sizeof (path), "%s/config.xml", target_basedir);
	if (xml_dump2file(targets_config, path) == False) {
		if (msg != NULL)
			xml_rtn_msg(msg, ERR_UPDATE_TARGCFG_FAILED);
		return (False);
	} else
		return (True);
}

Boolean_t
util_create_guid(char **guid)
{
	eui_16_t	eui;
	/*
	 * We only have room for 32bits of data in the GUID. The hiword/loword
	 * macros will not work on 64bit variables. The work, but produce
	 * invalid results on Big Endian based machines.
	 */
	uint32_t	tval = (uint_t)time((time_t *)0);
	size_t		guid_size;
	int		i,
			fd;

	if ((mac_len == 0) && (if_find_mac(NULL) == False)) {

		/*
		 * By default strict GUID generation is enforced. This can
		 * be disabled by using the correct XML tag in the configuration
		 * file.
		 */
		if (enforce_strict_guid == True)
			return (False);

		/*
		 * There's no MAC address available and we've even tried
		 * a second time to get one. So fallback to using a random
		 * number for the MAC address.
		 */
		if ((fd = open("/dev/random", O_RDONLY)) < 0)
			return (False);
		if (read(fd, &eui, sizeof (eui)) != sizeof (eui))
			return (False);
		(void) close(fd);

		eui.e_vers		= SUN_EUI_16_VERS;
		eui.e_company_id[0]	= 0;
		eui.e_company_id[1]	= 0;
		eui.e_company_id[2]	= SUN_EN;

	} else {
		bzero(&eui, sizeof (eui));

		eui.e_vers	= SUN_EUI_16_VERS;
		/* ---- [0] & [1] are zero for Sun's IEEE identifier ---- */
		eui.e_company_id[2]	= SUN_EN;
		eui.e_timestamp[0]	= hibyte(hiword(tval));
		eui.e_timestamp[1]	= lobyte(hiword(tval));
		eui.e_timestamp[2]	= hibyte(loword(tval));
		eui.e_timestamp[3]	= lobyte(loword(tval));
		for (i = 0; i < min(mac_len, sizeof (eui.e_mac)); i++) {
			eui.e_mac[i] = mac_addr[i];
		}

		/*
		 * To prevent duplicate GUIDs we need to sleep for one
		 * second here since part of the GUID is a time stamp with
		 * a one second resolution.
		 */
		sleep(1);
	}

	if (xml_encode_bytes((uint8_t *)&eui, sizeof (eui), guid,
	    &guid_size) == False) {
		return (False);
	} else
		return (True);
}

/*
 * []----
 * | create_geom -- based on size determine best fit for CHS
 * |
 * | Given size in bytes which will be adjusted to sectors, find
 * | the best fit for making C * H * S == sectors
 * []----
 */
void
create_geom(diskaddr_t size, int *cylinder, int *heads, int *spt)
{
	diskaddr_t	sects	= size >> 9,
			c, h, s, t;
	int		pass;

	/*
	 * For certain odd size LU we can't generate correct geometry.
	 * If this occurs the values will be set to zero and the MODE
	 * pages will return unsupported.
	 */
	*cylinder	= 0;
	*heads		= 0;
	*spt		= 0;

	for (pass = 0; pass < 2; pass++)
		for (c = 0x8000; c > 0; c >>= 1) {
			t = sects / c;
			if ((t == 0) || ((sects % c) != 0))
				continue;
			for (h = 1; h < 0xff; h++)
				if ((t % h) == 0) {
					s = t / h;
					if (s > 0xffff)
						continue;
					if ((pass == 0) &&
					    ((c < MIN_VAL) || (h < MIN_VAL) ||
					    (s < MIN_VAL))) {
						continue;
					}

					*cylinder	= (int)c;
					*heads		= (int)h;
					*spt		= (int)s;
					return;
				}
		}
}

/*
 * []----
 * | strtol_multiplier -- common method to deal with human type numbers
 * []----
 */
Boolean_t
strtoll_multiplier(char *str, uint64_t *sp)
{
	char		*m;
	uint64_t	size;

	size = strtoll(str, &m, 0);
	if (m && *m) {
		switch (*m) {
		case 't':
		case 'T':
			size *= 1024;
			/*FALLTHRU*/
		case 'g':
		case 'G':
			size *= 1024;
			/*FALLTHRU*/
		case 'm':
		case 'M':
			size *= 1024;
			/*FALLTHRU*/
		case 'k':
		case 'K':
			size *= 1024;
			break;

		default:
			return (False);
		}
	}

	*sp = size;
	return (True);
}

/*
 * []----
 * | util_title -- print out start/end title in consistent manner
 * []----
 */
void
util_title(target_queue_t *q, int type, int num, char *title)
{
	char	*type_str;
	int	len,
		pad;

	len	= strlen(title);
	pad	= len & 1;

	switch (type) {
	case Q_CONN_LOGIN:
	case Q_CONN_NONIO:
		type_str	= "CON";
		break;

	case Q_SESS_LOGIN:
	case Q_SESS_NONIO:
		type_str	= "SES";
		break;

	case Q_STE_NONIO:
		type_str	= "SAM";
		break;

	default:
		type_str	= "UGH";
		break;
	}

	queue_prt(q, type, "%s%x  ---- %*s%s%*s ----", type_str, num,
	    ((60 - len) / 2), "", title, ((60 - len) / 2) + pad, "");
}

/*
 * []----
 * | task_to_str -- convert task management event to string (DEBUG USE)
 * []----
 */
char *
task_to_str(int func)
{
	switch (func) {
	case ISCSI_TM_FUNC_ABORT_TASK:		return ("Abort");
	case ISCSI_TM_FUNC_ABORT_TASK_SET:	return ("Abort Set");
	case ISCSI_TM_FUNC_CLEAR_ACA:		return ("Clear ACA");
	case ISCSI_TM_FUNC_CLEAR_TASK_SET:	return ("Clear Task");
	case ISCSI_TM_FUNC_LOGICAL_UNIT_RESET:	return ("LUN Reset");
	case ISCSI_TM_FUNC_TARGET_WARM_RESET:	return ("Target Warm Reset");
	case ISCSI_TM_FUNC_TARGET_COLD_RESET:	return ("Target Cold Reset");
	case ISCSI_TM_FUNC_TASK_REASSIGN:	return ("Task Reassign");
	default:				return ("Unknown");
	}
}

/*
 * []----
 * | xml_rtn_msg -- create a common format for XML replies to management UI
 * []----
 */
void
xml_rtn_msg(char **buf, err_code_t code)
{
	char	lbuf[16];

	buf_add_tag_and_attr(buf, XML_ELEMENT_ERROR, "version='1.0'");
	(void) snprintf(lbuf, sizeof (lbuf), "%d", code);
	xml_add_tag(buf, XML_ELEMENT_CODE, lbuf);
	xml_add_tag(buf, XML_ELEMENT_MESSAGE, errcode_to_str(code));
	buf_add_tag(buf, XML_ELEMENT_ERROR, Tag_End);
}

/*
 * []----
 * | thick_provo_start -- start an initialization thread for targ/lun
 * []----
 */
void *
thick_provo_start(void *v)
{
	thick_provo_t	*tp	= (thick_provo_t *)v;
	msg_t		*m;
	Boolean_t	rval;
	char		*err	= NULL;

	/*
	 * Add this threads information to the main queue. This is
	 * used in case the administrator decides to remove the LU
	 * before the initialization is complete.
	 */
	(void) pthread_mutex_lock(&thick_mutex);
	if (thick_head == NULL) {
		thick_head = tp;
	} else {
		thick_tail->next = tp;
		tp->prev = thick_tail;
	}
	thick_tail = tp;
	(void) pthread_mutex_unlock(&thick_mutex);

	/*
	 * This let's the parent thread know this thread is running.
	 */
	queue_message_set(tp->q, 0, msg_mgmt_rply, 0);

	/* ---- Start the initialization of the LU ---- */
	rval = t10_thick_provision(tp->targ_name, tp->lun, tp->q);

	/* ---- Remove from the linked list ---- */
	(void) pthread_mutex_lock(&thick_mutex);
	if (tp->prev == NULL) {
		assert(tp == thick_head);
		thick_head = tp->next;
		if (tp->next == NULL) {
			assert(tp == thick_tail);
			thick_tail = NULL;
		} else
			tp->next->prev = NULL;
	} else {
		tp->prev->next = tp->next;
		if (tp->next != NULL)
			tp->next->prev = tp->prev;
		else
			thick_tail = tp->prev;
	}
	(void) pthread_mutex_unlock(&thick_mutex);

	/*
	 * There's a race condition where t10_thick_provision() could
	 * finish and before the thick_mutex lock is grabbed again
	 * that another thread running the thick_provo_stop() could
	 * find a match and send a shutdown message. If that happened
	 * that thread would wait forever in queue_message_get(). So,
	 * After this target/lun pair has been removed check the message
	 * queue one last time to see if there's a message available.
	 * If so, send an ack.
	 */
	m = queue_message_try_get(tp->q);
	if (m != NULL) {
		assert(m->msg_type == msg_shutdown);
		queue_message_set((target_queue_t *)m->msg_data, 0,
		    msg_shutdown_rsp, 0);
	}

	if (rval == True)
		iscsi_inventory_change(tp->targ_name);
	else {
		queue_prt(mgmtq, Q_GEN_ERRS, "Failed to initialize %s/%d",
		    tp->targ_name, tp->lun);
		syslog(LOG_ERR, "Failed to initialize %s, LU%d", tp->targ_name,
		    tp->lun);
		remove_target_common(tp->targ_name, tp->lun, &err);
		if (err != NULL) {

			/*
			 * There's not much we can do here. The most likely
			 * cause of not being able to remove the target is
			 * that it's LU 0 and there is currently another
			 * LU allocated.
			 */
			queue_prt(mgmtq, Q_GEN_ERRS, "Failed to remove target");
			syslog(LOG_ERR, "Failed to remove target/lun after "
			    "initialization failure");
		}
	}

	free(tp->targ_name);
	queue_free(tp->q, NULL);
	free(tp);

	return (NULL);
}

/*
 * []----
 * | thick_provo_stop -- stop initialization thread for given targ/lun
 * []----
 */
void
thick_provo_stop(char *targ, int lun)
{
	thick_provo_t	*tp;
	target_queue_t	*q	= queue_alloc();

	(void) pthread_mutex_lock(&thick_mutex);
	tp = thick_head;
	while (tp) {
		if ((strcmp(tp->targ_name, targ) == 0) && (tp->lun == lun)) {
			queue_message_set(tp->q, 0, msg_shutdown, (void *)q);
			/*
			 * Drop the global mutex because it's entirely
			 * possible for a thick_provo_start thread to be
			 * in the early stages in which it will can call
			 * thick_provo_chk() from the T10 SAM code.
			 */
			(void) pthread_mutex_unlock(&thick_mutex);

			queue_message_free(queue_message_get(q));

			/*
			 * Pick the lock back up since it'll make the
			 * finish stage easier to deal with.
			 */
			(void) pthread_mutex_lock(&thick_mutex);
			break;
		}
		tp = tp->next;
	}
	(void) pthread_mutex_unlock(&thick_mutex);
	queue_free(q, NULL);
}

/*
 * []----
 * | thick_provo_chk_thr -- see if there's an initialization thread running
 * []----
 */
Boolean_t
thick_provo_chk_thr(char *targ, int lun)
{
	thick_provo_t	*tp;
	Boolean_t	rval = False;

	(void) pthread_mutex_lock(&thick_mutex);
	tp = thick_head;
	while (tp) {
		if ((strcmp(tp->targ_name, targ) == 0) && (tp->lun == lun)) {
			rval = True;
			break;
		}
		tp = tp->next;
	}
	(void) pthread_mutex_unlock(&thick_mutex);

	return (rval);
}

/*
 * []----
 * | remove_target_common -- remove targ/lun from system.
 * |
 * | This is a common function that's used both by the normal remove
 * | target code and when a write failure occurs during initialization.
 * | It will handle being given either the local target name or the full
 * | IQN name of the target.
 * []----
 */
void
remove_target_common(char *name, int lun_num, char **msg)
{
	xml_node_t	*targ			= NULL,
			*list,
			*lun,
			*node,
			*c;
	char		path[MAXPATHLEN],
			*tname			= NULL,
			*iname			= NULL,
			*bs_path		= NULL;
	int		chk,
			xml_fd;
	Boolean_t	bs_delete		= False;
	xmlTextReaderPtr	r;

	(void) pthread_mutex_lock(&targ_config_mutex);
	while ((targ = xml_node_next(targets_config, XML_ELEMENT_TARG, targ)) !=
	    NULL) {
		/* ---- Look for a match on the friendly name ---- */
		if (strcmp(targ->x_value, name) == 0) {
			tname = name;
			break;
		}

		/* ---- Check to see if they gave the IQN name instead ---- */
		if ((xml_find_value_str(targ, XML_ELEMENT_INAME, &iname) ==
		    True) && (strcmp(iname, name) == 0))
			break;
		else {
			free(iname);
			iname = NULL;
		}
	}

	/* ---- Check to see if it's already been removed ---- */
	if (targ == NULL) {
		(void) pthread_mutex_unlock(&targ_config_mutex);
		return;
	}

	/*
	 * We need both the friendly and IQN names so figure out which wasn't
	 * given and find it's value.
	 */
	if (tname == NULL)
		tname = targ->x_value;
	if (iname == NULL) {
		if (xml_find_value_str(targ, XML_ELEMENT_INAME, &iname) ==
		    False) {
			xml_rtn_msg(msg, ERR_INTERNAL_ERROR);
			(void) pthread_mutex_unlock(&targ_config_mutex);
			return;
		}
	}

	if ((list = xml_node_next(targ, XML_ELEMENT_LUNLIST, NULL)) == NULL)
		goto error;

	if (lun_num == 0) {

		/*
		 * LUN must be the last one removed, so check to
		 * see if others are still present.
		 */
		lun = NULL;
		while ((lun = xml_node_next(list, XML_ELEMENT_LUN, lun)) !=
		    NULL) {
			if (xml_find_value_int(lun, XML_ELEMENT_LUN, &chk) ==
			    False)
				goto error;

			if (chk != lun_num) {
				xml_rtn_msg(msg, ERR_LUN_ZERO_NOT_LAST);
				goto error;
			}
		}
	} else {

		/*
		 * Make sure the LU exists that's being removed
		 */
		lun = NULL;
		while ((lun = xml_node_next(list, XML_ELEMENT_LUN, lun)) !=
		    NULL) {
			if (xml_find_value_int(lun, XML_ELEMENT_LUN, &chk) ==
			    False)
				goto error;

			if (chk == lun_num) {
				lun = xml_alloc_node(XML_ELEMENT_LUN, Int,
				    &lun_num);
				(void) xml_remove_child(list, lun, MatchBoth);
				xml_free_node(lun);
				break;
			}
		}
		if (lun == NULL) {
			xml_rtn_msg(msg, ERR_LUN_NOT_FOUND);
			goto error;
		}
	}

	/* ---- Say goodbye to that data ---- */
	(void) snprintf(path, sizeof (path), "%s/%s/%s%d", target_basedir,
	    iname, LUNBASE, lun_num);
	(void) unlink(path);
	(void) snprintf(path, sizeof (path), "%s/%s/%s%d", target_basedir,
	    iname, PARAMBASE, lun_num);

	/*
	 * See if there's a backing store for this lun, which means LUNBASE
	 * was just a symbolic link, and delete the backing store if we
	 * created it in the first place.
	 */
	xml_fd = open(path, O_RDONLY);
	if ((r = (xmlTextReaderPtr)xmlReaderForFd(xml_fd, NULL, NULL, 0)) !=
	    NULL) {
		node = NULL;
		while (xmlTextReaderRead(r) == 1)
			if (xml_process_node(r, &node) == False)
				break;
		close(xml_fd);
		xmlTextReaderClose(r);
		xmlFreeTextReader(r);
		xmlCleanupParser();

		(void) xml_find_value_str(node, XML_ELEMENT_BACK, &bs_path);
		if ((xml_find_value_boolean(node, XML_ELEMENT_DELETE_BACK,
		    &bs_delete) == True) && (bs_delete == True))
			unlink(bs_path);
		if (bs_path != NULL)
			free(bs_path);
		xml_tree_free(node);
	}

	(void) unlink(path);

	/*
	 * If the was LUN 0 then do to the previous check
	 * we know that no other files exist in the target
	 * directory so the target information can be removed
	 * along with the directory.
	 */
	if (lun_num == 0) {
		c = xml_alloc_node(XML_ELEMENT_TARG, String, tname);
		(void) xml_remove_child(targets_config, c, MatchBoth);
		xml_free_node(c);
		(void) snprintf(path, sizeof (path), "%s/%s", target_basedir,
		    iname);
		(void) rmdir(path);

		/*
		 * Don't forget to remove the symlink to
		 * the target directory.
		 */
		(void) snprintf(path, sizeof (path), "%s/%s", target_basedir,
		    tname);
		(void) unlink(path);
	}

	/*
	 * Not much we can do here if we fail to updated the config.
	 */
	if (update_config_targets(msg) == False)
		syslog(LOG_ERR, "Failed to update target configuration!");

error:
	(void) pthread_mutex_unlock(&targ_config_mutex);
	if (iname != NULL)
		free(iname);
}
