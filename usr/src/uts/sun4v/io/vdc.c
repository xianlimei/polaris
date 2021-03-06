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

#pragma ident	"@(#)vdc.c	1.6	06/08/07 SMI"

/*
 * LDoms virtual disk client (vdc) device driver
 *
 * This driver runs on a guest logical domain and communicates with the virtual
 * disk server (vds) driver running on the service domain which is exporting
 * virtualized "disks" to the guest logical domain.
 *
 * The driver can be divided into four sections:
 *
 * 1) generic device driver housekeeping
 *	_init, _fini, attach, detach, ops structures, etc.
 *
 * 2) communication channel setup
 *	Setup the communications link over the LDC channel that vdc uses to
 *	talk to the vDisk server. Initialise the descriptor ring which
 *	allows the LDC clients to transfer data via memory mappings.
 *
 * 3) Support exported to upper layers (filesystems, etc)
 *	The upper layers call into vdc via strategy(9E) and DKIO(7I)
 *	ioctl calls. vdc will copy the data to be written to the descriptor
 *	ring or maps the buffer to store the data read by the vDisk
 *	server into the descriptor ring. It then sends a message to the
 *	vDisk server requesting it to complete the operation.
 *
 * 4) Handling responses from vDisk server.
 *	The vDisk server will ACK some or all of the messages vdc sends to it
 *	(this is configured during the handshake). Upon receipt of an ACK
 *	vdc will check the descriptor ring and signal to the upper layer
 *	code waiting on the IO.
 */

#include <sys/atomic.h>
#include <sys/conf.h>
#include <sys/disp.h>
#include <sys/ddi.h>
#include <sys/dkio.h>
#include <sys/efi_partition.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/mach_descrip.h>
#include <sys/modctl.h>
#include <sys/mdeg.h>
#include <sys/note.h>
#include <sys/open.h>
#include <sys/sdt.h>
#include <sys/stat.h>
#include <sys/sunddi.h>
#include <sys/types.h>
#include <sys/promif.h>
#include <sys/vtoc.h>
#include <sys/archsystm.h>
#include <sys/sysmacros.h>

#include <sys/cdio.h>
#include <sys/dktp/cm.h>
#include <sys/dktp/fdisk.h>
#include <sys/scsi/generic/sense.h>
#include <sys/scsi/impl/uscsi.h>	/* Needed for defn of USCSICMD ioctl */
#include <sys/scsi/targets/sddef.h>

#include <sys/ldoms.h>
#include <sys/ldc.h>
#include <sys/vio_common.h>
#include <sys/vio_mailbox.h>
#include <sys/vdsk_common.h>
#include <sys/vdsk_mailbox.h>
#include <sys/vdc.h>

/*
 * function prototypes
 */

/* standard driver functions */
static int	vdc_open(dev_t *dev, int flag, int otyp, cred_t *cred);
static int	vdc_close(dev_t dev, int flag, int otyp, cred_t *cred);
static int	vdc_strategy(struct buf *buf);
static int	vdc_print(dev_t dev, char *str);
static int	vdc_dump(dev_t dev, caddr_t addr, daddr_t blkno, int nblk);
static int	vdc_read(dev_t dev, struct uio *uio, cred_t *cred);
static int	vdc_write(dev_t dev, struct uio *uio, cred_t *cred);
static int	vdc_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
			cred_t *credp, int *rvalp);
static int	vdc_aread(dev_t dev, struct aio_req *aio, cred_t *cred);
static int	vdc_awrite(dev_t dev, struct aio_req *aio, cred_t *cred);

static int	vdc_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd,
			void *arg, void **resultp);
static int	vdc_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int	vdc_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);

/* setup */
static int	vdc_send(vdc_t *vdc, caddr_t pkt, size_t *msglen);
static int	vdc_do_ldc_init(vdc_t *vdc);
static int	vdc_start_ldc_connection(vdc_t *vdc);
static int	vdc_create_device_nodes(vdc_t *vdc);
static int	vdc_create_device_nodes_efi(vdc_t *vdc);
static int	vdc_create_device_nodes_vtoc(vdc_t *vdc);
static int	vdc_create_device_nodes_props(vdc_t *vdc);
static int	vdc_get_ldc_id(dev_info_t *dip, uint64_t *ldc_id);
static int	vdc_do_ldc_up(vdc_t *vdc);
static void	vdc_terminate_ldc(vdc_t *vdc);
static int	vdc_init_descriptor_ring(vdc_t *vdc);
static void	vdc_destroy_descriptor_ring(vdc_t *vdc);
static int	vdc_setup_devid(vdc_t *vdc);
static void	vdc_store_efi(vdc_t *vdc, struct dk_gpt *efi);

/* handshake with vds */
static void		vdc_init_handshake_negotiation(void *arg);
static int		vdc_init_ver_negotiation(vdc_t *vdc, vio_ver_t ver);
static int		vdc_init_attr_negotiation(vdc_t *vdc);
static int		vdc_init_dring_negotiate(vdc_t *vdc);
static void		vdc_reset_connection(vdc_t *vdc, boolean_t resetldc);
static boolean_t	vdc_is_able_to_tx_data(vdc_t *vdc, int flag);
static boolean_t	vdc_is_supported_version(vio_ver_msg_t *ver_msg);

/* processing incoming messages from vDisk server */
static void	vdc_process_msg_thread(vdc_t *vdc);
static void	vdc_process_msg(void *arg);
static void	vdc_do_process_msg(vdc_t *vdc);
static uint_t	vdc_handle_cb(uint64_t event, caddr_t arg);
static int	vdc_process_ctrl_msg(vdc_t *vdc, vio_msg_t msg);
static int	vdc_process_data_msg(vdc_t *vdc, vio_msg_t msg);
static int	vdc_process_err_msg(vdc_t *vdc, vio_msg_t msg);
static int	vdc_handle_ver_msg(vdc_t *vdc, vio_ver_msg_t *ver_msg);
static int	vdc_handle_attr_msg(vdc_t *vdc, vd_attr_msg_t *attr_msg);
static int	vdc_handle_dring_reg_msg(vdc_t *vdc, vio_dring_reg_msg_t *msg);
static int	vdc_get_next_dring_entry_id(vdc_t *vdc, uint_t needed);
static int	vdc_populate_descriptor(vdc_t *vdc, caddr_t addr,
			size_t nbytes, int op, uint64_t arg, uint64_t slice);
static int	vdc_wait_for_descriptor_update(vdc_t *vdc, uint_t idx,
			vio_dring_msg_t dmsg);
static int	vdc_depopulate_descriptor(vdc_t *vdc, uint_t idx);
static int	vdc_populate_mem_hdl(vdc_t *vdc, uint_t idx,
			caddr_t addr, size_t nbytes, int operation);
static int	vdc_verify_seq_num(vdc_t *vdc, vio_dring_msg_t *dring_msg);

/* dkio */
static int	vd_process_ioctl(dev_t dev, int cmd, caddr_t arg, int mode);
static int	vdc_create_fake_geometry(vdc_t *vdc);
static int	vdc_setup_disk_layout(vdc_t *vdc);
static int	vdc_null_copy_func(vdc_t *vdc, void *from, void *to,
		    int mode, int dir);
static int	vdc_get_wce_convert(vdc_t *vdc, void *from, void *to,
		    int mode, int dir);
static int	vdc_set_wce_convert(vdc_t *vdc, void *from, void *to,
		    int mode, int dir);
static int	vdc_get_vtoc_convert(vdc_t *vdc, void *from, void *to,
		    int mode, int dir);
static int	vdc_set_vtoc_convert(vdc_t *vdc, void *from, void *to,
		    int mode, int dir);
static int	vdc_get_geom_convert(vdc_t *vdc, void *from, void *to,
		    int mode, int dir);
static int	vdc_set_geom_convert(vdc_t *vdc, void *from, void *to,
		    int mode, int dir);
static int	vdc_uscsicmd_convert(vdc_t *vdc, void *from, void *to,
		    int mode, int dir);
static int	vdc_get_efi_convert(vdc_t *vdc, void *from, void *to,
		    int mode, int dir);
static int	vdc_set_efi_convert(vdc_t *vdc, void *from, void *to,
		    int mode, int dir);

/*
 * Module variables
 */

/*
 * Tunable variables to control how long vdc waits before timing out on
 * various operations
 */
static int	vdc_retries = 10;

/* calculated from 'vdc_usec_timeout' during attach */
static uint64_t	vdc_hz_timeout;				/* units: Hz */
static uint64_t	vdc_usec_timeout = 30 * MICROSEC;	/* 30s units: ns */

static uint64_t	vdc_hz_timeout_ldc;			/* units: Hz */
static uint64_t	vdc_usec_timeout_ldc = 10 * MILLISEC;	/* 0.01s units: ns */

/* values for dumping - need to run in a tighter loop */
static uint64_t	vdc_usec_timeout_dump = 100 * MILLISEC;	/* 0.1s units: ns */
static int	vdc_dump_retries = 100;

/* Count of the number of vdc instances attached */
static volatile uint32_t	vdc_instance_count = 0;

/* Soft state pointer */
static void	*vdc_state;

/* variable level controlling the verbosity of the error/debug messages */
int	vdc_msglevel = 0;

/*
 * Supported vDisk protocol version pairs.
 *
 * The first array entry is the latest and preferred version.
 */
static const vio_ver_t	vdc_version[] = {{1, 0}};

static struct cb_ops vdc_cb_ops = {
	vdc_open,	/* cb_open */
	vdc_close,	/* cb_close */
	vdc_strategy,	/* cb_strategy */
	vdc_print,	/* cb_print */
	vdc_dump,	/* cb_dump */
	vdc_read,	/* cb_read */
	vdc_write,	/* cb_write */
	vdc_ioctl,	/* cb_ioctl */
	nodev,		/* cb_devmap */
	nodev,		/* cb_mmap */
	nodev,		/* cb_segmap */
	nochpoll,	/* cb_chpoll */
	ddi_prop_op,	/* cb_prop_op */
	NULL,		/* cb_str */
	D_MP | D_64BIT,	/* cb_flag */
	CB_REV,		/* cb_rev */
	vdc_aread,	/* cb_aread */
	vdc_awrite	/* cb_awrite */
};

static struct dev_ops vdc_ops = {
	DEVO_REV,	/* devo_rev */
	0,		/* devo_refcnt */
	vdc_getinfo,	/* devo_getinfo */
	nulldev,	/* devo_identify */
	nulldev,	/* devo_probe */
	vdc_attach,	/* devo_attach */
	vdc_detach,	/* devo_detach */
	nodev,		/* devo_reset */
	&vdc_cb_ops,	/* devo_cb_ops */
	NULL,		/* devo_bus_ops */
	nulldev		/* devo_power */
};

static struct modldrv modldrv = {
	&mod_driverops,
	"virtual disk client 1.6",
	&vdc_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

/* -------------------------------------------------------------------------- */

/*
 * Device Driver housekeeping and setup
 */

int
_init(void)
{
	int	status;

	if ((status = ddi_soft_state_init(&vdc_state, sizeof (vdc_t), 1)) != 0)
		return (status);
	if ((status = mod_install(&modlinkage)) != 0)
		ddi_soft_state_fini(&vdc_state);
	vdc_efi_init(vd_process_ioctl);
	return (status);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	int	status;

	if ((status = mod_remove(&modlinkage)) != 0)
		return (status);
	vdc_efi_fini();
	ddi_soft_state_fini(&vdc_state);
	return (0);
}

static int
vdc_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd,  void *arg, void **resultp)
{
	_NOTE(ARGUNUSED(dip))

	int	instance = SDUNIT(getminor((dev_t)arg));
	vdc_t	*vdc = NULL;

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((vdc = ddi_get_soft_state(vdc_state, instance)) == NULL) {
			*resultp = NULL;
			return (DDI_FAILURE);
		}
		*resultp = vdc->dip;
		return (DDI_SUCCESS);
	case DDI_INFO_DEVT2INSTANCE:
		*resultp = (void *)(uintptr_t)instance;
		return (DDI_SUCCESS);
	default:
		*resultp = NULL;
		return (DDI_FAILURE);
	}
}

static int
vdc_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int	instance;
	int	rv;
	uint_t	retries = 0;
	vdc_t	*vdc = NULL;

	switch (cmd) {
	case DDI_DETACH:
		/* the real work happens below */
		break;
	case DDI_SUSPEND:
		/* nothing to do for this non-device */
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	ASSERT(cmd == DDI_DETACH);
	instance = ddi_get_instance(dip);
	DMSG(1, "[%d] Entered\n", instance);

	if ((vdc = ddi_get_soft_state(vdc_state, instance)) == NULL) {
		cmn_err(CE_NOTE, "[%d] Couldn't get state structure", instance);
		return (DDI_FAILURE);
	}

	if (vdc->open) {
		DMSG(0, "[%d] Cannot detach: device is open", instance);
		return (DDI_FAILURE);
	}

	DMSG(0, "[%d] proceeding...\n", instance);

	/*
	 * try and disable callbacks to prevent another handshake
	 */
	rv = ldc_set_cb_mode(vdc->ldc_handle, LDC_CB_DISABLE);
	DMSG(0, "[%d] callback disabled (rv=%d)\n", instance, rv);

	/*
	 * Prevent any more attempts to start a handshake with the vdisk
	 * server and tear down the existing connection.
	 */
	mutex_enter(&vdc->lock);
	vdc->initialized |= VDC_HANDSHAKE_STOP;
	vdc_reset_connection(vdc, B_TRUE);
	mutex_exit(&vdc->lock);

	if (vdc->initialized & VDC_THREAD) {
		mutex_enter(&vdc->msg_proc_lock);
		vdc->msg_proc_thr_state = VDC_THR_STOP;
		vdc->msg_pending = B_TRUE;
		cv_signal(&vdc->msg_proc_cv);

		while (vdc->msg_proc_thr_state != VDC_THR_DONE) {
			DMSG(0, "[%d] Waiting for thread to exit\n", instance);
			rv = cv_timedwait(&vdc->msg_proc_cv,
				&vdc->msg_proc_lock,
				VD_GET_TIMEOUT_HZ(vdc_hz_timeout, 1));
			if ((rv == -1) && (retries++ > vdc_retries))
				break;
		}
		mutex_exit(&vdc->msg_proc_lock);
	}

	mutex_enter(&vdc->lock);

	if (vdc->initialized & VDC_DRING)
		vdc_destroy_descriptor_ring(vdc);

	if (vdc->initialized & VDC_LDC)
		vdc_terminate_ldc(vdc);

	mutex_exit(&vdc->lock);

	if (vdc->initialized & VDC_MINOR) {
		ddi_prop_remove_all(dip);
		ddi_remove_minor_node(dip, NULL);
	}

	if (vdc->initialized & VDC_LOCKS) {
		mutex_destroy(&vdc->lock);
		mutex_destroy(&vdc->attach_lock);
		mutex_destroy(&vdc->msg_proc_lock);
		mutex_destroy(&vdc->dring_lock);
		cv_destroy(&vdc->cv);
		cv_destroy(&vdc->attach_cv);
		cv_destroy(&vdc->msg_proc_cv);
	}

	if (vdc->minfo)
		kmem_free(vdc->minfo, sizeof (struct dk_minfo));

	if (vdc->cinfo)
		kmem_free(vdc->cinfo, sizeof (struct dk_cinfo));

	if (vdc->vtoc)
		kmem_free(vdc->vtoc, sizeof (struct vtoc));

	if (vdc->label)
		kmem_free(vdc->label, DK_LABEL_SIZE);

	if (vdc->devid) {
		ddi_devid_unregister(dip);
		ddi_devid_free(vdc->devid);
	}

	if (vdc->initialized & VDC_SOFT_STATE)
		ddi_soft_state_free(vdc_state, instance);

	DMSG(0, "[%d] End %p\n", instance, (void *)vdc);

	return (DDI_SUCCESS);
}


static int
vdc_do_attach(dev_info_t *dip)
{
	int		instance;
	vdc_t		*vdc = NULL;
	int		status;
	uint_t		retries = 0;

	ASSERT(dip != NULL);

	instance = ddi_get_instance(dip);
	if (ddi_soft_state_zalloc(vdc_state, instance) != DDI_SUCCESS) {
		cmn_err(CE_NOTE, "[%d] Couldn't alloc state structure",
		    instance);
		return (DDI_FAILURE);
	}

	if ((vdc = ddi_get_soft_state(vdc_state, instance)) == NULL) {
		cmn_err(CE_NOTE, "[%d] Couldn't get state structure", instance);
		return (DDI_FAILURE);
	}

	/*
	 * We assign the value to initialized in this case to zero out the
	 * variable and then set bits in it to indicate what has been done
	 */
	vdc->initialized = VDC_SOFT_STATE;

	vdc_hz_timeout = drv_usectohz(vdc_usec_timeout);
	vdc_hz_timeout_ldc = drv_usectohz(vdc_usec_timeout_ldc);

	vdc->dip	= dip;
	vdc->instance	= instance;
	vdc->open	= 0;
	vdc->vdisk_type	= VD_DISK_TYPE_UNK;
	vdc->vdisk_label = VD_DISK_LABEL_UNK;
	vdc->state	= VD_STATE_INIT;
	vdc->ldc_state	= 0;
	vdc->session_id = 0;
	vdc->block_size = DEV_BSIZE;
	vdc->max_xfer_sz = maxphys / DEV_BSIZE;

	vdc->vtoc = NULL;
	vdc->cinfo = NULL;
	vdc->minfo = NULL;

	mutex_init(&vdc->lock, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&vdc->attach_lock, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&vdc->msg_proc_lock, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&vdc->dring_lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&vdc->cv, NULL, CV_DRIVER, NULL);
	cv_init(&vdc->attach_cv, NULL, CV_DRIVER, NULL);
	cv_init(&vdc->msg_proc_cv, NULL, CV_DRIVER, NULL);
	vdc->initialized |= VDC_LOCKS;

	vdc->msg_pending = B_FALSE;
	vdc->msg_proc_thr_id = thread_create(NULL, 0, vdc_process_msg_thread,
		vdc, 0, &p0, TS_RUN, minclsyspri);
	if (vdc->msg_proc_thr_id == NULL) {
		cmn_err(CE_NOTE, "[%d] Failed to create msg processing thread",
				instance);
		return (DDI_FAILURE);
	}
	vdc->initialized |= VDC_THREAD;

	/* initialise LDC channel which will be used to communicate with vds */
	if (vdc_do_ldc_init(vdc) != 0) {
		cmn_err(CE_NOTE, "[%d] Couldn't initialize LDC", instance);
		return (DDI_FAILURE);
	}

	/* Bring up connection with vds via LDC */
	status = vdc_start_ldc_connection(vdc);
	if (status != 0) {
		cmn_err(CE_NOTE, "[%d] Could not start LDC", instance);
		return (DDI_FAILURE);
	}

	/*
	 * We need to wait until the handshake has completed before leaving
	 * the attach(). If this is the first vdc device attached (i.e. the root
	 * filesystem) we will wait much longer in the hope that we can finally
	 * communicate with the vDisk server (the service domain may be
	 * rebooting, etc.). This wait is necessary so that the device node(s)
	 * are created before the attach(9E) return (otherwise the open(9E) will
	 * fail and and the root file system will not boot).
	 */
	atomic_inc_32(&vdc_instance_count);
	mutex_enter(&vdc->attach_lock);
	while ((vdc->ldc_state != LDC_UP) || (vdc->state != VD_STATE_DATA)) {

		DMSG(0, "[%d] handshake in progress [VD %d (LDC %d)]\n",
			instance, vdc->state, vdc->ldc_state);

		status = cv_timedwait(&vdc->attach_cv, &vdc->attach_lock,
				VD_GET_TIMEOUT_HZ(vdc_hz_timeout, retries));
		if (status == -1) {
			/*
			 * If this is not the first instance attached or we
			 * have exceeeded the max number of retries we give
			 * up waiting and do not delay the attach any longer
			 */
			if ((vdc_instance_count != 1) ||
			    (retries >= vdc_retries)) {
				DMSG(0, "[%d] Giving up wait for handshake\n",
						instance);
				mutex_exit(&vdc->attach_lock);
				return (DDI_FAILURE);
			} else {
				DMSG(0, "[%d] Retry #%d for handshake.\n",
						instance, retries);
				vdc_init_handshake_negotiation(vdc);
				retries++;
			}
		}
	}
	mutex_exit(&vdc->attach_lock);

	/*
	 * Once the handshake is complete, we can use the DRing to send
	 * requests to the vDisk server to calculate the geometry and
	 * VTOC of the "disk"
	 */
	status = vdc_setup_disk_layout(vdc);
	if (status != 0) {
		cmn_err(CE_NOTE, "[%d] Failed to discover disk layout (err%d)",
				vdc->instance, status);
	}

	/*
	 * Now that we have the device info we can create the
	 * device nodes and properties
	 */
	status = vdc_create_device_nodes(vdc);
	if (status) {
		cmn_err(CE_NOTE, "[%d] Failed to create device nodes",
				instance);
		return (status);
	}
	status = vdc_create_device_nodes_props(vdc);
	if (status) {
		cmn_err(CE_NOTE, "[%d] Failed to create device nodes"
				" properties (%d)", instance, status);
		return (status);
	}

	/*
	 * Setup devid
	 */
	if (vdc_setup_devid(vdc)) {
		DMSG(0, "[%d] No device id available\n", instance);
	}

	ddi_report_dev(dip);

	DMSG(0, "[%d] Attach completed\n", instance);
	return (status);
}

