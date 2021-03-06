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
/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)zfs_dir.c	1.10	06/08/19 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mode.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/pathname.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/random.h>
#include <sys/policy.h>
#include <sys/zfs_dir.h>
#include <sys/zfs_acl.h>
#include <sys/fs/zfs.h>
#include "fs/fs_subr.h"
#include <sys/zap.h>
#include <sys/dmu.h>
#include <sys/atomic.h>
#include <sys/zfs_ctldir.h>
#include <sys/dnlc.h>

/*
 * Lock a directory entry.  A dirlock on <dzp, name> protects that name
 * in dzp's directory zap object.  As long as you hold a dirlock, you can
 * assume two things: (1) dzp cannot be reaped, and (2) no other thread
 * can change the zap entry for (i.e. link or unlink) this name.
 *
 * Input arguments:
 *	dzp	- znode for directory
 *	name	- name of entry to lock
 *	flag	- ZNEW: if the entry already exists, fail with EEXIST.
 *		  ZEXISTS: if the entry does not exist, fail with ENOENT.
 *		  ZSHARED: allow concurrent access with other ZSHARED callers.
 *		  ZXATTR: we want dzp's xattr directory
 *
 * Output arguments:
 *	zpp	- pointer to the znode for the entry (NULL if there isn't one)
 *	dlpp	- pointer to the dirlock for this entry (NULL on error)
 *
 * Return value: 0 on success or errno on failure.
 *
 * NOTE: Always checks for, and rejects, '.' and '..'.
 */
