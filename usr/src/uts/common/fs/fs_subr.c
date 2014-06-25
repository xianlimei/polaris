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
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)fs_subr.c	1.64	06/06/06 SMI"

/*
 * Generic vnode operations.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/unistd.h>
#include <sys/cred.h>
#include <sys/poll.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/stream.h>
#include <fs/fs_subr.h>
#include <sys/acl.h>
#include <sys/share.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/file.h>
#include <sys/nbmlock.h>
#include <acl/acl_common.h>

static callb_cpr_t *frlock_serialize_blocked(flk_cb_when_t, void *);

/*
 * Tunable to limit the number of retry to recover from STALE error.
 */
int fs_estale_retry = 5;

/*
 * The associated operation is not supported by the file system.
 */
int
fs_nosys()
{
	return (ENOSYS);
}

/*
 * The associated operation is invalid (on this vnode).
 */
int
fs_inval()
{
	return (EINVAL);
}

/*
 * The associated operation is valid only for directories.
 */
int
fs_notdir()
{
	return (ENOTDIR);
}

/*
 * Free the file system specific resources. For the file systems that
 * do not support the forced unmount, it will be a nop function.
 */

/*ARGSUSED*/
void
fs_freevfs(vfs_t *vfsp)
{
}

/* ARGSUSED */
int
fs_nosys_map(struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t *addrp,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	struct cred *cr)
{
	return (ENOSYS);
}

/* ARGSUSED */
int
fs_nosys_addmap(struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	struct cred *cr)
{
	return (ENOSYS);
}

/* ARGSUSED */
int
fs_nosys_poll(vnode_t *vp,
	register short events,
	int anyyet,
	register short *reventsp,
	struct pollhead **phpp)
{
	return (ENOSYS);
}


/*
 * The file system has nothing to sync to disk.  However, the
 * VFS_SYNC operation must not fail.
 */
/* ARGSUSED */
int
fs_sync(struct vfs *vfspp, short flag, cred_t *cr)
{
	return (0);
}

/*
 * Read/write lock/unlock.  Does nothing.
 */
/* ARGSUSED */
int
fs_rwlock(vnode_t *vp, int write_lock, caller_context_t *ctp)
{
	return (-1);
}

/* ARGSUSED */
void
fs_rwunlock(vnode_t *vp, int write_lock, caller_context_t *ctp)
{
}

/*
 * Compare two vnodes.
 */
int
fs_cmp(vnode_t *vp1, vnode_t *vp2)
{
	return (vp1 == vp2);
}

/*
 * No-op seek operation.
 */
/* ARGSUSED */
int
fs_seek(vnode_t *vp, offset_t ooff, offset_t *noffp)
{
	return ((*noffp < 0 || *noffp > MAXOFFSET_T) ? EINVAL : 0);
}

/*
 * File and record locking.
 */
