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


#pragma ident	"@(#)isheader.c	1.7	05/06/08 SMI" 	/* SVr4.0 1.	*/
#include "mail.h"
/*
 * isheader(lp, ctf) - check if lp is header line and return type
 *	lp	-> 	pointer to line
 *	ctfp	->	continuation flag (should be FALSE the first time
 *			isheader() is called on a message.  isheader() sets
 *			it for the remaining calls to that message)
 * returns
 *	FALSE	->	not header line
 *	H_*     ->	type of header line found.
 */
int
isheader(lp, ctfp)
char	*lp;
int	*ctfp;
{
	register char	*p, *q;
	register int	i;

	p = lp;
	while((*p) && (*p != '\n') && (isspace(*p))) {
		p++;
	}
	if((*p == NULL) || (*p == '\n')) {
		/* blank line */
		return (FALSE);
	}

	if ((*ctfp) && ((*lp == ' ') || (*lp == '\t'))) {
		return(H_CONT);
	}

	*ctfp = FALSE;
	for (i = 1; i < H_CONT; i++) {
		if (!isit(lp, i)) {
			continue;
		}
		if ((i == H_FROM) || (i == H_FROM1)) {
			/*
			 * Should NEVER get 'From ' or '>From ' line on stdin
			 * if invoked as mail (rather than rmail) since
			 * 'From ' and/or '>From ' lines are generated by
			 * program itself. Therefore, if it DOES match and
			 * ismail == TRUE, it must be part of the content.
			 */
			if (sending && ismail && !deliverflag) {
				return (FALSE);
			}
		}
		*ctfp = TRUE;
		return (i);
	}
	/*
	 * Check if name: value pair
 	 */
	if ((p = strpbrk(lp, ":")) != NULL ) {
		for(q = lp; q < p; q++)  {
			if ((*q == ' ') || (!isprint(*q)))  {
				return(FALSE);
			}
		}
		*ctfp = TRUE;
		return(H_NAMEVALUE);
	}
	return(FALSE);
}