static int
vdc_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int	status;

	switch (cmd) {
	case DDI_ATTACH:
		if ((status = vdc_do_attach(dip)) != 0)
			(void) vdc_detach(dip, DDI_DETACH);
		return (status);
	case DDI_RESUME:
		/* nothing to do for this non-device */
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

static int
vdc_do_ldc_init(vdc_t *vdc)
{
	int			status = 0;
	ldc_status_t		ldc_state;
	ldc_attr_t		ldc_attr;
	uint64_t		ldc_id = 0;
	dev_info_t		*dip = NULL;

	ASSERT(vdc != NULL);

	dip = vdc->dip;
	vdc->initialized |= VDC_LDC;

	if ((status = vdc_get_ldc_id(dip, &ldc_id)) != 0) {
		cmn_err(CE_NOTE, "[%d] Failed to get LDC channel ID property",
				vdc->instance);
		return (EIO);
	}
	vdc->ldc_id = ldc_id;

	ldc_attr.devclass = LDC_DEV_BLK;
	ldc_attr.instance = vdc->instance;
	ldc_attr.mode = LDC_MODE_UNRELIABLE;	/* unreliable transport */
	ldc_attr.mtu = VD_LDC_MTU;

	if ((vdc->initialized & VDC_LDC_INIT) == 0) {
		status = ldc_init(ldc_id, &ldc_attr, &vdc->ldc_handle);
		if (status != 0) {
			cmn_err(CE_NOTE, "[%d] ldc_init(chan %ld) returned %d",
					vdc->instance, ldc_id, status);
			return (status);
		}
		vdc->initialized |= VDC_LDC_INIT;
	}
	status = ldc_status(vdc->ldc_handle, &ldc_state);
	if (status != 0) {
		cmn_err(CE_NOTE, "[%d] Cannot discover LDC status [err=%d]",
				vdc->instance, status);
		return (status);
	}
	vdc->ldc_state = ldc_state;

	if ((vdc->initialized & VDC_LDC_CB) == 0) {
		status = ldc_reg_callback(vdc->ldc_handle, vdc_handle_cb,
		    (caddr_t)vdc);
		if (status != 0) {
			cmn_err(CE_NOTE, "[%d] LDC callback reg. failed (%d)",
					vdc->instance, status);
			return (status);
		}
		vdc->initialized |= VDC_LDC_CB;
	}

	vdc->initialized |= VDC_LDC;

	/*
	 * At this stage we have initialised LDC, we will now try and open
	 * the connection.
	 */
	if (vdc->ldc_state == LDC_INIT) {
		status = ldc_open(vdc->ldc_handle);
		if (status != 0) {
			cmn_err(CE_NOTE, "[%d] ldc_open(chan %ld) returned %d",
					vdc->instance, vdc->ldc_id, status);
			return (status);
		}
		vdc->initialized |= VDC_LDC_OPEN;
	}

	return (status);
}

static int
vdc_start_ldc_connection(vdc_t *vdc)
{
	int		status = 0;

	ASSERT(vdc != NULL);

	mutex_enter(&vdc->lock);

	if (vdc->ldc_state == LDC_UP) {
		DMSG(0, "[%d] LDC is already UP ..\n", vdc->instance);
		mutex_exit(&vdc->lock);
		return (0);
	}

	status = vdc_do_ldc_up(vdc);

	DMSG(0, "[%d] Finished bringing up LDC\n", vdc->instance);

	mutex_exit(&vdc->lock);

	return (status);
}

static int
vdc_create_device_nodes_efi(vdc_t *vdc)
{
	ddi_remove_minor_node(vdc->dip, "h");
	ddi_remove_minor_node(vdc->dip, "h,raw");

	if (ddi_create_minor_node(vdc->dip, "wd", S_IFBLK,
		VD_MAKE_DEV(vdc->instance, VD_EFI_WD_SLICE),
		DDI_NT_BLOCK, 0) != DDI_SUCCESS) {
		cmn_err(CE_NOTE, "[%d] Couldn't add block node 'wd'",
		    vdc->instance);
		return (EIO);
	}

	/* if any device node is created we set this flag */
	vdc->initialized |= VDC_MINOR;

	if (ddi_create_minor_node(vdc->dip, "wd,raw", S_IFCHR,
		VD_MAKE_DEV(vdc->instance, VD_EFI_WD_SLICE),
		DDI_NT_BLOCK, 0) != DDI_SUCCESS) {
		cmn_err(CE_NOTE, "[%d] Couldn't add block node 'wd,raw'",
		    vdc->instance);
		return (EIO);
	}

	return (0);
}

static int
vdc_create_device_nodes_vtoc(vdc_t *vdc)
{
	ddi_remove_minor_node(vdc->dip, "wd");
	ddi_remove_minor_node(vdc->dip, "wd,raw");

	if (ddi_create_minor_node(vdc->dip, "h", S_IFBLK,
		VD_MAKE_DEV(vdc->instance, VD_EFI_WD_SLICE),
		DDI_NT_BLOCK, 0) != DDI_SUCCESS) {
		cmn_err(CE_NOTE, "[%d] Couldn't add block node 'h'",
		    vdc->instance);
		return (EIO);
	}

	/* if any device node is created we set this flag */
	vdc->initialized |= VDC_MINOR;

	if (ddi_create_minor_node(vdc->dip, "h,raw", S_IFCHR,
		VD_MAKE_DEV(vdc->instance, VD_EFI_WD_SLICE),
		DDI_NT_BLOCK, 0) != DDI_SUCCESS) {
		cmn_err(CE_NOTE, "[%d] Couldn't add block node 'h,raw'",
		    vdc->instance);
		return (EIO);
	}

	return (0);
}

/*
 * Function:
 *	vdc_create_device_nodes
 *
 * Description:
 *	This function creates the block and character device nodes under
 *	/devices along with the node properties. It is called as part of
 *	the attach(9E) of the instance during the handshake with vds after
 *	vds has sent the attributes to vdc.
 *
 *	If the device is of type VD_DISK_TYPE_SLICE then the minor node
 *	of 2 is used in keeping with the Solaris convention that slice 2
 *	refers to a whole disk. Slices start at 'a'
 *
 * Parameters:
 *	vdc 		- soft state pointer
 *
 * Return Values
 *	0		- Success
 *	EIO		- Failed to create node
 *	EINVAL		- Unknown type of disk exported
 */
static int
vdc_create_device_nodes(vdc_t *vdc)
{
	char		name[sizeof ("s,raw")];
	dev_info_t	*dip = NULL;
	int		instance, status;
	int		num_slices = 1;
	int		i;

	ASSERT(vdc != NULL);

	instance = vdc->instance;
	dip = vdc->dip;

	switch (vdc->vdisk_type) {
	case VD_DISK_TYPE_DISK:
		num_slices = V_NUMPAR;
		break;
	case VD_DISK_TYPE_SLICE:
		num_slices = 1;
		break;
	case VD_DISK_TYPE_UNK:
	default:
		return (EINVAL);
	}

	/*
	 * Minor nodes are different for EFI disks: EFI disks do not have
	 * a minor node 'g' for the minor number corresponding to slice
	 * VD_EFI_WD_SLICE (slice 7) instead they have a minor node 'wd'
	 * representing the whole disk.
	 */
	for (i = 0; i < num_slices; i++) {

		if (i == VD_EFI_WD_SLICE) {
			if (vdc->vdisk_label == VD_DISK_LABEL_EFI)
				status = vdc_create_device_nodes_efi(vdc);
			else
				status = vdc_create_device_nodes_vtoc(vdc);
			if (status != 0)
				return (status);
			continue;
		}

		(void) snprintf(name, sizeof (name), "%c", 'a' + i);
		if (ddi_create_minor_node(dip, name, S_IFBLK,
		    VD_MAKE_DEV(instance, i), DDI_NT_BLOCK, 0) != DDI_SUCCESS) {
			cmn_err(CE_NOTE, "[%d] Couldn't add block node '%s'",
				instance, name);
			return (EIO);
		}

		/* if any device node is created we set this flag */
		vdc->initialized |= VDC_MINOR;

		(void) snprintf(name, sizeof (name), "%c%s",
			'a' + i, ",raw");
		if (ddi_create_minor_node(dip, name, S_IFCHR,
		    VD_MAKE_DEV(instance, i), DDI_NT_BLOCK, 0) != DDI_SUCCESS) {
			cmn_err(CE_NOTE, "[%d] Couldn't add raw node '%s'",
				instance, name);
			return (EIO);
		}
	}

	return (0);
}

/*
 * Function:
 *	vdc_create_device_nodes_props
 *
 * Description:
 *	This function creates the block and character device nodes under
 *	/devices along with the node properties. It is called as part of
 *	the attach(9E) of the instance during the handshake with vds after
 *	vds has sent the attributes to vdc.
 *
 * Parameters:
 *	vdc 		- soft state pointer
 *
 * Return Values
 *	0		- Success
 *	EIO		- Failed to create device node property
 *	EINVAL		- Unknown type of disk exported
 */
static int
vdc_create_device_nodes_props(vdc_t *vdc)
{
	dev_info_t	*dip = NULL;
	int		instance;
	int		num_slices = 1;
	int64_t		size = 0;
	dev_t		dev;
	int		rv;
	int		i;

	ASSERT(vdc != NULL);

	instance = vdc->instance;
	dip = vdc->dip;

	if ((vdc->vtoc == NULL) || (vdc->vtoc->v_sanity != VTOC_SANE)) {
		cmn_err(CE_NOTE, "![%d] Could not create device node property."
				" No VTOC available", instance);
		return (ENXIO);
	}

	switch (vdc->vdisk_type) {
	case VD_DISK_TYPE_DISK:
		num_slices = V_NUMPAR;
		break;
	case VD_DISK_TYPE_SLICE:
		num_slices = 1;
		break;
	case VD_DISK_TYPE_UNK:
	default:
		return (EINVAL);
	}

	for (i = 0; i < num_slices; i++) {
		dev = makedevice(ddi_driver_major(dip),
			VD_MAKE_DEV(instance, i));

		size = vdc->vtoc->v_part[i].p_size * vdc->vtoc->v_sectorsz;
		DMSG(0, "[%d] sz %ld (%ld Mb)  p_size %lx\n",
				instance, size, size / (1024 * 1024),
				vdc->vtoc->v_part[i].p_size);

		rv = ddi_prop_update_int64(dev, dip, VDC_SIZE_PROP_NAME, size);
		if (rv != DDI_PROP_SUCCESS) {
			cmn_err(CE_NOTE, "[%d] Couldn't add '%s' prop of [%ld]",
				instance, VDC_SIZE_PROP_NAME, size);
			return (EIO);
		}

		rv = ddi_prop_update_int64(dev, dip, VDC_NBLOCKS_PROP_NAME,
			lbtodb(size));
		if (rv != DDI_PROP_SUCCESS) {
			cmn_err(CE_NOTE, "[%d] Couldn't add '%s' prop [%llu]",
				instance, VDC_NBLOCKS_PROP_NAME, lbtodb(size));
			return (EIO);
		}
	}

	return (0);
}

static int
vdc_open(dev_t *dev, int flag, int otyp, cred_t *cred)
{
	_NOTE(ARGUNUSED(cred))

	int		instance;
	vdc_t		*vdc;

	ASSERT(dev != NULL);
	instance = SDUNIT(getminor(*dev));

	DMSG(0, "[%d] minor = %d flag = %x, otyp = %x\n",
			instance, getminor(*dev), flag, otyp);

	if ((otyp != OTYP_CHR) && (otyp != OTYP_BLK))
		return (EINVAL);

	if ((vdc = ddi_get_soft_state(vdc_state, instance)) == NULL) {
		cmn_err(CE_NOTE, "[%d] Couldn't get state structure", instance);
		return (ENXIO);
	}

	/*
	 * Check to see if we can communicate with vds
	 */
	if (!vdc_is_able_to_tx_data(vdc, flag)) {
		DMSG(0, "[%d] Not ready to transmit data (flag=%x)\n",
				instance, flag);
		return (ENOLINK);
	}

	mutex_enter(&vdc->lock);
	vdc->open++;
	mutex_exit(&vdc->lock);

	return (0);
}

static int
vdc_close(dev_t dev, int flag, int otyp, cred_t *cred)
{
	_NOTE(ARGUNUSED(cred))

	int	instance;
	vdc_t	*vdc;

	instance = SDUNIT(getminor(dev));

	DMSG(0, "[%d] flag = %x, otyp = %x\n", instance, flag, otyp);

	if ((otyp != OTYP_CHR) && (otyp != OTYP_BLK))
		return (EINVAL);

	if ((vdc = ddi_get_soft_state(vdc_state, instance)) == NULL) {
		cmn_err(CE_NOTE, "[%d] Couldn't get state structure", instance);
		return (ENXIO);
	}

	/*
	 * Check to see if we can communicate with vds
	 */
	if (!vdc_is_able_to_tx_data(vdc, 0)) {
		DMSG(0, "[%d] Not ready to transmit data (flag=%x)\n",
				instance, flag);
		return (ETIMEDOUT);
	}

	if (vdc->dkio_flush_pending) {
		DMSG(0, "[%d] Cannot detach: %d outstanding DKIO flushes\n",
			instance, vdc->dkio_flush_pending);
		return (EBUSY);
	}

	/*
	 * Should not need the mutex here, since the framework should protect
	 * against more opens on this device, but just in case.
	 */
	mutex_enter(&vdc->lock);
	vdc->open--;
	mutex_exit(&vdc->lock);

	return (0);
}

static int
vdc_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp, int *rvalp)
{
	_NOTE(ARGUNUSED(credp))
	_NOTE(ARGUNUSED(rvalp))

	return (vd_process_ioctl(dev, cmd, (caddr_t)arg, mode));
}

static int
vdc_print(dev_t dev, char *str)
{
	cmn_err(CE_NOTE, "vdc%d:  %s", SDUNIT(getminor(dev)), str);
	return (0);
}

static int
vdc_dump(dev_t dev, caddr_t addr, daddr_t blkno, int nblk)
{
	buf_t	*buf;	/* BWRITE requests need to be in a buf_t structure */
	int	rv;
	size_t	nbytes = nblk * DEV_BSIZE;
	int	instance = SDUNIT(getminor(dev));
	vdc_t	*vdc = NULL;

	if ((vdc = ddi_get_soft_state(vdc_state, instance)) == NULL) {
		cmn_err(CE_NOTE, "[%d] Couldn't get state structure", instance);
		return (ENXIO);
	}

	buf = kmem_alloc(sizeof (buf_t), KM_SLEEP);
	bioinit(buf);
	buf->b_un.b_addr = addr;
	buf->b_bcount = nbytes;
	buf->b_flags = B_BUSY | B_WRITE;
	buf->b_dev = dev;
	rv = vdc_populate_descriptor(vdc, (caddr_t)buf, nbytes,
			VD_OP_BWRITE, blkno, SDPART(getminor(dev)));

	/*
	 * If the OS instance is panicking, the call above will ensure that
	 * the descriptor is done before returning. This should always be
	 * case when coming through this function but we check just in case
	 * and wait if necessary for the vDisk server to ACK and trigger
	 * the biodone.
	 */
	if (!ddi_in_panic())
		rv = biowait(buf);

	biofini(buf);
	kmem_free(buf, sizeof (buf_t));

	DMSG(1, "[%d] status=%d\n", instance, rv);

	return (rv);
}

/* -------------------------------------------------------------------------- */

/*
 * Disk access routines
 *
 */

/*
 * vdc_strategy()
 *
 * Return Value:
 *	0:	As per strategy(9E), the strategy() function must return 0
 *		[ bioerror(9f) sets b_flags to the proper error code ]
 */
static int
vdc_strategy(struct buf *buf)
{
	int		rv = -1;
	vdc_t		*vdc = NULL;
	int		instance = SDUNIT(getminor(buf->b_edev));
	int	op = (buf->b_flags & B_READ) ? VD_OP_BREAD : VD_OP_BWRITE;

	DMSG(2, "[%d] %s %ld bytes at block %llx : b_addr=0x%p",
	    instance, (buf->b_flags & B_READ) ? "Read" : "Write",
	    buf->b_bcount, buf->b_lblkno, (void *)buf->b_un.b_addr);

	if ((vdc = ddi_get_soft_state(vdc_state, instance)) == NULL) {
		cmn_err(CE_NOTE, "[%d] Couldn't get state structure", instance);
		bioerror(buf, ENXIO);
		biodone(buf);
		return (0);
	}

	DTRACE_IO2(vstart, buf_t *, buf, vdc_t *, vdc);

	if (!vdc_is_able_to_tx_data(vdc, O_NONBLOCK)) {
		DMSG(0, "[%d] Not ready to transmit data\n", instance);
		bioerror(buf, ENXIO);
		biodone(buf);
		return (0);
	}
	bp_mapin(buf);

	rv = vdc_populate_descriptor(vdc, (caddr_t)buf, buf->b_bcount, op,
			buf->b_lblkno, SDPART(getminor(buf->b_edev)));

	/*
	 * If the request was successfully sent, the strategy call returns and
	 * the ACK handler calls the bioxxx functions when the vDisk server is
	 * done.
	 */
	if (rv) {
		DMSG(0, "[%d] Failed to read/write (err=%d)\n", instance, rv);
		bioerror(buf, rv);
		biodone(buf);
	}

	return (0);
}


