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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)_lwp_mutex_unlock.s	1.19	05/06/08 SMI"

	.file "_lwp_mutex_unlock.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_mutex_unlock,function)

#include "SYS.h"
#include <sys/synch32.h>
#include <../assym.h>

	ANSI_PRAGMA_WEAK2(_private_lwp_mutex_unlock,_lwp_mutex_unlock,function)

	ENTRY(_lwp_mutex_unlock)
	membar	#LoadStore|#StoreStore
	add	%o0, MUTEX_LOCK_WORD, %o1
	ld	[%o1], %o2
1:
	clr	%o3				! clear lock/get waiter field
	cas	[%o1], %o2, %o3			! atomically
	cmp	%o2, %o3
	bne,a,pn %icc, 1b
	  mov	%o3, %o2
	btst	WAITER_MASK, %o3		! check for waiters
	beq,a,pt %icc,2f			! if no waiters
	  clr 	%o0				! 	return 0
						! else (note that %o0 is still
						!	&mutex)
	SYSTRAP_RVAL1(lwp_mutex_wakeup)		! call kernel to wakeup waiter
	SYSLWPERR
2: 	RET
	SET_SIZE(_lwp_mutex_unlock)
