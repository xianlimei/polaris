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

#pragma ident	"@(#)unix_cred.c	1.21	06/03/10 SMI"

#include <nss_dbdefs.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <auth_attr.h>
#include <deflt.h>
#include <priv.h>
#include <secdb.h>
#include <user_attr.h>
#include <sys/task.h>
#include <libintl.h>
#include <project.h>
#include <errno.h>
#include <alloca.h>

#include <bsm/adt.h>
#include <bsm/adt_event.h>	/* adt_get_auid() */

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_impl.h>

#define	PROJECT		"project="
#define	PROJSZ		(sizeof (PROJECT) - 1)

/*
 *	unix_cred - PAM auth modules must contain both pam_sm_authenticate
 *		and pam_sm_setcred.  Some other auth module is responsible
 *		for authentication (e.g., pam_unix_auth.so), this module
 *		only implements pam_sm_setcred so that the authentication
 *		can be separated without knowledge of the Solaris Unix style
 *		credential setting.
 *		Solaris Unix style credential setting includes initializing
 *		the audit characteristics if not already initialized and
 *		setting the user's default and limit privileges.
 */

/*
 *	unix_cred - pam_sm_authenticate
 *
 *	Returns	PAM_IGNORE.
 */

/*ARGSUSED*/
int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return (PAM_IGNORE);
}

/*
 * Obtain a privilege set "keyname" from userattr; if none is present,
 * fall back to the default, "defname".
 */
static int
getset(char *keyname, char *defname, userattr_t *ua, priv_set_t **res)
{
	char *str;
	priv_set_t *tmp;
	char *badp;
	int len;

	if ((ua == NULL || ua->attr == NULL ||
	    (str = kva_match(ua->attr, keyname)) == NULL) &&
	    (str = defread(defname)) == NULL)
		return (0);

	len = strlen(str) + 1;
	badp = alloca(len);
	(void) memset(badp, '\0', len);
	do {
		const char *q, *endp;
		tmp = priv_str_to_set(str, ",", &endp);
		if (tmp == NULL) {
			if (endp == NULL)
				break;

			/* Now remove the bad privilege endp points to */
			q = strchr(endp, ',');
			if (q == NULL)
				q = endp + strlen(endp);

			if (*badp != '\0')
				(void) strlcat(badp, ",", len);
			/* Memset above guarantees NUL termination */
			/* LINTED */
			(void) strncat(badp, endp, q - endp);
			/* excise bad privilege; strtok ignores 2x sep */
			(void) memmove((void *)endp, q, strlen(q) + 1);
		}
	} while (tmp == NULL && *str != '\0');

	if (tmp == NULL) {
		syslog(LOG_AUTH|LOG_ERR,
		    "pam_setcred: can't parse privilege specification: %m\n");
		return (-1);
	} else if (*badp != '\0') {
		syslog(LOG_AUTH|LOG_DEBUG,
		    "pam_setcred: unrecognized privilege(s): %s\n", badp);
	}
	*res = tmp;
	return (0);
}

/*
 *	unix_cred - pam_sm_setcred
 *
 *	Entry flags = 	PAM_ESTABLISH_CRED, set up Solaris Unix cred.
 *			PAM_DELETE_CRED, NOP, return PAM_SUCCESS.
 *			PAM_REINITIALIZE_CRED, set up Solaris Unix cred,
 *				or merge the current context with the new
 *				user.
 *			PAM_REFRESH_CRED, set up Solaris Unix cred.
 *			PAM_SILENT, print no messages to user.
 *
 *	Returns	PAM_SUCCESS, if all successful.
 *		PAM_CRED_ERR, if unable to set credentials.
 *		PAM_USER_UNKNOWN, if PAM_USER not set, or unable to find
 *			user in databases.
 *		PAM_SYSTEM_ERR, if no valid flag, or unable to get/set
 *			user's audit state.
 */