static int
vdc_read(dev_t dev, struct uio *uio, cred_t *cred)
{
	_NOTE(ARGUNUSED(cred))

	DMSG(1, "[%d] Entered", SDUNIT(getminor(dev)));
	return (physio(vdc_strategy, NULL, dev, B_READ, minphys, uio));
}

static int
vdc_write(dev_t dev, struct uio *uio, cred_t *cred)
{
	_NOTE(ARGUNUSED(cred))

	DMSG(1, "[%d] Entered", SDUNIT(getminor(dev)));
	return (physio(vdc_strategy, NULL, dev, B_WRITE, minphys, uio));
}

static int
vdc_aread(dev_t dev, struct aio_req *aio, cred_t *cred)
{
	_NOTE(ARGUNUSED(cred))

	DMSG(1, "[%d] Entered", SDUNIT(getminor(dev)));
	return (aphysio(vdc_strategy, anocancel, dev, B_READ, minphys, aio));
}

static int
vdc_awrite(dev_t dev, struct aio_req *aio, cred_t *cred)
{
	_NOTE(ARGUNUSED(cred))

	DMSG(1, "[%d] Entered", SDUNIT(getminor(dev)));
	return (aphysio(vdc_strategy, anocancel, dev, B_WRITE, minphys, aio));
}


/* -------------------------------------------------------------------------- */

/*
 * Handshake support
 */

/*
 * vdc_init_handshake_negotiation
 *
 * Description:
 *	This function is called to trigger the handshake negotiations between
 *	the client (vdc) and the server (vds). It may be called multiple times.
 *
 * Parameters:
 *	vdc - soft state pointer
 */
static void
vdc_init_handshake_negotiation(void *arg)
{
	vdc_t		*vdc = (vdc_t *)(void *)arg;
	ldc_status_t	ldc_state;
	vd_state_t	state;
	int		status;

	ASSERT(vdc != NULL);

	DMSG(0, "[%d] Initializing vdc<->vds handshake\n", vdc->instance);

	/* get LDC state */
	status = ldc_status(vdc->ldc_handle, &ldc_state);
	if (status != 0) {
		cmn_err(CE_NOTE, "[%d] Couldn't get LDC status (err=%d)",
				vdc->instance, status);
		return;
	}

	/*
	 * If the LDC connection is not UP we bring it up now and return.
	 * The handshake will be started again when the callback is
	 * triggered due to the UP event.
	 */
	if (ldc_state != LDC_UP) {
		DMSG(0, "[%d] Triggering LDC_UP & returning\n", vdc->instance);
		(void) vdc_do_ldc_up(vdc);
		return;
	}

	mutex_enter(&vdc->lock);
	/*
	 * Do not continue if another thread has triggered a handshake which
	 * has not been reset or detach() has stopped further handshakes.
	 */
	if (vdc->initialized & (VDC_HANDSHAKE | VDC_HANDSHAKE_STOP)) {
		DMSG(0, "[%d] Negotiation not triggered. [init=%x]\n",
			vdc->instance, vdc->initialized);
		mutex_exit(&vdc->lock);
		return;
	}

	if (vdc->hshake_cnt++ > vdc_retries) {
		cmn_err(CE_NOTE, "[%d] Failed repeatedly to complete handshake"
				"with vDisk server", vdc->instance);
		mutex_exit(&vdc->lock);
		return;
	}

	vdc->initialized |= VDC_HANDSHAKE;
	vdc->ldc_state = ldc_state;

	state = vdc->state;

	if (state == VD_STATE_INIT) {
		/*
		 * Set the desired version parameter to the first entry in the
		 * version array. If this specific version is not supported,
		 * the response handling code will step down the version number
		 * to the next array entry and deal with it accordingly.
		 */
		(void) vdc_init_ver_negotiation(vdc, vdc_version[0]);
	} else if (state == VD_STATE_VER) {
		(void) vdc_init_attr_negotiation(vdc);
	} else if (state == VD_STATE_ATTR) {
		(void) vdc_init_dring_negotiate(vdc);
	} else if (state == VD_STATE_DATA) {
		/*
		 * nothing to do - we have already completed the negotiation
		 * and we can transmit data when ready.
		 */
		DMSG(0, "[%d] Negotiation triggered after handshake completed",
			vdc->instance);
	}

	mutex_exit(&vdc->lock);
}

/*
 * Function:
 *	vdc_init_ver_negotiation()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_init_ver_negotiation(vdc_t *vdc, vio_ver_t ver)
{
	vio_ver_msg_t	pkt;
	size_t		msglen = sizeof (pkt);
	int		status = -1;

	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));

	DMSG(0, "[%d] Entered.\n", vdc->instance);

	/*
	 * set the Session ID to a unique value
	 * (the lower 32 bits of the clock tick)
	 */
	vdc->session_id = ((uint32_t)gettick() & 0xffffffff);
	DMSG(0, "[%d] Set SID to 0x%lx\n", vdc->instance, vdc->session_id);

	pkt.tag.vio_msgtype = VIO_TYPE_CTRL;
	pkt.tag.vio_subtype = VIO_SUBTYPE_INFO;
	pkt.tag.vio_subtype_env = VIO_VER_INFO;
	pkt.tag.vio_sid = vdc->session_id;
	pkt.dev_class = VDEV_DISK;
	pkt.ver_major = ver.major;
	pkt.ver_minor = ver.minor;

	status = vdc_send(vdc, (caddr_t)&pkt, &msglen);
	DMSG(0, "[%d] Ver info sent (status = %d)\n", vdc->instance, status);

	if ((status != 0) || (msglen != sizeof (vio_ver_msg_t))) {
		cmn_err(CE_NOTE, "[%d] Failed to send Ver negotiation info: "
				"id(%lx) rv(%d) size(%ld)",
				vdc->instance, vdc->ldc_handle,
				status, msglen);
		if (msglen != sizeof (vio_ver_msg_t))
			status = ENOMSG;
	}

	return (status);
}

/*
 * Function:
 *	vdc_init_attr_negotiation()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_init_attr_negotiation(vdc_t *vdc)
{
	vd_attr_msg_t	pkt;
	size_t		msglen = sizeof (pkt);
	int		status;

	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));

	DMSG(0, "[%d] entered\n", vdc->instance);

	/* fill in tag */
	pkt.tag.vio_msgtype = VIO_TYPE_CTRL;
	pkt.tag.vio_subtype = VIO_SUBTYPE_INFO;
	pkt.tag.vio_subtype_env = VIO_ATTR_INFO;
	pkt.tag.vio_sid = vdc->session_id;
	/* fill in payload */
	pkt.max_xfer_sz = vdc->max_xfer_sz;
	pkt.vdisk_block_size = vdc->block_size;
	pkt.xfer_mode = VIO_DRING_MODE;
	pkt.operations = 0;	/* server will set bits of valid operations */
	pkt.vdisk_type = 0;	/* server will set to valid device type */
	pkt.vdisk_size = 0;	/* server will set to valid size */

	status = vdc_send(vdc, (caddr_t)&pkt, &msglen);
	DMSG(0, "[%d] Attr info sent (status = %d)\n", vdc->instance, status);

	if ((status != 0) || (msglen != sizeof (vio_ver_msg_t))) {
		cmn_err(CE_NOTE, "[%d] Failed to send Attr negotiation info: "
				"id(%lx) rv(%d) size(%ld)",
				vdc->instance, vdc->ldc_handle,
				status, msglen);
		if (msglen != sizeof (vio_ver_msg_t))
			status = ENOMSG;
	}

	return (status);
}

/*
 * Function:
 *	vdc_init_dring_negotiate()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_init_dring_negotiate(vdc_t *vdc)
{
	vio_dring_reg_msg_t	pkt;
	size_t			msglen = sizeof (pkt);
	int			status = -1;

	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));

	status = vdc_init_descriptor_ring(vdc);
	if (status != 0) {
		cmn_err(CE_CONT, "[%d] Failed to init DRing (status = %d)\n",
				vdc->instance, status);
		vdc_destroy_descriptor_ring(vdc);
		vdc_reset_connection(vdc, B_TRUE);
		return (status);
	}
	DMSG(0, "[%d] Init of descriptor ring completed (status = %d)\n",
			vdc->instance, status);

	/* fill in tag */
	pkt.tag.vio_msgtype = VIO_TYPE_CTRL;
	pkt.tag.vio_subtype = VIO_SUBTYPE_INFO;
	pkt.tag.vio_subtype_env = VIO_DRING_REG;
	pkt.tag.vio_sid = vdc->session_id;
	/* fill in payload */
	pkt.dring_ident = 0;
	pkt.num_descriptors = vdc->dring_len;
	pkt.descriptor_size = vdc->dring_entry_size;
	pkt.options = (VIO_TX_DRING | VIO_RX_DRING);
	pkt.ncookies = vdc->dring_cookie_count;
	pkt.cookie[0] = vdc->dring_cookie[0];	/* for now just one cookie */

	status = vdc_send(vdc, (caddr_t)&pkt, &msglen);
	if (status != 0) {
		cmn_err(CE_NOTE, "[%d] Failed to register DRing (err = %d)",
				vdc->instance, status);
		vdc_reset_connection(vdc, B_TRUE);
	}

	return (status);
}


/* -------------------------------------------------------------------------- */

/*
 * LDC helper routines
 */

/*
 * Function:
 *	vdc_send()
 *
 * Description:
 *	The function encapsulates the call to write a message using LDC.
 *	If LDC indicates that the call failed due to the queue being full,
 *	we retry the ldc_write() [ up to 'vdc_retries' time ], otherwise
 *	we return the error returned by LDC.
 *
 * Arguments:
 *	ldc_handle	- LDC handle for the channel this instance of vdc uses
 *	pkt		- address of LDC message to be sent
 *	msglen		- the size of the message being sent. When the function
 *			  returns, this contains the number of bytes written.
 *
 * Return Code:
 *	0		- Success.
 *	EINVAL		- pkt or msglen were NULL
 *	ECONNRESET	- The connection was not up.
 *	EWOULDBLOCK	- LDC queue is full
 *	xxx		- other error codes returned by ldc_write
 */
static int
vdc_send(vdc_t *vdc, caddr_t pkt, size_t *msglen)
{
	size_t	size = 0;
	int	retries = 0;
	int	status = 0;

	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));
	ASSERT(msglen != NULL);
	ASSERT(*msglen != 0);

	do {
		size = *msglen;
		status = ldc_write(vdc->ldc_handle, pkt, &size);
		if (status == EWOULDBLOCK)
			delay(vdc_hz_timeout_ldc);
	} while (status == EWOULDBLOCK && retries++ < vdc_retries);

	/* if LDC had serious issues --- reset vdc state */
	if (status == EIO || status == ECONNRESET) {
		vdc_reset_connection(vdc, B_TRUE);
	}

	/* return the last size written */
	*msglen = size;

	return (status);
}

/*
 * Function:
 *	vdc_get_ldc_id()
 *
 * Description:
 *	This function gets the 'ldc-id' for this particular instance of vdc.
 *	The id returned is the guest domain channel endpoint LDC uses for
 *	communication with vds.
 *
 * Arguments:
 *	dip	- dev info pointer for this instance of the device driver.
 *	ldc_id	- pointer to variable used to return the 'ldc-id' found.
 *
 * Return Code:
 *	0	- Success.
 *	ENOENT	- Expected node or property did not exist.
 *	ENXIO	- Unexpected error communicating with MD framework
 */
static int
vdc_get_ldc_id(dev_info_t *dip, uint64_t *ldc_id)
{
	int		status = ENOENT;
	char		*node_name = NULL;
	md_t		*mdp = NULL;
	int		num_nodes;
	int		num_vdevs;
	int		num_chans;
	mde_cookie_t	rootnode;
	mde_cookie_t	*listp = NULL;
	mde_cookie_t	*chanp = NULL;
	boolean_t	found_inst = B_FALSE;
	int		listsz;
	int		idx;
	uint64_t	md_inst;
	int		obp_inst;
	int		instance = ddi_get_instance(dip);

	ASSERT(ldc_id != NULL);
	*ldc_id = 0;

	/*
	 * Get the OBP instance number for comparison with the MD instance
	 *
	 * The "cfg-handle" property of a vdc node in an MD contains the MD's
	 * notion of "instance", or unique identifier, for that node; OBP
	 * stores the value of the "cfg-handle" MD property as the value of
	 * the "reg" property on the node in the device tree it builds from
	 * the MD and passes to Solaris.  Thus, we look up the devinfo node's
	 * "reg" property value to uniquely identify this device instance.
	 * If the "reg" property cannot be found, the device tree state is
	 * presumably so broken that there is no point in continuing.
	 */
	if (!ddi_prop_exists(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS, OBP_REG)) {
		cmn_err(CE_WARN, "'%s' property does not exist", OBP_REG);
		return (ENOENT);
	}
	obp_inst = ddi_prop_get_int(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
			OBP_REG, -1);
	DMSG(1, "[%d] OBP inst=%d\n", instance, obp_inst);

	/*
	 * We now walk the MD nodes and if an instance of a vdc node matches
	 * the instance got from OBP we get the ldc-id property.
	 */
	if ((mdp = md_get_handle()) == NULL) {
		cmn_err(CE_WARN, "unable to init machine description");
		return (ENXIO);
	}

	num_nodes = md_node_count(mdp);
	ASSERT(num_nodes > 0);

	listsz = num_nodes * sizeof (mde_cookie_t);

	/* allocate memory for nodes */
	listp = kmem_zalloc(listsz, KM_SLEEP);
	chanp = kmem_zalloc(listsz, KM_SLEEP);

	rootnode = md_root_node(mdp);
	ASSERT(rootnode != MDE_INVAL_ELEM_COOKIE);

	/*
	 * Search for all the virtual devices, we will then check to see which
	 * ones are disk nodes.
	 */
	num_vdevs = md_scan_dag(mdp, rootnode,
			md_find_name(mdp, VDC_MD_VDEV_NAME),
			md_find_name(mdp, "fwd"), listp);

	if (num_vdevs <= 0) {
		cmn_err(CE_NOTE, "No '%s' node found", VDC_MD_VDEV_NAME);
		status = ENOENT;
		goto done;
	}

	DMSG(1, "[%d] num_vdevs=%d\n", instance, num_vdevs);
	for (idx = 0; idx < num_vdevs; idx++) {
		status = md_get_prop_str(mdp, listp[idx], "name", &node_name);
		if ((status != 0) || (node_name == NULL)) {
			cmn_err(CE_NOTE, "Unable to get name of node type '%s'"
					": err %d", VDC_MD_VDEV_NAME, status);
			continue;
		}

		DMSG(1, "[%d] Found node '%s'\n", instance, node_name);
		if (strcmp(VDC_MD_DISK_NAME, node_name) == 0) {
			status = md_get_prop_val(mdp, listp[idx],
					VDC_MD_CFG_HDL, &md_inst);
			DMSG(1, "[%d] vdc inst in MD=%lx\n", instance, md_inst);
			if ((status == 0) && (md_inst == obp_inst)) {
				found_inst = B_TRUE;
				break;
			}
		}
	}

	if (!found_inst) {
		cmn_err(CE_NOTE, "Unable to find correct '%s' node",
				VDC_MD_DISK_NAME);
		status = ENOENT;
		goto done;
	}
	DMSG(0, "[%d] MD inst=%lx\n", instance, md_inst);

	/* get the channels for this node */
	num_chans = md_scan_dag(mdp, listp[idx],
			md_find_name(mdp, VDC_MD_CHAN_NAME),
			md_find_name(mdp, "fwd"), chanp);

	/* expecting at least one channel */
	if (num_chans <= 0) {
		cmn_err(CE_NOTE, "No '%s' node for '%s' port",
				VDC_MD_CHAN_NAME, VDC_MD_VDEV_NAME);
		status = ENOENT;
		goto done;

	} else if (num_chans != 1) {
		DMSG(0, "[%d] Expected 1 '%s' node for '%s' port, found %d\n",
			instance, VDC_MD_CHAN_NAME, VDC_MD_VDEV_NAME,
			num_chans);
	}

	/*
	 * We use the first channel found (index 0), irrespective of how
	 * many are there in total.
	 */
	if (md_get_prop_val(mdp, chanp[0], VDC_ID_PROP, ldc_id) != 0) {
		cmn_err(CE_NOTE, "Channel '%s' property not found",
				VDC_ID_PROP);
		status = ENOENT;
	}

	DMSG(0, "[%d] LDC id is 0x%lx\n", instance, *ldc_id);

done:
	if (chanp)
		kmem_free(chanp, listsz);
	if (listp)
		kmem_free(listp, listsz);

	(void) md_fini_handle(mdp);

	return (status);
}

static int
vdc_do_ldc_up(vdc_t *vdc)
{
	int	status;

	DMSG(0, "[%d] Bringing up channel %lx\n", vdc->instance, vdc->ldc_id);

	if ((status = ldc_up(vdc->ldc_handle)) != 0) {
		switch (status) {
		case ECONNREFUSED:	/* listener not ready at other end */
			DMSG(0, "[%d] ldc_up(%lx,...) return %d\n",
					vdc->instance, vdc->ldc_id, status);
			status = 0;
			break;
		default:
			cmn_err(CE_NOTE, "[%d] Failed to bring up LDC: "
					"channel=%ld, err=%d",
					vdc->instance, vdc->ldc_id, status);
		}
	}

	return (status);
}


/*
 * vdc_is_able_to_tx_data()
 *
 * Description:
 *	This function checks if we are able to send data to the
 *	vDisk server (vds). The LDC connection needs to be up and
 *	vdc & vds need to have completed the handshake negotiation.
 *
 * Parameters:
 *	vdc 		- soft state pointer
 *	flag		- flag to indicate if we can block or not
 *			  [ If O_NONBLOCK or O_NDELAY (which are defined in
 *			    open(2)) are set then do not block)
 *
 * Return Values
 *	B_TRUE		- can talk to vds
 *	B_FALSE		- unable to talk to vds
 */
