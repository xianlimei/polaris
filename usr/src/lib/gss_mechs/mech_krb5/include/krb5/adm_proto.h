#pragma ident	"@(#)adm_proto.h	1.3	05/09/26 SMI"
/*
 * include/krb5/adm_proto.h
 *
 * Copyright 1995 by the Massachusetts Institute of Technology.
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
 */
#ifndef	KRB5_ADM_PROTO_H__
#define	KRB5_ADM_PROTO_H__

/*
 * This is ugly, but avoids having to include k5-int or kdb.h for this.
 */
#ifndef	KRB5_KDB5__
struct _krb5_db_entry;
typedef struct _krb5_db_entry krb5_db_entry;
#endif	/* KRB5_KDB5__ */

/* Ditto for adm.h or kadm5/admin.h */

/*
 * XXXX krb5_realm params is defined in two header files!!!!
 * This really needs to be fixed!!!
 */
#if !defined(KRB5_ADM_H__) && !defined(__KADM5_ADMIN_H__)
#ifndef __KADM5_ADMIN_H__
struct ___krb5_realm_params;
typedef struct ___krb5_realm_params krb5_realm_params;

struct ___krb5_key_salt_tuple;
typedef struct ___krb5_key_salt_tuple krb5_key_salt_tuple;
#endif	/* __KADM5_ADMIN_H__ */
#endif	/* KRB5_ADM_H__ */

/*
 * Function prototypes.
 */

/* adm_conn.c */
krb5_error_code KRB5_CALLCONV krb5_adm_connect
	(krb5_context,
		   char *,
		   char *,
		   char *,
		   int *,
		   krb5_auth_context *,
		   krb5_ccache *,
		   char *,
		   krb5_timestamp);
 void KRB5_CALLCONV krb5_adm_disconnect
	(krb5_context,
		   int *,
		   krb5_auth_context,
		   krb5_ccache);

#if !defined(_MSDOS) && !defined(_WIN32) && !defined(macintosh)
/* adm_kw_dec.c */
krb5_error_code krb5_adm_proto_to_dbent
	(krb5_context,
		   krb5_int32,
		   krb5_data *,
		   krb5_ui_4 *,
		   krb5_db_entry *,
		   char **);

/* adm_kw_enc.c */
krb5_error_code krb5_adm_dbent_to_proto
	(krb5_context,
		   krb5_ui_4,
		   krb5_db_entry *,
		   char *,
		   krb5_int32 *,
		   krb5_data **);
#endif /* !(windows or macintosh) */

/* adm_kt_dec.c */
krb5_error_code krb5_adm_proto_to_ktent
	(krb5_context,
		   krb5_int32,
		   krb5_data *,
		   krb5_keytab_entry *);

/* adm_kt_enc.c */
krb5_error_code krb5_adm_ktent_to_proto
	(krb5_context,
		   krb5_keytab_entry *,
		   krb5_int32 *,
		   krb5_data **);

/* adm_rw.c */
void KRB5_CALLCONV krb5_free_adm_data
	(krb5_context,
		   krb5_int32,
		   krb5_data *);

krb5_error_code KRB5_CALLCONV krb5_send_adm_cmd
	(krb5_context,
		   krb5_pointer,
		   krb5_auth_context,
		   krb5_int32,
		   krb5_data *);
krb5_error_code krb5_send_adm_reply
	(krb5_context,
		   krb5_pointer,
		   krb5_auth_context,
		   krb5_int32,
		   krb5_int32,
		   krb5_data *);
krb5_error_code krb5_read_adm_cmd
	(krb5_context,
		   krb5_pointer,
		   krb5_auth_context,
		   krb5_int32 *,
		   krb5_data **);
krb5_error_code KRB5_CALLCONV krb5_read_adm_reply
	(krb5_context,
		   krb5_pointer,
		   krb5_auth_context,
		   krb5_int32 *,
		   krb5_int32 *,
		   krb5_data **);

/* logger.c */
krb5_error_code krb5_klog_init
	(krb5_context,
		   char *,
		   char *,
		   krb5_boolean);
void krb5_klog_close (krb5_context);
int krb5_klog_syslog (int, const char *, ...);
void krb5_klog_reopen (krb5_context);

/* alt_prof.c */
krb5_error_code krb5_aprof_init
	(char *, char *, krb5_pointer *);
krb5_error_code krb5_aprof_getvals
	(krb5_pointer, const char **, char ***);
krb5_error_code krb5_aprof_get_deltat
	(krb5_pointer,
			const char **,
			krb5_boolean,
			krb5_deltat *);
krb5_error_code krb5_aprof_get_string
	(krb5_pointer, const char **, krb5_boolean, char **);
krb5_error_code krb5_aprof_get_int32
	(krb5_pointer,
			const char **,
			krb5_boolean,
			krb5_int32 *);
krb5_error_code krb5_aprof_finish (krb5_pointer);

krb5_error_code krb5_read_realm_params (krb5_context,
						       char *,
						       char *,
						       char *,
						       krb5_realm_params **);
krb5_error_code krb5_free_realm_params (krb5_context,
						       krb5_realm_params *);

/* str_conv.c */
krb5_error_code
krb5_string_to_flags (char *,
				     const char *,
				     const char *,
				     krb5_flags *);
krb5_error_code
krb5_flags_to_string (krb5_flags,
				     const char *,
				     char *,
				     size_t);
krb5_error_code
krb5_input_flag_to_string (int, 
					char *,
					size_t);

/* keysalt.c */
krb5_boolean
krb5_keysalt_is_present (krb5_key_salt_tuple *,
					krb5_int32,
					krb5_enctype,
					krb5_int32);
krb5_error_code
krb5_keysalt_iterate
	(krb5_key_salt_tuple *,
			krb5_int32,
			krb5_boolean,
			krb5_error_code (*)
				(krb5_key_salt_tuple *,
						 krb5_pointer),
			krb5_pointer);
				     
krb5_error_code
krb5_string_to_keysalts (char *,
					const char *,
					const char *,
					krb5_boolean,
					krb5_key_salt_tuple **,
					krb5_int32 *);
#endif	/* KRB5_ADM_PROTO_H__ */
