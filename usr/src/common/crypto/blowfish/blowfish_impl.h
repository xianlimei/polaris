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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_BLOWFISH_IMPL_H
#define	_BLOWFISH_IMPL_H

#pragma ident	"@(#)blowfish_impl.h	1.2	05/06/08 SMI"

/*
 * Common definitions used by Blowfish.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	BLOWFISH_MINBITS	32
#define	BLOWFISH_MINBYTES	(BLOWFISH_MINBITS >> 3)
#ifdef	CRYPTO_UNLIMITED
#define	BLOWFISH_MAXBITS	448
#else /* !CRYPTO_UNLIMITED */
#define	BLOWFISH_MAXBITS	128
#endif /* CRYPTO_UNLIMITED */
#define	BLOWFISH_MAXBYTES	(BLOWFISH_MAXBITS >> 3)

#define	BLOWFISH_IV_LEN		8
#define	BLOWFISH_BLOCK_LEN	8
#define	BLOWFISH_KEY_INCREMENT	8
#define	BLOWFISH_DEFAULT	128

extern void blowfish_encrypt_block(void *, uint8_t *, uint8_t *);
extern void blowfish_decrypt_block(void *, uint8_t *, uint8_t *);
extern void blowfish_init_keysched(uint8_t *, uint_t, void *);
extern void *blowfish_alloc_keysched(size_t *, int);

#ifdef	__cplusplus
}
#endif

#endif	/* _BLOWFISH_IMPL_H */
