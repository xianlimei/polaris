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

%#pragma ident	"@(#)fmd_rpc_api.x	1.2	05/06/08 SMI"

program FMD_API {
	version FMD_API_VERSION_1 {
		void FMD_HDL_CREATE(int) = 1;
	} = 1;
} = 100170;

%extern void fmd_api_1(struct svc_req *, SVCXPRT *);

%#undef	RW_READ_HELD
%#undef	RW_WRITE_HELD
%#undef	RW_LOCK_HELD
%#undef	MUTEX_HELD