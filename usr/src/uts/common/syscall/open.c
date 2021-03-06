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

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Portions of this source code were derived from Berkeley 4.3 BSD
 * under license from the Regents of the University of California.
 */

#pragma ident	"@(#)open.c	1.15	05/06/08 SMI"

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/file.h>
#include <sys/mode.h>
#include <sys/uio.h>
#include <sys/debug.h>
#include <c2/audit.h>

/*
 * Common code for open()/openat() and creat().  Check permissions, allocate
 * an open file structure, and call the device open routine (if any).
 */

static int
copen(int startfd, char *fname, int filemode, int createmode)
{
	struct pathname pn;
	vnode_t *vp, *sdvp;
	file_t *fp, *startfp;
	enum vtype type;
	int error;
	int fd, dupfd;
	vnode_t *startvp;
	proc_t *p = curproc;

	if (startfd == AT_FDCWD) {
		/*
		 * Regular open()
		 */
		startvp = NULL;
	} else {
		/*
		 * We're here via openat()
		 */
		char startchar;

		if (copyin(fname, &startchar, sizeof (char)))
			return (set_errno(EFAULT));

		/*
		 * if startchar is / then startfd is ignored
		 */
		if (startchar == '/')
			startvp = NULL;
		else {
			if ((startfp = getf(startfd)) == NULL)
				return (set_errno(EBADF));
			startvp = startfp->f_vnode;
			VN_HOLD(startvp);
			releasef(startfd);
		}
	}

	if (filemode & FXATTR) {

		/*
		 * Make sure we have a valid request.
		 * We must either have a real fd or AT_FDCWD
		 */

		if (startfd != AT_FDCWD && startvp == NULL) {
			error = EINVAL;
			goto out;
		}

		if (error = pn_get(fname, UIO_USERSPACE, &pn)) {
			goto out;
		}

		if (startfd == AT_FDCWD) {
			mutex_enter(&p->p_lock);
			startvp = PTOU(p)->u_cdir;
			VN_HOLD(startvp);
			mutex_exit(&p->p_lock);
		}

		/*
		 * Verify permission to put attributes on file
		 */

		if ((VOP_ACCESS(startvp, VREAD, 0, CRED()) != 0) &&
		    (VOP_ACCESS(startvp, VWRITE, 0, CRED()) != 0) &&
		    (VOP_ACCESS(startvp, VEXEC, 0, CRED()) != 0)) {
			error = EACCES;
			pn_free(&pn);
			goto out;
		}

		if ((startvp->v_vfsp->vfs_flag & VFS_XATTR) != 0) {
			error = VOP_LOOKUP(startvp, "", &sdvp, &pn,
			    LOOKUP_XATTR|CREATE_XATTR_DIR, rootvp, CRED());
		} else {
			error = EINVAL;
		}
		pn_free(&pn);
		if (error != 0)
			goto out;

		VN_RELE(startvp);
		startvp = sdvp;
	}

	if ((filemode & (FREAD|FWRITE)) != 0) {
		if ((filemode & (FNONBLOCK|FNDELAY)) == (FNONBLOCK|FNDELAY))
			filemode &= ~FNDELAY;
		error = falloc((vnode_t *)NULL, filemode, &fp, &fd);
		if (error == 0) {
#ifdef C2_AUDIT
			if (audit_active)
				audit_setfsat_path(1);
#endif /* C2_AUDIT */
			/*
			 * Last arg is a don't-care term if
			 * !(filemode & FCREAT).
			 */
			error = vn_openat(fname, UIO_USERSPACE, filemode,
			    (int)(createmode & MODEMASK), &vp, CRCREAT,
			    u.u_cmask, startvp);

			if (startvp != NULL)
				VN_RELE(startvp);
			if (error == 0) {
#ifdef C2_AUDIT
				if (audit_active)
					audit_copen(fd, fp, vp);
#endif /* C2_AUDIT */
				if ((vp->v_flag & VDUP) == 0) {
					fp->f_vnode = vp;
					mutex_exit(&fp->f_tlock);
					/*
					 * We must now fill in the slot
					 * falloc reserved.
					 */
					setf(fd, fp);
					return (fd);
				} else {
					/*
					 * Special handling for /dev/fd.
					 * Give up the file pointer
					 * and dup the indicated file descriptor
					 * (in v_rdev). This is ugly, but I've
					 * seen worse.
					 */
					unfalloc(fp);
					dupfd = getminor(vp->v_rdev);
					type = vp->v_type;
					mutex_enter(&vp->v_lock);
					vp->v_flag &= ~VDUP;
					mutex_exit(&vp->v_lock);
					VN_RELE(vp);
					if (type != VCHR)
						return (set_errno(EINVAL));
					if ((fp = getf(dupfd)) == NULL) {
						setf(fd, NULL);
						return (set_errno(EBADF));
					}
					mutex_enter(&fp->f_tlock);
					fp->f_count++;
					mutex_exit(&fp->f_tlock);
					setf(fd, fp);
					releasef(dupfd);
				}
				return (fd);
			} else {
				setf(fd, NULL);
				unfalloc(fp);
				return (set_errno(error));
			}
		}
	} else {
		error = EINVAL;
	}
out:
	if (startvp != NULL)
		VN_RELE(startvp);
	return (set_errno(error));
}

#define	OPENMODE32(fmode)	((int)((fmode)-FOPEN))
#define	CREATMODE32		(FWRITE|FCREAT|FTRUNC)
#define	OPENMODE64(fmode)	(OPENMODE32(fmode) | FOFFMAX)
#define	CREATMODE64		(CREATMODE32 | FOFFMAX)
#ifdef _LP64
#define	OPENMODE(fmode)		OPENMODE64(fmode)
#define	CREATMODE		CREATMODE64
#else
#define	OPENMODE		OPENMODE32
#define	CREATMODE		CREATMODE32
#endif

/*
 * Open a file.
 */
int
open(char *fname, int fmode, int cmode)
{
	return (copen(AT_FDCWD, fname, OPENMODE(fmode), cmode));
}

/*
 * Create a file.
 */
int
creat(char *fname, int cmode)
{
	return (copen(AT_FDCWD, fname, CREATMODE, cmode));
}

int
openat(int fd, char *path, int fmode, int cmode)
{
	return (copen(fd, path, OPENMODE(fmode), cmode));
}

#if defined(_ILP32) || defined(_SYSCALL32_IMPL)
/*
 * Open and Creat for large files in 32-bit environment. Sets the FOFFMAX flag.
 */
int
open64(char *fname, int fmode, int cmode)
{
	return (copen(AT_FDCWD, fname, OPENMODE64(fmode), cmode));
}

int
creat64(char *fname, int cmode)
{
	return (copen(AT_FDCWD, fname, CREATMODE64, cmode));
}

int
openat64(int fd, char *path, int fmode, int cmode)
{
	return (copen(fd, path, OPENMODE64(fmode), cmode));
}

#endif	/* _ILP32 || _SYSCALL32_IMPL */

#ifdef _SYSCALL32_IMPL
/*
 * Open and Creat for 32-bit compatibility on 64-bit kernel
 */
int
open32(char *fname, int fmode, int cmode)
{
	return (copen(AT_FDCWD, fname, OPENMODE32(fmode), cmode));
}

int
creat32(char *fname, int cmode)
{
	return (copen(AT_FDCWD, fname, CREATMODE32, cmode));
}

int
openat32(int fd, char *path, int fmode, int cmode)
{
	return (copen(fd, path, OPENMODE32(fmode), cmode));
}
#endif	/* _SYSCALL32_IMPL */
