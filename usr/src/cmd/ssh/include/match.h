/*	$OpenBSD: match.h,v 1.12 2002/03/01 13:12:10 markus Exp $	*/

#ifndef	_MATCH_H
#define	_MATCH_H

#pragma ident	"@(#)match.h	1.4	03/11/19 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

int	 match_pattern(const char *, const char *);
int	 match_pattern_list(const char *, const char *, u_int, int);
int	 match_hostname(const char *, const char *, u_int);
int	 match_host_and_ip(const char *, const char *, const char *);
int	 match_user(const char *, const char *, const char *, const char *);
char	*match_list(const char *, const char *, u_int *);

#ifdef __cplusplus
}
#endif

#endif /* _MATCH_H */
