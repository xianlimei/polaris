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

#pragma ident	"@(#)args.c	1.7	06/03/07 SMI"

#include	<debug.h>
#include	"_debug.h"
#include	"msg.h"

void
Dbg_args_flags(Lm_list *lml, int ndx, int c)
{
	if (DBG_NOTCLASS(DBG_C_ARGS))
		return;

	dbg_print(lml, MSG_INTL(MSG_ARG_FLAG), ndx, c);
}

void
Dbg_args_files(Lm_list *lml, int ndx, char *file)
{
	if (DBG_NOTCLASS(DBG_C_ARGS))
		return;

	dbg_print(lml, MSG_INTL(MSG_ARG_FILE), ndx, file);
}
