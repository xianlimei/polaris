/*	$OpenBSD: deattack.h,v 1.7 2001/06/26 17:27:23 markus Exp $	*/

#ifndef	_DEATTACK_H
#define	_DEATTACK_H

#pragma ident	"@(#)deattack.h	1.4	03/11/19 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Cryptographic attack detector for ssh - Header file
 *
 * Copyright (c) 1998 CORE SDI S.A., Buenos Aires, Argentina.
 *
 * All rights reserved. Redistribution and use in source and binary
 * forms, with or without modification, are permitted provided that
 * this copyright notice is retained.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES ARE DISCLAIMED. IN NO EVENT SHALL CORE SDI S.A. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY OR
 * CONSEQUENTIAL DAMAGES RESULTING FROM THE USE OR MISUSE OF THIS
 * SOFTWARE.
 *
 * Ariel Futoransky <futo@core-sdi.com>
 * <http://www.core-sdi.com>
 */

/* Return codes */
#define DEATTACK_OK		0
#define DEATTACK_DETECTED	1

int	 detect_attack(u_char *, u_int32_t, u_char[8]);

#ifdef __cplusplus
}
#endif

#endif /* _DEATTACK_H */
