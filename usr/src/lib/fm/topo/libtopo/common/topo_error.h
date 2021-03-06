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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_TOPO_ERROR_H
#define	_TOPO_ERROR_H

#pragma ident	"@(#)topo_error.h	1.1	06/02/11 SMI"

#include <topo_tree.h>
#include <topo_module.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This enum definition is used to define a set of error tags associated with
 * the fmd daemon's various error conditions.  The shell script mkerror.sh is
 * used to parse this file and create a corresponding topo_error.c source file.
 * If you do something other than add a new error tag here, you may need to
 * update the mkerror shell script as it is based upon simple regexps.
 */
typedef enum topo_errno {
    ETOPO_UNKNOWN = 1000, /* unknown libtopo error */
    ETOPO_NOMEM,	/* memory limit exceeded */
    ETOPO_MODULE,	/* module detected or caused an error */
    ETOPO_HDL_VER,	/* handle opened with invalid ABI version */
    ETOPO_HDL_SNAP,	/* snapshot already taken */
    ETOPO_HDL_INVAL,	/* invalid argument specified */
    ETOPO_HDL_UUID,	/* uuid already set */
    ETOPO_MOD_INIT,	/* failed to initialize module */
    ETOPO_MOD_FINI,	/* failed to uninitialize module */
    ETOPO_MOD_LOADED,	/* specified module is already loaded */
    ETOPO_MOD_NOMOD,	/* specified module is not loaded */
    ETOPO_MOD_NONVL,	/* specified module is not loaded */
    ETOPO_MOD_INVAL,	/* module invalid argument */
    ETOPO_MOD_DUP,	/* module duplicate node entry */
    ETOPO_MOD_NOREG,	/* module failed to register */
    ETOPO_MOD_NOENT,	/* module path invalid */
    ETOPO_MOD_NOSUP,	/* enumerator not supported in this module */
    ETOPO_RTLD_OPEN,	/* rtld failed to open shared library plug-in */
    ETOPO_RTLD_INIT,	/* shared library plug-in does not define _topo_init */
    ETOPO_RTLD_NOMEM,	/* memory limit exceeded when opening shared library */
    ETOPO_BLTIN_NAME,	/* built-in plug-in name not found in definition list */
    ETOPO_BLTIN_INIT,	/* built-in plug-in does not define init function */
    ETOPO_NODE_INVAL,	/* node opertation invalid argument */
    ETOPO_NODE_LINKED,	/* node already linked */
    ETOPO_NODE_BOUND,	/* node already bound */
    ETOPO_NODE_DUP,	/* duplicate node */
    ETOPO_NODE_RANGE,	/* invalid instance range */
    ETOPO_NODE_NOENT,	/* node not found */
    ETOPO_NODE_FMRI,	/* no fmri specified */
    ETOPO_VER_OLD,	/* plugin compiled using an obsolete topo ABI */
    ETOPO_VER_NEW,	/* plugin is compiled using a newer topo ABI */
    ETOPO_ENUM_PARTIAL,	/* partial enumeration completed for client */
    ETOPO_PROP_NOENT,	/* undefined property or property group */
    ETOPO_PROP_DEFD,	/* static property already defined */
    ETOPO_PROP_NOMEM,	/* memory limit exceeded during property allocation */
    ETOPO_PROP_TYPE,	/* invalid property type */
    ETOPO_PROP_NOINHERIT, /* can not inherit property */
    ETOPO_FMRI_NVL,	/* nvlist allocation failure for FMRI */
    ETOPO_FMRI_VERSION, /* invalid FMRI scheme version */
    ETOPO_FMRI_MALFORM,	/* malformed FMRI */
    ETOPO_NVL_INVAL,	/* invalid nvlist function argument */
    ETOPO_METHOD_INVAL,	/* invalid method registration */
    ETOPO_METHOD_NOTSUP, /* method not supported */
    ETOPO_METHOD_FAIL,	/* method failed */
    ETOPO_FILE_NOENT,	/* no topology file found */
    ETOPO_PRSR_BADGRP,	/* unrecognized grouping */
    ETOPO_PRSR_BADNUM,	/* unable to interpret attribute numerically */
    ETOPO_PRSR_BADRNG,	/* non-sensical range */
    ETOPO_PRSR_BADSCH,	/* unrecognized scheme */
    ETOPO_PRSR_BADSTAB,	/* unrecognized stability */
    ETOPO_PRSR_BADTYPE,	/* unrecognized property value type */
    ETOPO_PRSR_NOATTR,	/* tag missing attribute */
    ETOPO_PRSR_NOENT,	/* topology xml file not found */
    ETOPO_PRSR_NOMETH,	/* range missing enum-method */
    ETOPO_PRSR_NVPROP,	/* properties as nvlist missing crucial field */
    ETOPO_PRSR_OOR,	/* node instance out of declared range */
    ETOPO_WALK_EMPTY,	/* empty topology */
    ETOPO_WALK_NOTFOUND, /* scheme based topology not found */
    ETOPO_END		/* end of custom errno list (to ease auto-merge) */
} topo_errno_t;

extern int topo_hdl_seterrno(topo_hdl_t *, int);
extern const char *topo_hdl_errmsg(topo_hdl_t *);
extern int topo_hdl_errno(topo_hdl_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _TOPO_ERROR_H */
