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

#pragma ident	"@(#)ns_confmgr.c	1.9	06/06/01 SMI"

/* libsldap - cachemgr side configuration components */

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <libintl.h>
#include <string.h>
#include <ctype.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <locale.h>
#include <errno.h>
#include <sys/time.h>

#include "ns_sldap.h"
#include "ns_internal.h"
#include "ns_cache_door.h"

#define	ALWAYS		1


/*
 * **************************************************************
 * Configuration File Routines
 * **************************************************************
 */


/* Size of the errstr buffer needs to be MAXERROR */
static int
read_line(FILE *fp, char *buffer, int buflen, char *errstr)
{
	int	linelen;
	char	c;

	*errstr = '\0';

	for (linelen = 0; linelen < buflen; ) {
		c = getc(fp);
		if (c == EOF)
			break;
		switch (c) {
		case '\n':
		    if (linelen > 0 && buffer[linelen - 1] == '\\') {
			/* Continuation line found */
			--linelen;
		    } else {
			/* end of line found */
			buffer[linelen] = '\0';
			return (linelen);
		    }
		    break;
		default:
		    buffer[linelen++] = c;
		}
	}

	if (linelen >= buflen) {
		(void) snprintf(errstr, MAXERROR,
			gettext("Buffer overflow, line too long."));
		return (-2);
	} else if (linelen > 0 && buffer[linelen - 1] == '\\') {
		(void) snprintf(errstr, MAXERROR,
			gettext("Unterminated continuation line."));
		return (-2);
	} else {
		/* end of file */
		buffer[linelen] = '\0';
	}
	return (linelen > 0 ? linelen : -1);
}


