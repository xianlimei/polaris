/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)kdb5_util.c	1.11	05/09/26 SMI"

/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 *	Openvision retains the copyright to derivative works of
 *	this source code.  Do *NOT* create a derivative of this
 *	source code before consulting with your legal department.
 *	Do *NOT* integrate *ANY* of this source code into another
 *	product before consulting with your legal department.
 *
 *	For further information, read the top-level Openvision
 *	copyright which is contained in the top-level MIT Kerberos
 *	copyright.
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 */


/*
 * admin/edit/kdb5_edit.c
 *
 * (C) Copyright 1990,1991, 1996 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * Edit a KDC database.
 */

/*
 *  Yes, I know this is a hack, but we need admin.h without including the
 *  rpc.h header. Additionally, our rpc.h header brings in
 *  a des.h header which causes other problems.
 */
#define	_RPC_RPC_H

#include <stdio.h>
#define KDB5_DISPATCH
#define KRB5_KDB5_DBM__
#include <k5-int.h>
/* #define these to avoid an indirection function; for future implementations,
   these may be redirected from a dispatch table/routine */
#define krb5_dbm_db_set_name krb5_db_set_name
#define krb5_dbm_db_set_nonblocking krb5_db_set_nonblocking
#define krb5_dbm_db_init krb5_db_init
#define krb5_dbm_db_get_age krb5_db_get_age
#define krb5_dbm_db_create krb5_db_create
#define krb5_dbm_db_rename krb5_db_rename
#define krb5_dbm_db_get_principal krb5_db_get_principal
#define krb5_dbm_db_free_principal krb5_db_free_principal
#define krb5_dbm_db_put_principal krb5_db_put_principal
#define krb5_dbm_db_delete_principal krb5_db_delete_principal
#define krb5_dbm_db_lock krb5_db_lock
#define krb5_dbm_db_unlock krb5_db_unlock
#define krb5_dbm_db_set_lockmode krb5_db_set_lockmode
#define krb5_dbm_db_close_database krb5_db_close_database
#define krb5_dbm_db_open_database krb5_db_open_database

#include <kadm5/admin.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <kadm5/adb.h>
#include <time.h>
#include <libintl.h>
#include <locale.h>
#include "kdb5_util.h"

char	*Err_no_master_msg = "Master key not entered!\n";
char	*Err_no_database = "Database not currently opened!\n";

/*
 * XXX Ick, ick, ick.  These global variables shouldn't be global....
 */
char *mkey_password = 0;

/*
 * I can't figure out any way for this not to be global, given how ss
 * works.
 */

int exit_status = 0;
krb5_context util_context;
osa_adb_policy_t policy_db;
kadm5_config_params global_params;

void
usage()
{
	fprintf(stderr, "%s: "
	   "kdb5_util cmd [-r realm] [-d dbname] [-k mkeytype] [-M mkeyname]\n"
	    "\t         [-f] [stashfile] [-P password] [-m ] [cmd options]\n"
	    "\tcreate	[-s]\n"
	    "\tdestroy	\n"
	    "\tstash	\n"
	    "\tdump	[-old] [-ov] [-b6] [-verbose] [filename	[princs...]]\n"
	    "\tload	[-old] [-ov] [-b6] [-verbose] [-update] filename\n"
#ifdef SUNWOFF
	    "\tload_v4	[-t] [-n] [-v] [-K] [-s stashfile] inputfile\n"
#endif
	    "\tark	[-e etype_list] principal\n",
	    gettext("Usage"));
     exit(1);	    
}

krb5_keyblock master_key;
extern krb5_principal master_princ;
krb5_db_entry master_entry;
int	valid_master_key = 0;
int     close_policy_db = 0;

char *progname;
krb5_boolean manual_mkey = FALSE;
krb5_boolean dbactive = FALSE;

int kdb5_create(int, char **);
int kdb5_destroy(int, char **);
int kdb5_stash(int, char **);
int dump_db(int, char **);
int load_db(int, char **);
int open_db_and_mkey();
int add_random_key(int, char **);
   
typedef int (*cmd_func)(int, char **);

struct _cmd_table {
     char *name;
     cmd_func func;
     int opendb;
} cmd_table[] = {
     "create", kdb5_create, 0,
     "destroy", kdb5_destroy, 1,
     "stash", kdb5_stash, 1,
     "dump", dump_db, 1,
     "load", load_db, 0,
     "ark", add_random_key, 1,
     NULL, NULL, 0,
};

struct _cmd_table *
cmd_lookup(name)
   char *name;
{
     struct _cmd_table *cmd = cmd_table;

     while (cmd->name) {
	  if (strcmp(cmd->name, name) == 0)
			return (cmd);
	  else
	       cmd++;
     }
     
	return (NULL);
}

