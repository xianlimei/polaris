/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright (c) 1999-2001 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getuserattr.c	1.3	05/06/08 SMI"


#include "files_common.h"
#include <user_attr.h>
#include <string.h>

/*
 *    files/getuserattr.c --
 *           "files" backend for nsswitch "user_attr" database
 */
static int
check_name(nss_XbyY_args_t *args)
{
	userstr_t *user = (userstr_t *)args->returnval;
	const char *name    = args->key.name;

	if (strcmp(user->name, name) == 0) {
		return (1);
	}
	return (0);
}

static nss_status_t
getbyname(files_backend_ptr_t be, void *a)
{
	nss_XbyY_args_t *argp = (nss_XbyY_args_t *)a;

	return (_nss_files_XY_all(be, argp, 1, argp->key.name, check_name));
}

static files_backend_op_t userattr_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_netdb,
	getbyname
};

nss_backend_t *
_nss_files_user_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5)
{
	return (_nss_files_constr(userattr_ops,
	    sizeof (userattr_ops) / sizeof (userattr_ops[0]),
	    USERATTR_FILENAME,
	    NSS_LINELEN_USERATTR,
	    NULL));
}