static ns_parse_status
read_file(ns_config_t *ptr, int cred_file, ns_ldap_error_t **error)
{
	ParamIndexType	i = 0;
	char		errstr[MAXERROR];
	char		buffer[BUFSIZE], *name, *value;
	int		emptyfile, lineno;
	FILE		*fp;
	int		ret;
	int		linelen;
	char		*file;
	int		first = 1;


	if (cred_file) {
		file = NSCREDFILE;
	} else {
		file = NSCONFIGFILE;
	}
	fp = fopen(file, "rF");
	if (fp == NULL) {
		(void) snprintf(errstr, sizeof (errstr),
			gettext("Unable to open filename '%s' "
			"for reading (errno=%d)."), file, errno);
		MKERROR(LOG_ERR, *error, NS_CONFIG_FILE, strdup(errstr), NULL);
		return (NS_NOTFOUND);
	}

	emptyfile = 1;
	lineno = 0;
	for (; ; ) {
		if ((linelen = read_line(fp, buffer, sizeof (buffer),
			errstr)) < 0)
			/* End of file */
			break;
		lineno++;
		if (linelen == 0)
			continue;
		/* get rid of comment lines */
		if (buffer[0] == '#')
			continue;
		emptyfile = 0;
		name = NULL;
		value = NULL;
		__s_api_split_key_value(buffer, &name, &value);
		if (name == NULL || value == NULL) {
			(void) snprintf(errstr, sizeof (errstr),
			    gettext("Missing Name or Value on line %d."),
				    lineno);
			MKERROR(LOG_ERR, *error, NS_CONFIG_SYNTAX,
				strdup(errstr), NULL);
			(void) fclose(fp);
			return (NS_PARSE_ERR);
		}
		if (__s_api_get_versiontype(ptr, name, &i) != 0) {
			(void) snprintf(errstr, sizeof (errstr),
			    gettext("Illegal profile type on line %d."),
				    lineno);
			MKERROR(LOG_ERR, *error, NS_CONFIG_SYNTAX,
				strdup(errstr), NULL);
			(void) fclose(fp);
			return (NS_PARSE_ERR);
		}
		if (!first && i == NS_LDAP_FILE_VERSION_P) {
			(void) snprintf(errstr, sizeof (errstr),
				gettext("Illegal NS_LDAP_FILE_VERSION "
				"on line %d."), lineno);
			MKERROR(LOG_ERR, *error, NS_CONFIG_SYNTAX,
				strdup(errstr), NULL);
			(void) fclose(fp);
			return (NS_PARSE_ERR);
		}
		first = 0;
		switch (__s_api_get_configtype(i)) {
		case SERVERCONFIG:
		case CLIENTCONFIG:
			if (cred_file == 0) {
				ret = __ns_ldap_setParamValue(ptr, i, value,
					error);
				if (ret != NS_SUCCESS) {
					(void) fclose(fp);
					return (ret);
				}
			} else if (i != NS_LDAP_FILE_VERSION_P) {
				(void) snprintf(errstr, sizeof (errstr),
					gettext("Illegal entry in '%s' on "
					"line %d"), file, lineno);
				MKERROR(LOG_ERR, *error, NS_CONFIG_SYNTAX,
					strdup(errstr), NULL);
				(void) fclose(fp);
				return (NS_PARSE_ERR);
			}
			break;
		case CREDCONFIG:
			if (i == NS_LDAP_FILE_VERSION_P)
				break;
			if (cred_file) {
				ret = __ns_ldap_setParamValue(ptr, i, value,
					error);
				if (ret != NS_SUCCESS) {
					(void) fclose(fp);
					return (ret);
				}
			} else {
				(void) snprintf(errstr, sizeof (errstr),
					gettext("Illegal entry in '%s' on "
					"line %d"), file, lineno);
				MKERROR(LOG_ERR, *error, NS_CONFIG_SYNTAX,
					strdup(errstr), NULL);
				(void) fclose(fp);
				return (NS_PARSE_ERR);
			}
		}
	}
	(void) fclose(fp);
	if (!cred_file && emptyfile) {
		/* Error in read_line */
		(void) snprintf(errstr, sizeof (errstr),
			gettext("Empty config file: '%s'"), file);
		MKERROR(LOG_ERR, *error, NS_CONFIG_SYNTAX, strdup(errstr),
			NULL);
		return (NS_PARSE_ERR);
	}
	if (linelen == -2) {
		/* Error in read_line */
		(void) snprintf(errstr, sizeof (errstr),
			gettext("Line too long in '%s'"), file);
		MKERROR(LOG_ERR, *error, NS_CONFIG_SYNTAX, strdup(errstr),
			NULL);
		return (NS_PARSE_ERR);
	}
	return (NS_SUCCESS);
}


/*
 * Cache Manager side of configuration file loading
 */

ns_ldap_error_t *
__ns_ldap_LoadConfiguration()
{
	ns_ldap_error_t	*error = NULL;
	ns_config_t	*ptr = NULL;
	char		errstr[MAXERROR];
	ns_parse_status	ret;


	ptr = __s_api_create_config();
	if (ptr == NULL) {
		(void) snprintf(errstr, sizeof (errstr),
			gettext("__ns_ldap_LoadConfiguration: "
				"Out of memory."));
		MKERROR(LOG_ERR, error, NS_CONFIG_NOTLOADED,
			strdup(errstr), NULL);
		return (error);
	}

	/* Load in Configuration file */
	ret = read_file(ptr, 0, &error);
	if (ret != NS_SUCCESS) {
		__s_api_destroy_config(ptr);
		return (error);
	}

	/* Load in Credential file */
	ret = read_file(ptr, 1, &error);
	if (ret != NS_SUCCESS) {
		__s_api_destroy_config(ptr);
		return (error);
	}

	if (__s_api_crosscheck(ptr, errstr, B_TRUE) != NS_SUCCESS) {
		__s_api_destroy_config(ptr);
		MKERROR(LOG_ERR, error, NS_CONFIG_SYNTAX, strdup(errstr), NULL);
		return (error);
	}

	__s_api_init_config(ptr);
	return (NULL);
}


