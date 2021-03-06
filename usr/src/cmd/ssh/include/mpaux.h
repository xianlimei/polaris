/*	$OpenBSD: mpaux.h,v 1.12 2002/03/04 17:27:39 stevesk Exp $	*/

#ifndef	_MPAUX_H
#define	_MPAUX_H

#pragma ident	"@(#)mpaux.h	1.4	03/11/19 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * This file contains various auxiliary functions related to multiple
 * precision integers.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

void	 compute_session_id(u_char[16], u_char[8], BIGNUM *, BIGNUM *);

#ifdef __cplusplus
}
#endif

#endif /* _MPAUX_H */