#define ARG_VAL (--argc > 0 ? (optarg = *(++argv)) : (char *)(usage(), NULL))
     
int
main(argc, argv)
    int argc;
    char *argv[];
{
    struct _cmd_table *cmd = NULL;
    char *optarg, **cmd_argv;
    int cmd_argc;
    krb5_error_code retval;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)  /* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"	/* Use this only if it weren't */
#endif

	(void) textdomain(TEXT_DOMAIN);

	Err_no_master_msg = gettext("Master key not entered!\n");
	Err_no_database = gettext("Database not currently opened!\n");

    retval = krb5_init_context(&util_context);
    if (retval) {
	    com_err (progname, retval, 
		gettext("while initializing Kerberos code"));
	    exit(1);
    }
	progname = (strrchr(argv[0], '/') ?
		    strrchr(argv[0], '/') + 1 : argv[0]);

    cmd_argv = (char **) malloc(sizeof(char *)*argc);
    if (cmd_argv == NULL) {
		com_err(progname, ENOMEM,
		    gettext("while creating sub-command arguments"));
	 exit(1);
    }
    memset(cmd_argv, 0, sizeof(char *)*argc);
    cmd_argc = 1;
	 
	argv++;
	argc--;
    while (*argv) {
       if (strcmp(*argv, "-P") == 0 && ARG_VAL) {
	    mkey_password = optarg;
	    manual_mkey = TRUE;
       } else if (strcmp(*argv, "-d") == 0 && ARG_VAL) {
	    global_params.dbname = optarg;
	    global_params.mask |= KADM5_CONFIG_DBNAME;
       } else if (strcmp(*argv, "-r") == 0 && ARG_VAL) {
	    global_params.realm = optarg;
	    global_params.mask |= KADM5_CONFIG_REALM;
	    /* not sure this is really necessary */
	    if ((retval = krb5_set_default_realm(util_context,
						 global_params.realm))) {
				com_err(progname, retval,
					gettext("while setting default "
						"realm name"));
		 exit(1);
	    }
       } else if (strcmp(*argv, "-k") == 0 && ARG_VAL) {
			if (krb5_string_to_enctype(optarg,
						    &global_params.enctype))
				com_err(argv[0], 0,
					gettext("%s is an invalid enctype"),
					optarg);
	    else
		 global_params.mask |= KADM5_CONFIG_ENCTYPE;
       } else if (strcmp(*argv, "-M") == 0 && ARG_VAL) {
	    global_params.mkey_name = optarg;
	    global_params.mask |= KADM5_CONFIG_MKEY_NAME;
       } else if (((strcmp(*argv, "-sf") == 0)
		/* SUNWresync121 - carry the old -f forward too */
		|| (strcmp(*argv, "-f") == 0)) && ARG_VAL) {
	    global_params.stash_file = optarg;
	    global_params.mask |= KADM5_CONFIG_STASH_FILE;
       } else if (strcmp(*argv, "-m") == 0) {
	    manual_mkey = TRUE;
	    global_params.mkey_from_kbd = 1;
	    global_params.mask |= KADM5_CONFIG_MKEY_FROM_KBD;
       } else if (cmd_lookup(*argv) != NULL) {
	    if (cmd_argv[0] == NULL)
		 cmd_argv[0] = *argv;
	    else
		 usage();
       } else {
	    cmd_argv[cmd_argc++] = *argv;
       }
		argv++;
		argc--;
    }

    if (cmd_argv[0] == NULL)
	 usage();
    
    if (retval = kadm5_get_config_params(util_context, NULL, NULL,
					 &global_params, &global_params)) {
		com_err(argv[0], retval,
		    gettext("while retreiving configuration parameters"));
	 exit(1);
    }
    /*
     * Dump creates files which should not be world-readable.  It is
     * easiest to do a single umask call here.
     */
    (void) umask(077);

    (void) memset(&master_key, 0, sizeof (krb5_keyblock)); 

    if ((global_params.enctype != ENCTYPE_UNKNOWN) &&
	(!krb5_c_valid_enctype(global_params.enctype))) {
	com_err(argv[0], KRB5_PROG_KEYTYPE_NOSUPP,
	    gettext("while setting up enctype %d"), global_params.enctype);
    }

    cmd = cmd_lookup(cmd_argv[0]);
    if (cmd->opendb && open_db_and_mkey())
		return (exit_status);

	if (global_params.iprop_enabled == TRUE)
		ulog_set_role(util_context, IPROP_MASTER);
	else
		ulog_set_role(util_context, IPROP_NULL);

    (*cmd->func)(cmd_argc, cmd_argv);

    if(close_policy_db) {
         (void) osa_adb_close_policy(policy_db);
    }      
    kadm5_free_config_params(util_context, &global_params);
    krb5_free_context(util_context);
    return (exit_status);
}