static int
_print2buf(LineBuf *line, char *toprint, int addsep)
{
	int	newsz = 0;
	int	newmax = 0;
	char	*str;

	if (line == NULL)
		return (-1);

	newsz = strlen(toprint) + line->len + 1;
	if (addsep) {
		newsz += strlen(DOORLINESEP);
	}
	if (line->alloc == 0 || newsz > line->alloc) {
		/* Round up to next buffer and add 1 */
		newmax = (((newsz+(BUFSIZ-1))/BUFSIZ)+1) * BUFSIZ;
		if (line->alloc == 0)
			line->str = (char *)calloc(newmax, 1);
		else {
			/*
			 * if realloc() returns NULL,
			 * the original buffer is untouched.
			 * It needs to be freed.
			 */
			str = (char *)realloc(line->str, newmax);
			if (str == NULL) {
				free(line->str);
				line->str = NULL;
			}
			else
				line->str = str;
		}
		line->alloc = newmax;
		if (line->str == NULL) {
			line->alloc = 0;
			line->len = 0;
			return (-1);
		}
	}
	/* now add new 'toprint' data to buffer */
	(void) strlcat(line->str, toprint, line->alloc);
	if (addsep) {
		(void) strlcat(line->str, DOORLINESEP, line->alloc);
	}
	line->len = newsz;
	return (0);
}


/*
 * __ns_ldap_LoadDoorInfo is a routine used by the ldapcachemgr
 * to create a configuration buffer to transmit back to a client
 * domainname is transmitted to ldapcachemgr and ldapcachemgr uses
 * it to select a configuration to transmit back.  Otherwise it
 * is essentially unused in sldap.
 */

ns_ldap_error_t *
__ns_ldap_LoadDoorInfo(LineBuf *configinfo, char *domainname)
{
	ns_config_t	*ptr;
	char		errstr[MAXERROR];
	ns_ldap_error_t	*errorp;
	char		string[BUFSIZE];
	char		*str;
	ParamIndexType	i = 0;

	ptr = __s_api_get_default_config();
	if (ptr == NULL) {
		(void) snprintf(errstr, sizeof (errstr),
		    gettext("No configuration information available for %s."),
		    domainname == NULL ? "<no domain specified>" : domainname);
		MKERROR(LOG_WARNING, errorp, NS_CONFIG_NOTLOADED,
			strdup(errstr), NULL);
		return (errorp);
	}
	(void) memset((char *)configinfo, 0, sizeof (LineBuf));
	for (i = 0; i <= NS_LDAP_MAX_PIT_P; i++) {
		str = __s_api_strValue(ptr, string, sizeof (string),
			i, NS_DOOR_FMT);
		if (str == NULL)
			continue;
		if (_print2buf(configinfo, str, 1) != 0) {
			(void) snprintf(errstr, sizeof (errstr),
				gettext("_print2buf: Out of memory."));
			MKERROR(LOG_WARNING, errorp, NS_CONFIG_NOTLOADED,
				strdup(errstr), NULL);
			__s_api_release_config(ptr);
			if (str != (char *)&string[0]) {
				free(str);
				str = NULL;
			}
			return (errorp);
		}
		if (str != (char *)&string[0]) {
			free(str);
			str = NULL;
		}
	}
	__s_api_release_config(ptr);
	return (NULL);
}