int
zfs_dirent_lock(zfs_dirlock_t **dlpp, znode_t *dzp, char *name, znode_t **zpp,
	int flag)
{
	zfsvfs_t	*zfsvfs = dzp->z_zfsvfs;
	zfs_dirlock_t	*dl;
	uint64_t	zoid;
	int		error;
	vnode_t		*vp;

	*zpp = NULL;
	*dlpp = NULL;

	/*
	 * Verify that we are not trying to lock '.', '..', or '.zfs'
	 */
	if (name[0] == '.' &&
	    (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')) ||
	    zfs_has_ctldir(dzp) && strcmp(name, ZFS_CTLDIR_NAME) == 0)
		return (EEXIST);

	/*
	 * Wait until there are no locks on this name.
	 */
	mutex_enter(&dzp->z_lock);
	for (;;) {
		if (dzp->z_reap) {
			mutex_exit(&dzp->z_lock);
			return (ENOENT);
		}
		for (dl = dzp->z_dirlocks; dl != NULL; dl = dl->dl_next)
			if (strcmp(name, dl->dl_name) == 0)
				break;
		if (dl == NULL)	{
			/*
			 * Allocate a new dirlock and add it to the list.
			 */
			dl = kmem_alloc(sizeof (zfs_dirlock_t), KM_SLEEP);
			cv_init(&dl->dl_cv, NULL, CV_DEFAULT, NULL);
			dl->dl_name = name;
			dl->dl_sharecnt = 0;
			dl->dl_namesize = 0;
			dl->dl_dzp = dzp;
			dl->dl_next = dzp->z_dirlocks;
			dzp->z_dirlocks = dl;
			break;
		}
		if ((flag & ZSHARED) && dl->dl_sharecnt != 0)
			break;
		cv_wait(&dl->dl_cv, &dzp->z_lock);
	}

	if ((flag & ZSHARED) && ++dl->dl_sharecnt > 1 && dl->dl_namesize == 0) {
		/*
		 * We're the second shared reference to dl.  Make a copy of
		 * dl_name in case the first thread goes away before we do.
		 * Note that we initialize the new name before storing its
		 * pointer into dl_name, because the first thread may load
		 * dl->dl_name at any time.  He'll either see the old value,
		 * which is his, or the new shared copy; either is OK.
		 */
		dl->dl_namesize = strlen(dl->dl_name) + 1;
		name = kmem_alloc(dl->dl_namesize, KM_SLEEP);
		bcopy(dl->dl_name, name, dl->dl_namesize);
		dl->dl_name = name;
	}

	mutex_exit(&dzp->z_lock);

	/*
	 * We have a dirlock on the name.  (Note that it is the dirlock,
	 * not the dzp's z_lock, that protects the name in the zap object.)
	 * See if there's an object by this name; if so, put a hold on it.
	 */
	if (flag & ZXATTR) {
		zoid = dzp->z_phys->zp_xattr;
		error = (zoid == 0 ? ENOENT : 0);
	} else {
		vp = dnlc_lookup(ZTOV(dzp), name);
		if (vp == DNLC_NO_VNODE) {
			VN_RELE(vp);
			error = ENOENT;
		} else if (vp) {
			if (flag & ZNEW) {
				zfs_dirent_unlock(dl);
				VN_RELE(vp);
				return (EEXIST);
			}
			*dlpp = dl;
			*zpp = VTOZ(vp);
			return (0);
		} else {
			error = zap_lookup(zfsvfs->z_os, dzp->z_id, name,
			    8, 1, &zoid);
			if (error == ENOENT)
				dnlc_update(ZTOV(dzp), name, DNLC_NO_VNODE);
		}
	}
	if (error) {
		if (error != ENOENT || (flag & ZEXISTS)) {
			zfs_dirent_unlock(dl);
			return (error);
		}
	} else {
		if (flag & ZNEW) {
			zfs_dirent_unlock(dl);
			return (EEXIST);
		}
		error = zfs_zget(zfsvfs, zoid, zpp);
		if (error) {
			zfs_dirent_unlock(dl);
			return (error);
		}
		if (!(flag & ZXATTR))
			dnlc_update(ZTOV(dzp), name, ZTOV(*zpp));
	}

	*dlpp = dl;

	return (0);
}

/*
 * Unlock this directory entry and wake anyone who was waiting for it.
 */
void
zfs_dirent_unlock(zfs_dirlock_t *dl)
{
	znode_t *dzp = dl->dl_dzp;
	zfs_dirlock_t **prev_dl, *cur_dl;

	mutex_enter(&dzp->z_lock);
	if (dl->dl_sharecnt > 1) {
		dl->dl_sharecnt--;
		mutex_exit(&dzp->z_lock);
		return;
	}
	prev_dl = &dzp->z_dirlocks;
	while ((cur_dl = *prev_dl) != dl)
		prev_dl = &cur_dl->dl_next;
	*prev_dl = dl->dl_next;
	cv_broadcast(&dl->dl_cv);
	mutex_exit(&dzp->z_lock);

	if (dl->dl_namesize != 0)
		kmem_free(dl->dl_name, dl->dl_namesize);
	cv_destroy(&dl->dl_cv);
	kmem_free(dl, sizeof (*dl));
}

/*
 * Look up an entry in a directory.
 *
 * NOTE: '.' and '..' are handled as special cases because
 *	no directory entries are actually stored for them.  If this is
 *	the root of a filesystem, then '.zfs' is also treated as a
 *	special pseudo-directory.
 */
int
zfs_dirlook(znode_t *dzp, char *name, vnode_t **vpp)
{
	zfs_dirlock_t *dl;
	znode_t *zp;
	int error = 0;

	if (name[0] == 0 || (name[0] == '.' && name[1] == 0)) {
		*vpp = ZTOV(dzp);
		VN_HOLD(*vpp);
	} else if (name[0] == '.' && name[1] == '.' && name[2] == 0) {
		zfsvfs_t *zfsvfs = dzp->z_zfsvfs;
		/*
		 * If we are a snapshot mounted under .zfs, return
		 * the vp for the snapshot directory.
		 */
		if (dzp->z_phys->zp_parent == dzp->z_id &&
		    zfsvfs->z_parent != zfsvfs) {
			error = zfsctl_root_lookup(zfsvfs->z_parent->z_ctldir,
			    "snapshot", vpp, NULL, 0, NULL, kcred);
			return (error);
		}
		rw_enter(&dzp->z_parent_lock, RW_READER);
		error = zfs_zget(zfsvfs, dzp->z_phys->zp_parent, &zp);
		if (error == 0)
			*vpp = ZTOV(zp);
		rw_exit(&dzp->z_parent_lock);
	} else if (zfs_has_ctldir(dzp) && strcmp(name, ZFS_CTLDIR_NAME) == 0) {
		*vpp = zfsctl_root(dzp);
	} else {
		error = zfs_dirent_lock(&dl, dzp, name, &zp, ZEXISTS | ZSHARED);
		if (error == 0) {
			*vpp = ZTOV(zp);
			zfs_dirent_unlock(dl);
			dzp->z_zn_prefetch = B_TRUE; /* enable prefetching */
		}
	}

	return (error);
}

static char *
zfs_dq_hexname(char namebuf[17], uint64_t x)
{
	char *name = &namebuf[16];
	const char digits[16] = "0123456789abcdef";

	*name = '\0';
	do {
		*--name = digits[x & 0xf];
		x >>= 4;
	} while (x != 0);

	return (name);
}

/*
 * Delete Queue Error Handling
 *
 * When dealing with the delete queue, we dmu_tx_hold_zap(), but we
 * don't specify the name of the entry that we will be manipulating.  We
 * also fib and say that we won't be adding any new entries to the
 * delete queue, even though we might (this is to lower the minimum file
 * size that can be deleted in a full filesystem).  So on the small
 * chance that the delete queue is using a fat zap (ie. has more than
 * 2000 entries), we *may* not pre-read a block that's needed.
 * Therefore it is remotely possible for some of the assertions
 * regarding the delete queue below to fail due to i/o error.  On a
 * nondebug system, this will result in the space being leaked.
 */

void
zfs_dq_add(znode_t *zp, dmu_tx_t *tx)
{
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	char obj_name[17];
	int error;

	ASSERT(zp->z_reap);
	ASSERT3U(zp->z_phys->zp_links, ==, 0);

	error = zap_add(zfsvfs->z_os, zfsvfs->z_dqueue,
	    zfs_dq_hexname(obj_name, zp->z_id), 8, 1, &zp->z_id, tx);
	ASSERT3U(error, ==, 0);
}

/*
 * Delete the entire contents of a directory.  Return a count
 * of the number of entries that could not be deleted.
 *
 * NOTE: this function assumes that the directory is inactive,
 *	so there is no need to lock its entries before deletion.
 *	Also, it assumes the directory contents is *only* regular
 *	files.
 */
static int
zfs_purgedir(znode_t *dzp)
{
	zap_cursor_t	zc;
	zap_attribute_t	zap;
	znode_t		*xzp;
	dmu_tx_t	*tx;
	zfsvfs_t	*zfsvfs = dzp->z_zfsvfs;
	zfs_dirlock_t	dl;
	int skipped = 0;
	int error;


	for (zap_cursor_init(&zc, zfsvfs->z_os, dzp->z_id);
	    (error = zap_cursor_retrieve(&zc, &zap)) == 0;
	    zap_cursor_advance(&zc)) {
		error = zfs_zget(zfsvfs, zap.za_first_integer, &xzp);
		ASSERT3U(error, ==, 0);

		ASSERT((ZTOV(xzp)->v_type == VREG) ||
		    (ZTOV(xzp)->v_type == VLNK));

		tx = dmu_tx_create(zfsvfs->z_os);
		dmu_tx_hold_bonus(tx, dzp->z_id);
		dmu_tx_hold_zap(tx, dzp->z_id, FALSE, zap.za_name);
		dmu_tx_hold_bonus(tx, xzp->z_id);
		dmu_tx_hold_zap(tx, zfsvfs->z_dqueue, FALSE, NULL);
		error = dmu_tx_assign(tx, TXG_WAIT);
		if (error) {
			dmu_tx_abort(tx);
			VN_RELE(ZTOV(xzp));
			skipped += 1;
			continue;
		}
		bzero(&dl, sizeof (dl));
		dl.dl_dzp = dzp;
		dl.dl_name = zap.za_name;

		error = zfs_link_destroy(&dl, xzp, tx, 0, NULL);
		ASSERT3U(error, ==, 0);
		dmu_tx_commit(tx);

		VN_RELE(ZTOV(xzp));
	}
	zap_cursor_fini(&zc);
	ASSERT(error == ENOENT);
	return (skipped);
}

/*
 * Special function to requeue the znodes for deletion that were
 * in progress when we either crashed or umounted the file system.
 *
 * returns 1 if queue was drained.
 */
static int
zfs_drain_dq(zfsvfs_t *zfsvfs)
{
	zap_cursor_t	zc;
	zap_attribute_t zap;
	dmu_object_info_t doi;
	znode_t		*zp;
	int		error;

	/*
	 * Interate over the contents of the delete queue.
	 */
	for (zap_cursor_init(&zc, zfsvfs->z_os, zfsvfs->z_dqueue);
	    zap_cursor_retrieve(&zc, &zap) == 0;
	    zap_cursor_advance(&zc)) {

		/*
		 * Create more threads if necessary to balance the load.
		 * quit if the delete threads have been shut down.
		 */
		if (zfs_delete_thread_target(zfsvfs, -1) != 0)
			return (0);

		/*
		 * See what kind of object we have in queue
		 */

		error = dmu_object_info(zfsvfs->z_os,
		    zap.za_first_integer, &doi);
		if (error != 0)
			continue;

		ASSERT((doi.doi_type == DMU_OT_PLAIN_FILE_CONTENTS) ||
		    (doi.doi_type == DMU_OT_DIRECTORY_CONTENTS));
		/*
		 * We need to re-mark these queue entries for reaping,
		 * so we pull them back into core and set zp->z_reap.
		 */
		error = zfs_zget(zfsvfs, zap.za_first_integer, &zp);

		/*
		 * We may pick up znodes that are already marked for reaping.
		 * This could happen during the purge of an extended attribute
		 * directory.  All we need to do is skip over them, since they
		 * are already in the system to be processed by the delete
		 * thread(s).
		 */
		if (error != 0) {
			continue;
		}

		zp->z_reap = 1;
		VN_RELE(ZTOV(zp));
	}
	zap_cursor_fini(&zc);
	return (1);
}

void
zfs_delete_thread(void *arg)
{
	zfsvfs_t	*zfsvfs = arg;
	zfs_delete_t 	*zd = &zfsvfs->z_delete_head;
	znode_t		*zp;
	callb_cpr_t	cprinfo;
	int		drained;

	CALLB_CPR_INIT(&cprinfo, &zd->z_mutex, callb_generic_cpr, "zfs_delete");

	mutex_enter(&zd->z_mutex);

	if (!zd->z_drained && !zd->z_draining) {
		zd->z_draining = B_TRUE;
		mutex_exit(&zd->z_mutex);
		drained = zfs_drain_dq(zfsvfs);
		mutex_enter(&zd->z_mutex);
		zd->z_draining = B_FALSE;
		zd->z_drained = drained;
		cv_broadcast(&zd->z_quiesce_cv);
	}

	while (zd->z_thread_count <= zd->z_thread_target) {
		zp = list_head(&zd->z_znodes);
		if (zp == NULL) {
			ASSERT(zd->z_znode_count == 0);
			CALLB_CPR_SAFE_BEGIN(&cprinfo);
			cv_wait(&zd->z_cv, &zd->z_mutex);
			CALLB_CPR_SAFE_END(&cprinfo, &zd->z_mutex);
			continue;
		}
		ASSERT(zd->z_znode_count != 0);
		list_remove(&zd->z_znodes, zp);
		if (--zd->z_znode_count == 0)
			cv_broadcast(&zd->z_quiesce_cv);
		mutex_exit(&zd->z_mutex);
		zfs_rmnode(zp);
		(void) zfs_delete_thread_target(zfsvfs, -1);
		mutex_enter(&zd->z_mutex);
	}

	ASSERT(zd->z_thread_count != 0);
	if (--zd->z_thread_count == 0)
		cv_broadcast(&zd->z_cv);

	CALLB_CPR_EXIT(&cprinfo);	/* NB: drops z_mutex */
	thread_exit();
}

static int zfs_work_per_thread_shift = 11;	/* 2048 (2^11) per thread */

/*
 * Set the target number of delete threads to 'nthreads'.
 * If nthreads == -1, choose a number based on current workload.
 * If nthreads == 0, don't return until the threads have exited.
 */
int
zfs_delete_thread_target(zfsvfs_t *zfsvfs, int nthreads)
{
	zfs_delete_t *zd = &zfsvfs->z_delete_head;

	mutex_enter(&zd->z_mutex);

	if (nthreads == -1) {
		if (zd->z_thread_target == 0) {
			mutex_exit(&zd->z_mutex);
			return (EBUSY);
		}
		nthreads = zd->z_znode_count >> zfs_work_per_thread_shift;
		nthreads = MIN(nthreads, ncpus << 1);
		nthreads = MAX(nthreads, 1);
		nthreads += !!zd->z_draining;
	}

	zd->z_thread_target = nthreads;

	while (zd->z_thread_count < zd->z_thread_target) {
		(void) thread_create(NULL, 0, zfs_delete_thread, zfsvfs,
		    0, &p0, TS_RUN, minclsyspri);
		zd->z_thread_count++;
	}

	while (zd->z_thread_count > zd->z_thread_target && nthreads == 0) {
		cv_broadcast(&zd->z_cv);
		cv_wait(&zd->z_cv, &zd->z_mutex);
	}

	mutex_exit(&zd->z_mutex);

	return (0);
}

/*
 * Wait until everything that's been queued has been deleted.
 */
void
zfs_delete_wait_empty(zfsvfs_t *zfsvfs)
{
	zfs_delete_t *zd = &zfsvfs->z_delete_head;

	mutex_enter(&zd->z_mutex);
	ASSERT(zd->z_thread_target != 0);
	while (!zd->z_drained || zd->z_znode_count != 0) {
		ASSERT(zd->z_thread_target != 0);
		cv_wait(&zd->z_quiesce_cv, &zd->z_mutex);
	}
	mutex_exit(&zd->z_mutex);
}

void
zfs_rmnode(znode_t *zp)
{
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	objset_t	*os = zfsvfs->z_os;
	znode_t		*xzp = NULL;
	char		obj_name[17];
	dmu_tx_t	*tx;
	uint64_t	acl_obj;
	int		error;

	ASSERT(ZTOV(zp)->v_count == 0);
	ASSERT(zp->z_phys->zp_links == 0);

	/*
	 * If this is an attribute directory, purge its contents.
	 */
	if (ZTOV(zp)->v_type == VDIR && (zp->z_phys->zp_flags & ZFS_XATTR))
		if (zfs_purgedir(zp) != 0) {
			zfs_delete_t *delq = &zfsvfs->z_delete_head;
			/*
			 * Add this back to the delete list to be retried later.
			 *
			 * XXX - this could just busy loop on us...
			 */
			mutex_enter(&delq->z_mutex);
			list_insert_tail(&delq->z_znodes, zp);
			delq->z_znode_count++;
			mutex_exit(&delq->z_mutex);
			return;
		}

	/*
	 * If the file has extended attributes, unlink the xattr dir.
	 */
	if (zp->z_phys->zp_xattr) {
		error = zfs_zget(zfsvfs, zp->z_phys->zp_xattr, &xzp);
		ASSERT(error == 0);
	}

	acl_obj = zp->z_phys->zp_acl.z_acl_extern_obj;

	/*
	 * Set up the transaction.
	 */
	tx = dmu_tx_create(os);
	dmu_tx_hold_free(tx, zp->z_id, 0, DMU_OBJECT_END);
	dmu_tx_hold_zap(tx, zfsvfs->z_dqueue, FALSE, NULL);
	if (xzp) {
		dmu_tx_hold_bonus(tx, xzp->z_id);
		dmu_tx_hold_zap(tx, zfsvfs->z_dqueue, TRUE, NULL);
	}
	if (acl_obj)
		dmu_tx_hold_free(tx, acl_obj, 0, DMU_OBJECT_END);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		zfs_delete_t *delq = &zfsvfs->z_delete_head;

		dmu_tx_abort(tx);
		/*
		 * Add this back to the delete list to be retried later.
		 *
		 * XXX - this could just busy loop on us...
		 */
		mutex_enter(&delq->z_mutex);
		list_insert_tail(&delq->z_znodes, zp);
		delq->z_znode_count++;
		mutex_exit(&delq->z_mutex);
		return;
	}

	if (xzp) {
		dmu_buf_will_dirty(xzp->z_dbuf, tx);
		mutex_enter(&xzp->z_lock);
		xzp->z_reap = 1;		/* mark xzp for deletion */
		xzp->z_phys->zp_links = 0;	/* no more links to it */
		mutex_exit(&xzp->z_lock);
		zfs_dq_add(xzp, tx);		/* add xzp to delete queue */
	}

	/*
	 * Remove this znode from delete queue
	 */
	error = zap_remove(os, zfsvfs->z_dqueue,
	    zfs_dq_hexname(obj_name, zp->z_id), tx);
	ASSERT3U(error, ==, 0);

	zfs_znode_delete(zp, tx);

	dmu_tx_commit(tx);

	if (xzp)
		VN_RELE(ZTOV(xzp));
}

