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

#ifndef _CONSOLE_H
#define	_CONSOLE_H

#pragma ident	"@(#)console.h	1.5	06/06/13 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	CONS_INVALID		-1
#define	CONS_SCREEN_TEXT	0
#define	CONS_SCREEN_GRAPHICS	1
#define	CONS_TTYA		2
#define	CONS_TTYB		3
#define	CONS_USBSER		4

#define	CONS_COLOR	7

#define	VGA_GRAPHICS 0x12

extern void kb_init(void);
extern int kb_getchar(void);
extern int kb_ischar(void);

extern char *console_init(char *);
extern void console_init2(char *, char *, char *);
extern void text_init(void);
extern void putchar(int);
extern int getchar(void);
extern int ischar(void);
extern int cons_gets(char *, int);

#ifdef __cplusplus
}
#endif

#endif /* _CONSOLE_H */
