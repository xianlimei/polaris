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

#pragma ident	"@(#)__clock_gettime.s	1.3	05/06/08 SMI"

	.file	"__clock_gettime.s"

#include <sys/time_impl.h>
#include "SYS.h"

/*
 * int
 * __clock_gettime(clockid_t clock_id, timespec_t *tp)
 */

	ENTRY(__clock_gettime)
	cmp	%o0, __CLOCK_REALTIME0		! if (clock_id)
	beq	2f				! equal to __CLOCK_REALTIME0
	cmp	%o0, CLOCK_REALTIME		! or
	bne	1f				! equal to CLOCK_REALTIME
	.empty					! optimize for CLOCK_REALTIME
2:
	mov	%o1, %o5
	SYSFASTTRAP(GETHRESTIME)
	stn	%o0, [%o5]
	stn	%o1, [%o5 + CLONGSIZE]
	RETC
1:
	SYSTRAP_RVAL1(clock_gettime)
	SYSCERROR
	RETC
	SET_SIZE(__clock_gettime)
