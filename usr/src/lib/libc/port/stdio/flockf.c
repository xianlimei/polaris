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

/* Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T */
/* All Rights Reserved */

#pragma ident	"@(#)flockf.c	1.31	05/06/08 SMI"

#pragma weak flockfile = _flockfile
#pragma weak ftrylockfile = _ftrylockfile
#pragma weak funlockfile = _funlockfile

#include "synonyms.h"
#include "mtlib.h"

#define	_iob	__iob

#include "file64.h"
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio_ext.h>
#include "stdiom.h"

/*
 * The rmutex_lock/unlock routines are only called (for stdio FILE
 * locking in libc) by _flockget, _flockrel, flockfile, ftrylockfile,
 * and funlockfile.  _flockget and _flockrel are only called by the
 * FLOCKFILE/FUNLOCKFILE macros in mtlib.h.  rmutex_trylock(), as a
 * special case, is called from GETIOP()/getiop() in _findiop().
 */

/*
 * compute the lock's position, acquire it and return its pointer
 */

rmutex_t *
_flockget(FILE *iop)
{
	rmutex_t *rl = IOB_LCK(iop);

	if (rl != NULL)
		rmutex_lock(rl);
	return (rl);
}

int
ftrylockfile(FILE *iop)
{
	rmutex_t *rl = IOB_LCK(iop);

	if (rl != NULL)
		return (rmutex_trylock(rl));
	return (0);	/* can't happen? */
}

void
flockfile(FILE *iop)
{
	rmutex_t *rl = IOB_LCK(iop);

	if (rl != NULL)
		rmutex_lock(rl);
}

void
funlockfile(FILE *iop)
{
	rmutex_t *rl = IOB_LCK(iop);

	if (rl != NULL)
		rmutex_unlock(rl);
}

int
__fsetlocking(FILE *iop, int type)
{
	int	ret = 0;

	ret = GET_IONOLOCK(iop) ? FSETLOCKING_BYCALLER : FSETLOCKING_INTERNAL;

	switch (type) {

	case FSETLOCKING_QUERY:
		break;

	case FSETLOCKING_INTERNAL:
		CLEAR_IONOLOCK(iop);
		break;

	case FSETLOCKING_BYCALLER:
		SET_IONOLOCK(iop);
		break;

	default:
		errno = EINVAL;
		return (-1);
	}

	return (ret);
}
