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

#pragma ident	"@(#)func_decim.c	1.12	05/06/08 SMI"

#pragma weak func_to_decimal = _func_to_decimal

#include "synonyms.h"
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include "base_conversion.h"
#include <locale.h>

void
func_to_decimal(char **ppc, int nmax, int fortran_conventions,
		decimal_record *pd, enum decimal_string_form *pform,
		char **pechar, int (*pget)(void), int *pnread,
		int (*punget)(int))
{
	char	*cp = *ppc - 1;	/* last character seen */
	char	*good = cp;	/* last character accepted */
	int	current;	/* *cp or EOF */
	int	nread = 0;	/* number of characters read so far */

#define	NEXT \
	if (nread < nmax) { \
		current = (*pget)(); \
		if (current != EOF) { \
			*++cp = (char)current; \
			nread++; \
		} \
	} else { \
		current = EOF; \
	}

	NEXT;

#include "char_to_decimal.h"

	if (punget != NULL) {
		/*
		 * If we read any characters beyond the end of the accepted
		 * token, try to push them back.
		 */
		while (cp >= *ppc) {
			if ((*punget)((int)(unsigned char)*cp) == EOF)
				break;
			cp--;
			nread--;
		}
	}

	*++cp = '\0';
	*pnread = nread;
}