ns_ldap_error_t *
__ns_ldap_DumpLdif(char *filename)
{
	ns_config_t	*ptr;
	char		errstr[MAXERROR];
	ns_ldap_error_t	*errorp;
	char		string[BUFSIZE];
	char		*str;
	FILE		*fp;
	ParamIndexType	i = 0;
	char		*profile, *container, *base;

	ptr = __s_api_get_default_config();
	if (ptr == NULL) {
		(void) snprintf(errstr, sizeof (errstr),
		    gettext("No configuration information available."));
		MKERROR(LOG_ERR, errorp, NS_CONFIG_NOTLOADED, strdup(errstr),
			NULL);
		return (errorp);
	}

	if (filename == NULL) {
		fp = stdout;
	} else {
		fp = fopen(filename, "wF");
		if (fp == NULL) {
			(void) snprintf(errstr, sizeof (errstr),
				gettext("Unable to open filename %s for ldif "
				"dump (errno=%d)."), filename, errno);
			MKERROR(LOG_WARNING, errorp, NS_CONFIG_FILE,
				strdup(errstr), NULL);
			__s_api_release_config(ptr);
			return (errorp);
		}
		(void) fchmod(fileno(fp), 0444);
	}

	if (ptr->paramList[NS_LDAP_SEARCH_BASEDN_P].ns_ptype != CHARPTR ||
	    ptr->paramList[NS_LDAP_PROFILE_P].ns_ptype != CHARPTR) {
		(void) snprintf(errstr, sizeof (errstr),
			gettext("Required BaseDN and/or Profile name "
				"ldif fields not present"));
		MKERROR(LOG_WARNING, errorp, NS_CONFIG_FILE, strdup(errstr),
			NULL);
		__s_api_release_config(ptr);
		return (errorp);
	}

	profile = ptr->paramList[NS_LDAP_PROFILE_P].ns_pc;
	base = ptr->paramList[NS_LDAP_SEARCH_BASEDN_P].ns_pc;
	container = _PROFILE_CONTAINER;

	/*
	 * Construct DN, but since this is the profile, there is no need
	 * to worry about mapping.  The profile itself can not be mapped
	 */
	(void) fprintf(fp, "dn: cn=%s,ou=%s,%s\n", profile, container, base);

	/* dump objectclass names */
	if (ptr->version == NS_LDAP_V1) {
		(void) fprintf(fp,
			"ObjectClass: top\nObjectClass: %s\n",
			_PROFILE1_OBJECTCLASS);
	} else {
		(void) fprintf(fp,
			"ObjectClass: top\n"
			"ObjectClass: %s\n",
			_PROFILE2_OBJECTCLASS);
	}

	/* For each parameter - construct value */
	for (i = 0; i <= NS_LDAP_MAX_PIT_P; i++) {
		str = __s_api_strValue(ptr, string, BUFSIZ, i, NS_LDIF_FMT);
		if (str == NULL)
			continue;
		/*
		 * don't dump binddn, bind password, or cert path as they
		 * are not part of version 2 profiles
		 */
		if ((i != NS_LDAP_BINDDN_P) && (i != NS_LDAP_BINDPASSWD_P) &&
				(i != NS_LDAP_HOST_CERTPATH_P))
			(void) fprintf(fp, "%s\n", str);
		if (str != (char *)&string[0]) {
			free(str);
			str = NULL;
		}
	}

	if (filename != NULL)
		(void) fclose(fp);

	__s_api_release_config(ptr);
	return (NULL);
}

/*
 * This routine can process the configuration  and/or
 * the credential files at the same time.
 * files is char *[3] = { "config", "cred", NULL };
 */

