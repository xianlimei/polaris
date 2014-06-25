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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/


/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)pread.s	1.10	05/06/08 SMI"

	.file	"pread.s"

/* C library -- pread						*/
/* int pread (int fildes, void *buf, unsigned nbyte, off_t offset);	*/

#include "SYS.h"

#if !defined(_LARGEFILE_SOURCE)

	SYSCALL_RESTART_RVAL1(pread)
	RET
	SET_SIZE(pread)

#else

/* C library -- pread64 transitional large file API	*/
/* ssize_t pread(int, void *, size_t, off64_t);		*/

	SYSCALL_RESTART_RVAL1(pread64)
	RET
	SET_SIZE(pread64)

#endif