/*
 * Link zp into dl.  Can only fail if zp has been reaped.
 */
int
zfs_link_create(zfs_dirlock_t *dl, znode_t *zp, dmu_tx_t *tx, int flag)
{
	znode_t *dzp = dl->dl_dzp;
	vnode_t *vp = ZTOV(zp);
	int zp_is_dir = (vp->v_type == VDIR);
	int error;

	dmu_buf_will_dirty(zp->z_dbuf, tx);
	mutex_enter(&zp->z_lock);

	if (!(flag & ZRENAMING)) {
		if (zp->z_reap) {	/* no new links to reaped zp */
			ASSERT(!(flag & (ZNEW | ZEXISTS)));
			mutex_exit(&zp->z_lock);
			return (ENOENT);
		}
		zp->z_phys->zp_links++;
	}
	zp->z_phys->zp_parent = dzp->z_id;	/* dzp is now zp's parent */

	if (!(flag & ZNEW))
		zfs_time_stamper_locked(zp, STATE_CHANGED, tx);
	mutex_exit(&zp->z_lock);

	dmu_buf_will_dirty(dzp->z_dbuf, tx);
	mutex_enter(&dzp->z_lock);
	dzp->z_phys->zp_size++;			/* one dirent added */
	dzp->z_phys->zp_links += zp_is_dir;	/* ".." link from zp */
	zfs_time_stamper_locked(dzp, CONTENT_MODIFIED, tx);
	mutex_exit(&dzp->z_lock);

	error = zap_add(zp->z_zfsvfs->z_os, dzp->z_id, dl->dl_name,
	    8, 1, &zp->z_id, tx);
	ASSERT(error == 0);

	dnlc_update(ZTOV(dzp), dl->dl_name, vp);

	return (0);
}

