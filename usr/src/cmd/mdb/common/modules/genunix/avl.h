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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_MDB_AVL_H
#define	_MDB_AVL_H

#pragma ident	"@(#)avl.h	1.1	05/10/30 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	AVL_WALK_NAME	"avl"
#define	AVL_WALK_DESC	"given any avl_tree_t *, forward walk all " \
			"entries in tree"

extern int avl_walk_init(mdb_walk_state_t *);
extern int avl_walk_step(mdb_walk_state_t *);
extern void avl_walk_fini(mdb_walk_state_t *wsp);

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_AVL_H */