int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	int	i;
	int	debug = 0;
	uint_t	nowarn = flags & PAM_SILENT;
	int	ret = PAM_SUCCESS;
	char	*user;
	char	*ruser;
	char	*rhost;
	char	*tty;
	au_id_t	auid;
	adt_session_data_t *ah;
	adt_termid_t	*termid = NULL;
	userattr_t	*ua;
	priv_set_t	*lim, *def, *tset;
	char		messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	char		buf[PROJECT_BUFSZ];
	struct project	proj, *pproj;
	int		error;
	char		*projname;
	char		*kvs;
	struct passwd	pwd;
	char		pwbuf[NSS_BUFLEN_PASSWD];

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0)
			debug = 1;
		else if (strcmp(argv[i], "nowarn") == 0)
			nowarn |= 1;
	}

	if (debug)
		syslog(LOG_AUTH | LOG_DEBUG,
		    "pam_unix_cred: pam_sm_setcred(flags = %x, argc= %d)",
		    flags, argc);

	(void) pam_get_item(pamh, PAM_USER, (void **)&user);

	if (user == NULL || *user == '\0') {
		syslog(LOG_AUTH | LOG_ERR,
		    "pam_unix_cred: USER NULL or empty!\n");
		return (PAM_USER_UNKNOWN);
	}
	(void) pam_get_item(pamh, PAM_RUSER, (void **)&ruser);
	(void) pam_get_item(pamh, PAM_RHOST, (void **)&rhost);
	(void) pam_get_item(pamh, PAM_TTY, (void **)&tty);
	if (debug)
		syslog(LOG_AUTH | LOG_DEBUG,
		    "pam_unix_cred: user = %s, ruser = %s, rhost = %s, "
		    "tty = %s", user,
		    (ruser == NULL) ? "NULL" : (*ruser == '\0') ? "ZERO" :
		    ruser,
		    (rhost == NULL) ? "NULL" : (*rhost == '\0') ? "ZERO" :
		    rhost,
		    (tty == NULL) ? "NULL" : (*tty == '\0') ? "ZERO" :
		    tty);

	/* validate flags */
	switch (flags & (PAM_ESTABLISH_CRED | PAM_DELETE_CRED |
	    PAM_REINITIALIZE_CRED | PAM_REFRESH_CRED)) {
	case 0:
		/* set default flag */
		flags |= PAM_ESTABLISH_CRED;
		break;
	case PAM_ESTABLISH_CRED:
	case PAM_REINITIALIZE_CRED:
	case PAM_REFRESH_CRED:
		break;
	case PAM_DELETE_CRED:
		return (PAM_SUCCESS);
	default:
		syslog(LOG_AUTH | LOG_ERR,
		    "pam_unix_cred: invalid flags %x", flags);
		return (PAM_SYSTEM_ERR);
	}

	/*
	 * if auditing on and process audit state not set,
	 * setup audit context for process.
	 */
	if (adt_start_session(&ah, NULL, ADT_USE_PROC_DATA) != 0) {
		syslog(LOG_AUTH | LOG_ERR,
		    "pam_unix_cred: cannot create start audit session %m");
		return (PAM_SYSTEM_ERR);
	}
	adt_get_auid(ah, &auid);
	if (debug) {
		int	auditstate;

		if (auditon(A_GETCOND, (caddr_t)&auditstate,
		    sizeof (auditstate)) != 0) {
			auditstate = AUC_DISABLED;
		}
		syslog(LOG_AUTH | LOG_DEBUG,
		    "pam_unix_cred: state = %d, auid = %d", auditstate,
		    auid);
	}
	if (getpwnam_r(user, &pwd, pwbuf, sizeof (pwbuf)) == NULL) {
		syslog(LOG_AUTH | LOG_ERR,
		    "pam_unix_cred: cannot get passwd entry for user = %s",
		    user);
		ret = PAM_USER_UNKNOWN;
		goto adt_done;
	}

	if ((auid == AU_NOAUDITID) &&
	    (flags & PAM_ESTABLISH_CRED)) {
		struct passwd	rpwd;
		char	rpwbuf[NSS_BUFLEN_PASSWD];

		if ((rhost == NULL || *rhost == '\0')) {
			if (adt_load_ttyname(tty, &termid) != 0) {
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: cannot load ttyname %m");
				ret = PAM_SYSTEM_ERR;
				goto adt_done;
			}
		} else {
			if (adt_load_hostname(rhost, &termid) != 0) {
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: cannot load hostname %m");
				ret = PAM_SYSTEM_ERR;
				goto adt_done;
			}
		}
		if ((ruser != NULL) && (*ruser != '\0') &&
		    (getpwnam_r(ruser, &rpwd, rpwbuf,
		    sizeof (rpwbuf)) != NULL)) {
			/*
			 * set up the initial audit for user coming
			 * from another user
			 */
			if (adt_set_user(ah, rpwd.pw_uid, rpwd.pw_gid,
			    rpwd.pw_uid, rpwd.pw_gid, termid, ADT_NEW) != 0) {
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: cannot set ruser audit "
				    "%m");
				ret = PAM_SYSTEM_ERR;
				goto adt_done;
			}
			if (adt_set_user(ah, pwd.pw_uid, pwd.pw_gid,
			    pwd.pw_uid, pwd.pw_gid, NULL,
			    ADT_UPDATE) != 0) {
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: cannot merge user audit "
				    "%m");
				ret = PAM_SYSTEM_ERR;
				goto adt_done;
			}
			if (debug) {
				syslog(LOG_AUTH | LOG_DEBUG,
				    "pam_unix_cred: new audit set for %d:%d",
				    rpwd.pw_uid, pwd.pw_uid);
			}
		} else {
			/*
			 * No remote user or remote user is not a local
			 * user, no remote attribution, set up the initial
			 * audit as for direct user login
			 */
			if (adt_set_user(ah, pwd.pw_uid, pwd.pw_gid,
			    pwd.pw_uid, pwd.pw_gid, termid, ADT_NEW) != 0) {
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: cannot set user audit %m");
				ret = PAM_SYSTEM_ERR;
				goto adt_done;
			}
		}
		if (adt_set_proc(ah) != 0) {
			syslog(LOG_AUTH | LOG_ERR,
			    "pam_unix_cred: cannot set process audit %m");
			ret = PAM_CRED_ERR;
			goto adt_done;
		}
		if (debug) {
			syslog(LOG_AUTH | LOG_DEBUG,
			    "pam_unix_cred: new audit set for %d",
			    pwd.pw_uid);
		}
	} else if ((auid != AU_NOAUDITID) &&
	    (flags & PAM_REINITIALIZE_CRED)) {
		if (adt_set_user(ah, pwd.pw_uid, pwd.pw_gid, pwd.pw_uid,
		    pwd.pw_gid, NULL, ADT_UPDATE) != 0) {
			syslog(LOG_AUTH | LOG_ERR,
			    "pam_unix_cred: cannot set user audit %m");
			ret = PAM_SYSTEM_ERR;
			goto adt_done;
		}
		if (adt_set_proc(ah) != 0) {
			syslog(LOG_AUTH | LOG_ERR,
			    "pam_unix_cred: cannot set process audit %m");
			ret = PAM_CRED_ERR;
			goto adt_done;
		}
		if (debug) {
			syslog(LOG_AUTH | LOG_DEBUG,
			    "pam_unix_cred: audit merged for %d:%d",
			    auid, pwd.pw_uid);
		}
	} else if (debug) {
		syslog(LOG_AUTH | LOG_DEBUG,
		    "pam_unix_cred: audit already set for %d", auid);
	}
