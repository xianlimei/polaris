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
 * Copyright 1988 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)key_prot.h	1.13	05/06/08 SMI"

/*
 * Compiled from key_prot.x using rpcgen.
 * DO NOT EDIT THIS FILE!
 * This is NOT source code!
 */

#ifndef _rpc_key_prot_h
#define	_rpc_key_prot_h

#define	KEY_PROG 100029
#define	KEY_VERS 1
#define	KEY_SET 1
#define	KEY_ENCRYPT 2
#define	KEY_DECRYPT 3
#define	KEY_GEN 4
#define	KEY_GETCRED 5

#define	PROOT 3
#define	HEXMODULUS "d4a0ba0250b6fd2ec626e7efd637df76c716e22d0944b88b"
#define	HEXKEYBYTES 48
#define	KEYSIZE 192
#define	KEYBYTES 24
#define	KEYCHECKSUMSIZE 16

enum keystatus {
	KEY_SUCCESS = 0,
	KEY_NOSECRET = 1,
	KEY_UNKNOWN = 2,
	KEY_SYSTEMERR = 3,
};
typedef enum keystatus keystatus;
bool_t xdr_keystatus();

#ifndef KERNEL

typedef char keybuf[HEXKEYBYTES];
bool_t xdr_keybuf();

#endif

typedef char *netnamestr;
bool_t xdr_netnamestr();


struct cryptkeyarg {
	netnamestr remotename;
	des_block deskey;
};
typedef struct cryptkeyarg cryptkeyarg;
bool_t xdr_cryptkeyarg();


struct cryptkeyres {
	keystatus status;
	union {
		des_block deskey;
	} cryptkeyres_u;
};
typedef struct cryptkeyres cryptkeyres;
bool_t xdr_cryptkeyres();

#define	MAXGIDS 16

struct unixcred {
	u_int uid;
	u_int gid;
	struct {
		u_int gids_len;
		u_int *gids_val;
	} gids;
};
typedef struct unixcred unixcred;
bool_t xdr_unixcred();


struct getcredres {
	keystatus status;
	union {
		unixcred cred;
	} getcredres_u;
};
typedef struct getcredres getcredres;
bool_t xdr_getcredres();

#endif /*!_rpc_key_prot_h*/