static
ns_ldap_error_t *
__ns_ldap_DumpConfigFiles(char **files)
{
	char		*filename;
	int		fi;
	int		docred;
	ns_config_t	*ptr;
	char		string[BUFSIZE];
	char		*str;
	char		errstr[MAXERROR];
	ParamIndexType	i = 0;
	FILE		*fp;
	int		rc;
	ns_ldap_error_t	*errorp;
	struct stat	buf;
	int		cfgtype;

	ptr = __s_api_get_default_config();
	if (ptr == NULL) {
		(void) snprintf(errstr, sizeof (errstr),
			gettext("No configuration information available."));
		MKERROR(LOG_ERR, errorp, NS_CONFIG_NOTLOADED, strdup(errstr),
			NULL);
		return (errorp);
	}

	for (fi = 0; fi < 2; fi++) {
		docred = 0;
		filename = files[fi];
		if (filename == NULL)
			continue;
		if (fi == 1)
			docred++;
		rc = stat(filename, &buf);
		fp = fopen(filename, "wF");
		if (fp == NULL) {
			(void) snprintf(errstr, sizeof (errstr),
				gettext("Unable to open filename %s"
				" for configuration dump (errno=%d)."),
				filename, errno);
			MKERROR(LOG_WARNING, errorp, NS_CONFIG_FILE,
				strdup(errstr), NULL);
			__s_api_release_config(ptr);
			return (errorp);
		}
		if (rc == 0)
			(void) fchmod(fileno(fp), buf.st_mode);
		else
			(void) fchmod(fileno(fp), 0400);
		(void) fprintf(fp, "#\n# %s\n#\n", DONOTEDIT);

		/* assume VERSION is set and it outputs first */

		/* For each parameter - construct value */
		for (i = 0; i <= NS_LDAP_MAX_PIT_P; i++) {
			cfgtype = __s_api_get_configtype(i);
			if ((docred == 0 && cfgtype == CREDCONFIG) ||
				(docred == 1 && cfgtype != CREDCONFIG))
				continue;

			str = __s_api_strValue(ptr, string, BUFSIZ,
					i, NS_FILE_FMT);
			if (str == NULL)
				continue;
			(void) fprintf(fp, "%s\n", str);
			if (str != (char *)&string[0]) {
				free(str);
				str = NULL;
			}
		}
		(void) fclose(fp);
	}

	__s_api_release_config(ptr);
	return (NULL);
}

ns_ldap_error_t *
__ns_ldap_DumpConfiguration(char *file)
{
	ns_ldap_error_t	*ret;
	char		*files[3];

	files[0] = NULL;
	files[1] = NULL;
	files[2] = NULL;
	if (strcmp(file, NSCONFIGFILE) == 0) {
		files[0] = file;
	} else if (strcmp(file, NSCONFIGREFRESH) == 0) {
		files[0] = file;
	} else if (strcmp(file, NSCREDFILE) == 0) {
		files[1] = file;
	} else if (strcmp(file, NSCREDREFRESH) == 0) {
		files[1] = file;
	}
	ret = __ns_ldap_DumpConfigFiles(files);
	return (ret);
}

/*
 * **************************************************************
 * Misc Routines
 * **************************************************************
 */

