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
 * Copyright 1996,2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_ZSMACH_H
#define	_SYS_ZSMACH_H

#pragma ident	"@(#)zsmach.h	1.7	05/06/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * sun4 platform dependent software definitions for zs driver.
 */
#define	ZSDELAY()
#define	ZSFLUSH()	(void) zs->zs_addr->zscc_control
#define	ZSNEXTPOLL(zs, zscurr)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ZSMACH_H */