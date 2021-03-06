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
 * Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 * Copyright 2001-2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */


#pragma ident	"@(#)groups.c	1.8	05/06/08 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cred_impl.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/policy.h>

int
setgroups(int gidsetsize, gid_t *gidset)
{
	proc_t	*p;
	cred_t	*cr, *newcr;
	int	i;
	int	n = gidsetsize;
	gid_t	*groups = NULL;
	int	error;

	/* Perform the cheapest tests before grabbing p_crlock  */
	if (n > ngroups_max || n < 0)
		return (set_errno(EINVAL));

	if (n != 0) {
		groups = kmem_alloc(n * sizeof (gid_t), KM_SLEEP);

		if (copyin(gidset, groups, n * sizeof (gid_t)) != 0) {
			kmem_free(groups, n * sizeof (gid_t));
			return (set_errno(EFAULT));
		}

		for (i = 0; i < n; i++) {
			if (groups[i] < 0 || groups[i] > MAXUID) {
				kmem_free(groups, n * sizeof (gid_t));
				return (set_errno(EINVAL));
			}
		}
	}

	/*
	 * Need to pre-allocate the new cred structure before acquiring
	 * the p_crlock mutex.
	 */
	newcr = cralloc();
	p = ttoproc(curthread);
	mutex_enter(&p->p_crlock);
	cr = p->p_cred;

	if ((error = secpolicy_allow_setid(cr, -1, B_FALSE)) != 0) {
		mutex_exit(&p->p_crlock);
		if (groups != NULL)
			kmem_free(groups, n * sizeof (gid_t));
		crfree(newcr);
		return (set_errno(error));
	}

	crdup_to(cr, newcr);

	if (n != 0) {
		bcopy(groups, newcr->cr_groups, n * sizeof (gid_t));
		kmem_free(groups, n * sizeof (gid_t));
	}

	newcr->cr_ngroups = n;

	p->p_cred = newcr;
	crhold(newcr);			/* hold for the current thread */
	crfree(cr);			/* free the old one */
	mutex_exit(&p->p_crlock);

	/*
	 * Broadcast new cred to process threads (including the current one).
	 */
	crset(p, newcr);

	return (0);
}

int
getgroups(int gidsetsize, gid_t *gidset)
{
	struct cred *cr;
	int n;

	cr = curthread->t_cred;
	n = (int)cr->cr_ngroups;

	if (gidsetsize != 0) {
		if (gidsetsize < n)
			return (set_errno(EINVAL));
		if (copyout(cr->cr_groups, gidset, n * sizeof (gid_t)))
			return (set_errno(EFAULT));
	}

	return (n);
}