ns_config_t *
__ns_ldap_make_config(ns_ldap_result_t *result)
{
	int		l, m;
	char		val[BUFSIZ];
	char    	*attrname;
	ns_ldap_entry_t	*entry;
	ns_ldap_attr_t	*attr;
	char		**attrval;
	ParamIndexType	index;
	ns_config_t	*ptr;
	ns_ldap_error_t	*error = NULL;
	int		firsttime;
	int		prof_ver;
	ns_config_t	*curr_ptr = NULL;
	char		errstr[MAXERROR];
	ns_ldap_error_t	*errorp;

	if (result == NULL)
		return (NULL);

	if (result->entries_count > 1) {
		(void) snprintf(errstr, MAXERROR,
			gettext("Configuration Error: More than"
				" one profile found"));
		MKERROR(LOG_ERR, errorp, NS_PARSE_ERR, strdup(errstr), NULL);
		(void) __ns_ldap_freeError(&errorp);
		return (NULL);
	}

	ptr = __s_api_create_config();
	if (ptr == NULL)
		return (NULL);

	curr_ptr = __s_api_get_default_config();
	if (curr_ptr == NULL) {
		__s_api_destroy_config(ptr);
		return (NULL);
	}

	/* Check to see if the profile is version 1 or version 2 */
	prof_ver = 1;
	entry = result->entry;
	for (l = 0; l < entry->attr_count; l++) {
		attr = entry->attr_pair[l];

		attrname = attr->attrname;
		if (attrname == NULL)
			continue;
		if (strcasecmp(attrname, "objectclass") == 0) {
			for (m = 0; m < attr->value_count; m++) {
				if (strcasecmp(_PROFILE2_OBJECTCLASS,
					attr->attrvalue[m]) == 0) {
					prof_ver = 2;
					break;
				}
			}
		}
	}
	/* update the configuration to accept v1 or v2 attributes */
	if (prof_ver == 1) {
		(void) strcpy(val, NS_LDAP_VERSION_1);
		(void) __ns_ldap_setParamValue(ptr,
				NS_LDAP_FILE_VERSION_P, val, &error);
	} else {
		(void) strcpy(val, NS_LDAP_VERSION_2);
		(void) __ns_ldap_setParamValue(ptr,
				NS_LDAP_FILE_VERSION_P, val, &error);
	}

	for (l = 0; l < entry->attr_count; l++) {
		attr = entry->attr_pair[l];

		attrname = attr->attrname;
		if (attrname == NULL)
			continue;
		if (__s_api_get_profiletype(attrname, &index) != 0)
			continue;

		attrval = attr->attrvalue;
		switch (index) {
		case NS_LDAP_SEARCH_DN_P:
		case NS_LDAP_SERVICE_SEARCH_DESC_P:
		case NS_LDAP_ATTRIBUTEMAP_P:
		case NS_LDAP_OBJECTCLASSMAP_P:
		case NS_LDAP_SERVICE_CRED_LEVEL_P:
		case NS_LDAP_SERVICE_AUTH_METHOD_P:
			/* Multiple Value - insert 1 at a time */
			for (m = 0; m < attr->value_count; m++) {
				(void) __ns_ldap_setParamValue(ptr, index,
						attrval[m], &error);
			}
			break;
		default:
			firsttime = 1;
			/* Single or Multiple Value */
			val[0] = '\0';
			for (m = 0; m < attr->value_count; m++) {
				if (firsttime == 1) {
					firsttime = 0;
					(void) strlcpy(val, attrval[m],
						sizeof (val));
				} else {
					(void) strlcat(val, " ", sizeof (val));
					(void) strlcat(val, attrval[m],
						sizeof (val));
				}
			}
			(void) __ns_ldap_setParamValue(ptr, index, val,
						&error);
			break;
		}
	}
	if (ptr->version != NS_LDAP_V1) {
	    if (curr_ptr->paramList[NS_LDAP_BINDDN_P].ns_ptype == CHARPTR) {
		(void) __ns_ldap_setParamValue(ptr, NS_LDAP_BINDDN_P,
			curr_ptr->paramList[NS_LDAP_BINDDN_P].ns_pc, &error);
	    }
	    if (curr_ptr->paramList[NS_LDAP_BINDPASSWD_P].ns_ptype == CHARPTR) {
		(void) __ns_ldap_setParamValue(ptr, NS_LDAP_BINDPASSWD_P,
			curr_ptr->paramList[NS_LDAP_BINDPASSWD_P].ns_pc,
			&error);
	    }
	    if (curr_ptr->paramList[NS_LDAP_HOST_CERTPATH_P].ns_ptype ==
			CHARPTR) {
		(void) __ns_ldap_setParamValue(ptr, NS_LDAP_HOST_CERTPATH_P,
			curr_ptr->paramList[NS_LDAP_HOST_CERTPATH_P].ns_pc,
			&error);
	    }
	}
	__s_api_release_config(curr_ptr);
	return (ptr);
}

/*
 * Download a profile into our internal structure.  The calling application
 * needs to DumpConfig() to save the information to NSCONFIGFILE and NSCREDFILE
 * if desired.
 */