/* ARGSUSED */
int
fs_frlock(register vnode_t *vp, int cmd, struct flock64 *bfp, int flag,
	offset_t offset, flk_callback_t *flk_cbp, cred_t *cr)
{
	int frcmd;
	int nlmid;
	int error = 0;
	flk_callback_t serialize_callback;
	int serialize = 0;

	switch (cmd) {

	case F_GETLK:
	case F_O_GETLK:
		if (flag & F_REMOTELOCK) {
			frcmd = RCMDLCK;
			break;
		}
		if (flag & F_PXFSLOCK) {
			frcmd = PCMDLCK;
			break;
		}
		bfp->l_pid = ttoproc(curthread)->p_pid;
		bfp->l_sysid = 0;
		frcmd = 0;
		break;

	case F_SETLK_NBMAND:
		/*
		 * Are NBMAND locks allowed on this file?
		 */
		if (!vp->v_vfsp ||
		    !(vp->v_vfsp->vfs_flag & VFS_NBMAND)) {
			error = EINVAL;
			goto done;
		}
		if (vp->v_type != VREG) {
			error = EINVAL;
			goto done;
		}
		/*FALLTHROUGH*/

	case F_SETLK:
		/*
		 * Check whether there is an NBMAND share reservation that
		 * conflicts with the lock request.
		 */
		if (nbl_need_check(vp)) {
			nbl_start_crit(vp, RW_WRITER);
			serialize = 1;
			if (share_blocks_lock(vp, bfp)) {
				error = EAGAIN;
				goto done;
			}
		}
		if (flag & F_REMOTELOCK) {
			frcmd = SETFLCK|RCMDLCK;
			break;
		}
		if (flag & F_PXFSLOCK) {
			frcmd = SETFLCK|PCMDLCK;
			break;
		}
		bfp->l_pid = ttoproc(curthread)->p_pid;
		bfp->l_sysid = 0;
		frcmd = SETFLCK;
		if (cmd == F_SETLK_NBMAND &&
		    (bfp->l_type == F_RDLCK || bfp->l_type == F_WRLCK)) {
			/* would check here for conflict with mapped region */
			frcmd |= NBMLCK;
		}
		break;

	case F_SETLKW:
		/*
		 * If there is an NBMAND share reservation that conflicts
		 * with the lock request, block until the conflicting share
		 * reservation goes away.
		 */
		if (nbl_need_check(vp)) {
			nbl_start_crit(vp, RW_WRITER);
			serialize = 1;
			if (share_blocks_lock(vp, bfp)) {
				error = wait_for_share(vp, bfp);
				if (error != 0)
					goto done;
			}
		}
		if (flag & F_REMOTELOCK) {
			frcmd = SETFLCK|SLPFLCK|RCMDLCK;
			break;
		}
		if (flag & F_PXFSLOCK) {
			frcmd = SETFLCK|SLPFLCK|PCMDLCK;
			break;
		}
		bfp->l_pid = ttoproc(curthread)->p_pid;
		bfp->l_sysid = 0;
		frcmd = SETFLCK|SLPFLCK;
		break;

	case F_HASREMOTELOCKS:
		nlmid = GETNLMID(bfp->l_sysid);
		if (nlmid != 0) {	/* booted as a cluster */
			l_has_rmt(bfp) =
				cl_flk_has_remote_locks_for_nlmid(vp, nlmid);
		} else {		/* not booted as a cluster */
			l_has_rmt(bfp) = flk_has_remote_locks(vp);
		}

		goto done;

	default:
		error = EINVAL;
		goto done;
	}

	/*
	 * If this is a blocking lock request and we're serializing lock
	 * requests, modify the callback list to leave the critical region
	 * while we're waiting for the lock.
	 */

	if (serialize && (frcmd & SLPFLCK) != 0) {
		flk_add_callback(&serialize_callback,
				frlock_serialize_blocked, vp, flk_cbp);
		flk_cbp = &serialize_callback;
	}

	error = reclock(vp, bfp, frcmd, flag, offset, flk_cbp);

done:
	if (serialize)
		nbl_end_crit(vp);

	return (error);
}

/*
 * Callback when a lock request blocks and we are serializing requests.  If
 * before sleeping, leave the critical region.  If after wakeup, reenter
 * the critical region.
 */

static callb_cpr_t *
frlock_serialize_blocked(flk_cb_when_t when, void *infop)
{
	vnode_t *vp = (vnode_t *)infop;

	if (when == FLK_BEFORE_SLEEP)
		nbl_end_crit(vp);
	else {
		nbl_start_crit(vp, RW_WRITER);
	}

	return (NULL);
}

/*
 * Allow any flags.
 */
/* ARGSUSED */
int
fs_setfl(vnode_t *vp, int oflags, int nflags, cred_t *cr)
{
	return (0);
}

/*
 * Return the answer requested to poll() for non-device files.
 * Only POLLIN, POLLRDNORM, and POLLOUT are recognized.
 */
struct pollhead fs_pollhd;

