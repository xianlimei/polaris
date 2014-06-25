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

#ifndef _MEMUTILS_H
#define	_MEMUTILS_H

#pragma ident	"@(#)memutils.h	1.7	06/05/03 SMI"

#include <note.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)
extern void *xmalloc(size_t);
extern void *xcalloc(size_t, size_t);
extern void *xrealloc(void *, size_t);
#else
extern void *xmalloc();
extern void *xcalloc();
extern void *xrealloc();
#endif /* __STDC__ */

NOTE(ALIGNMENT(xmalloc, 8))
NOTE(ALIGNMENT(xcalloc, 8))
NOTE(ALIGNMENT(xrealloc, 8))

#ifdef	__cplusplus
}
#endif

#endif /* _MEMUTILS_H */