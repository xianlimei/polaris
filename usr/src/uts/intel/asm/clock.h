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

#ifndef _ASM_CLOCK_H
#define	_ASM_CLOCK_H

#pragma ident	"@(#)clock.h	1.2	05/06/08 SMI"

#include <sys/types.h>
#include <sys/time.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(__lint) && defined(__GNUC__)

#include <sys/machlock.h>

extern __inline__ void unlock_hres_lock(void)
{
	__asm__ __volatile__(
	    "lock; incl %0"
	    : /* no output */
	    : "m" (hres_lock)
	    : "cc");
}

extern __inline__ hrtime_t tsc_read(void)
{
#if defined(__amd64)
	uint32_t lobits, hibits;

	__asm__ __volatile__(
	    "rdtsc"
	    : "=a" (lobits), "=d" (hibits));
	return (lobits | ((hrtime_t)hibits << 32));
#elif defined(__i386)
	hrtime_t __value;

	__asm__ __volatile__(
	    "rdtsc"
	    : "=A" (__value));
	return (__value);
#else
#error	"port me"
#endif
}

#endif	/* !__lint && __GNUC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _ASM_CLOCK_H */