static boolean_t
vdc_is_able_to_tx_data(vdc_t *vdc, int flag)
{
	vd_state_t	state;
	uint32_t	ldc_state;
	uint_t		retries = 0;
	int		rv = -1;

	ASSERT(vdc != NULL);

	mutex_enter(&vdc->lock);
	state = vdc->state;
	ldc_state = vdc->ldc_state;
	mutex_exit(&vdc->lock);

	if ((state == VD_STATE_DATA) && (ldc_state == LDC_UP))
		return (B_TRUE);

	if ((flag & O_NONBLOCK) || (flag & O_NDELAY)) {
		DMSG(0, "[%d] Not ready to tx - state %d LDC state %d\n",
			vdc->instance, state, ldc_state);
		return (B_FALSE);
	}

	/*
	 * We want to check and see if any negotiations triggered earlier
	 * have succeeded. We are prepared to wait a little while in case
	 * they are still in progress.
	 */
	mutex_enter(&vdc->lock);
	while ((vdc->ldc_state != LDC_UP) || (vdc->state != VD_STATE_DATA)) {
		DMSG(0, "[%d] Waiting for connection. (state %d : LDC %d)\n",
			vdc->instance, vdc->state, vdc->ldc_state);

		rv = cv_timedwait(&vdc->cv, &vdc->lock,
			VD_GET_TIMEOUT_HZ(vdc_hz_timeout, retries));

		/*
		 * An rv of -1 indicates that we timed out without the LDC
		 * state changing so it looks like the other side (vdc) is
		 * not yet ready/responding.
		 *
		 * Any other value of rv indicates that the LDC triggered an
		 * interrupt so we just loop again, check the handshake state
		 * and keep waiting if necessary.
		 */
		if (rv == -1) {
			if (retries >= vdc_retries) {
				DMSG(0, "[%d] handshake wait timed out\n",
						vdc->instance);
				mutex_exit(&vdc->lock);
				return (B_FALSE);
			} else {
				DMSG(1, "[%d] Handshake retry #%d timed out\n",
					vdc->instance, retries);
				retries++;
			}
		}
	}

	ASSERT(vdc->ldc_state == LDC_UP);
	ASSERT(vdc->state == VD_STATE_DATA);

	mutex_exit(&vdc->lock);

	return (B_TRUE);
}


/*
 * Function:
 *	vdc_terminate_ldc()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	None
 */
static void
vdc_terminate_ldc(vdc_t *vdc)
{
	int	instance = ddi_get_instance(vdc->dip);

	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));

	DMSG(0, "[%d] initialized=%x\n", instance, vdc->initialized);

	if (vdc->initialized & VDC_LDC_OPEN) {
		DMSG(0, "[%d] ldc_close()\n", instance);
		(void) ldc_close(vdc->ldc_handle);
	}
	if (vdc->initialized & VDC_LDC_CB) {
		DMSG(0, "[%d] ldc_unreg_callback()\n", instance);
		(void) ldc_unreg_callback(vdc->ldc_handle);
	}
	if (vdc->initialized & VDC_LDC) {
		DMSG(0, "[%d] ldc_fini()\n", instance);
		(void) ldc_fini(vdc->ldc_handle);
		vdc->ldc_handle = NULL;
	}

	vdc->initialized &= ~(VDC_LDC | VDC_LDC_CB | VDC_LDC_OPEN);
}

/*
 * Function:
 *	vdc_reset_connection()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *	reset_ldc - Flag whether or not to reset the LDC connection also.
 *
 * Return Code:
 *	None
 */
static void
vdc_reset_connection(vdc_t *vdc, boolean_t reset_ldc)
{
	int	status;

	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));

	cmn_err(CE_CONT, "?[%d] Resetting connection to vDisk server\n",
			vdc->instance);

	vdc->state = VD_STATE_INIT;

	if (reset_ldc) {
		status = ldc_down(vdc->ldc_handle);
		DMSG(0, "[%d]  ldc_down() = %d\n", vdc->instance, status);
	}

	vdc->initialized &= ~VDC_HANDSHAKE;
	DMSG(0, "[%d] initialized=%x\n", vdc->instance, vdc->initialized);
}

/* -------------------------------------------------------------------------- */

/*
 * Descriptor Ring helper routines
 */

/*
 * Function:
 *	vdc_init_descriptor_ring()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_init_descriptor_ring(vdc_t *vdc)
{
	vd_dring_entry_t	*dep = NULL;	/* DRing Entry pointer */
	int	status = 0;
	int	i;

	DMSG(0, "[%d] initialized=%x\n", vdc->instance, vdc->initialized);

	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));
	ASSERT(vdc->ldc_handle != NULL);

	/* ensure we have enough room to store max sized block */
	ASSERT(maxphys <= VD_MAX_BLOCK_SIZE);

	if ((vdc->initialized & VDC_DRING_INIT) == 0) {
		DMSG(0, "[%d] ldc_mem_dring_create\n", vdc->instance);
		/*
		 * Calculate the maximum block size we can transmit using one
		 * Descriptor Ring entry from the attributes returned by the
		 * vDisk server. This is subject to a minimum of 'maxphys'
		 * as we do not have the capability to split requests over
		 * multiple DRing entries.
		 */
		if ((vdc->max_xfer_sz * vdc->block_size) < maxphys) {
			DMSG(0, "[%d] using minimum DRing size\n",
					vdc->instance);
			vdc->dring_max_cookies = maxphys / PAGESIZE;
		} else {
			vdc->dring_max_cookies =
				(vdc->max_xfer_sz * vdc->block_size) / PAGESIZE;
		}
		vdc->dring_entry_size = (sizeof (vd_dring_entry_t) +
				(sizeof (ldc_mem_cookie_t) *
					(vdc->dring_max_cookies - 1)));
		vdc->dring_len = VD_DRING_LEN;

		status = ldc_mem_dring_create(vdc->dring_len,
				vdc->dring_entry_size, &vdc->ldc_dring_hdl);
		if ((vdc->ldc_dring_hdl == NULL) || (status != 0)) {
			cmn_err(CE_NOTE, "[%d] Descriptor ring creation failed",
					vdc->instance);
			return (status);
		}
		vdc->initialized |= VDC_DRING_INIT;
	}

	if ((vdc->initialized & VDC_DRING_BOUND) == 0) {
		DMSG(0, "[%d] ldc_mem_dring_bind\n", vdc->instance);
		vdc->dring_cookie =
			kmem_zalloc(sizeof (ldc_mem_cookie_t), KM_SLEEP);

		status = ldc_mem_dring_bind(vdc->ldc_handle, vdc->ldc_dring_hdl,
				LDC_SHADOW_MAP|LDC_DIRECT_MAP, LDC_MEM_RW,
				&vdc->dring_cookie[0],
				&vdc->dring_cookie_count);
		if (status != 0) {
			cmn_err(CE_NOTE, "[%d] Failed to bind descriptor ring "
				"(%lx) to channel (%lx)\n", vdc->instance,
				vdc->ldc_dring_hdl, vdc->ldc_handle);
			return (status);
		}
		ASSERT(vdc->dring_cookie_count == 1);
		vdc->initialized |= VDC_DRING_BOUND;
	}

	status = ldc_mem_dring_info(vdc->ldc_dring_hdl, &vdc->dring_mem_info);
	if (status != 0) {
		DMSG(0, "[%d] Failed to get info for descriptor ring (%lx)\n",
			vdc->instance, vdc->ldc_dring_hdl);
		return (status);
	}

	if ((vdc->initialized & VDC_DRING_LOCAL) == 0) {
		DMSG(0, "[%d] local dring\n", vdc->instance);

		/* Allocate the local copy of this dring */
		vdc->local_dring =
			kmem_zalloc(vdc->dring_len * sizeof (vdc_local_desc_t),
						KM_SLEEP);
		vdc->initialized |= VDC_DRING_LOCAL;
	}

	/*
	 * Mark all DRing entries as free and initialize the private
	 * descriptor's memory handles. If any entry is initialized,
	 * we need to free it later so we set the bit in 'initialized'
	 * at the start.
	 */
	vdc->initialized |= VDC_DRING_ENTRY;
	for (i = 0; i < vdc->dring_len; i++) {
		dep = VDC_GET_DRING_ENTRY_PTR(vdc, i);
		dep->hdr.dstate = VIO_DESC_FREE;

		status = ldc_mem_alloc_handle(vdc->ldc_handle,
				&vdc->local_dring[i].desc_mhdl);
		if (status != 0) {
			cmn_err(CE_NOTE, "![%d] Failed to alloc mem handle for"
					" descriptor %d", vdc->instance, i);
			return (status);
		}
		vdc->local_dring[i].flags = VIO_DESC_FREE;
		vdc->local_dring[i].dep = dep;

		mutex_init(&vdc->local_dring[i].lock, NULL, MUTEX_DRIVER, NULL);
		cv_init(&vdc->local_dring[i].cv, NULL, CV_DRIVER, NULL);
	}

	/*
	 * We init the index of the last DRing entry used. Since the code to
	 * get the next available entry increments it before selecting one,
	 * we set it to the last DRing entry so that it wraps around to zero
	 * for the 1st entry to be used.
	 */
	vdc->dring_curr_idx = vdc->dring_len - 1;

	vdc->dring_notify_server = B_TRUE;

	return (status);
}

/*
 * Function:
 *	vdc_destroy_descriptor_ring()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	None
 */
static void
vdc_destroy_descriptor_ring(vdc_t *vdc)
{
	vdc_local_desc_t	*ldep = NULL;	/* Local Dring Entry Pointer */
	ldc_mem_handle_t	mhdl = NULL;
	int			status = -1;
	int			i;	/* loop */

	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));
	ASSERT(vdc->state == VD_STATE_INIT);

	DMSG(0, "[%d] Entered\n", vdc->instance);

	if (vdc->initialized & VDC_DRING_ENTRY) {
		DMSG(0, "[%d] Removing Local DRing entries\n", vdc->instance);
		for (i = 0; i < vdc->dring_len; i++) {
			ldep = &vdc->local_dring[i];
			mhdl = ldep->desc_mhdl;

			if (mhdl == NULL)
				continue;

			(void) ldc_mem_free_handle(mhdl);
			mutex_destroy(&ldep->lock);
			cv_destroy(&ldep->cv);
		}
		vdc->initialized &= ~VDC_DRING_ENTRY;
	}

	if (vdc->initialized & VDC_DRING_LOCAL) {
		DMSG(0, "[%d] Freeing Local DRing\n", vdc->instance);
		kmem_free(vdc->local_dring,
				vdc->dring_len * sizeof (vdc_local_desc_t));
		vdc->initialized &= ~VDC_DRING_LOCAL;
	}

	if (vdc->initialized & VDC_DRING_BOUND) {
		DMSG(0, "[%d] Unbinding DRing\n", vdc->instance);
		status = ldc_mem_dring_unbind(vdc->ldc_dring_hdl);
		if (status == 0) {
			vdc->initialized &= ~VDC_DRING_BOUND;
		} else {
			cmn_err(CE_NOTE, "[%d] Error %d unbinding DRing %lx",
				vdc->instance, status, vdc->ldc_dring_hdl);
		}
	}

	if (vdc->initialized & VDC_DRING_INIT) {
		DMSG(0, "[%d] Destroying DRing\n", vdc->instance);
		status = ldc_mem_dring_destroy(vdc->ldc_dring_hdl);
		if (status == 0) {
			vdc->ldc_dring_hdl = NULL;
			bzero(&vdc->dring_mem_info, sizeof (ldc_mem_info_t));
			vdc->initialized &= ~VDC_DRING_INIT;
		} else {
			cmn_err(CE_NOTE, "[%d] Error %d destroying DRing (%lx)",
				vdc->instance, status, vdc->ldc_dring_hdl);
		}
	}
}

/*
 * vdc_get_next_dring_entry_idx()
 *
 * Description:
 *	This function gets the index of the next Descriptor Ring entry available
 *	If the ring is full, it will back off and wait for the next entry to be
 *	freed (the ACK handler will signal).
 *
 * Return Value:
 *	0 <= rv < vdc->dring_len		Next available slot
 *	-1 				DRing is full
 */
static int
vdc_get_next_dring_entry_idx(vdc_t *vdc, uint_t num_slots_needed)
{
	_NOTE(ARGUNUSED(num_slots_needed))

	vd_dring_entry_t	*dep = NULL;	/* DRing Entry Pointer */
	vdc_local_desc_t	*ldep = NULL;	/* Local DRing Entry Pointer */
	int			idx = -1;

	ASSERT(vdc != NULL);
	ASSERT(vdc->dring_len == vdc->dring_len);
	ASSERT(vdc->dring_curr_idx >= 0);
	ASSERT(vdc->dring_curr_idx < vdc->dring_len);
	ASSERT(mutex_owned(&vdc->dring_lock));

	/* pick the next descriptor after the last one used */
	idx = (vdc->dring_curr_idx + 1) % vdc->dring_len;
	ldep = &vdc->local_dring[idx];
	ASSERT(ldep != NULL);
	dep = ldep->dep;
	ASSERT(dep != NULL);

	mutex_enter(&ldep->lock);
	if (dep->hdr.dstate == VIO_DESC_FREE) {
		vdc->dring_curr_idx = idx;
	} else {
		DTRACE_PROBE(full);
		(void) cv_timedwait(&ldep->cv, &ldep->lock,
					VD_GET_TIMEOUT_HZ(vdc_hz_timeout, 1));
		if (dep->hdr.dstate == VIO_DESC_FREE) {
			vdc->dring_curr_idx = idx;
		} else {
			DMSG(0, "[%d] Entry %d unavailable still in state %d\n",
					vdc->instance, idx, dep->hdr.dstate);
			idx = -1; /* indicate that the ring is full */
		}
	}
	mutex_exit(&ldep->lock);

	return (idx);
}

/*
 * Function:
 *	vdc_populate_descriptor
 *
 * Description:
 *	This routine writes the data to be transmitted to vds into the
 *	descriptor, notifies vds that the ring has been updated and
 *	then waits for the request to be processed.
 *
 * Arguments:
 *	vdc	- the soft state pointer
 *	addr	- address of structure to be written. In the case of block
 *		  reads and writes this structure will be a buf_t and the
 *		  address of the data to be written will be in the b_un.b_addr
 *		  field. Otherwise the value of addr will be the address
 *		  to be written.
 *	nbytes	- number of bytes to read/write
 *	operation - operation we want vds to perform (VD_OP_XXX)
 *	arg	- parameter to be sent to server (depends on VD_OP_XXX type)
 *			. mode for ioctl(9e)
 *			. LP64 diskaddr_t (block I/O)
 *	slice	- the disk slice this request is for
 *
 * Return Codes:
 *	0
 *	EAGAIN
 *		EFAULT
 *		ENXIO
 *		EIO
 */
static int
vdc_populate_descriptor(vdc_t *vdc, caddr_t addr, size_t nbytes, int operation,
				uint64_t arg, uint64_t slice)
{
	vdc_local_desc_t *local_dep = NULL;	/* Local Dring Entry Pointer */
	vd_dring_entry_t *dep = NULL;		/* Dring Entry Pointer */
	int			idx = 0;	/* Index of DRing entry used */
	vio_dring_msg_t		dmsg;
	size_t			msglen = sizeof (dmsg);
	int			retries = 0;
	int			rv;

	ASSERT(vdc != NULL);
	ASSERT(slice < V_NUMPAR);

	/*
	 * Get next available DRing entry.
	 */
	mutex_enter(&vdc->dring_lock);
	idx = vdc_get_next_dring_entry_idx(vdc, 1);
	if (idx == -1) {
		mutex_exit(&vdc->dring_lock);
		DMSG(0, "[%d] no descriptor ring entry avail, last seq=%ld\n",
				vdc->instance, vdc->seq_num - 1);

		/*
		 * Since strategy should not block we don't wait for the DRing
		 * to empty and instead return
		 */
		return (EAGAIN);
	}

	ASSERT(idx < vdc->dring_len);
	local_dep = &vdc->local_dring[idx];
	dep = local_dep->dep;
	ASSERT(dep != NULL);

	/*
	 * We now get the lock for this descriptor before dropping the overall
	 * DRing lock. This prevents a race condition where another vdc thread
	 * could grab the descriptor we selected.
	 */
	ASSERT(MUTEX_NOT_HELD(&local_dep->lock));
	mutex_enter(&local_dep->lock);
	mutex_exit(&vdc->dring_lock);

	switch (operation) {
	case VD_OP_BREAD:
	case VD_OP_BWRITE:
		local_dep->buf = (struct buf *)addr;
		local_dep->addr = local_dep->buf->b_un.b_addr;
		DMSG(2, "[%d] buf=%p, block=%lx, nbytes=%lu\n",
				vdc->instance, (void *)addr, arg, nbytes);
		dep->payload.addr = (diskaddr_t)arg;
		rv = vdc_populate_mem_hdl(vdc, idx, local_dep->addr,
						nbytes, operation);
		break;

	case VD_OP_GET_WCE:
	case VD_OP_SET_WCE:
	case VD_OP_GET_VTOC:
	case VD_OP_SET_VTOC:
	case VD_OP_GET_DISKGEOM:
	case VD_OP_SET_DISKGEOM:
	case VD_OP_SCSICMD:
	case VD_OP_GET_DEVID:
	case VD_OP_GET_EFI:
	case VD_OP_SET_EFI:
		local_dep->addr = addr;
		if (nbytes > 0) {
			rv = vdc_populate_mem_hdl(vdc, idx, addr, nbytes,
							operation);
		}
		break;

	case VD_OP_FLUSH:
		rv = 0;		/* nothing to bind */
		break;

	default:
		cmn_err(CE_CONT, "?[%d] Unsupported vDisk operation [%d]\n",
				vdc->instance, operation);
		rv = EINVAL;
	}

	if (rv != 0) {
		mutex_exit(&local_dep->lock);
		return (rv);
	}

	/*
	 * fill in the data details into the DRing
	 */
	dep->payload.req_id = VDC_GET_NEXT_REQ_ID(vdc);
	dep->payload.operation = operation;
	dep->payload.nbytes = nbytes;
	dep->payload.status = -1;	/* vds will set valid value */
	dep->payload.slice = slice;
	dep->hdr.dstate = VIO_DESC_READY;
	dep->hdr.ack = 1;		/* request an ACK for every message */

	local_dep->flags = VIO_DESC_READY;

	/*
	 * Send a msg with the DRing details to vds
	 */
	mutex_enter(&vdc->lock);
	VIO_INIT_DRING_DATA_TAG(dmsg);
	VDC_INIT_DRING_DATA_MSG_IDS(dmsg, vdc);
	dmsg.dring_ident = vdc->dring_ident;
	dmsg.start_idx = idx;
	dmsg.end_idx = idx;

	DTRACE_IO2(send, vio_dring_msg_t *, &dmsg, vdc_t *, vdc);

	DMSG(2, "[%d] ident=0x%lx, st=%u, end=%u, seq=%ld req=%ld dep=%p\n",
			vdc->instance, vdc->dring_ident,
			dmsg.start_idx, dmsg.end_idx,
			dmsg.seq_num, dep->payload.req_id, (void *)dep);

	rv = vdc_send(vdc, (caddr_t)&dmsg, &msglen);
	DMSG(1, "[%d] send via LDC: rv=%d\n", vdc->instance, rv);
	if (rv != 0) {
		DMSG(0, "[%d] err (%d) sending DRing data msg via LDC",
				vdc->instance, rv);

		/* Clear the DRing entry */
		rv = vdc_depopulate_descriptor(vdc, idx);

		mutex_exit(&vdc->lock);
		mutex_exit(&local_dep->lock);

		return (rv ? rv : EAGAIN);
	}

	/*
	 * If the message was successfully sent, we increment the sequence
	 * number to be used by the next message
	 */
	vdc->seq_num++;
	mutex_exit(&vdc->lock);

	/*
	 * When a guest is panicking, the completion of requests needs to be
	 * handled differently because interrupts are disabled and vdc
	 * will not get messages. We have to poll for the messages instead.
	 */
	if (ddi_in_panic()) {
		int start = 0;
		retries = 0;
		for (;;) {
			msglen = sizeof (dmsg);
			rv = ldc_read(vdc->ldc_handle, (caddr_t)&dmsg,
					&msglen);
			if (rv) {
				rv = EINVAL;
				break;
			}

			/*
			 * if there are no packets wait and check again
			 */
			if ((rv == 0) && (msglen == 0)) {
				if (retries++ > vdc_dump_retries) {
					DMSG(0, "[%d] Stopping wait, idx %d\n",
							vdc->instance, idx);
					rv = EAGAIN;
					break;
				}

				DMSG(1, "Waiting for next packet @ %d\n", idx);
				drv_usecwait(vdc_usec_timeout_dump);
				continue;
			}

			/*
			 * Ignore all messages that are not ACKs/NACKs to
			 * DRing requests.
			 */
			if ((dmsg.tag.vio_msgtype != VIO_TYPE_DATA) ||
			    (dmsg.tag.vio_subtype_env != VIO_DRING_DATA)) {
				DMSG(0, "discard pkt: type=%d sub=%d env=%d\n",
					dmsg.tag.vio_msgtype,
					dmsg.tag.vio_subtype,
					dmsg.tag.vio_subtype_env);
				continue;
			}

			/*
			 * set the appropriate return value for the
			 * current request.
			 */
			switch (dmsg.tag.vio_subtype) {
			case VIO_SUBTYPE_ACK:
				rv = 0;
				break;
			case VIO_SUBTYPE_NACK:
				rv = EAGAIN;
				break;
			default:
				continue;
			}

			start = dmsg.start_idx;
			if (start >= vdc->dring_len) {
				DMSG(0, "[%d] Bogus ack data : start %d\n",
					vdc->instance, start);
				continue;
			}

			dep = VDC_GET_DRING_ENTRY_PTR(vdc, start);

			DMSG(1, "[%d] Dumping start=%d idx=%d state=%d\n",
				vdc->instance, start, idx, dep->hdr.dstate);

			if (dep->hdr.dstate != VIO_DESC_DONE) {
				DMSG(0, "[%d] Entry @ %d - state !DONE %d\n",
					vdc->instance, start, dep->hdr.dstate);
				continue;
			}

			(void) vdc_depopulate_descriptor(vdc, start);

			/*
			 * We want to process all Dring entries up to
			 * the current one so that we can return an
			 * error with the correct request.
			 */
			if (idx > start) {
				DMSG(0, "[%d] Looping: start %d, idx %d\n",
						vdc->instance, idx, start);
				continue;
			}

			/* exit - all outstanding requests are completed */
			break;
		}

		mutex_exit(&local_dep->lock);

		return (rv);
	}

	/*
	 * In the case of calls from strategy and dump (in the non-panic case),
	 * instead of waiting for a response from the vDisk server return now.
	 * They will be processed asynchronously and the vdc ACK handling code
	 * will trigger the biodone(9F)
	 */
	if ((operation == VD_OP_BREAD) || (operation == VD_OP_BWRITE)) {
		mutex_exit(&local_dep->lock);
		return (rv);
	}

	/*
	 * In the case of synchronous calls we watch the DRing entries we
	 * modified and await the response from vds.
	 */
	rv = vdc_wait_for_descriptor_update(vdc, idx, dmsg);
	if (rv == ETIMEDOUT) {
		/* debug info when dumping state on vds side */
		dep->payload.status = ECANCELED;
	}

	rv = vdc_depopulate_descriptor(vdc, idx);
	DMSG(0, "[%d] Exiting: status=%d\n", vdc->instance, rv);

	mutex_exit(&local_dep->lock);

	return (rv);
}