/*
 * Unlink zp from dl, and mark zp for reaping if this was the last link.
 * Can fail if zp is a mount point (EBUSY) or a non-empty directory (EEXIST).
 * If 'reaped_ptr' is NULL, we put reaped znodes on the delete queue.
 * If it's non-NULL, we use it to indicate whether the znode needs reaping,
 * and it's the caller's job to do it.
 */
int
zfs_link_destroy(zfs_dirlock_t *dl, znode_t *zp, dmu_tx_t *tx, int flag,
	int *reaped_ptr)
{
	znode_t *dzp = dl->dl_dzp;
	vnode_t *vp = ZTOV(zp);
	int zp_is_dir = (vp->v_type == VDIR);
	int reaped = 0;
	int error;

	dnlc_remove(ZTOV(dzp), dl->dl_name);

	if (!(flag & ZRENAMING)) {
		dmu_buf_will_dirty(zp->z_dbuf, tx);

		if (vn_vfswlock(vp))		/* prevent new mounts on zp */
			return (EBUSY);

		if (vn_ismntpt(vp)) {		/* don't remove mount point */
			vn_vfsunlock(vp);
			return (EBUSY);
		}

		mutex_enter(&zp->z_lock);
		if (zp_is_dir && !zfs_dirempty(zp)) {	/* dir not empty */
			mutex_exit(&zp->z_lock);
			vn_vfsunlock(vp);
			return (EEXIST);
		}
		ASSERT(zp->z_phys->zp_links > zp_is_dir);
		if (--zp->z_phys->zp_links == zp_is_dir) {
			zp->z_reap = 1;
			zp->z_phys->zp_links = 0;
			reaped = 1;
		} else {
			zfs_time_stamper_locked(zp, STATE_CHANGED, tx);
		}
		mutex_exit(&zp->z_lock);
		vn_vfsunlock(vp);
	}

	dmu_buf_will_dirty(dzp->z_dbuf, tx);
	mutex_enter(&dzp->z_lock);
	dzp->z_phys->zp_size--;			/* one dirent removed */
	dzp->z_phys->zp_links -= zp_is_dir;	/* ".." link from zp */
	zfs_time_stamper_locked(dzp, CONTENT_MODIFIED, tx);
	mutex_exit(&dzp->z_lock);

	error = zap_remove(zp->z_zfsvfs->z_os, dzp->z_id, dl->dl_name, tx);
	ASSERT(error == 0);

	if (reaped_ptr != NULL)
		*reaped_ptr = reaped;
	else if (reaped)
		zfs_dq_add(zp, tx);

	return (0);
}