int
__ns_ldap_download(const char *profile, char *addr, char *baseDN,
	ns_ldap_error_t **errorp)
{
	char filter[BUFSIZ];
	int rc;
	ns_ldap_result_t *result = NULL;
	ns_config_t	*ptr = NULL;
	ns_config_t	*new_ptr = NULL;
	char		errstr[MAXERROR];

	*errorp = NULL;
	if (baseDN == NULL)
		return (NS_LDAP_INVALID_PARAM);

	ptr = __s_api_get_default_config();
	if (ptr == NULL) {
		(void) snprintf(errstr, sizeof (errstr),
		    gettext("No configuration information available."));
		MKERROR(LOG_ERR, *errorp, NS_CONFIG_NOTLOADED, strdup(errstr),
			NULL);
		return (NS_LDAP_CONFIG);
	}

	rc = __ns_ldap_setParamValue(ptr, NS_LDAP_SEARCH_BASEDN_P,
				baseDN, errorp);
	if (rc != NS_LDAP_SUCCESS) {
		__s_api_release_config(ptr);
		return (rc);
	}

	rc = __ns_ldap_setParamValue(ptr, NS_LDAP_SERVERS_P, addr, errorp);
	__s_api_release_config(ptr);
	if (rc != NS_LDAP_SUCCESS)
		return (rc);

	(void) snprintf(filter, sizeof (filter), _PROFILE_FILTER,
			_PROFILE1_OBJECTCLASS,
			_PROFILE2_OBJECTCLASS,
			profile);
	rc = __ns_ldap_list(_PROFILE_CONTAINER, (const char *)filter,
		NULL, NULL, NULL, 0, &result, errorp, NULL, NULL);

	if (rc != NS_LDAP_SUCCESS)
		return (rc);

	new_ptr = __ns_ldap_make_config(result);
	(void) __ns_ldap_freeResult(&result);

	if (new_ptr == NULL)
		return (NS_LDAP_OP_FAILED);

	rc = __s_api_crosscheck(new_ptr, errstr, B_FALSE);
	if (rc != NS_LDAP_SUCCESS) {
		__s_api_destroy_config(new_ptr);
		MKERROR(LOG_ERR, *errorp, NS_CONFIG_NOTLOADED, strdup(errstr),
			NULL);
		return (NS_LDAP_CONFIG);
	}

	__s_api_init_config(new_ptr);
	return (rc);
}

/*
 * **************************************************************
 * Configuration Printing Routines
 * **************************************************************
 */

/*
 * Yes the use of stdio is okay here because all we are doing is sending
 * output to stdout.  This would not be necessary if we could get to the
 * configuration pointer outside this file.
 */
ns_ldap_error_t *
__ns_ldap_print_config(int verbose)
{
	ns_config_t	*ptr;
	char		errstr[MAXERROR];
	ns_ldap_error_t *errorp;
	char		string[BUFSIZE];
	char		*str;
	int		i;

	ptr = __s_api_get_default_config();
	if (ptr == NULL) {
		errorp = __ns_ldap_LoadConfiguration();
		if (errorp != NULL)
			return (errorp);
		ptr = __s_api_get_default_config();
		if (ptr == NULL) {
			(void) snprintf(errstr, sizeof (errstr),
			    gettext("No configuration information."));
			MKERROR(LOG_WARNING, errorp, NS_CONFIG_NOTLOADED,
				strdup(errstr), NULL);
			return (errorp);
		}
	}

	if (verbose && (ptr->domainName != NULL)) {
		(void) fputs("ptr->domainName ", stdout);
		(void) fputs(ptr->domainName, stdout);
		(void) putchar('\n');
	}
	/* For each parameter - construct value */
	for (i = 0; i <= NS_LDAP_MAX_PIT_P; i++) {
			/*
			 * Version 1 skipped this entry because:
			 *
			 * don't print default cache TTL for now since
			 * we don't store it in the ldap_client_file.
			 */
		if ((i == NS_LDAP_CACHETTL_P) && (ptr->version == NS_LDAP_V1))
			continue;

		str = __s_api_strValue(ptr, string, BUFSIZ, i, NS_FILE_FMT);
		if (str == NULL)
			continue;
		if (verbose)
			(void) putchar('\t');
		(void) fprintf(stdout, "%s\n", str);
		if (str != (char *)&string[0]) {
			free(str);
			str = NULL;
		}
	}

	__s_api_release_config(ptr);
	return (NULL);
}
