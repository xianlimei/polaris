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
 * Copyright 1988 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)_Q_stoq.c	1.4	05/09/30 SMI"

#include "_Qquad.h"
#include "_Qglobals.h"

QUAD
_Q_stoq(SINGLE x)
{
	unpacked	px;
	QUAD		q;
	_fp_current_exceptions = 0;
	_fp_unpack(&px, (int *)&x,fp_op_single);
	_fp_pack(&px, (int *)&q,fp_op_extended);
	_Q_set_exception(_fp_current_exceptions);
	return (q);
}