/*
 * Function:
 *	vdc_wait_for_descriptor_update()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *	idx	- Index of the Descriptor Ring entry being modified
 *	dmsg	- LDC message sent by vDisk server
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_wait_for_descriptor_update(vdc_t *vdc, uint_t idx, vio_dring_msg_t dmsg)
{
	vd_dring_entry_t *dep = NULL;		/* Dring Entry Pointer */
	vdc_local_desc_t *local_dep = NULL;	/* Local Dring Entry Pointer */
	size_t	msglen = sizeof (dmsg);
	int	retries = 0;
	int	status = 0;
	int	rv = 0;

	ASSERT(vdc != NULL);
	ASSERT(idx < vdc->dring_len);
	local_dep = &vdc->local_dring[idx];
	ASSERT(local_dep != NULL);
	ASSERT(MUTEX_HELD(&local_dep->lock));
	dep = local_dep->dep;
	ASSERT(dep != NULL);

	while (dep->hdr.dstate != VIO_DESC_DONE) {
		rv = cv_timedwait(&local_dep->cv, &local_dep->lock,
			VD_GET_TIMEOUT_HZ(vdc_hz_timeout, retries));
		if (rv == -1) {
			/*
			 * If they persist in ignoring us we'll storm off in a
			 * huff and return ETIMEDOUT to the upper layers.
			 */
			if (retries >= vdc_retries) {
				DMSG(0, "[%d] Finished waiting on entry %d\n",
					vdc->instance, idx);
				status = ETIMEDOUT;
				break;
			} else {
				retries++;
				DMSG(0, "[%d] Timeout #%d on entry %d "
				    "[seq %lu][req %lu]\n", vdc->instance,
				    retries, idx, dmsg.seq_num,
				    dep->payload.req_id);
			}

			if (dep->hdr.dstate & VIO_DESC_ACCEPTED) {
				DMSG(0, "[%d] entry %d ACCEPTED [seq %lu]"
				    "[req %lu] but not ACK'ed by vds yet\n",
				    vdc->instance, idx, dmsg.seq_num,
				    dep->payload.req_id);
				continue;
			}

			/*
			 * we resend the message as it may have been dropped
			 * and have never made it to the other side (vds).
			 * (We reuse the original message but update seq ID)
			 */
			mutex_enter(&vdc->lock);
			VDC_INIT_DRING_DATA_MSG_IDS(dmsg, vdc);
			retries = 0;
			status = vdc_send(vdc, (caddr_t)&dmsg, &msglen);
			if (status != 0) {
				mutex_exit(&vdc->lock);
				cmn_err(CE_NOTE, "[%d] Error (%d) while sending"
						" after timeout",
						vdc->instance, status);
				status = ETIMEDOUT;
				break;
			}
			/*
			 * If the message was successfully sent, we increment
			 * the sequence number to be used by the next message.
			 */
			vdc->seq_num++;
			mutex_exit(&vdc->lock);
		}
	}

	return (status);
}


/*
 * Function:
 *	vdc_depopulate_descriptor()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *	idx	- Index of the Descriptor Ring entry being modified
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_depopulate_descriptor(vdc_t *vdc, uint_t idx)
{
	vd_dring_entry_t *dep = NULL;		/* Dring Entry Pointer */
	vdc_local_desc_t *ldep = NULL;		/* Local Dring Entry Pointer */
	int		status = ENXIO;
	int		operation;
	int		rv = 0;

	ASSERT(vdc != NULL);
	ASSERT(idx < vdc->dring_len);
	ldep = &vdc->local_dring[idx];
	ASSERT(ldep != NULL);
	ASSERT(MUTEX_HELD(&ldep->lock));
	dep = ldep->dep;
	ASSERT(dep != NULL);
	ASSERT((dep->hdr.dstate == VIO_DESC_DONE) ||
			(dep->payload.status == ECANCELED));

	VDC_MARK_DRING_ENTRY_FREE(vdc, idx);
	VIO_SET_DESC_STATE(ldep->flags, VIO_DESC_FREE);
	status = dep->payload.status;
	operation = dep->payload.operation;

	/* the DKIO FLUSH operation never bind handles so we can return now */
	if (operation == VD_OP_FLUSH)
		return (status);

	/*
	 * If the upper layer passed in a misaligned address we copied the
	 * data into an aligned buffer before sending it to LDC - we now
	 * copy it back to the original buffer.
	 */
	if (ldep->align_addr) {
		ASSERT(ldep->addr != NULL);
		ASSERT(dep->payload.nbytes > 0);

		bcopy(ldep->align_addr, ldep->addr, dep->payload.nbytes);
		kmem_free(ldep->align_addr,
			sizeof (caddr_t) * P2ROUNDUP(dep->payload.nbytes, 8));
		ldep->align_addr = NULL;
	}

	rv = ldc_mem_unbind_handle(ldep->desc_mhdl);
	if (rv != 0) {
		cmn_err(CE_CONT, "?[%d] unbind mhdl 0x%lx @ idx %d failed (%d)",
				vdc->instance, ldep->desc_mhdl, idx, rv);
		/*
		 * The error returned by the vDisk server is more informative
		 * and thus has a higher priority but if it isn't set we ensure
		 * that this function returns an error.
		 */
		if (status == 0)
			status = EINVAL;
	}

	return (status);
}

/*
 * Function:
 *	vdc_populate_mem_hdl()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *	idx	- Index of the Descriptor Ring entry being modified
 *	addr	- virtual address being mapped in
 *	nybtes	- number of bytes in 'addr'
 *	operation - the vDisk operation being performed (VD_OP_xxx)
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_populate_mem_hdl(vdc_t *vdc, uint_t idx, caddr_t addr, size_t nbytes,
			int operation)
{
	vd_dring_entry_t	*dep = NULL;
	vdc_local_desc_t	*ldep = NULL;
	ldc_mem_handle_t	mhdl;
	caddr_t			vaddr;
	uint8_t			perm = LDC_MEM_RW;
	uint8_t			maptype;
	int			rv = 0;
	int			i;

	ASSERT(vdc != NULL);
	ASSERT(idx < vdc->dring_len);

	dep = VDC_GET_DRING_ENTRY_PTR(vdc, idx);
	ldep = &vdc->local_dring[idx];
	mhdl = ldep->desc_mhdl;

	switch (operation) {
	case VD_OP_BREAD:
		perm = LDC_MEM_W;
		break;

	case VD_OP_BWRITE:
		perm = LDC_MEM_R;
		break;

	case VD_OP_GET_WCE:
	case VD_OP_SET_WCE:
	case VD_OP_GET_VTOC:
	case VD_OP_SET_VTOC:
	case VD_OP_GET_DISKGEOM:
	case VD_OP_SET_DISKGEOM:
	case VD_OP_SCSICMD:
	case VD_OP_GET_DEVID:
	case VD_OP_GET_EFI:
	case VD_OP_SET_EFI:
		perm = LDC_MEM_RW;
		break;

	default:
		ASSERT(0);	/* catch bad programming in vdc */
	}

	/*
	 * LDC expects any addresses passed in to be 8-byte aligned. We need
	 * to copy the contents of any misaligned buffers to a newly allocated
	 * buffer and bind it instead (and copy the the contents back to the
	 * original buffer passed in when depopulating the descriptor)
	 */
	vaddr = addr;
	if (((uint64_t)addr & 0x7) != 0) {
		ASSERT(ldep->align_addr == NULL);
		ldep->align_addr =
			kmem_zalloc(sizeof (caddr_t) * P2ROUNDUP(nbytes, 8),
					KM_SLEEP);
		DMSG(0, "[%d] Misaligned address %p reallocating "
		    "(buf=%p nb=%ld op=%d entry=%d)\n",
		    vdc->instance, (void *)addr, (void *)ldep->align_addr,
		    nbytes, operation, idx);
		bcopy(addr, ldep->align_addr, nbytes);
		vaddr = ldep->align_addr;
	}

	maptype = LDC_IO_MAP|LDC_SHADOW_MAP|LDC_DIRECT_MAP;
	rv = ldc_mem_bind_handle(mhdl, vaddr, P2ROUNDUP(nbytes, 8),
		maptype, perm, &dep->payload.cookie[0],
		&dep->payload.ncookies);
	DMSG(2, "[%d] bound mem handle; ncookies=%d\n",
			vdc->instance, dep->payload.ncookies);
	if (rv != 0) {
		cmn_err(CE_CONT, "?[%d] Failed to bind LDC memory handle "
		    "(mhdl=%p, buf=%p entry=%u err=%d)\n",
		    vdc->instance, (void *)mhdl, (void *)addr, idx, rv);
		if (ldep->align_addr) {
			kmem_free(ldep->align_addr,
				sizeof (caddr_t) * P2ROUNDUP(nbytes, 8));
			ldep->align_addr = NULL;
		}
		return (EAGAIN);
	}

	/*
	 * Get the other cookies (if any).
	 */
	for (i = 1; i < dep->payload.ncookies; i++) {
		rv = ldc_mem_nextcookie(mhdl, &dep->payload.cookie[i]);
		if (rv != 0) {
			(void) ldc_mem_unbind_handle(mhdl);
			cmn_err(CE_CONT, "?[%d] Failed to get next cookie "
					"(mhdl=%lx cnum=%d), err=%d",
					vdc->instance, mhdl, i, rv);
			if (ldep->align_addr) {
				kmem_free(ldep->align_addr,
					sizeof (caddr_t) * dep->payload.nbytes);
				ldep->align_addr = NULL;
			}
			return (EAGAIN);
		}
	}

	return (rv);
}

/*
 * Interrupt handlers for messages from LDC
 */

/*
 * Function:
 *	vdc_handle_cb()
 *
 * Description:
 *
 * Arguments:
 *	event	- Type of event (LDC_EVT_xxx) that triggered the callback
 *	arg	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	0	- Success
 */
static uint_t
vdc_handle_cb(uint64_t event, caddr_t arg)
{
	ldc_status_t	ldc_state;
	int		rv = 0;

	vdc_t	*vdc = (vdc_t *)(void *)arg;

	ASSERT(vdc != NULL);

	DMSG(1, "[%d] evt=%lx seqID=%ld\n", vdc->instance, event, vdc->seq_num);

	/*
	 * Depending on the type of event that triggered this callback,
	 * we modify the handhske state or read the data.
	 *
	 * NOTE: not done as a switch() as event could be triggered by
	 * a state change and a read request. Also the ordering	of the
	 * check for the event types is deliberate.
	 */
	if (event & LDC_EVT_UP) {
		DMSG(0, "[%d] Received LDC_EVT_UP\n", vdc->instance);

		/* get LDC state */
		rv = ldc_status(vdc->ldc_handle, &ldc_state);
		if (rv != 0) {
			cmn_err(CE_NOTE, "[%d] Couldn't get LDC status %d",
					vdc->instance, rv);
			mutex_enter(&vdc->lock);
			vdc_reset_connection(vdc, B_TRUE);
			mutex_exit(&vdc->lock);
			return (LDC_SUCCESS);
		}

		/*
		 * Reset the transaction sequence numbers when LDC comes up.
		 * We then kick off the handshake negotiation with the vDisk
		 * server.
		 */
		mutex_enter(&vdc->lock);
		vdc->seq_num = 1;
		vdc->seq_num_reply = 0;
		vdc->ldc_state = ldc_state;
		ASSERT(ldc_state == LDC_UP);
		mutex_exit(&vdc->lock);

		vdc_init_handshake_negotiation(vdc);

		ASSERT((event & (LDC_EVT_RESET | LDC_EVT_DOWN)) == 0);
	}

	if (event & LDC_EVT_READ) {
		/*
		 * Wake up the worker thread to process the message
		 */
		mutex_enter(&vdc->msg_proc_lock);
		vdc->msg_pending = B_TRUE;
		cv_signal(&vdc->msg_proc_cv);
		mutex_exit(&vdc->msg_proc_lock);

		ASSERT((event & (LDC_EVT_RESET | LDC_EVT_DOWN)) == 0);

		/* that's all we have to do - no need to handle DOWN/RESET */
		return (LDC_SUCCESS);
	}

	if (event & LDC_EVT_RESET) {
		DMSG(0, "[%d] Received LDC RESET event\n", vdc->instance);

		/* get LDC state */
		rv = ldc_status(vdc->ldc_handle, &ldc_state);
		if (rv != 0) {
			cmn_err(CE_NOTE, "[%d] Couldn't get LDC status %d",
					vdc->instance, rv);
			ldc_state = LDC_OPEN;
		}
		mutex_enter(&vdc->lock);
		vdc->ldc_state = ldc_state;
		vdc_reset_connection(vdc, B_TRUE);
		mutex_exit(&vdc->lock);

		vdc_init_handshake_negotiation(vdc);
	}

	if (event & LDC_EVT_DOWN) {
		DMSG(0, "[%d] Received LDC DOWN event\n", vdc->instance);

		/* get LDC state */
		rv = ldc_status(vdc->ldc_handle, &ldc_state);
		if (rv != 0) {
			cmn_err(CE_NOTE, "[%d] Couldn't get LDC status %d",
					vdc->instance, rv);
			ldc_state = LDC_OPEN;
		}
		mutex_enter(&vdc->lock);
		vdc->ldc_state = ldc_state;
		vdc_reset_connection(vdc, B_TRUE);
		mutex_exit(&vdc->lock);
	}

	if (event & ~(LDC_EVT_UP | LDC_EVT_RESET | LDC_EVT_DOWN | LDC_EVT_READ))
		cmn_err(CE_NOTE, "![%d] Unexpected LDC event (%lx) received",
				vdc->instance, event);

	return (LDC_SUCCESS);
}

/* -------------------------------------------------------------------------- */

/*
 * The following functions process the incoming messages from vds
 */


/*
 * Function:
 *	vdc_process_msg_thread()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	None
 */
static void
vdc_process_msg_thread(vdc_t *vdc)
{
	int		status = 0;
	boolean_t	q_has_pkts = B_FALSE;

	ASSERT(vdc != NULL);

	mutex_enter(&vdc->msg_proc_lock);
	DMSG(0, "[%d] Starting\n", vdc->instance);

	vdc->msg_proc_thr_state = VDC_THR_RUNNING;

	while (vdc->msg_proc_thr_state == VDC_THR_RUNNING) {

		DMSG(2, "[%d] Waiting\n", vdc->instance);
		while (!vdc->msg_pending)
			cv_wait(&vdc->msg_proc_cv, &vdc->msg_proc_lock);

		DMSG(2, "[%d] Message Received\n", vdc->instance);

		/* check if there is data */
		status = ldc_chkq(vdc->ldc_handle, &q_has_pkts);
		if ((status != 0) &&
		    (vdc->msg_proc_thr_state == VDC_THR_RUNNING)) {
			cmn_err(CE_NOTE, "[%d] Unable to communicate with vDisk"
					" server. Cannot check LDC queue: %d",
					vdc->instance, status);
			mutex_enter(&vdc->lock);
			vdc_reset_connection(vdc, B_TRUE);
			mutex_exit(&vdc->lock);
			vdc->msg_proc_thr_state = VDC_THR_STOP;
			continue;
		}

		if (q_has_pkts) {
			DMSG(2, "[%d] new pkt(s) available\n", vdc->instance);
			vdc_process_msg(vdc);
		}

		vdc->msg_pending = B_FALSE;
	}

	DMSG(0, "[%d] Message processing thread stopped\n", vdc->instance);
	vdc->msg_pending = B_FALSE;
	vdc->msg_proc_thr_state = VDC_THR_DONE;
	cv_signal(&vdc->msg_proc_cv);
	mutex_exit(&vdc->msg_proc_lock);
	thread_exit();
}


