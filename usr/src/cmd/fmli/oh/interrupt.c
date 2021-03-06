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


/*
 * Copyright  (c) 1988 AT&T
 *	All Rights Reserved
 */
#ident	"@(#)interrupt.c	1.6	05/06/08 SMI"       /* SVr4.0 1.2 */

#include "wish.h"

/*  routines defined in this file: */

void intr_handler();



/* declare data structures for interrupt  feature
 * the following is a copy of interrupt.h without the `extern'
 */


struct {
    bool  interrupt;
    char *oninterrupt;
    bool  skip_eval;
} Cur_intr;


/* intr_handler
          This routine will execute  on receipt of a SIGINT while
	  executing a fmli builtin  or external executable (if
	  interrupts are enabled)
	  
	  Sets flag  to tell eval() to throw away the rest of the
	  descriptor it is parsing.
*/
void
intr_handler()
{
    Cur_intr.skip_eval = TRUE;
}