/* ARGSUSED */
int
fs_poll(vnode_t *vp,
	register short events,
	int anyyet,
	register short *reventsp,
	struct pollhead **phpp)
{
	*reventsp = 0;
	if (events & POLLIN)
		*reventsp |= POLLIN;
	if (events & POLLRDNORM)
		*reventsp |= POLLRDNORM;
	if (events & POLLRDBAND)
		*reventsp |= POLLRDBAND;
	if (events & POLLOUT)
		*reventsp |= POLLOUT;
	if (events & POLLWRBAND)
		*reventsp |= POLLWRBAND;
	*phpp = !anyyet && !*reventsp ? &fs_pollhd : (struct pollhead *)NULL;
	return (0);
}

/*
 * POSIX pathconf() support.
 */
/* ARGSUSED */
int
fs_pathconf(vnode_t *vp, int cmd, ulong_t *valp, cred_t *cr)
{
	register ulong_t val;
	register int error = 0;
	struct statvfs64 vfsbuf;

	switch (cmd) {

	case _PC_LINK_MAX:
		val = MAXLINK;
		break;

	case _PC_MAX_CANON:
		val = MAX_CANON;
		break;

	case _PC_MAX_INPUT:
		val = MAX_INPUT;
		break;

	case _PC_NAME_MAX:
		bzero(&vfsbuf, sizeof (vfsbuf));
		if (error = VFS_STATVFS(vp->v_vfsp, &vfsbuf))
			break;
		val = vfsbuf.f_namemax;
		break;

	case _PC_PATH_MAX:
	case _PC_SYMLINK_MAX:
		val = MAXPATHLEN;
		break;

	case _PC_PIPE_BUF:
		val = PIPE_BUF;
		break;

	case _PC_NO_TRUNC:
		if (vp->v_vfsp->vfs_flag & VFS_NOTRUNC)
			val = 1;	/* NOTRUNC is enabled for vp */
		else
			val = (ulong_t)-1;
		break;

	case _PC_VDISABLE:
		val = _POSIX_VDISABLE;
		break;

	case _PC_CHOWN_RESTRICTED:
		if (rstchown)
			val = rstchown; /* chown restricted enabled */
		else
			val = (ulong_t)-1;
		break;

	case _PC_FILESIZEBITS:

		/*
		 * If ever we come here it means that underlying file system
		 * does not recognise the command and therefore this
		 * configurable limit cannot be determined. We return -1
		 * and don't change errno.
		 */

		val = (ulong_t)-1;    /* large file support */
		break;

	case _PC_ACL_ENABLED:
		val = 0;
		break;

	default:
		error = EINVAL;
		break;
	}

	if (error == 0)
		*valp = val;
	return (error);
}

/*
 * Dispose of a page.
 */
/* ARGSUSED */
void
fs_dispose(struct vnode *vp, page_t *pp, int fl, int dn, struct cred *cr)
{

	ASSERT(fl == B_FREE || fl == B_INVAL);

	if (fl == B_FREE)
		page_free(pp, dn);
	else
		page_destroy(pp, dn);
}

/* ARGSUSED */
void
fs_nodispose(struct vnode *vp, page_t *pp, int fl, int dn, struct cred *cr)
{
	cmn_err(CE_PANIC, "fs_nodispose invoked");
}

/*
 * fabricate acls for file systems that do not support acls.
 */