/*
 * Function:
 *	vdc_process_msg()
 *
 * Description:
 *	This function is called by the message processing thread each time it
 *	is triggered when LDC sends an interrupt to indicate that there are
 *	more packets on the queue. When it is called it will continue to loop
 *	and read the messages until there are no more left of the queue. If it
 *	encounters an invalid sized message it will drop it and check the next
 *	message.
 *
 * Arguments:
 *	arg	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	None.
 */
static void
vdc_process_msg(void *arg)
{
	vdc_t		*vdc = (vdc_t *)(void *)arg;
	vio_msg_t	vio_msg;
	size_t		nbytes = sizeof (vio_msg);
	int		status;

	ASSERT(vdc != NULL);

	mutex_enter(&vdc->lock);

	DMSG(1, "[%d]\n", vdc->instance);

	for (;;) {

		/* read all messages - until no more left */
		status = ldc_read(vdc->ldc_handle, (caddr_t)&vio_msg, &nbytes);

		if (status) {
			cmn_err(CE_CONT, "?[%d] Error %d reading LDC msg\n",
					vdc->instance, status);

			/* if status is ECONNRESET --- reset vdc state */
			if (status == EIO || status == ECONNRESET) {
				vdc_reset_connection(vdc, B_TRUE);
			}

			mutex_exit(&vdc->lock);
			return;
		}

		if ((nbytes > 0) && (nbytes < sizeof (vio_msg_tag_t))) {
			cmn_err(CE_CONT, "?[%d] Expect %lu bytes; recv'd %lu\n",
				vdc->instance, sizeof (vio_msg_tag_t), nbytes);
			mutex_exit(&vdc->lock);
			return;
		}

		if (nbytes == 0) {
			DMSG(3, "[%d] ldc_read() done..\n", vdc->instance);
			mutex_exit(&vdc->lock);
			return;
		}

		DMSG(2, "[%d] (%x/%x/%x)\n", vdc->instance,
		    vio_msg.tag.vio_msgtype,
		    vio_msg.tag.vio_subtype,
		    vio_msg.tag.vio_subtype_env);

		/*
		 * Verify the Session ID of the message
		 *
		 * Every message after the Version has been negotiated should
		 * have the correct session ID set.
		 */
		if ((vio_msg.tag.vio_sid != vdc->session_id) &&
		    (vio_msg.tag.vio_subtype_env != VIO_VER_INFO)) {
			cmn_err(CE_NOTE, "[%d] Invalid SID: received 0x%x, "
					"expected 0x%lx [seq num %lx @ %d]",
				vdc->instance, vio_msg.tag.vio_sid,
				vdc->session_id,
				((vio_dring_msg_t *)&vio_msg)->seq_num,
				((vio_dring_msg_t *)&vio_msg)->start_idx);
			vdc_reset_connection(vdc, B_TRUE);
			mutex_exit(&vdc->lock);
			return;
		}

		switch (vio_msg.tag.vio_msgtype) {
		case VIO_TYPE_CTRL:
			status = vdc_process_ctrl_msg(vdc, vio_msg);
			break;
		case VIO_TYPE_DATA:
			status = vdc_process_data_msg(vdc, vio_msg);
			break;
		case VIO_TYPE_ERR:
			status = vdc_process_err_msg(vdc, vio_msg);
			break;
		default:
			cmn_err(CE_NOTE, "[%d] Unknown VIO message type",
					vdc->instance);
			status = EINVAL;
			break;
		}

		if (status != 0) {
			DMSG(0, "[%d] Error (%d) occurred processing req %lu\n",
					vdc->instance, status,
					vdc->req_id_proc);
			vdc_reset_connection(vdc, B_TRUE);

			/* we need to drop the lock to trigger the handshake */
			mutex_exit(&vdc->lock);
			vdc_init_handshake_negotiation(vdc);
			mutex_enter(&vdc->lock);
		}
	}
	_NOTE(NOTREACHED)
}

/*
 * Function:
 *	vdc_process_ctrl_msg()
 *
 * Description:
 *	This function is called by the message processing thread each time
 *	an LDC message with a msgtype of VIO_TYPE_CTRL is received.
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *	msg	- the LDC message sent by vds
 *
 * Return Codes:
 *	0	- Success.
 *	EPROTO	- A message was received which shouldn't have happened according
 *		  to the protocol
 *	ENOTSUP	- An action which is allowed according to the protocol but which
 *		  isn't (or doesn't need to be) implemented yet.
 *	EINVAL	- An invalid value was returned as part of a message.
 */
static int
vdc_process_ctrl_msg(vdc_t *vdc, vio_msg_t msg)
{
	int			status = -1;

	ASSERT(msg.tag.vio_msgtype == VIO_TYPE_CTRL);
	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));

	/* Depending on which state we are in; process the message */
	switch (vdc->state) {
	case VD_STATE_INIT:
		status = vdc_handle_ver_msg(vdc, (vio_ver_msg_t *)&msg);
		break;

	case VD_STATE_VER:
		status = vdc_handle_attr_msg(vdc, (vd_attr_msg_t *)&msg);
		break;

	case VD_STATE_ATTR:
		status = vdc_handle_dring_reg_msg(vdc,
				(vio_dring_reg_msg_t *)&msg);
		break;

	case VD_STATE_RDX:
		if (msg.tag.vio_subtype_env != VIO_RDX) {
			status = EPROTO;
			break;
		}

		DMSG(0, "[%d] Received RDX: handshake done\n", vdc->instance);

		vdc->hshake_cnt = 0;	/* reset failed handshake count */
		status = 0;
		vdc->state = VD_STATE_DATA;

		cv_broadcast(&vdc->attach_cv);
		break;

	case VD_STATE_DATA:
	default:
		cmn_err(CE_NOTE, "[%d] Unexpected handshake state %d",
				vdc->instance, vdc->state);
		status = EPROTO;
		break;
	}

	return (status);
}


/*
 * Function:
 *	vdc_process_data_msg()
 *
 * Description:
 *	This function is called by the message processing thread each time
 *	a message with a msgtype of VIO_TYPE_DATA is received. It will either
 *	be an ACK or NACK from vds[1] which vdc handles as follows.
 *		ACK	- wake up the waiting thread
 *		NACK	- resend any messages necessary
 *
 *	[1] Although the message format allows it, vds should not send a
 *	    VIO_SUBTYPE_INFO message to vdc asking it to read data; if for
 *	    some bizarre reason it does, vdc will reset the connection.
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *	msg	- the LDC message sent by vds
 *
 * Return Code:
 *	0	- Success.
 *	> 0	- error value returned by LDC
 */
static int
vdc_process_data_msg(vdc_t *vdc, vio_msg_t msg)
{
	int			status = 0;
	vdc_local_desc_t	*ldep = NULL;
	vio_dring_msg_t		*dring_msg = NULL;
	uint_t			start;
	int			end;
	uint_t			count = 0;
	uint_t			operation;
	uint_t			idx;

	ASSERT(msg.tag.vio_msgtype == VIO_TYPE_DATA);
	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));

	dring_msg = (vio_dring_msg_t *)&msg;

	/*
	 * Check to see if the message has bogus data
	 */
	idx = start = dring_msg->start_idx;
	end = dring_msg->end_idx;
	if ((start >= vdc->dring_len) ||
	    (end >= vdc->dring_len) || (end < -1)) {
		cmn_err(CE_CONT, "?[%d] Bogus ACK data : start %d, end %d\n",
			vdc->instance, start, end);
		return (EINVAL);
	}

	DTRACE_IO2(recv, vio_dring_msg_t, dring_msg, vdc_t *, vdc);

	/*
	 * Verify that the sequence number is what vdc expects.
	 */
	switch (vdc_verify_seq_num(vdc, dring_msg)) {
	case VDC_SEQ_NUM_TODO:
		break;	/* keep processing this message */
	case VDC_SEQ_NUM_SKIP:
		return (0);
	case VDC_SEQ_NUM_INVALID:
		return (ENXIO);
	}

	if (msg.tag.vio_subtype == VIO_SUBTYPE_NACK) {
		DMSG(0, "[%d] DATA NACK\n", vdc->instance);
		VDC_DUMP_DRING_MSG(dring_msg);
		return (EIO);

	} else if (msg.tag.vio_subtype == VIO_SUBTYPE_INFO) {
		return (EPROTO);
	}

	ldep = &vdc->local_dring[start];
	if (ldep->dep->hdr.dstate == VIO_DESC_DONE) {
		mutex_enter(&ldep->lock);
		operation = ldep->dep->payload.operation;
		vdc->req_id_proc = ldep->dep->payload.req_id;
		vdc->dring_proc_idx = idx;
		ASSERT(ldep->dep->hdr.dstate == VIO_DESC_DONE);

		if ((operation == VD_OP_BREAD) || (operation == VD_OP_BWRITE)) {
			bioerror(ldep->buf, ldep->dep->payload.status);

			/* Clear the DRing entry */
			status = vdc_depopulate_descriptor(vdc, idx);

			/*
			 * biodone() should be called after clearing the DRing
			 * entry because biodone() will release the IO buffer.
			 */
			biodone(ldep->buf);
			DTRACE_IO2(vdone, buf_t *, ldep->buf, vdc_t *, vdc);
		}
		cv_signal(&ldep->cv);
		mutex_exit(&ldep->lock);
	}

	/* probe gives the count of how many entries were processed */
	DTRACE_IO2(processed, int, count, vdc_t *, vdc);

	return (status);
}

/*
 * Function:
 *	vdc_process_err_msg()
 *
 * NOTE: No error messages are used as part of the vDisk protocol
 */
static int
vdc_process_err_msg(vdc_t *vdc, vio_msg_t msg)
{
	_NOTE(ARGUNUSED(vdc))
	_NOTE(ARGUNUSED(msg))

	ASSERT(msg.tag.vio_msgtype == VIO_TYPE_ERR);
	cmn_err(CE_NOTE, "[%d] Got an ERR msg", vdc->instance);

	return (ENOTSUP);
}

/*
 * Function:
 *	vdc_handle_ver_msg()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *	ver_msg	- LDC message sent by vDisk server
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_handle_ver_msg(vdc_t *vdc, vio_ver_msg_t *ver_msg)
{
	int status = 0;

	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));

	if (ver_msg->tag.vio_subtype_env != VIO_VER_INFO) {
		return (EPROTO);
	}

	if (ver_msg->dev_class != VDEV_DISK_SERVER) {
		return (EINVAL);
	}

	switch (ver_msg->tag.vio_subtype) {
	case VIO_SUBTYPE_ACK:
		/*
		 * We check to see if the version returned is indeed supported
		 * (The server may have also adjusted the minor number downwards
		 * and if so 'ver_msg' will contain the actual version agreed)
		 */
		if (vdc_is_supported_version(ver_msg)) {
			vdc->ver.major = ver_msg->ver_major;
			vdc->ver.minor = ver_msg->ver_minor;
			ASSERT(vdc->ver.major > 0);

			vdc->state = VD_STATE_VER;
			status = vdc_init_attr_negotiation(vdc);
		} else {
			status = EPROTO;
		}
		break;

	case VIO_SUBTYPE_NACK:
		/*
		 * call vdc_is_supported_version() which will return the next
		 * supported version (if any) in 'ver_msg'
		 */
		(void) vdc_is_supported_version(ver_msg);
		if (ver_msg->ver_major > 0) {
			size_t len = sizeof (*ver_msg);

			ASSERT(vdc->ver.major > 0);

			/* reset the necessary fields and resend */
			ver_msg->tag.vio_subtype = VIO_SUBTYPE_INFO;
			ver_msg->dev_class = VDEV_DISK;

			status = vdc_send(vdc, (caddr_t)ver_msg, &len);
			DMSG(0, "[%d] Resend VER info (LDC status = %d)\n",
					vdc->instance, status);
			if (len != sizeof (*ver_msg))
				status = EBADMSG;
		} else {
			cmn_err(CE_NOTE, "[%d] No common version with "
					"vDisk server", vdc->instance);
			status = ENOTSUP;
		}

		break;
	case VIO_SUBTYPE_INFO:
		/*
		 * Handle the case where vds starts handshake
		 * (for now only vdc is the instigatior)
		 */
		status = ENOTSUP;
		break;

	default:
		status = EINVAL;
		break;
	}

	return (status);
}

/*
 * Function:
 *	vdc_handle_attr_msg()
 *
 * Description:
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *	attr_msg	- LDC message sent by vDisk server
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_handle_attr_msg(vdc_t *vdc, vd_attr_msg_t *attr_msg)
{
	int status = 0;

	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));

	if (attr_msg->tag.vio_subtype_env != VIO_ATTR_INFO) {
		return (EPROTO);
	}

	switch (attr_msg->tag.vio_subtype) {
	case VIO_SUBTYPE_ACK:
		/*
		 * We now verify the attributes sent by vds.
		 */
		vdc->vdisk_size = attr_msg->vdisk_size;
		vdc->vdisk_type = attr_msg->vdisk_type;

		DMSG(0, "[%d] max_xfer_sz: sent %lx acked %lx\n",
			vdc->instance, vdc->max_xfer_sz, attr_msg->max_xfer_sz);
		DMSG(0, "[%d] vdisk_block_size: sent %lx acked %x\n",
			vdc->instance, vdc->block_size,
			attr_msg->vdisk_block_size);

		/*
		 * We don't know at compile time what the vDisk server will
		 * think are good values but we apply an large (arbitrary)
		 * upper bound to prevent memory exhaustion in vdc if it was
		 * allocating a DRing based of huge values sent by the server.
		 * We probably will never exceed this except if the message
		 * was garbage.
		 */
		if ((attr_msg->max_xfer_sz * attr_msg->vdisk_block_size) <=
				(PAGESIZE * DEV_BSIZE)) {
			vdc->max_xfer_sz = attr_msg->max_xfer_sz;
			vdc->block_size = attr_msg->vdisk_block_size;
		} else {
			cmn_err(CE_NOTE, "[%d] vds block transfer size too big;"
				" using max supported by vdc", vdc->instance);
		}

		if ((attr_msg->xfer_mode != VIO_DRING_MODE) ||
		    (attr_msg->vdisk_size > INT64_MAX) ||
		    (attr_msg->vdisk_type > VD_DISK_TYPE_DISK)) {
			cmn_err(CE_NOTE, "[%d] Invalid attributes from vds",
					vdc->instance);
			status = EINVAL;
			break;
		}

		vdc->state = VD_STATE_ATTR;
		status = vdc_init_dring_negotiate(vdc);
		break;

	case VIO_SUBTYPE_NACK:
		/*
		 * vds could not handle the attributes we sent so we
		 * stop negotiating.
		 */
		status = EPROTO;
		break;

	case VIO_SUBTYPE_INFO:
		/*
		 * Handle the case where vds starts the handshake
		 * (for now; vdc is the only supported instigatior)
		 */
		status = ENOTSUP;
		break;

	default:
		status = ENOTSUP;
		break;
	}

	return (status);
}

/*
 * Function:
 *	vdc_handle_dring_reg_msg()
 *
 * Description:
 *
 * Arguments:
 *	vdc		- soft state pointer for this instance of the driver.
 *	dring_msg	- LDC message sent by vDisk server
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_handle_dring_reg_msg(vdc_t *vdc, vio_dring_reg_msg_t *dring_msg)
{
	int		status = 0;
	vio_rdx_msg_t	msg = {0};
	size_t		msglen = sizeof (msg);

	ASSERT(vdc != NULL);
	ASSERT(mutex_owned(&vdc->lock));

	if (dring_msg->tag.vio_subtype_env != VIO_DRING_REG) {
		return (EPROTO);
	}

	switch (dring_msg->tag.vio_subtype) {
	case VIO_SUBTYPE_ACK:
		/* save the received dring_ident */
		vdc->dring_ident = dring_msg->dring_ident;
		DMSG(0, "[%d] Received dring ident=0x%lx\n",
			vdc->instance, vdc->dring_ident);

		/*
		 * Send an RDX message to vds to indicate we are ready
		 * to send data
		 */
		msg.tag.vio_msgtype = VIO_TYPE_CTRL;
		msg.tag.vio_subtype = VIO_SUBTYPE_INFO;
		msg.tag.vio_subtype_env = VIO_RDX;
		msg.tag.vio_sid = vdc->session_id;
		status = vdc_send(vdc, (caddr_t)&msg, &msglen);
		if (status != 0) {
			cmn_err(CE_NOTE, "[%d] Failed to send RDX"
				" message (%d)", vdc->instance, status);
			break;
		}

		vdc->state = VD_STATE_RDX;
		break;

	case VIO_SUBTYPE_NACK:
		/*
		 * vds could not handle the DRing info we sent so we
		 * stop negotiating.
		 */
		cmn_err(CE_CONT, "server could not register DRing\n");
		vdc_reset_connection(vdc, B_TRUE);
		vdc_destroy_descriptor_ring(vdc);
		status = EPROTO;
		break;

	case VIO_SUBTYPE_INFO:
		/*
		 * Handle the case where vds starts handshake
		 * (for now only vdc is the instigatior)
		 */
		status = ENOTSUP;
		break;
	default:
		status = ENOTSUP;
	}

	return (status);
}

/*
 * Function:
 *	vdc_verify_seq_num()
 *
 * Description:
 *	This functions verifies that the sequence number sent back by the vDisk
 *	server with the latest message is what is expected (i.e. it is greater
 *	than the last seq num sent by the vDisk server and less than or equal
 *	to the last seq num generated by vdc).
 *
 *	It then checks the request ID to see if any requests need processing
 *	in the DRing.
 *
 * Arguments:
 *	vdc		- soft state pointer for this instance of the driver.
 *	dring_msg	- pointer to the LDC message sent by vds
 *
 * Return Code:
 *	VDC_SEQ_NUM_TODO	- Message needs to be processed
 *	VDC_SEQ_NUM_SKIP	- Message has already been processed
 *	VDC_SEQ_NUM_INVALID	- The seq numbers are so out of sync,
 *				  vdc cannot deal with them
 */