/*
 * Indicate whether the directory is empty.  Works with or without z_lock
 * held, but can only be consider a hint in the latter case.  Returns true
 * if only "." and ".." remain and there's no work in progress.
 */
boolean_t
zfs_dirempty(znode_t *dzp)
{
	return (dzp->z_phys->zp_size == 2 && dzp->z_dirlocks == 0);
}

int
zfs_make_xattrdir(znode_t *zp, vattr_t *vap, vnode_t **xvpp, cred_t *cr)
{
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	znode_t *xzp;
	dmu_tx_t *tx;
	uint64_t xoid;
	int error;

	*xvpp = NULL;

	if (error = zfs_zaccess(zp, ACE_WRITE_NAMED_ATTRS, cr))
		return (error);

	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_bonus(tx, zp->z_id);
	dmu_tx_hold_zap(tx, DMU_NEW_OBJECT, FALSE, NULL);
	error = dmu_tx_assign(tx, zfsvfs->z_assign);
	if (error) {
		if (error == ERESTART && zfsvfs->z_assign == TXG_NOWAIT)
			dmu_tx_wait(tx);
		dmu_tx_abort(tx);
		return (error);
	}
	zfs_mknode(zp, vap, &xoid, tx, cr, IS_XATTR, &xzp, 0);
	ASSERT(xzp->z_id == xoid);
	ASSERT(xzp->z_phys->zp_parent == zp->z_id);
	dmu_buf_will_dirty(zp->z_dbuf, tx);
	zp->z_phys->zp_xattr = xoid;

	(void) zfs_log_create(zfsvfs->z_log, tx, TX_MKXATTR, zp, xzp, "");
	dmu_tx_commit(tx);

	*xvpp = ZTOV(xzp);

	return (0);
}