/* ARGSUSED */
int
fs_fab_acl(vp, vsecattr, flag, cr)
vnode_t		*vp;
vsecattr_t	*vsecattr;
int		flag;
cred_t		*cr;
{
	aclent_t	*aclentp;
	ace_t		*acep;
	struct vattr	vattr;
	int		error;

	vsecattr->vsa_aclcnt	= 0;
	vsecattr->vsa_aclentp	= NULL;
	vsecattr->vsa_dfaclcnt	= 0;	/* Default ACLs are not fabricated */
	vsecattr->vsa_dfaclentp	= NULL;

	vattr.va_mask = AT_MODE | AT_UID | AT_GID;
	if (error = VOP_GETATTR(vp, &vattr, 0, cr))
		return (error);

	if (vsecattr->vsa_mask & (VSA_ACLCNT | VSA_ACL)) {
		vsecattr->vsa_aclcnt	= 4; /* USER, GROUP, OTHER, and CLASS */
		vsecattr->vsa_aclentp = kmem_zalloc(4 * sizeof (aclent_t),
		    KM_SLEEP);
		aclentp = vsecattr->vsa_aclentp;

		aclentp->a_type = USER_OBJ;	/* Owner */
		aclentp->a_perm = ((ushort_t)(vattr.va_mode & 0700)) >> 6;
		aclentp->a_id = vattr.va_uid;   /* Really undefined */
		aclentp++;

		aclentp->a_type = GROUP_OBJ;    /* Group */
		aclentp->a_perm = ((ushort_t)(vattr.va_mode & 0070)) >> 3;
		aclentp->a_id = vattr.va_gid;   /* Really undefined */
		aclentp++;

		aclentp->a_type = OTHER_OBJ;    /* Other */
		aclentp->a_perm = vattr.va_mode & 0007;
		aclentp->a_id = -1;		/* Really undefined */
		aclentp++;

		aclentp->a_type = CLASS_OBJ;    /* Class */
		aclentp->a_perm = (ushort_t)(0007);
		aclentp->a_id = -1;		/* Really undefined */
	} else if (vsecattr->vsa_mask & (VSA_ACECNT | VSA_ACE)) {
		vsecattr->vsa_aclcnt	= 6;
		vsecattr->vsa_aclentp = kmem_zalloc(6 * sizeof (ace_t),
		    KM_SLEEP);
		acep = vsecattr->vsa_aclentp;
		(void) memcpy(acep, trivial_acl, sizeof (ace_t) * 6);
		adjust_ace_pair(acep, (vattr.va_mode & 0700) >> 6);
		adjust_ace_pair(acep + 2, (vattr.va_mode & 0070) >> 3);
		adjust_ace_pair(acep + 4, vattr.va_mode & 0007);
	}

	return (0);
}

/*
 * Common code for implementing DOS share reservations
 */
/* ARGSUSED4 */
int
fs_shrlock(struct vnode *vp, int cmd, struct shrlock *shr, int flag, cred_t *cr)
{
	int error;

	/*
	 * Make sure that the file was opened with permissions appropriate
	 * for the request, and make sure the caller isn't trying to sneak
	 * in an NBMAND request.
	 */
	if (cmd == F_SHARE) {
		if (((shr->s_access & F_RDACC) && (flag & FREAD) == 0) ||
		    ((shr->s_access & F_WRACC) && (flag & FWRITE) == 0))
			return (EBADF);
		if (shr->s_deny & F_MANDDNY)
			return (EINVAL);
	}
	if (cmd == F_SHARE_NBMAND) {
		/* must have write permission to deny read access */
		if ((shr->s_deny & F_RDDNY) && (flag & FWRITE) == 0)
			return (EBADF);
		/* make sure nbmand is allowed on the file */
		if (!vp->v_vfsp ||
		    !(vp->v_vfsp->vfs_flag & VFS_NBMAND)) {
			return (EINVAL);
		}
		if (vp->v_type != VREG) {
			return (EINVAL);
		}
	}

	nbl_start_crit(vp, RW_WRITER);

	switch (cmd) {

	case F_SHARE_NBMAND:
		shr->s_deny |= F_MANDDNY;
		/*FALLTHROUGH*/
	case F_SHARE:
		error = add_share(vp, shr);
		break;

	case F_UNSHARE:
		error = del_share(vp, shr);
		break;

	case F_HASREMOTELOCKS:
		/*
		 * We are overloading this command to refer to remote
		 * shares as well as remote locks, despite its name.
		 */
		shr->s_access = shr_has_remote_shares(vp, shr->s_sysid);
		error = 0;
		break;

	default:
		error = EINVAL;
		break;
	}

	nbl_end_crit(vp);
	return (error);
}