static int
vdc_verify_seq_num(vdc_t *vdc, vio_dring_msg_t *dring_msg)
{
	ASSERT(vdc != NULL);
	ASSERT(dring_msg != NULL);
	ASSERT(mutex_owned(&vdc->lock));

	/*
	 * Check to see if the messages were responded to in the correct
	 * order by vds.
	 */
	if ((dring_msg->seq_num <= vdc->seq_num_reply) ||
	    (dring_msg->seq_num > vdc->seq_num)) {
		cmn_err(CE_CONT, "?[%d] Bogus sequence_number %lu: "
			"%lu > expected <= %lu (last proc req %lu sent %lu)\n",
				vdc->instance, dring_msg->seq_num,
				vdc->seq_num_reply, vdc->seq_num,
				vdc->req_id_proc, vdc->req_id);
		return (VDC_SEQ_NUM_INVALID);
	}
	vdc->seq_num_reply = dring_msg->seq_num;

	if (vdc->req_id_proc < vdc->req_id)
		return (VDC_SEQ_NUM_TODO);
	else
		return (VDC_SEQ_NUM_SKIP);
}


/*
 * Function:
 *	vdc_is_supported_version()
 *
 * Description:
 *	This routine checks if the major/minor version numbers specified in
 *	'ver_msg' are supported. If not it finds the next version that is
 *	in the supported version list 'vdc_version[]' and sets the fields in
 *	'ver_msg' to those values
 *
 * Arguments:
 *	ver_msg	- LDC message sent by vDisk server
 *
 * Return Code:
 *	B_TRUE	- Success
 *	B_FALSE	- Version not supported
 */
static boolean_t
vdc_is_supported_version(vio_ver_msg_t *ver_msg)
{
	int vdc_num_versions = sizeof (vdc_version) / sizeof (vdc_version[0]);

	for (int i = 0; i < vdc_num_versions; i++) {
		ASSERT(vdc_version[i].major > 0);
		ASSERT((i == 0) ||
		    (vdc_version[i].major < vdc_version[i-1].major));

		/*
		 * If the major versions match, adjust the minor version, if
		 * necessary, down to the highest value supported by this
		 * client. The server should support all minor versions lower
		 * than the value it sent
		 */
		if (ver_msg->ver_major == vdc_version[i].major) {
			if (ver_msg->ver_minor > vdc_version[i].minor) {
				DMSG(0, "Adjusting minor version from %u to %u",
				    ver_msg->ver_minor, vdc_version[i].minor);
				ver_msg->ver_minor = vdc_version[i].minor;
			}
			return (B_TRUE);
		}

		/*
		 * If the message contains a higher major version number, set
		 * the message's major/minor versions to the current values
		 * and return false, so this message will get resent with
		 * these values, and the server will potentially try again
		 * with the same or a lower version
		 */
		if (ver_msg->ver_major > vdc_version[i].major) {
			ver_msg->ver_major = vdc_version[i].major;
			ver_msg->ver_minor = vdc_version[i].minor;
			DMSG(0, "Suggesting major/minor (0x%x/0x%x)\n",
				ver_msg->ver_major, ver_msg->ver_minor);

			return (B_FALSE);
		}

		/*
		 * Otherwise, the message's major version is less than the
		 * current major version, so continue the loop to the next
		 * (lower) supported version
		 */
	}

	/*
	 * No common version was found; "ground" the version pair in the
	 * message to terminate negotiation
	 */
	ver_msg->ver_major = 0;
	ver_msg->ver_minor = 0;

	return (B_FALSE);
}
/* -------------------------------------------------------------------------- */

/*
 * DKIO(7) support
 */

typedef struct vdc_dk_arg {
	struct dk_callback	dkc;
	int			mode;
	dev_t			dev;
	vdc_t			*vdc;
} vdc_dk_arg_t;

/*
 * Function:
 * 	vdc_dkio_flush_cb()
 *
 * Description:
 *	This routine is a callback for DKIOCFLUSHWRITECACHE which can be called
 *	by kernel code.
 *
 * Arguments:
 *	arg	- a pointer to a vdc_dk_arg_t structure.
 */
void
vdc_dkio_flush_cb(void *arg)
{
	struct vdc_dk_arg	*dk_arg = (struct vdc_dk_arg *)arg;
	struct dk_callback	*dkc = NULL;
	vdc_t			*vdc = NULL;
	int			rv;

	if (dk_arg == NULL) {
		cmn_err(CE_CONT, "?[Unk] DKIOCFLUSHWRITECACHE arg is NULL\n");
		return;
	}
	dkc = &dk_arg->dkc;
	vdc = dk_arg->vdc;
	ASSERT(vdc != NULL);

	rv = vdc_populate_descriptor(vdc, NULL, 0, VD_OP_FLUSH,
		dk_arg->mode, SDPART(getminor(dk_arg->dev)));
	if (rv != 0) {
		DMSG(0, "[%d] DKIOCFLUSHWRITECACHE failed %d : model %x\n",
			vdc->instance, rv,
			ddi_model_convert_from(dk_arg->mode & FMODELS));
	}

	/*
	 * Trigger the call back to notify the caller the the ioctl call has
	 * been completed.
	 */
	if ((dk_arg->mode & FKIOCTL) &&
	    (dkc != NULL) &&
	    (dkc->dkc_callback != NULL)) {
		ASSERT(dkc->dkc_cookie != NULL);
		(*dkc->dkc_callback)(dkc->dkc_cookie, rv);
	}

	/* Indicate that one less DKIO write flush is outstanding */
	mutex_enter(&vdc->lock);
	vdc->dkio_flush_pending--;
	ASSERT(vdc->dkio_flush_pending >= 0);
	mutex_exit(&vdc->lock);

	/* free the mem that was allocated when the callback was dispatched */
	kmem_free(arg, sizeof (vdc_dk_arg_t));
}

/*
 * This structure is used in the DKIO(7I) array below.
 */
typedef struct vdc_dk_ioctl {
	uint8_t		op;		/* VD_OP_XXX value */
	int		cmd;		/* Solaris ioctl operation number */
	size_t		nbytes;		/* size of structure to be copied */

	/* function to convert between vDisk and Solaris structure formats */
	int	(*convert)(vdc_t *vdc, void *vd_buf, void *ioctl_arg,
	    int mode, int dir);
} vdc_dk_ioctl_t;

/*
 * Subset of DKIO(7I) operations currently supported
 */
static vdc_dk_ioctl_t	dk_ioctl[] = {
	{VD_OP_FLUSH,		DKIOCFLUSHWRITECACHE,	sizeof (int),
		vdc_null_copy_func},
	{VD_OP_GET_WCE,		DKIOCGETWCE,		sizeof (int),
		vdc_get_wce_convert},
	{VD_OP_SET_WCE,		DKIOCSETWCE,		sizeof (int),
		vdc_set_wce_convert},
	{VD_OP_GET_VTOC,	DKIOCGVTOC,		sizeof (vd_vtoc_t),
		vdc_get_vtoc_convert},
	{VD_OP_SET_VTOC,	DKIOCSVTOC,		sizeof (vd_vtoc_t),
		vdc_set_vtoc_convert},
	{VD_OP_GET_DISKGEOM,	DKIOCGGEOM,		sizeof (vd_geom_t),
		vdc_get_geom_convert},
	{VD_OP_GET_DISKGEOM,	DKIOCG_PHYGEOM,		sizeof (vd_geom_t),
		vdc_get_geom_convert},
	{VD_OP_GET_DISKGEOM, 	DKIOCG_VIRTGEOM,	sizeof (vd_geom_t),
		vdc_get_geom_convert},
	{VD_OP_SET_DISKGEOM,	DKIOCSGEOM,		sizeof (vd_geom_t),
		vdc_set_geom_convert},
	{VD_OP_GET_EFI,		DKIOCGETEFI,		0,
		vdc_get_efi_convert},
	{VD_OP_SET_EFI,		DKIOCSETEFI,		0,
		vdc_set_efi_convert},

	/*
	 * These particular ioctls are not sent to the server - vdc fakes up
	 * the necessary info.
	 */
	{0, DKIOCINFO, sizeof (struct dk_cinfo), vdc_null_copy_func},
	{0, DKIOCGMEDIAINFO, sizeof (struct dk_minfo), vdc_null_copy_func},
	{0, USCSICMD,	sizeof (struct uscsi_cmd), vdc_null_copy_func},
	{0, DKIOCREMOVABLE, 0, vdc_null_copy_func},
	{0, CDROMREADOFFSET, 0, vdc_null_copy_func}
};

/*
 * Function:
 *	vd_process_ioctl()
 *
 * Description:
 *	This routine processes disk specific ioctl calls
 *
 * Arguments:
 *	dev	- the device number
 *	cmd	- the operation [dkio(7I)] to be processed
 *	arg	- pointer to user provided structure
 *		  (contains data to be set or reference parameter for get)
 *	mode	- bit flag, indicating open settings, 32/64 bit type, etc
 *
 * Return Code:
 *	0
 *	EFAULT
 *	ENXIO
 *	EIO
 *	ENOTSUP
 */
static int
vd_process_ioctl(dev_t dev, int cmd, caddr_t arg, int mode)
{
	int		instance = SDUNIT(getminor(dev));
	vdc_t		*vdc = NULL;
	int		rv = -1;
	int		idx = 0;		/* index into dk_ioctl[] */
	size_t		len = 0;		/* #bytes to send to vds */
	size_t		alloc_len = 0;		/* #bytes to allocate mem for */
	caddr_t		mem_p = NULL;
	size_t		nioctls = (sizeof (dk_ioctl)) / (sizeof (dk_ioctl[0]));
	struct vtoc	vtoc_saved;

	DMSG(0, "[%d] Processing ioctl(%x) for dev %lx : model %x\n",
		instance, cmd, dev, ddi_model_convert_from(mode & FMODELS));

	vdc = ddi_get_soft_state(vdc_state, instance);
	if (vdc == NULL) {
		cmn_err(CE_NOTE, "![%d] Could not get soft state structure",
		    instance);
		return (ENXIO);
	}

	/*
	 * Check to see if we can communicate with the vDisk server
	 */
	if (!vdc_is_able_to_tx_data(vdc, O_NONBLOCK)) {
		DMSG(0, "[%d] Not ready to transmit data\n", instance);
		return (ENOLINK);
	}

	/*
	 * Validate the ioctl operation to be performed.
	 *
	 * If we have looped through the array without finding a match then we
	 * don't support this ioctl.
	 */
	for (idx = 0; idx < nioctls; idx++) {
		if (cmd == dk_ioctl[idx].cmd)
			break;
	}

	if (idx >= nioctls) {
		cmn_err(CE_CONT, "?[%d] Unsupported ioctl (0x%x)\n",
				vdc->instance, cmd);
		return (ENOTSUP);
	}

	if (cmd == DKIOCGETEFI || cmd == DKIOCSETEFI) {
		/* size is not fixed for EFI ioctls, it depends on ioctl arg */
		dk_efi_t	dk_efi;

		rv = ddi_copyin(arg, &dk_efi, sizeof (dk_efi_t), mode);
		if (rv != 0)
			return (EFAULT);

		len = sizeof (vd_efi_t) - 1 + dk_efi.dki_length;
	} else {
		len = dk_ioctl[idx].nbytes;
	}

	/*
	 * Deal with the ioctls which the server does not provide. vdc can
	 * fake these up and return immediately
	 */
	switch (cmd) {
	case CDROMREADOFFSET:
	case DKIOCREMOVABLE:
	case USCSICMD:
		return (ENOTTY);

	case DKIOCINFO:
		{
			struct dk_cinfo	cinfo;
			if (vdc->cinfo == NULL)
				return (ENXIO);

			bcopy(vdc->cinfo, &cinfo, sizeof (struct dk_cinfo));
			cinfo.dki_partition = SDPART(getminor(dev));

			rv = ddi_copyout(&cinfo, (void *)arg,
					sizeof (struct dk_cinfo), mode);
			if (rv != 0)
				return (EFAULT);

			return (0);
		}

	case DKIOCGMEDIAINFO:
		{
			if (vdc->minfo == NULL)
				return (ENXIO);

			rv = ddi_copyout(vdc->minfo, (void *)arg,
					sizeof (struct dk_minfo), mode);
			if (rv != 0)
				return (EFAULT);

			return (0);
		}

	case DKIOCFLUSHWRITECACHE:
		{
			struct dk_callback *dkc = (struct dk_callback *)arg;
			vdc_dk_arg_t	*dkarg = NULL;

			DMSG(1, "[%d] Flush W$: mode %x\n", instance, mode);

			/*
			 * If the backing device is not a 'real' disk then the
			 * W$ operation request to the vDisk server will fail
			 * so we might as well save the cycles and return now.
			 */
			if (vdc->vdisk_type != VD_DISK_TYPE_DISK)
				return (ENOTTY);

			/*
			 * If arg is NULL, then there is no callback function
			 * registered and the call operates synchronously; we
			 * break and continue with the rest of the function and
			 * wait for vds to return (i.e. after the request to
			 * vds returns successfully, all writes completed prior
			 * to the ioctl will have been flushed from the disk
			 * write cache to persistent media.
			 *
			 * If a callback function is registered, we dispatch
			 * the request on a task queue and return immediately.
			 * The callback will deal with informing the calling
			 * thread that the flush request is completed.
			 */
			if (dkc == NULL)
				break;

			dkarg = kmem_zalloc(sizeof (vdc_dk_arg_t), KM_SLEEP);

			dkarg->mode = mode;
			dkarg->dev = dev;
			bcopy(dkc, &dkarg->dkc, sizeof (*dkc));

			mutex_enter(&vdc->lock);
			vdc->dkio_flush_pending++;
			dkarg->vdc = vdc;
			mutex_exit(&vdc->lock);

			/* put the request on a task queue */
			rv = taskq_dispatch(system_taskq, vdc_dkio_flush_cb,
				(void *)dkarg, DDI_SLEEP);

			return (rv == NULL ? ENOMEM : 0);
		}
	}

	/* catch programming error in vdc - should be a VD_OP_XXX ioctl */
	ASSERT(dk_ioctl[idx].op != 0);

	/* LDC requires that the memory being mapped is 8-byte aligned */
	alloc_len = P2ROUNDUP(len, sizeof (uint64_t));
	DMSG(1, "[%d] struct size %ld alloc %ld\n", instance, len, alloc_len);

	ASSERT(alloc_len != 0);	/* sanity check */
	mem_p = kmem_zalloc(alloc_len, KM_SLEEP);

	if (cmd == DKIOCSVTOC) {
		/*
		 * Save a copy of the current VTOC so that we can roll back
		 * if the setting of the new VTOC fails.
		 */
		bcopy(vdc->vtoc, &vtoc_saved, sizeof (struct vtoc));
	}

	/*
	 * Call the conversion function for this ioctl whhich if necessary
	 * converts from the Solaris format to the format ARC'ed
	 * as part of the vDisk protocol (FWARC 2006/195)
	 */
	ASSERT(dk_ioctl[idx].convert != NULL);
	rv = (dk_ioctl[idx].convert)(vdc, arg, mem_p, mode, VD_COPYIN);
	if (rv != 0) {
		DMSG(0, "[%d] convert func returned %d for ioctl 0x%x\n",
				instance, rv, cmd);
		if (mem_p != NULL)
			kmem_free(mem_p, alloc_len);
		return (rv);
	}

	/*
	 * send request to vds to service the ioctl.
	 */
	rv = vdc_populate_descriptor(vdc, mem_p, alloc_len, dk_ioctl[idx].op,
			mode, SDPART((getminor(dev))));
	if (rv != 0) {
		/*
		 * This is not necessarily an error. The ioctl could
		 * be returning a value such as ENOTTY to indicate
		 * that the ioctl is not applicable.
		 */
		DMSG(0, "[%d] vds returned %d for ioctl 0x%x\n",
			instance, rv, cmd);
		if (mem_p != NULL)
			kmem_free(mem_p, alloc_len);

		if (cmd == DKIOCSVTOC) {
			/* update of the VTOC has failed, roll back */
			bcopy(&vtoc_saved, vdc->vtoc, sizeof (struct vtoc));
		}

		return (rv);
	}

	if (cmd == DKIOCSVTOC) {
		/*
		 * The VTOC has been changed. We need to update the device
		 * nodes to handle the case where an EFI label has been
		 * changed to a VTOC label. We also try and update the device
		 * node properties. Failing to set the properties should
		 * not cause an error to be return the caller though.
		 */
		vdc->vdisk_label = VD_DISK_LABEL_VTOC;
		(void) vdc_create_device_nodes_vtoc(vdc);

		if (vdc_create_device_nodes_props(vdc)) {
			cmn_err(CE_NOTE, "![%d] Failed to update device nodes"
			    " properties", vdc->instance);
		}

	} else if (cmd == DKIOCSETEFI) {
		/*
		 * The EFI has been changed. We need to update the device
		 * nodes to handle the case where a VTOC label has been
		 * changed to an EFI label. We also try and update the device
		 * node properties. Failing to set the properties should
		 * not cause an error to be return the caller though.
		 */
		struct dk_gpt *efi;
		size_t efi_len;

		vdc->vdisk_label = VD_DISK_LABEL_EFI;
		(void) vdc_create_device_nodes_efi(vdc);

		rv = vdc_efi_alloc_and_read(dev, &efi, &efi_len);

		if (rv == 0) {
			vdc_store_efi(vdc, efi);
			rv = vdc_create_device_nodes_props(vdc);
			vd_efi_free(efi, efi_len);
		}

		if (rv) {
			cmn_err(CE_NOTE, "![%d] Failed to update device nodes"
			    " properties", vdc->instance);
		}
	}

	/*
	 * Call the conversion function (if it exists) for this ioctl
	 * which converts from the format ARC'ed as part of the vDisk
	 * protocol (FWARC 2006/195) back to a format understood by
	 * the rest of Solaris.
	 */
	rv = (dk_ioctl[idx].convert)(vdc, mem_p, arg, mode, VD_COPYOUT);
	if (rv != 0) {
		DMSG(0, "[%d] convert func returned %d for ioctl 0x%x\n",
				instance, rv, cmd);
		if (mem_p != NULL)
			kmem_free(mem_p, alloc_len);
		return (rv);
	}

	if (mem_p != NULL)
		kmem_free(mem_p, alloc_len);

	return (rv);
}

/*
 * Function:
 *
 * Description:
 *	This is an empty conversion function used by ioctl calls which
 *	do not need to convert the data being passed in/out to userland
 */
static int
vdc_null_copy_func(vdc_t *vdc, void *from, void *to, int mode, int dir)
{
	_NOTE(ARGUNUSED(vdc))
	_NOTE(ARGUNUSED(from))
	_NOTE(ARGUNUSED(to))
	_NOTE(ARGUNUSED(mode))
	_NOTE(ARGUNUSED(dir))

	return (0);
}

static int
vdc_get_wce_convert(vdc_t *vdc, void *from, void *to,
    int mode, int dir)
{
	_NOTE(ARGUNUSED(vdc))

	if (dir == VD_COPYIN)
		return (0);		/* nothing to do */

	if (ddi_copyout(from, to, sizeof (int), mode) != 0)
		return (EFAULT);

	return (0);
}

static int
vdc_set_wce_convert(vdc_t *vdc, void *from, void *to,
    int mode, int dir)
{
	_NOTE(ARGUNUSED(vdc))

	if (dir == VD_COPYOUT)
		return (0);		/* nothing to do */

	if (ddi_copyin(from, to, sizeof (int), mode) != 0)
		return (EFAULT);

	return (0);
}

