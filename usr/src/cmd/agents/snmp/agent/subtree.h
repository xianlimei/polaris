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
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)subtree.h	1.3	05/06/12 SMI"

#ifndef _SUBTREE_H_
#define _SUBTREE_H_

typedef struct _Subtree {
	int regTreeIndex;
	int regTreeAgentID;
	Oid		name;
	int regTreeStatus;
	struct _Subtree	*next_subtree;
	struct _Agent	*agent;
	struct _Subtree	*next_agent_subtree;
} Subtree;

typedef Subtree SSA_Subtree;

#define INVALID_HANDLER         0

extern int subtree_add(Agent *agent, Subid *subids, int len);
extern Subtree *subtree_match(u_char type, Oid *oid);
extern void subtree_list_delete();
extern void trace_subtrees();

#endif /* _SUBTREE_H_ */
