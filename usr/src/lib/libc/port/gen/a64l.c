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

#pragma ident	"@(#)a64l.c	1.12	05/06/08 SMI"

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * convert base 64 ascii to long int
 * char set is [./0-9A-Za-z]
 *
 */
#pragma weak a64l = _a64l
#include "synonyms.h"
#include <sys/types.h>
#include <stdlib.h>

#define	BITSPERCHAR	6 /* to hold entire character set */
#define	MAXBITS		(6 * BITSPERCHAR) /* maximum number */
			/* of 6 chars converted */

long
a64l(const char *s)
{
	int i, c;
	int lg = 0;

	for (i = 0; ((c = *s++) != '\0') && (i < MAXBITS); i += BITSPERCHAR) {
		if (c > 'Z')
			c -= 'a' - 'Z' - 1;
		if (c > '9')
			c -= 'A' - '9' - 1;
		lg |= (c - ('0' - 2)) << i;
	}
	return ((long)lg);
}