#if 0
/*
 * This function is no longer used in kdb5_util (and it would no
 * longer work, anyway).
 */
void
set_dbname(argc, argv)
    int argc;
    char *argv[];
{
    krb5_error_code retval;

    if (argc < 3) {
		com_err(argv[0], 0, gettext("Too few arguments"));
		com_err(argv[0], 0, gettext("Usage: %s dbpathname realmname"),
			argv[0]);
	exit_status++;
	return;
    }
    if (dbactive) {
		if ((retval = krb5_db_fini(util_context)) &&
				retval != KRB5_KDB_DBNOTINITED) {
			com_err(argv[0], retval,
				gettext("while closing previous database"));
	    exit_status++;
	    return;
	}
	if (valid_master_key) {
	    krb5_free_keyblock_contents(util_context, &master_key);
	    valid_master_key = 0;
	}
	krb5_free_principal(util_context, master_princ);
	dbactive = FALSE;
    }

    (void) set_dbname_help(argv[0], argv[1]);
}

#endif

/*
 * open_db_and_mkey: Opens the KDC and policy database, and sets the
 * global master_* variables.  Sets dbactive to TRUE if the databases
 * are opened, and valid_master_key to 1 if the global master
 * variables are set properly.  Returns 0 on success, and 1 on
 * failure, but it is not considered a failure if the master key
 * cannot be fetched (the master key stash file may not exist when the
 * program is run).
 */
int
open_db_and_mkey()
{
    krb5_error_code retval;
    int nentries;
    krb5_boolean more;
    krb5_data scratch, pwd, seed;

    dbactive = FALSE;
    valid_master_key = 0;

    if ((retval = krb5_db_set_name(util_context, global_params.dbname))) {
		com_err(progname, retval,
		    gettext("while setting active database to '%s'"),
		global_params.dbname);
	exit_status++;
	return(1);
    } 
    if ((retval = krb5_db_init(util_context))) {
		com_err(progname, retval,
		    gettext("while initializing database"));
	exit_status++;
	return(1);
    }
    if (retval = osa_adb_open_policy(&policy_db, &global_params)) {
		com_err(progname, retval,
		    gettext("opening policy database"));
	 exit_status++;
	 return (1);
    }
   /* assemble & parse the master key name */

    if ((retval = krb5_db_setup_mkey_name(util_context,
					  global_params.mkey_name,
					  global_params.realm, 
					  0, &master_princ))) {
		com_err(progname, retval,
		    gettext("while setting up master key name"));
	exit_status++;
	return(1);
    }
    nentries = 1;
    if ((retval = krb5_db_get_principal(util_context, master_princ, 
					&master_entry, &nentries, &more))) {
		com_err(progname, retval,
		    gettext("while retrieving master entry"));
	exit_status++;
	(void) krb5_db_fini(util_context);
	return(1);
    } else if (more) {
	com_err(progname, KRB5KDC_ERR_PRINCIPAL_NOT_UNIQUE,
		    gettext("while retrieving master entry"));
	exit_status++;
	(void) krb5_db_fini(util_context);
	return(1);
    } else if (!nentries) {
		com_err(progname, KRB5_KDB_NOENTRY,
		    gettext("while retrieving master entry"));
	exit_status++;
	(void) krb5_db_fini(util_context);
	return(1);
    }
    krb5_db_free_principal(util_context, &master_entry, nentries);

    /* the databases are now open, and the master principal exists */
    dbactive = TRUE;
    
    if (mkey_password) {
	pwd.data = mkey_password;
	pwd.length = strlen(mkey_password);
	retval = krb5_principal2salt(util_context,
			    master_princ, &scratch);
	if (retval) {
		com_err(progname, retval,
		    gettext("while calculated master key salt"));
		return(1);
	}
	/* If no encryption type is set, use the default */
	if (global_params.enctype == ENCTYPE_UNKNOWN) {
	    global_params.enctype = DEFAULT_KDC_ENCTYPE;
	    if (!krb5_c_valid_enctype(global_params.enctype))
		com_err(progname, KRB5_PROG_KEYTYPE_NOSUPP,
			gettext("while setting up enctype %d"),
			global_params.enctype);
	}

	retval = krb5_c_string_to_key(util_context,
				global_params.enctype, 
				&pwd, &scratch, &master_key);
	if (retval) {
	    com_err(progname, retval,
		gettext("while transforming master key from password"));
	    return(1);
	}
	free(scratch.data);
	mkey_password = 0;
    } else if ((retval = krb5_db_fetch_mkey(util_context, master_princ, 
					    global_params.enctype,
					    manual_mkey, FALSE,
					    global_params.stash_file,
					    0, &master_key))) {
	com_err(progname, retval,
	    gettext("while reading master key"));
	com_err(progname, 0,
	    gettext("Warning: proceeding without master key"));
	exit_status++;
	return(0);
    }
    if ((retval = krb5_db_verify_master_key(util_context, master_princ, 
		&master_key))) {
	com_err(progname, retval,
		gettext("while verifying master key"));
	exit_status++;
	krb5_free_keyblock_contents(util_context, &master_key);
	return(1);
    }

    seed.length = master_key.length;
    seed.data = (char *)master_key.contents;

    if ((retval = krb5_c_random_seed(util_context, &seed))) {
	com_err(progname, retval, 
		gettext("while initializing random key generator"));
	exit_status++;
	krb5_free_keyblock_contents(util_context, &master_key);
	return(1);
    }

    valid_master_key = 1;
    dbactive = TRUE;
	return (0);
}