/*ARGSUSED1*/
int
fs_vnevent_nosupport(vnode_t *vp, vnevent_t vnevent)
{
	ASSERT(vp != NULL);
	return (ENOTSUP);
}

/*ARGSUSED1*/
int
fs_vnevent_support(vnode_t *vp, vnevent_t vnevent)
{
	ASSERT(vp != NULL);
	return (0);
}

/*
 * return 1 for non-trivial ACL.
 *
 * NB: It is not necessary for the caller to VOP_RWLOCK since
 *	we only issue VOP_GETSECATTR.
 *
 * Returns 0 == trivial
 *         1 == NOT Trivial
 *	   <0 could not determine.
 */
int
fs_acl_nontrivial(vnode_t *vp, cred_t *cr)
{
	ulong_t		acl_styles;
	ulong_t		acl_flavor;
	vsecattr_t 	vsecattr;
	int 		error;
	int		isnontrivial;

	/* determine the forms of ACLs maintained */
	error = VOP_PATHCONF(vp, _PC_ACL_ENABLED, &acl_styles, cr);

	/* clear bits we don't understand and establish default acl_style */
	acl_styles &= (_ACL_ACLENT_ENABLED | _ACL_ACE_ENABLED);
	if (error || (acl_styles == 0))
		acl_styles = _ACL_ACLENT_ENABLED;

	vsecattr.vsa_aclentp = NULL;
	vsecattr.vsa_dfaclentp = NULL;
	vsecattr.vsa_aclcnt = 0;
	vsecattr.vsa_dfaclcnt = 0;

	while (acl_styles) {
		/* select one of the styles as current flavor */
		acl_flavor = 0;
		if (acl_styles & _ACL_ACLENT_ENABLED) {
			acl_flavor = _ACL_ACLENT_ENABLED;
			vsecattr.vsa_mask = VSA_ACLCNT | VSA_DFACLCNT;
		} else if (acl_styles & _ACL_ACE_ENABLED) {
			acl_flavor = _ACL_ACE_ENABLED;
			vsecattr.vsa_mask = VSA_ACECNT | VSA_ACE;
		}

		ASSERT(vsecattr.vsa_mask && acl_flavor);
		error = VOP_GETSECATTR(vp, &vsecattr, 0, cr);
		if (error == 0)
			break;

		/* that flavor failed */
		acl_styles &= ~acl_flavor;
	}

	/* if all styles fail then assume trivial */
	if (acl_styles == 0)
		return (0);

	/* process the flavor that worked */
	isnontrivial = 0;
	if (acl_flavor & _ACL_ACLENT_ENABLED) {
		if (vsecattr.vsa_aclcnt > MIN_ACL_ENTRIES)
			isnontrivial = 1;
		if (vsecattr.vsa_aclcnt && vsecattr.vsa_aclentp != NULL)
			kmem_free(vsecattr.vsa_aclentp,
			    vsecattr.vsa_aclcnt * sizeof (aclent_t));
		if (vsecattr.vsa_dfaclcnt && vsecattr.vsa_dfaclentp != NULL)
			kmem_free(vsecattr.vsa_dfaclentp,
			    vsecattr.vsa_dfaclcnt * sizeof (aclent_t));
	}
	if (acl_flavor & _ACL_ACE_ENABLED) {

		isnontrivial = ace_trivial(vsecattr.vsa_aclentp,
		    vsecattr.vsa_aclcnt);

		if (vsecattr.vsa_aclcnt && vsecattr.vsa_aclentp != NULL)
			kmem_free(vsecattr.vsa_aclentp,
			    vsecattr.vsa_aclcnt * sizeof (ace_t));
		/* ACE has no vsecattr.vsa_dfaclcnt */
	}
	return (isnontrivial);
}

/*
 * Check whether we need a retry to recover from STALE error.
 */
int
fs_need_estale_retry(int retry_count)
{
	if (retry_count < fs_estale_retry)
		return (1);
	else
		return (0);
}