adt_done:
	if (termid != NULL)
		free(termid);
	if (adt_end_session(ah) != 0) {
		syslog(LOG_AUTH | LOG_ERR,
		    "pam_unix_cred: unable to end audit session");
	}

	if (ret != PAM_SUCCESS)
		return (ret);

	/* Initialize the user's project */
	(void) pam_get_item(pamh, PAM_RESOURCE, (void **)&kvs);
	if (kvs != NULL) {
		char *tmp, *lasts, *tok;

		kvs = tmp = strdup(kvs);
		if (kvs == NULL)
			return (PAM_BUF_ERR);

		while ((tok = strtok_r(tmp, ";", &lasts)) != NULL) {
			if (strncmp(tok, PROJECT, PROJSZ) == 0) {
				projname = tok + PROJSZ;
				break;
			}
			tmp = NULL;
		}
	} else {
		projname = NULL;
	}

	if (projname == NULL || *projname == '\0') {
		pproj = getdefaultproj(user, &proj, (void *)&buf,
		    PROJECT_BUFSZ);
	} else {
		pproj = getprojbyname(projname, &proj, (void *)&buf,
		    PROJECT_BUFSZ);
	}
	/* projname points into kvs, so this is the first opportunity to free */
	if (kvs != NULL)
		free(kvs);
	if (pproj == NULL) {
		syslog(LOG_AUTH | LOG_ERR,
		    "pam_unix_cred: no default project for user %s", user);
		if (!nowarn) {
			(void) snprintf(messages[0], sizeof (messages[0]),
			    dgettext(TEXT_DOMAIN, "No default project!"));
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
			    1, messages, NULL);
		}
		return (PAM_SYSTEM_ERR);
	}
	if ((error = setproject(proj.pj_name, user, TASK_NORMAL)) != 0) {
		kva_t *kv_array;

		switch (error) {
		case SETPROJ_ERR_TASK:
			if (errno == EAGAIN) {
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: project \"%s\" resource "
				    "control limit has been reached",
				    proj.pj_name);
				(void) snprintf(messages[0],
				    sizeof (messages[0]), dgettext(
				    TEXT_DOMAIN,
				    "Resource control limit has been "
				    "reached"));
			} else {
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: user %s could not join "
				    "project \"%s\": %m", user, proj.pj_name);
				(void) snprintf(messages[0],
				    sizeof (messages[0]), dgettext(
				    TEXT_DOMAIN,
				    "Could not join default project"));
			}
			if (!nowarn)
				(void) __pam_display_msg(pamh, PAM_ERROR_MSG, 1,
				    messages, NULL);
			break;
		case SETPROJ_ERR_POOL:
			(void) snprintf(messages[0], sizeof (messages[0]),
			    dgettext(TEXT_DOMAIN,
			    "Could not bind to resource pool"));
			switch (errno) {
			case EACCES:
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: project \"%s\" could not "
				    "bind to resource pool: No resource pool "
				    "accepting default bindings exists",
				    proj.pj_name);
				(void) snprintf(messages[1],
				    sizeof (messages[1]),
				    dgettext(TEXT_DOMAIN,
				    "No resource pool accepting "
				    "default bindings exists"));
				break;
			case ESRCH:
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: project \"%s\" could not "
				    "bind to resource pool: The resource pool "
				    "is unknown", proj.pj_name);
				(void) snprintf(messages[1],
				    sizeof (messages[1]),
				    dgettext(TEXT_DOMAIN,
				    "The specified resource pool "
				    "is unknown"));
				break;
			default:
				(void) snprintf(messages[1],
				    sizeof (messages[1]),
				    dgettext(TEXT_DOMAIN,
				    "Failure during pool binding"));
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: project \"%s\" could not "
				    "bind to resource pool: %m", proj.pj_name);
			}
			if (!nowarn)
				(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				    2, messages, NULL);
			break;
		default:
			/*
			 * Resource control assignment failed.  Unlike
			 * newtask(1m), we treat this as an error.
			 */
			if (error < 0) {
				/*
				 * This isn't supposed to happen, but in
				 * case it does, this error message
				 * doesn't use error as an index, like
				 * the others might.
				 */
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: unkwown error joining "
				    "project \"%s\" (%d)", proj.pj_name, error);
				(void) snprintf(messages[0],
				    sizeof (messages[0]),
				    dgettext(TEXT_DOMAIN,
				    "unkwown error joining project \"%s\""
				    " (%d)"), proj.pj_name, error);
			} else if ((kv_array = _str2kva(proj.pj_attr, KV_ASSIGN,
			    KV_DELIMITER)) != NULL) {
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: %s resource control "
				    "assignment failed for project \"%s\"",
				    kv_array->data[error - 1].key,
				    proj.pj_name);
				(void) snprintf(messages[0],
				    sizeof (messages[0]),
				    dgettext(TEXT_DOMAIN,
				    "%s resource control assignment failed for "
				    "project \"%s\""),
				    kv_array->data[error - 1].key,
				    proj.pj_name);
				_kva_free(kv_array);
			} else {
				syslog(LOG_AUTH | LOG_ERR,
				    "pam_unix_cred: resource control "
				    "assignment failed for project \"%s\""
				    "attribute %d", proj.pj_name, error);
				(void) snprintf(messages[0],
				    sizeof (messages[0]),
				    dgettext(TEXT_DOMAIN,
				    "resource control assignment failed for "
				    "project \"%s\" attribute %d"),
				    proj.pj_name, error);
			}
			if (!nowarn)
				(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				    1, messages, NULL);
		}
		return (PAM_SYSTEM_ERR);
	}

	ua = getusernam(user);

	(void) defopen(AUTH_POLICY);

	tset = def = lim = NULL;

	if (getset(USERATTR_LIMPRIV_KW, DEF_LIMITPRIV, ua, &lim) != 0 ||
	    getset(USERATTR_DFLTPRIV_KW, DEF_DFLTPRIV, ua, &def) != 0) {
		ret = PAM_SYSTEM_ERR;
		goto out;
	}

	if (def == NULL) {
		def = priv_str_to_set("basic", ",", NULL);
		if (pathconf("/", _PC_CHOWN_RESTRICTED) == -1)
			(void) priv_addset(def, PRIV_FILE_CHOWN_SELF);
	}
	/*
	 * Silently limit the privileges to those actually available
	 * in the current zone.
	 */
	tset = priv_allocset();
	if (tset == NULL) {
		ret = PAM_SYSTEM_ERR;
		goto out;
	}
	if (getppriv(PRIV_PERMITTED, tset) != 0) {
		ret = PAM_SYSTEM_ERR;
		goto out;
	}
	if (!priv_issubset(def, tset))
		priv_intersect(tset, def);
	/*
	 * We set privilege awareness here so that I gets copied to
	 * P & E when the final setuid(uid) happens.
	 */
	(void) setpflags(PRIV_AWARE, 1);
	if (setppriv(PRIV_SET, PRIV_INHERITABLE, def) != 0) {
		syslog(LOG_AUTH | LOG_ERR,
			    "pam_setcred: setppriv(defaultpriv) failed: %m");
		ret = PAM_CRED_ERR;
	}

	if (lim != NULL) {
		/*
		 * Silently limit the privileges to the limit set available.
		 */
		if (getppriv(PRIV_LIMIT, tset) != 0) {
			ret = PAM_SYSTEM_ERR;
			goto out;
		}
		if (!priv_issubset(lim, tset))
			priv_intersect(tset, lim);
		/*
		 * In order not to suprise certain applications, we
		 * need to retain privilege awareness and thus we must
		 * also set P and E.
		 */
		if (setppriv(PRIV_SET, PRIV_LIMIT, lim) != 0 ||
		    setppriv(PRIV_SET, PRIV_PERMITTED, lim) != 0) {
			syslog(LOG_AUTH | LOG_ERR,
				"pam_setcred: setppriv(limitpriv) failed: %m");
			ret = PAM_CRED_ERR;
		}
	}
	(void) setpflags(PRIV_AWARE, 0);

out:
	(void) defopen(NULL);

	if (ua != NULL)
		free_userattr(ua);
	if (lim != NULL)
		priv_freeset(lim);
	if (def != NULL)
		priv_freeset(def);
	if (tset != NULL)
		priv_freeset(tset);

	return (ret);
}