/*
 * Return a znode for the extended attribute directory for zp.
 * ** If the directory does not already exist, it is created **
 *
 *	IN:	zp	- znode to obtain attribute directory from
 *		cr	- credentials of caller
 *
 *	OUT:	xzpp	- pointer to extended attribute znode
 *
 *	RETURN:	0 on success
 *		error number on failure
 */
int
zfs_get_xattrdir(znode_t *zp, vnode_t **xvpp, cred_t *cr)
{
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	znode_t		*xzp;
	zfs_dirlock_t	*dl;
	vattr_t		va;
	int		error;
top:
	error = zfs_dirent_lock(&dl, zp, "", &xzp, ZXATTR);
	if (error)
		return (error);

	if (xzp != NULL) {
		*xvpp = ZTOV(xzp);
		zfs_dirent_unlock(dl);
		return (0);
	}

	ASSERT(zp->z_phys->zp_xattr == 0);

	if (zfsvfs->z_vfs->vfs_flag & VFS_RDONLY) {
		zfs_dirent_unlock(dl);
		return (EROFS);
	}

	/*
	 * The ability to 'create' files in an attribute
	 * directory comes from the write_xattr permission on the base file.
	 *
	 * The ability to 'search' an attribute directory requires
	 * read_xattr permission on the base file.
	 *
	 * Once in a directory the ability to read/write attributes
	 * is controlled by the permissions on the attribute file.
	 */
	va.va_mask = AT_TYPE | AT_MODE | AT_UID | AT_GID;
	va.va_type = VDIR;
	va.va_mode = S_IFDIR | S_ISVTX | 0777;
	va.va_uid = (uid_t)zp->z_phys->zp_uid;
	va.va_gid = (gid_t)zp->z_phys->zp_gid;

	error = zfs_make_xattrdir(zp, &va, xvpp, cr);
	zfs_dirent_unlock(dl);

	if (error == ERESTART && zfsvfs->z_assign == TXG_NOWAIT) {
		/* NB: we already did dmu_tx_wait() if necessary */
		goto top;
	}

	return (error);
}

/*
 * Decide whether it is okay to remove within a sticky directory.
 *
 * In sticky directories, write access is not sufficient;
 * you can remove entries from a directory only if:
 *
 *	you own the directory,
 *	you own the entry,
 *	the entry is a plain file and you have write access,
 *	or you are privileged (checked in secpolicy...).
 *
 * The function returns 0 if remove access is granted.
 */
int
zfs_sticky_remove_access(znode_t *zdp, znode_t *zp, cred_t *cr)
{
	uid_t  		uid;

	if (zdp->z_zfsvfs->z_assign >= TXG_INITIAL)	/* ZIL replay */
		return (0);

	if ((zdp->z_phys->zp_mode & S_ISVTX) == 0 ||
	    (uid = crgetuid(cr)) == zdp->z_phys->zp_uid ||
	    uid == zp->z_phys->zp_uid ||
	    (ZTOV(zp)->v_type == VREG &&
	    zfs_zaccess(zp, ACE_WRITE_DATA, cr) == 0))
		return (0);
	else
		return (secpolicy_vnode_remove(cr));
}
