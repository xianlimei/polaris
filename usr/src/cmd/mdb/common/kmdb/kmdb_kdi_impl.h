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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _KMDB_KDI_IMPL_H
#define	_KMDB_KDI_IMPL_H

#pragma ident	"@(#)kmdb_kdi_impl.h	1.2	05/06/08 SMI"

#include <kmdb/kmdb_kdi.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void kdi_cpu_init(void);
extern void kdi_usecwait(clock_t);

#ifdef __cplusplus
}
#endif

#endif /* _KMDB_KDI_IMPL_H */