#ifdef HAVE_GETCWD
#undef getwd
#endif

int 
quit()
{
    krb5_error_code retval;
    static krb5_boolean finished = 0;

    if (finished)
		return (0);
    retval = krb5_db_fini(util_context);
    krb5_free_keyblock_contents(util_context, &master_key);
    finished = TRUE;
    krb5_free_context(util_context);
    if (retval && retval != KRB5_KDB_DBNOTINITED) {
		com_err(progname, retval, gettext("while closing database"));
	exit_status++;
	return (1);
    }
    return (0);
}

int
add_random_key(argc, argv)
    int argc;
    char **argv;
{
    krb5_error_code ret;
    krb5_principal princ;
    krb5_db_entry dbent;
    int n, i;
    krb5_boolean more;
    krb5_timestamp now;

    krb5_key_salt_tuple *keysalts = NULL;
    krb5_int32 num_keysalts = 0;

    int free_keysalts;
    char *me = argv[0];
    char *ks_str = NULL;
    char *pr_str;

    if (argc < 2)
	usage();
    for (argv++, argc--; *argv; argv++, argc--) {
	if (!strcmp(*argv, "-e")) {
	    argv++; argc--;
	    ks_str = *argv;
	    continue;
	} else
	    break;
    }
    if (argc < 1)
	usage();
    pr_str = *argv;
    ret = krb5_parse_name(util_context, pr_str, &princ);
    if (ret) {
	com_err(me, ret, gettext("while parsing principal name %s"), pr_str);
	return 1;
    }
    n = 1;
    ret = krb5_db_get_principal(util_context, princ, &dbent,
				&n, &more);
    if (ret) {
	com_err(me, ret, gettext("while fetching principal %s"), pr_str);
	return 1;
    }
    if (n != 1) {
	fprintf(stderr, gettext("principal %s not found\n"), pr_str);
	return 1;
    }
    if (more) {
	fprintf(stderr, gettext("principal %s not unique\n"), pr_str);
	krb5_dbe_free_contents(util_context, &dbent);
	return 1;
    }
    ret = krb5_string_to_keysalts(ks_str,
				  ", \t", ":.-", 0,
				  &keysalts,
				  &num_keysalts);
    if (ret) {
	com_err(me, ret, gettext("while parsing keysalts %s"), ks_str);
	return 1;
    }
    if (!num_keysalts || keysalts == NULL) {
	num_keysalts = global_params.num_keysalts;
	keysalts = global_params.keysalts;
	free_keysalts = 0;
    } else
	free_keysalts = 1;
    ret = krb5_dbe_ark(util_context, &master_key,
		       keysalts, num_keysalts,
		       &dbent);
    if (free_keysalts)
	free(keysalts);
    if (ret) {
	com_err(me, ret, gettext("while randomizing principal %s"), pr_str);
	krb5_dbe_free_contents(util_context, &dbent);
	return 1;
    }
    dbent.attributes &= ~KRB5_KDB_REQUIRES_PWCHANGE;
    ret = krb5_timeofday(util_context, &now);
    if (ret) {
	com_err(me, ret, gettext("while getting time"));
	krb5_dbe_free_contents(util_context, &dbent);
	return 1;
    }
    ret = krb5_dbe_update_last_pwd_change(util_context, &dbent, now);
    if (ret) {
	com_err(me, ret, gettext("while setting changetime"));
	krb5_dbe_free_contents(util_context, &dbent);
	return 1;
    }
    ret = krb5_db_put_principal(util_context, &dbent, &n);
    krb5_dbe_free_contents(util_context, &dbent);
    if (ret) {
	com_err(me, ret, gettext("while saving principal %s"), pr_str);
	return 1;
    }
    printf("%s changed\n", pr_str);
    return 0;
}
