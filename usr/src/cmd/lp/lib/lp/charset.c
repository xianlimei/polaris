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
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


#ident	"@(#)charset.c	1.5	05/06/08 SMI"	/* SVr4.0 1.4	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "string.h"

#include "lp.h"

/**
 ** search_cslist() - SEARCH CHARACTER SET ALIASES FOR CHARACTER SET
 **/

char *
#if	defined(__STDC__)
search_cslist (
	char *			item,
	char **			list
)
#else
search_cslist (item, list)
	register char		*item;
	register char		**list;
#endif
{
	register char		*alias;


	if (!list || !*list)
		return (0);

	else if (STREQU(item, NAME_ANY))
		return (item);

	/*
	 * This is a linear search--we believe that the lists
	 * will be short.
	 */
	while (*list) {
		alias = strchr(*list, '=');
		if (alias && STREQU(alias+1, item))
			return (*list);
		list++;
	}
	return (0);
}