/*
 * Function:
 *	vdc_get_vtoc_convert()
 *
 * Description:
 *	This routine performs the necessary convertions from the DKIOCGVTOC
 *	Solaris structure to the format defined in FWARC 2006/195.
 *
 *	In the struct vtoc definition, the timestamp field is marked as not
 *	supported so it is not part of vDisk protocol (FWARC 2006/195).
 *	However SVM uses that field to check it can write into the VTOC,
 *	so we fake up the info of that field.
 *
 * Arguments:
 *	vdc	- the vDisk client
 *	from	- the buffer containing the data to be copied from
 *	to	- the buffer to be copied to
 *	mode	- flags passed to ioctl() call
 *	dir	- the "direction" of the copy - VD_COPYIN or VD_COPYOUT
 *
 * Return Code:
 *	0	- Success
 *	ENXIO	- incorrect buffer passed in.
 *	EFAULT	- ddi_copyout routine encountered an error.
 */
static int
vdc_get_vtoc_convert(vdc_t *vdc, void *from, void *to, int mode, int dir)
{
	int		i;
	void		*tmp_mem = NULL;
	void		*tmp_memp;
	struct vtoc	vt;
	struct vtoc32	vt32;
	int		copy_len = 0;
	int		rv = 0;

	if (dir != VD_COPYOUT)
		return (0);	/* nothing to do */

	if ((from == NULL) || (to == NULL))
		return (ENXIO);

	if (ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32)
		copy_len = sizeof (struct vtoc32);
	else
		copy_len = sizeof (struct vtoc);

	tmp_mem = kmem_alloc(copy_len, KM_SLEEP);

	VD_VTOC2VTOC((vd_vtoc_t *)from, &vt);

	/* fake the VTOC timestamp field */
	for (i = 0; i < V_NUMPAR; i++) {
		vt.timestamp[i] = vdc->vtoc->timestamp[i];
	}

	if (ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32) {
		vtoctovtoc32(vt, vt32);
		tmp_memp = &vt32;
	} else {
		tmp_memp = &vt;
	}
	rv = ddi_copyout(tmp_memp, to, copy_len, mode);
	if (rv != 0)
		rv = EFAULT;

	kmem_free(tmp_mem, copy_len);
	return (rv);
}

/*
 * Function:
 *	vdc_set_vtoc_convert()
 *
 * Description:
 *	This routine performs the necessary convertions from the DKIOCSVTOC
 *	Solaris structure to the format defined in FWARC 2006/195.
 *
 * Arguments:
 *	vdc	- the vDisk client
 *	from	- Buffer with data
 *	to	- Buffer where data is to be copied to
 *	mode	- flags passed to ioctl
 *	dir	- direction of copy (in or out)
 *
 * Return Code:
 *	0	- Success
 *	ENXIO	- Invalid buffer passed in
 *	EFAULT	- ddi_copyin of data failed
 */
static int
vdc_set_vtoc_convert(vdc_t *vdc, void *from, void *to, int mode, int dir)
{
	void		*tmp_mem = NULL;
	struct vtoc	vt;
	struct vtoc	*vtp = &vt;
	vd_vtoc_t	vtvd;
	int		copy_len = 0;
	int		rv = 0;

	if (dir != VD_COPYIN)
		return (0);	/* nothing to do */

	if ((from == NULL) || (to == NULL))
		return (ENXIO);

	if (ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32)
		copy_len = sizeof (struct vtoc32);
	else
		copy_len = sizeof (struct vtoc);

	tmp_mem = kmem_alloc(copy_len, KM_SLEEP);

	rv = ddi_copyin(from, tmp_mem, copy_len, mode);
	if (rv != 0) {
		kmem_free(tmp_mem, copy_len);
		return (EFAULT);
	}

	if (ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32) {
		vtoc32tovtoc((*(struct vtoc32 *)tmp_mem), vt);
	} else {
		vtp = tmp_mem;
	}

	/*
	 * The VTOC is being changed, then vdc needs to update the copy
	 * it saved in the soft state structure.
	 */
	bcopy(vtp, vdc->vtoc, sizeof (struct vtoc));

	VTOC2VD_VTOC(vtp, &vtvd);
	bcopy(&vtvd, to, sizeof (vd_vtoc_t));
	kmem_free(tmp_mem, copy_len);

	return (0);
}

/*
 * Function:
 *	vdc_get_geom_convert()
 *
 * Description:
 *	This routine performs the necessary convertions from the DKIOCGGEOM,
 *	DKIOCG_PHYSGEOM and DKIOG_VIRTGEOM Solaris structures to the format
 *	defined in FWARC 2006/195
 *
 * Arguments:
 *	vdc	- the vDisk client
 *	from	- Buffer with data
 *	to	- Buffer where data is to be copied to
 *	mode	- flags passed to ioctl
 *	dir	- direction of copy (in or out)
 *
 * Return Code:
 *	0	- Success
 *	ENXIO	- Invalid buffer passed in
 *	EFAULT	- ddi_copyout of data failed
 */
static int
vdc_get_geom_convert(vdc_t *vdc, void *from, void *to, int mode, int dir)
{
	_NOTE(ARGUNUSED(vdc))

	struct dk_geom	geom;
	int	copy_len = sizeof (struct dk_geom);
	int	rv = 0;

	if (dir != VD_COPYOUT)
		return (0);	/* nothing to do */

	if ((from == NULL) || (to == NULL))
		return (ENXIO);

	VD_GEOM2DK_GEOM((vd_geom_t *)from, &geom);
	rv = ddi_copyout(&geom, to, copy_len, mode);
	if (rv != 0)
		rv = EFAULT;

	return (rv);
}

/*
 * Function:
 *	vdc_set_geom_convert()
 *
 * Description:
 *	This routine performs the necessary convertions from the DKIOCSGEOM
 *	Solaris structure to the format defined in FWARC 2006/195.
 *
 * Arguments:
 *	vdc	- the vDisk client
 *	from	- Buffer with data
 *	to	- Buffer where data is to be copied to
 *	mode	- flags passed to ioctl
 *	dir	- direction of copy (in or out)
 *
 * Return Code:
 *	0	- Success
 *	ENXIO	- Invalid buffer passed in
 *	EFAULT	- ddi_copyin of data failed
 */
static int
vdc_set_geom_convert(vdc_t *vdc, void *from, void *to, int mode, int dir)
{
	_NOTE(ARGUNUSED(vdc))

	vd_geom_t	vdgeom;
	void		*tmp_mem = NULL;
	int		copy_len = sizeof (struct dk_geom);
	int		rv = 0;

	if (dir != VD_COPYIN)
		return (0);	/* nothing to do */

	if ((from == NULL) || (to == NULL))
		return (ENXIO);

	tmp_mem = kmem_alloc(copy_len, KM_SLEEP);

	rv = ddi_copyin(from, tmp_mem, copy_len, mode);
	if (rv != 0) {
		kmem_free(tmp_mem, copy_len);
		return (EFAULT);
	}
	DK_GEOM2VD_GEOM((struct dk_geom *)tmp_mem, &vdgeom);
	bcopy(&vdgeom, to, sizeof (vdgeom));
	kmem_free(tmp_mem, copy_len);

	return (0);
}

static int
vdc_get_efi_convert(vdc_t *vdc, void *from, void *to, int mode, int dir)
{
	_NOTE(ARGUNUSED(vdc))

	vd_efi_t	*vd_efi;
	dk_efi_t	dk_efi;
	int		rv = 0;
	void		*uaddr;

	if ((from == NULL) || (to == NULL))
		return (ENXIO);

	if (dir == VD_COPYIN) {

		vd_efi = (vd_efi_t *)to;

		rv = ddi_copyin(from, &dk_efi, sizeof (dk_efi_t), mode);
		if (rv != 0)
			return (EFAULT);

		vd_efi->lba = dk_efi.dki_lba;
		vd_efi->length = dk_efi.dki_length;
		bzero(vd_efi->data, vd_efi->length);

	} else {

		rv = ddi_copyin(to, &dk_efi, sizeof (dk_efi_t), mode);
		if (rv != 0)
			return (EFAULT);

		uaddr = dk_efi.dki_data;

		dk_efi.dki_data = kmem_alloc(dk_efi.dki_length, KM_SLEEP);

		VD_EFI2DK_EFI((vd_efi_t *)from, &dk_efi);

		rv = ddi_copyout(dk_efi.dki_data, uaddr, dk_efi.dki_length,
		    mode);
		if (rv != 0)
			return (EFAULT);

		kmem_free(dk_efi.dki_data, dk_efi.dki_length);
	}

	return (0);
}

static int
vdc_set_efi_convert(vdc_t *vdc, void *from, void *to, int mode, int dir)
{
	_NOTE(ARGUNUSED(vdc))

	dk_efi_t	dk_efi;
	void		*uaddr;

	if (dir == VD_COPYOUT)
		return (0);	/* nothing to do */

	if ((from == NULL) || (to == NULL))
		return (ENXIO);

	if (ddi_copyin(from, &dk_efi, sizeof (dk_efi_t), mode) != 0)
		return (EFAULT);

	uaddr = dk_efi.dki_data;

	dk_efi.dki_data = kmem_alloc(dk_efi.dki_length, KM_SLEEP);

	if (ddi_copyin(uaddr, dk_efi.dki_data, dk_efi.dki_length, mode) != 0)
		return (EFAULT);

	DK_EFI2VD_EFI(&dk_efi, (vd_efi_t *)to);

	kmem_free(dk_efi.dki_data, dk_efi.dki_length);

	return (0);
}

/*
 * Function:
 *	vdc_create_fake_geometry()
 *
 * Description:
 *	This routine fakes up the disk info needed for some DKIO ioctls.
 *		- DKIOCINFO
 *		- DKIOCGMEDIAINFO
 *
 *	[ just like lofi(7D) and ramdisk(7D) ]
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_create_fake_geometry(vdc_t *vdc)
{
	int	rv = 0;

	ASSERT(vdc != NULL);

	/*
	 * DKIOCINFO support
	 */
	vdc->cinfo = kmem_zalloc(sizeof (struct dk_cinfo), KM_SLEEP);

	(void) strcpy(vdc->cinfo->dki_cname, VDC_DRIVER_NAME);
	(void) strcpy(vdc->cinfo->dki_dname, VDC_DRIVER_NAME);
	/* max_xfer_sz is #blocks so we don't need to divide by DEV_BSIZE */
	vdc->cinfo->dki_maxtransfer = vdc->max_xfer_sz;
	vdc->cinfo->dki_ctype = DKC_SCSI_CCS;
	vdc->cinfo->dki_flags = DKI_FMTVOL;
	vdc->cinfo->dki_cnum = 0;
	vdc->cinfo->dki_addr = 0;
	vdc->cinfo->dki_space = 0;
	vdc->cinfo->dki_prio = 0;
	vdc->cinfo->dki_vec = 0;
	vdc->cinfo->dki_unit = vdc->instance;
	vdc->cinfo->dki_slave = 0;
	/*
	 * The partition number will be created on the fly depending on the
	 * actual slice (i.e. minor node) that is used to request the data.
	 */
	vdc->cinfo->dki_partition = 0;

	/*
	 * DKIOCGMEDIAINFO support
	 */
	if (vdc->minfo == NULL)
		vdc->minfo = kmem_zalloc(sizeof (struct dk_minfo), KM_SLEEP);
	vdc->minfo->dki_media_type = DK_FIXED_DISK;
	vdc->minfo->dki_capacity = vdc->vdisk_size;
	vdc->minfo->dki_lbsize = DEV_BSIZE;

	return (rv);
}

/*
 * Function:
 *	vdc_setup_disk_layout()
 *
 * Description:
 *	This routine discovers all the necessary details about the "disk"
 *	by requesting the data that is available from the vDisk server and by
 *	faking up the rest of the data.
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	0	- Success
 */
static int
vdc_setup_disk_layout(vdc_t *vdc)
{
	buf_t	*buf;	/* BREAD requests need to be in a buf_t structure */
	dev_t	dev;
	int	slice = 0;
	int	rv;

	ASSERT(vdc != NULL);

	rv = vdc_create_fake_geometry(vdc);
	if (rv != 0) {
		cmn_err(CE_NOTE, "[%d] Failed to create disk geometry (err%d)",
				vdc->instance, rv);
	}

	if (vdc->vtoc == NULL)
		vdc->vtoc = kmem_zalloc(sizeof (struct vtoc), KM_SLEEP);

	dev = makedevice(ddi_driver_major(vdc->dip),
				VD_MAKE_DEV(vdc->instance, 0));
	rv = vd_process_ioctl(dev, DKIOCGVTOC, (caddr_t)vdc->vtoc, FKIOCTL);

	if (rv && rv != ENOTSUP) {
		cmn_err(CE_NOTE, "[%d] Failed to get VTOC (err=%d)",
				vdc->instance, rv);
		return (rv);
	}

	if (rv == ENOTSUP) {
		/*
		 * If the device does not support VTOC then we try
		 * to read an EFI label.
		 */
		struct dk_gpt *efi;
		size_t efi_len;

		rv = vdc_efi_alloc_and_read(dev, &efi, &efi_len);

		if (rv) {
			cmn_err(CE_NOTE, "[%d] Failed to get EFI (err=%d)",
			    vdc->instance, rv);
			return (rv);
		}

		vdc->vdisk_label = VD_DISK_LABEL_EFI;
		vdc_store_efi(vdc, efi);
		vd_efi_free(efi, efi_len);

		return (0);
	}

	vdc->vdisk_label = VD_DISK_LABEL_VTOC;

	/*
	 * find the slice that represents the entire "disk" and use that to
	 * read the disk label. The convention in Solaris is that slice 2
	 * represents the whole disk so we check that it is, otherwise we
	 * default to slice 0
	 */
	if ((vdc->vdisk_type == VD_DISK_TYPE_DISK) &&
	    (vdc->vtoc->v_part[2].p_tag == V_BACKUP)) {
		slice = 2;
	} else {
		slice = 0;
	}

	/*
	 * Read disk label from start of disk
	 */
	vdc->label = kmem_zalloc(DK_LABEL_SIZE, KM_SLEEP);
	buf = kmem_alloc(sizeof (buf_t), KM_SLEEP);
	bioinit(buf);
	buf->b_un.b_addr = (caddr_t)vdc->label;
	buf->b_bcount = DK_LABEL_SIZE;
	buf->b_flags = B_BUSY | B_READ;
	buf->b_dev = dev;
	rv = vdc_populate_descriptor(vdc, (caddr_t)buf, DK_LABEL_SIZE,
			VD_OP_BREAD, 0, slice);
	rv = biowait(buf);
	biofini(buf);
	kmem_free(buf, sizeof (buf_t));

	return (rv);
}

/*
 * Function:
 *	vdc_setup_devid()
 *
 * Description:
 *	This routine discovers the devid of a vDisk. It requests the devid of
 *	the underlying device from the vDisk server, builds an encapsulated
 *	devid based on the retrieved devid and registers that new devid to
 *	the vDisk.
 *
 * Arguments:
 *	vdc	- soft state pointer for this instance of the device driver.
 *
 * Return Code:
 *	0	- A devid was succesfully registered for the vDisk
 */
static int
vdc_setup_devid(vdc_t *vdc)
{
	int rv;
	vd_devid_t *vd_devid;
	size_t bufsize, bufid_len;

	/*
	 * At first sight, we don't know the size of the devid that the
	 * server will return but this size will be encoded into the
	 * reply. So we do a first request using a default size then we
	 * check if this size was large enough. If not then we do a second
	 * request with the correct size returned by the server. Note that
	 * ldc requires size to be 8-byte aligned.
	 */
	bufsize = P2ROUNDUP(VD_DEVID_SIZE(VD_DEVID_DEFAULT_LEN),
	    sizeof (uint64_t));
	vd_devid = kmem_zalloc(bufsize, KM_SLEEP);
	bufid_len = bufsize - sizeof (vd_efi_t) - 1;

	rv = vdc_populate_descriptor(vdc, (caddr_t)vd_devid, bufsize,
	    VD_OP_GET_DEVID, 0, 0);
	if (rv) {
		kmem_free(vd_devid, bufsize);
		return (rv);
	}

	if (vd_devid->length > bufid_len) {
		/*
		 * The returned devid is larger than the buffer used. Try again
		 * with a buffer with the right size.
		 */
		kmem_free(vd_devid, bufsize);
		bufsize = P2ROUNDUP(VD_DEVID_SIZE(vd_devid->length),
		    sizeof (uint64_t));
		vd_devid = kmem_zalloc(bufsize, KM_SLEEP);
		bufid_len = bufsize - sizeof (vd_efi_t) - 1;

		rv = vdc_populate_descriptor(vdc, (caddr_t)vd_devid, bufsize,
		    VD_OP_GET_DEVID, 0, 0);
		if (rv) {
			kmem_free(vd_devid, bufsize);
			return (rv);
		}
	}

	/*
	 * The virtual disk should have the same device id as the one associated
	 * with the physical disk it is mapped on, otherwise sharing a disk
	 * between a LDom and a non-LDom may not work (for example for a shared
	 * SVM disk set).
	 *
	 * The DDI framework does not allow creating a device id with any
	 * type so we first create a device id of type DEVID_ENCAP and then
	 * we restore the orignal type of the physical device.
	 */

	/* build an encapsulated devid based on the returned devid */
	if (ddi_devid_init(vdc->dip, DEVID_ENCAP, vd_devid->length,
		vd_devid->id, &vdc->devid) != DDI_SUCCESS) {
		DMSG(1, "[%d] Fail to created devid\n", vdc->instance);
		kmem_free(vd_devid, bufsize);
		return (1);
	}

	DEVID_FORMTYPE((impl_devid_t *)vdc->devid, vd_devid->type);

	ASSERT(ddi_devid_valid(vdc->devid) == DDI_SUCCESS);

	kmem_free(vd_devid, bufsize);

	if (ddi_devid_register(vdc->dip, vdc->devid) != DDI_SUCCESS) {
		DMSG(1, "[%d] Fail to register devid\n", vdc->instance);
		return (1);
	}

	return (0);
}

static void
vdc_store_efi(vdc_t *vdc, struct dk_gpt *efi)
{
	struct vtoc *vtoc = vdc->vtoc;

	vd_efi_to_vtoc(efi, vtoc);
	if (vdc->vdisk_type == VD_DISK_TYPE_SLICE) {
		/*
		 * vd_efi_to_vtoc() will store information about the EFI Sun
		 * reserved partition (representing the entire disk) into
		 * partition 7. However single-slice device will only have
		 * that single partition and the vdc driver expects to find
		 * information about that partition in slice 0. So we need
		 * to copy information from slice 7 to slice 0.
		 */
		vtoc->v_part[0].p_tag = vtoc->v_part[VD_EFI_WD_SLICE].p_tag;
		vtoc->v_part[0].p_flag = vtoc->v_part[VD_EFI_WD_SLICE].p_flag;
		vtoc->v_part[0].p_start = vtoc->v_part[VD_EFI_WD_SLICE].p_start;
		vtoc->v_part[0].p_size =  vtoc->v_part[VD_EFI_WD_SLICE].p_size;
	}
}
