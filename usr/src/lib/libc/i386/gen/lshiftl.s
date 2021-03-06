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

	.ident	"@(#)lshiftl.s	1.5	05/06/08 SMI"

	.file	"lshiftl.s"

/ Shift a double long value.

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lshiftl,function)

#include "SYS.h"

	.set	arg,8
	.set	cnt,16
	.set	ans,0

	ENTRY(lshiftl)
	popl	%eax
	xchgl	%eax,0(%esp)

	pushl	%eax
	movl	arg(%esp),%eax
	movl	arg+4(%esp),%edx
	movl	cnt(%esp),%ecx
	orl	%ecx,%ecx
	jz	.lshiftld
	jns	.lshiftlp

/ We are doing a negative (right) shift

	negl	%ecx

.lshiftln:
	sarl	$1,%edx
	rcrl	$1,%eax
	loop	.lshiftln
	jmp	.lshiftld

/ We are doing a positive (left) shift

.lshiftlp:
	shll	$1,%eax
	rcll	$1,%edx
	loop	.lshiftlp

/ We are done.

.lshiftld:
	movl	%eax,%ecx
	popl	%eax
	movl	%ecx,ans(%eax)
	movl	%edx,ans+4(%eax)

	ret
	SET_SIZE(lshiftl)
