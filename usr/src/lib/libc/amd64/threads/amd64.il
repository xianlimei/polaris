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

#pragma ident	"@(#)amd64.il	1.6	05/06/08 SMI"
	.inline	_curthread, 0
	movq	%fs:0, %rax
	.end

	.inline	__curthread, 0
	xorq	%rax, %rax
	movw	%fs, %ax
	andq	%rax, %rax
	je	1f
	movq	%fs:0, %rax
1:
	.end

	.inline	stkptr, 0
	movq	%rsp, %rax
	.end

	.inline	gethrtime, 0
	movl	$3, %eax
	int	$0xd2
	.end

	.inline	set_lock_byte, 0
	movl	$1, %eax
	xchgb	%al, (%rdi)
	.end

	.inline	cas32, 0
	movl	%esi, %eax
	lock
	cmpxchgl %edx, (%rdi)
	.end

	.inline	swap32, 0
	movl	%esi, %eax
	xchgl	(%rdi), %eax
	.end

	.inline	incr32, 0
	lock
	incl	(%rdi)
	.end

	.inline	decr32, 0
	lock
	decl	(%rdi)
	.end

	.inline ht_pause, 0
	rep			/ "rep nop" is equivalent to "pause"
	nop
	.end
