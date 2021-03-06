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

#pragma ident	"@(#)scsi_subr.c	1.88	06/07/19 SMI"

#include <sys/scsi/scsi.h>

/*
 * Utility SCSI routines
 */

/*
 * Polling support routines
 */

extern uintptr_t scsi_callback_id;

/*
 * Common buffer for scsi_log
 */

extern kmutex_t scsi_log_mutex;
static char scsi_log_buffer[MAXPATHLEN + 1];


#define	A_TO_TRAN(ap)	(ap->a_hba_tran)
#define	P_TO_TRAN(pkt)	((pkt)->pkt_address.a_hba_tran)
#define	P_TO_ADDR(pkt)	(&((pkt)->pkt_address))

#define	CSEC		10000			/* usecs */
#define	SEC_TO_CSEC	(1000000/CSEC)

extern ddi_dma_attr_t scsi_alloc_attr;

/*PRINTFLIKE4*/
static void impl_scsi_log(dev_info_t *dev, char *label, uint_t level,
    const char *fmt, ...) __KPRINTFLIKE(4);
/*PRINTFLIKE4*/
static void v_scsi_log(dev_info_t *dev, char *label, uint_t level,
    const char *fmt, va_list ap) __KVPRINTFLIKE(4);

static int
scsi_get_next_descr(uint8_t *sdsp,
    int sense_buf_len, struct scsi_descr_template **descrpp);

#define	DESCR_GOOD	0
#define	DESCR_PARTIAL	1
#define	DESCR_END	2

static int
scsi_validate_descr(struct scsi_descr_sense_hdr *sdsp,
    int valid_sense_length, struct scsi_descr_template *descrp);

int
scsi_poll(struct scsi_pkt *pkt)
{
	register int busy_count, rval = -1, savef;
	long savet;
	void (*savec)();
	extern int do_polled_io;

	/*
	 * save old flags..
	 */
	savef = pkt->pkt_flags;
	savec = pkt->pkt_comp;
	savet = pkt->pkt_time;

	pkt->pkt_flags |= FLAG_NOINTR;

	/*
	 * XXX there is nothing in the SCSA spec that states that we should not
	 * do a callback for polled cmds; however, removing this will break sd
	 * and probably other target drivers
	 */
	pkt->pkt_comp = 0;

	/*
	 * we don't like a polled command without timeout.
	 * 60 seconds seems long enough.
	 */
	if (pkt->pkt_time == 0)
		pkt->pkt_time = SCSI_POLL_TIMEOUT;

	/*
	 * Send polled cmd.
	 *
	 * We do some error recovery for various errors.  Tran_busy,
	 * queue full, and non-dispatched commands are retried every 10 msec.
	 * as they are typically transient failures.  Busy status is retried
	 * every second as this status takes a while to change.
	 */
	for (busy_count = 0; busy_count < (pkt->pkt_time * SEC_TO_CSEC);
		busy_count++) {
		int rc;
		int poll_delay;

		/*
		 * Initialize pkt status variables.
		 */
		*pkt->pkt_scbp = pkt->pkt_reason = pkt->pkt_state = 0;

		if ((rc = scsi_transport(pkt)) != TRAN_ACCEPT) {
			if (rc != TRAN_BUSY) {
				/* Transport failed - give up. */
				break;
			} else {
				/* Transport busy - try again. */
				poll_delay = 1 *CSEC;		/* 10 msec. */
			}
		} else {
			/*
			 * Transport accepted - check pkt status.
			 */
			rc = (*pkt->pkt_scbp) & STATUS_MASK;

			if (pkt->pkt_reason == CMD_CMPLT &&
			    rc == STATUS_GOOD) {
				/* No error - we're done */
				rval = 0;
				break;

			} else if (pkt->pkt_reason == CMD_INCOMPLETE &&
			    pkt->pkt_state == 0) {
				/* Pkt not dispatched - try again. */
				poll_delay = 1 *CSEC;		/* 10 msec. */

			} else if (pkt->pkt_reason == CMD_CMPLT &&
			    rc == STATUS_QFULL) {
				/* Queue full - try again. */
				poll_delay = 1 *CSEC;		/* 10 msec. */

			} else if (pkt->pkt_reason == CMD_CMPLT &&
			    rc == STATUS_BUSY) {
				/* Busy - try again. */
				poll_delay = 100 *CSEC;		/* 1 sec. */
				busy_count += (SEC_TO_CSEC - 1);

			} else {
				/* BAD status - give up. */
				break;
			}
		}

		if ((curthread->t_flag & T_INTR_THREAD) == 0 &&
		    !do_polled_io) {
			delay(drv_usectohz(poll_delay));
		} else {
			/* we busy wait during cpr_dump or interrupt threads */
			drv_usecwait(poll_delay);
		}
	}

	pkt->pkt_flags = savef;
	pkt->pkt_comp = savec;
	pkt->pkt_time = savet;
	return (rval);
}

/*
 * Command packaging routines.
 *
 * makecom_g*() are original routines and scsi_setup_cdb()
 * is the new and preferred routine.
 */

/*
 * These routines put LUN information in CDB byte 1 bits 7-5.
 * This was required in SCSI-1. SCSI-2 allowed it but it preferred
 * sending LUN information as part of IDENTIFY message.
 * This is not allowed in SCSI-3.
 */

void
makecom_g0(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int addr, int cnt)
{
	MAKECOM_G0(pkt, devp, flag, cmd, addr, (uchar_t)cnt);
}

void
makecom_g0_s(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int cnt, int fixbit)
{
	MAKECOM_G0_S(pkt, devp, flag, cmd, cnt, (uchar_t)fixbit);
}

void
makecom_g1(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int addr, int cnt)
{
	MAKECOM_G1(pkt, devp, flag, cmd, addr, cnt);
}

void
makecom_g5(struct scsi_pkt *pkt, struct scsi_device *devp,
    int flag, int cmd, int addr, int cnt)
{
	MAKECOM_G5(pkt, devp, flag, cmd, addr, cnt);
}

/*
 * Following routine does not put LUN information in CDB.
 * This interface must be used for SCSI-2 targets having
 * more than 8 LUNs or a SCSI-3 target.
 */
int
scsi_setup_cdb(union scsi_cdb *cdbp, uchar_t cmd, uint_t addr, uint_t cnt,
    uint_t addtl_cdb_data)
{
	uint_t	addr_cnt;

	cdbp->scc_cmd = cmd;

	switch (CDB_GROUPID(cmd)) {
		case CDB_GROUPID_0:
			/*
			 * The following calculation is to take care of
			 * the fact that format of some 6 bytes tape
			 * command is different (compare 6 bytes disk and
			 * tape read commands).
			 */
			addr_cnt = (addr << 8) + cnt;
			addr = (addr_cnt & 0x1fffff00) >> 8;
			cnt = addr_cnt & 0xff;
			FORMG0ADDR(cdbp, addr);
			FORMG0COUNT(cdbp, cnt);
			break;

		case CDB_GROUPID_1:
		case CDB_GROUPID_2:
			FORMG1ADDR(cdbp, addr);
			FORMG1COUNT(cdbp, cnt);
			break;

		case CDB_GROUPID_4:
			FORMG4ADDR(cdbp, addr);
			FORMG4COUNT(cdbp, cnt);
			FORMG4ADDTL(cdbp, addtl_cdb_data);
			break;

		case CDB_GROUPID_5:
			FORMG5ADDR(cdbp, addr);
			FORMG5COUNT(cdbp, cnt);
			break;

		default:
			return (0);
	}

	return (1);
}


/*
 * Common iopbmap data area packet allocation routines
 */

struct scsi_pkt *
get_pktiopb(struct scsi_address *ap, caddr_t *datap, int cdblen, int statuslen,
    int datalen, int readflag, int (*func)())
{
	scsi_hba_tran_t	*tran = A_TO_TRAN(ap);
	dev_info_t	*pdip = tran->tran_hba_dip;
	struct scsi_pkt	*pkt = NULL;
	struct buf	local;
	size_t		rlen;

	if (!datap)
		return (pkt);
	*datap = (caddr_t)0;
	bzero((caddr_t)&local, sizeof (struct buf));

	/*
	 * use i_ddi_mem_alloc() for now until we have an interface to allocate
	 * memory for DMA which doesn't require a DMA handle. ddi_iopb_alloc()
	 * is obsolete and we want more flexibility in controlling the DMA
	 * address constraints.
	 */
	if (i_ddi_mem_alloc(pdip, &scsi_alloc_attr, datalen,
	    ((func == SLEEP_FUNC) ? 1 : 0), 0, NULL, &local.b_un.b_addr, &rlen,
	    NULL) != DDI_SUCCESS) {
		return (pkt);
	}
	if (readflag)
		local.b_flags = B_READ;
	local.b_bcount = datalen;
	pkt = (*tran->tran_init_pkt) (ap, NULL, &local,
		cdblen, statuslen, 0, PKT_CONSISTENT,
		(func == SLEEP_FUNC) ? SLEEP_FUNC : NULL_FUNC,
		NULL);
	if (!pkt) {
		i_ddi_mem_free(local.b_un.b_addr, NULL);
		if (func != NULL_FUNC) {
			ddi_set_callback(func, NULL, &scsi_callback_id);
		}
	} else {
		*datap = local.b_un.b_addr;
	}
	return (pkt);
}

/*
 *  Equivalent deallocation wrapper
 */

void
free_pktiopb(struct scsi_pkt *pkt, caddr_t datap, int datalen)
{
	register struct scsi_address	*ap = P_TO_ADDR(pkt);
	register scsi_hba_tran_t	*tran = A_TO_TRAN(ap);

	(*tran->tran_destroy_pkt)(ap, pkt);
	if (datap && datalen) {
		i_ddi_mem_free(datap, NULL);
	}
	if (scsi_callback_id != 0) {
		ddi_run_callback(&scsi_callback_id);
	}
}

/*
 * Common naming functions
 */

static char scsi_tmpname[64];

char *
scsi_dname(int dtyp)
{
	static char *dnames[] = {
		"Direct Access",
		"Sequential Access",
		"Printer",
		"Processor",
		"Write-Once/Read-Many",
		"Read-Only Direct Access",
		"Scanner",
		"Optical",
		"Changer",
		"Communications",
		"Array Controller"
	};

	if ((dtyp & DTYPE_MASK) <= DTYPE_COMM) {
		return (dnames[dtyp&DTYPE_MASK]);
	} else if (dtyp == DTYPE_NOTPRESENT) {
		return ("Not Present");
	}
	return ("<unknown device type>");

}

char *
scsi_rname(uchar_t reason)
{
	static char *rnames[] = {
		"cmplt",
		"incomplete",
		"dma_derr",
		"tran_err",
		"reset",
		"aborted",
		"timeout",
		"data_ovr",
		"cmd_ovr",
		"sts_ovr",
		"badmsg",
		"nomsgout",
		"xid_fail",
		"ide_fail",
		"abort_fail",
		"reject_fail",
		"nop_fail",
		"per_fail",
		"bdr_fail",
		"id_fail",
		"unexpected_bus_free",
		"tag reject",
		"terminated"
	};
	if (reason > CMD_TAG_REJECT) {
		return ("<unknown reason>");
	} else {
		return (rnames[reason]);
	}
}

char *
scsi_mname(uchar_t msg)
{
	static char *imsgs[23] = {
		"COMMAND COMPLETE",
		"EXTENDED",
		"SAVE DATA POINTER",
		"RESTORE POINTERS",
		"DISCONNECT",
		"INITIATOR DETECTED ERROR",
		"ABORT",
		"REJECT",
		"NO-OP",
		"MESSAGE PARITY",
		"LINKED COMMAND COMPLETE",
		"LINKED COMMAND COMPLETE (W/FLAG)",
		"BUS DEVICE RESET",
		"ABORT TAG",
		"CLEAR QUEUE",
		"INITIATE RECOVERY",
		"RELEASE RECOVERY",
		"TERMINATE PROCESS",
		"CONTINUE TASK",
		"TARGET TRANSFER DISABLE",
		"RESERVED (0x14)",
		"RESERVED (0x15)",
		"CLEAR ACA"
	};
	static char *imsgs_2[6] = {
		"SIMPLE QUEUE TAG",
		"HEAD OF QUEUE TAG",
		"ORDERED QUEUE TAG",
		"IGNORE WIDE RESIDUE",
		"ACA",
		"LOGICAL UNIT RESET"
	};

	if (msg < 23) {
		return (imsgs[msg]);
	} else if (IS_IDENTIFY_MSG(msg)) {
		return ("IDENTIFY");
	} else if (IS_2BYTE_MSG(msg) &&
	    (int)((msg) & 0xF) < (sizeof (imsgs_2) / sizeof (char *))) {
		return (imsgs_2[msg & 0xF]);
	} else {
		return ("<unknown msg>");
	}

}

char *
scsi_cname(uchar_t cmd, register char **cmdvec)
{
	while (*cmdvec != (char *)0) {
		if (cmd == **cmdvec) {
			return ((char *)((long)(*cmdvec)+1));
		}
		cmdvec++;
	}
	return (sprintf(scsi_tmpname, "<undecoded cmd 0x%x>", cmd));
}

char *
scsi_cmd_name(uchar_t cmd, struct scsi_key_strings *cmdlist, char *tmpstr)
{
	int i = 0;

	while (cmdlist[i].key !=  -1) {
		if (cmd == cmdlist[i].key) {
			return ((char *)cmdlist[i].message);
		}
		i++;
	}
	return (sprintf(tmpstr, "<undecoded cmd 0x%x>", cmd));
}

static struct scsi_asq_key_strings extended_sense_list[] = {
	0x00, 0x00, "no additional sense info",
	0x00, 0x01, "filemark detected",
	0x00, 0x02, "end of partition/medium detected",
	0x00, 0x03, "setmark detected",
	0x00, 0x04, "begining of partition/medium detected",
	0x00, 0x05, "end of data detected",
	0x00, 0x06, "i/o process terminated",
	0x00, 0x11, "audio play operation in progress",
	0x00, 0x12, "audio play operation paused",
	0x00, 0x13, "audio play operation successfully completed",
	0x00, 0x14, "audio play operation stopped due to error",
	0x00, 0x15, "no current audio status to return",
	0x00, 0x16, "operation in progress",
	0x00, 0x17, "cleaning requested",
	0x00, 0x18, "erase operation in progress",
	0x00, 0x19, "locate operation in progress",
	0x00, 0x1A, "rewind operation in progress",
	0x00, 0x1B, "set capacity operation in progress",
	0x00, 0x1C, "verify operation in progress",
	0x01, 0x00, "no index/sector signal",
	0x02, 0x00, "no seek complete",
	0x03, 0x00, "peripheral device write fault",
	0x03, 0x01, "no write current",
	0x03, 0x02, "excessive write errors",
	0x04, 0x00, "LUN not ready",
	0x04, 0x01, "LUN is becoming ready",
	0x04, 0x02, "LUN initializing command required",
	0x04, 0x03, "LUN not ready intervention required",
	0x04, 0x04, "LUN not ready format in progress",
	0x04, 0x05, "LUN not ready, rebuild in progress",
	0x04, 0x06, "LUN not ready, recalculation in progress",
	0x04, 0x07, "LUN not ready, operation in progress",
	0x04, 0x08, "LUN not ready, long write in progress",
	0x04, 0x09, "LUN not ready, self-test in progress",
	0x04, 0x0A, "LUN not accessible, asymmetric access state transition",
	0x04, 0x0B, "LUN not accessible, target port in standby state",
	0x04, 0x0C, "LUN not accessible, target port in unavailable state",
	0x04, 0x10, "LUN not ready, auxiliary memory not accessible",
	0x05, 0x00, "LUN does not respond to selection",
	0x06, 0x00, "reference position found",
	0x07, 0x00, "multiple peripheral devices selected",
	0x08, 0x00, "LUN communication failure",
	0x08, 0x01, "LUN communication time-out",
	0x08, 0x02, "LUN communication parity error",
	0x08, 0x03, "LUN communication crc error (ultra-DMA/32)",
	0x08, 0x04, "unreachable copy target",
	0x09, 0x00, "track following error",
	0x09, 0x01, "tracking servo failure",
	0x09, 0x02, "focus servo failure",
	0x09, 0x03, "spindle servo failure",
	0x09, 0x04, "head select fault",
	0x0a, 0x00, "error log overflow",
	0x0b, 0x00, "warning",
	0x0b, 0x01, "warning - specified temperature exceeded",
	0x0b, 0x02, "warning - enclosure degraded",
	0x0c, 0x00, "write error",
	0x0c, 0x01, "write error - recovered with auto reallocation",
	0x0c, 0x02, "write error - auto reallocation failed",
	0x0c, 0x03, "write error - recommend reassignment",
	0x0c, 0x04, "compression check miscompare error",
	0x0c, 0x05, "data expansion occurred during compression",
	0x0c, 0x06, "block not compressible",
	0x0c, 0x07, "write error - recovery needed",
	0x0c, 0x08, "write error - recovery failed",
	0x0c, 0x09, "write error - loss of streaming",
	0x0c, 0x0a, "write error - padding blocks added",
	0x0c, 0x0b, "auxiliary memory write error",
	0x0c, 0x0c, "write error - unexpected unsolicited data",
	0x0c, 0x0d, "write error - not enough unsolicited data",
	0x0d, 0x00, "error detected by third party temporary initiator",
	0x0d, 0x01, "third party device failure",
	0x0d, 0x02, "copy target device not reachable",
	0x0d, 0x03, "incorrect copy target device type",
	0x0d, 0x04, "copy target device data underrun",
	0x0d, 0x05, "copy target device data overrun",
	0x0e, 0x00, "invalid information unit",
	0x0e, 0x01, "information unit too short",
	0x0e, 0x02, "information unit too long",
	0x10, 0x00, "ID CRC or ECC error",
	0x11, 0x00, "unrecovered read error",
	0x11, 0x01, "read retries exhausted",
	0x11, 0x02, "error too long to correct",
	0x11, 0x03, "multiple read errors",
	0x11, 0x04, "unrecovered read error - auto reallocate failed",
	0x11, 0x05, "L-EC uncorrectable error",
	0x11, 0x06, "CIRC unrecovered error",
	0x11, 0x07, "data re-synchronization error",
	0x11, 0x08, "incomplete block read",
	0x11, 0x09, "no gap found",
	0x11, 0x0a, "miscorrected error",
	0x11, 0x0b, "unrecovered read error - recommend reassignment",
	0x11, 0x0c, "unrecovered read error - recommend rewrite the data",
	0x11, 0x0d, "de-compression crc error",
	0x11, 0x0e, "cannot decompress using declared algorithm",
	0x11, 0x0f, "error reading UPC/EAN number",
	0x11, 0x10, "error reading ISRC number",
	0x11, 0x11, "read error - loss of streaming",
	0x11, 0x12, "auxiliary memory read error",
	0x11, 0x13, "read error - failed retransmission request",
	0x12, 0x00, "address mark not found for ID field",
	0x13, 0x00, "address mark not found for data field",
	0x14, 0x00, "recorded entity not found",
	0x14, 0x01, "record not found",
	0x14, 0x02, "filemark or setmark not found",
	0x14, 0x03, "end-of-data not found",
	0x14, 0x04, "block sequence error",
	0x14, 0x05, "record not found - recommend reassignment",
	0x14, 0x06, "record not found - data auto-reallocated",
	0x14, 0x07, "locate operation failure",
	0x15, 0x00, "random positioning error",
	0x15, 0x01, "mechanical positioning error",
	0x15, 0x02, "positioning error detected by read of medium",
	0x16, 0x00, "data sync mark error",
	0x16, 0x01, "data sync error - data rewritten",
	0x16, 0x02, "data sync error - recommend rewrite",
	0x16, 0x03, "data sync error - data auto-reallocated",
	0x16, 0x04, "data sync error - recommend reassignment",
	0x17, 0x00, "recovered data with no error correction",
	0x17, 0x01, "recovered data with retries",
	0x17, 0x02, "recovered data with positive head offset",
	0x17, 0x03, "recovered data with negative head offset",
	0x17, 0x04, "recovered data with retries and/or CIRC applied",
	0x17, 0x05, "recovered data using previous sector id",
	0x17, 0x06, "recovered data without ECC - data auto-reallocated",
	0x17, 0x07, "recovered data without ECC - recommend reassignment",
	0x17, 0x08, "recovered data without ECC - recommend rewrite",
	0x17, 0x09, "recovered data without ECC - data rewritten",
	0x18, 0x00, "recovered data with error correction",
	0x18, 0x01, "recovered data with error corr. & retries applied",
	0x18, 0x02, "recovered data - data auto-reallocated",
	0x18, 0x03, "recovered data with CIRC",
	0x18, 0x04, "recovered data with L-EC",
	0x18, 0x05, "recovered data - recommend reassignment",
	0x18, 0x06, "recovered data - recommend rewrite",
	0x18, 0x07, "recovered data with ECC - data rewritten",
	0x18, 0x08, "recovered data with linking",
	0x19, 0x00, "defect list error",
	0x1a, 0x00, "parameter list length error",
	0x1b, 0x00, "synchronous data xfer error",
	0x1c, 0x00, "defect list not found",
	0x1c, 0x01, "primary defect list not found",
	0x1c, 0x02, "grown defect list not found",
	0x1d, 0x00, "miscompare during verify",
	0x1e, 0x00, "recovered ID with ECC",
	0x1f, 0x00, "partial defect list transfer",
	0x20, 0x00, "invalid command operation code",
	0x20, 0x01, "access denied - initiator pending-enrolled",
	0x20, 0x02, "access denied - no access rights",
	0x20, 0x03, "access denied - invalid mgmt id key",
	0x20, 0x04, "illegal command while in write capable state",
	0x20, 0x06, "illegal command while in explicit address mode",
	0x20, 0x07, "illegal command while in implicit address mode",
	0x20, 0x08, "access denied - enrollment conflict",
	0x20, 0x09, "access denied - invalid lu identifier",
	0x20, 0x0a, "access denied - invalid proxy token",
	0x20, 0x0b, "access denied - ACL LUN conflict",
	0x21, 0x00, "logical block address out of range",
	0x21, 0x01, "invalid element address",
	0x21, 0x02, "invalid address for write",
	0x22, 0x00, "illegal function",
	0x24, 0x00, "invalid field in cdb",
	0x24, 0x01, "cdb decryption error",
	0x25, 0x00, "LUN not supported",
	0x26, 0x00, "invalid field in param list",
	0x26, 0x01, "parameter not supported",
	0x26, 0x02, "parameter value invalid",
	0x26, 0x03, "threshold parameters not supported",
	0x26, 0x04, "invalid release of persistent reservation",
	0x26, 0x05, "data decryption error",
	0x26, 0x06, "too many target descriptors",
	0x26, 0x07, "unsupported target descriptor type code",
	0x26, 0x08, "too many segment descriptors",
	0x26, 0x09, "unsupported segment descriptor type code",
	0x26, 0x0a, "unexpected inexact segment",
	0x26, 0x0b, "inline data length exceeded",
	0x26, 0x0c, "invalid operation for copy source or destination",
	0x26, 0x0d, "copy segment granularity violation",
	0x27, 0x00, "write protected",
	0x27, 0x01, "hardware write protected",
	0x27, 0x02, "LUN software write protected",
	0x27, 0x03, "associated write protect",
	0x27, 0x04, "persistent write protect",
	0x27, 0x05, "permanent write protect",
	0x27, 0x06, "conditional write protect",
	0x27, 0x80, "unable to overwrite data",
	0x28, 0x00, "medium may have changed",
	0x28, 0x01, "import or export element accessed",
	0x29, 0x00, "power on, reset, or bus reset occurred",
	0x29, 0x01, "power on occurred",
	0x29, 0x02, "scsi bus reset occurred",
	0x29, 0x03, "bus device reset message occurred",
	0x29, 0x04, "device internal reset",
	0x29, 0x05, "transceiver mode changed to single-ended",
	0x29, 0x06, "transceiver mode changed to LVD",
	0x29, 0x07, "i_t nexus loss occurred",
	0x2a, 0x00, "parameters changed",
	0x2a, 0x01, "mode parameters changed",
	0x2a, 0x02, "log parameters changed",
	0x2a, 0x03, "reservations preempted",
	0x2a, 0x04, "reservations released",
	0x2a, 0x05, "registrations preempted",
	0x2a, 0x06, "asymmetric access state changed",
	0x2a, 0x07, "implicit asymmetric access state transition failed",
	0x2b, 0x00, "copy cannot execute since host cannot disconnect",
	0x2c, 0x00, "command sequence error",
	0x2c, 0x03, "current program area is not empty",
	0x2c, 0x04, "current program area is empty",
	0x2c, 0x06, "persistent prevent conflict",
	0x2c, 0x07, "previous busy status",
	0x2c, 0x08, "previous task set full status",
	0x2c, 0x09, "previous reservation conflict status",
	0x2d, 0x00, "overwrite error on update in place",
	0x2e, 0x00, "insufficient time for operation",
	0x2f, 0x00, "commands cleared by another initiator",
	0x30, 0x00, "incompatible medium installed",
	0x30, 0x01, "cannot read medium - unknown format",
	0x30, 0x02, "cannot read medium - incompatible format",
	0x30, 0x03, "cleaning cartridge installed",
	0x30, 0x04, "cannot write medium - unknown format",
	0x30, 0x05, "cannot write medium - incompatible format",
	0x30, 0x06, "cannot format medium - incompatible medium",
	0x30, 0x07, "cleaning failure",
	0x30, 0x08, "cannot write - application code mismatch",
	0x30, 0x09, "current session not fixated for append",
	0x30, 0x0b, "WORM medium - Overwrite attempted",
	0x30, 0x0c, "WORM medium - Cannot Erase",
	0x30, 0x0d, "WORM medium - Integrity Check",
	0x30, 0x10, "medium not formatted",
	0x31, 0x00, "medium format corrupted",
	0x31, 0x01, "format command failed",
	0x31, 0x02, "zoned formatting failed due to spare linking",
	0x31, 0x94, "WORM media corrupted",
	0x32, 0x00, "no defect spare location available",
	0x32, 0x01, "defect list update failure",
	0x33, 0x00, "tape length error",
	0x34, 0x00, "enclosure failure",
	0x35, 0x00, "enclosure services failure",
	0x35, 0x01, "unsupported enclosure function",
	0x35, 0x02, "enclosure services unavailable",
	0x35, 0x03, "enclosure services transfer failure",
	0x35, 0x04, "enclosure services transfer refused",
	0x36, 0x00, "ribbon, ink, or toner failure",
	0x37, 0x00, "rounded parameter",
	0x39, 0x00, "saving parameters not supported",
	0x3a, 0x00, "medium not present",
	0x3a, 0x01, "medium not present - tray closed",
	0x3a, 0x02, "medium not present - tray open",
	0x3a, 0x03, "medium not present - loadable",
	0x3a, 0x04, "medium not present - medium auxiliary memory accessible",
	0x3b, 0x00, "sequential positioning error",
	0x3b, 0x01, "tape position error at beginning-of-medium",
	0x3b, 0x02, "tape position error at end-of-medium",
	0x3b, 0x08, "reposition error",
	0x3b, 0x0c, "position past beginning of medium",
	0x3b, 0x0d, "medium destination element full",
	0x3b, 0x0e, "medium source element empty",
	0x3b, 0x0f, "end of medium reached",
	0x3b, 0x11, "medium magazine not accessible",
	0x3b, 0x12, "medium magazine removed",
	0x3b, 0x13, "medium magazine inserted",
	0x3b, 0x14, "medium magazine locked",
	0x3b, 0x15, "medium magazine unlocked",
	0x3b, 0x16, "mechanical positioning or changer error",
	0x3d, 0x00, "invalid bits in indentify message",
	0x3e, 0x00, "LUN has not self-configured yet",
	0x3e, 0x01, "LUN failure",
	0x3e, 0x02, "timeout on LUN",
	0x3e, 0x03, "LUN failed self-test",
	0x3e, 0x04, "LUN unable to update self-test log",
	0x3f, 0x00, "target operating conditions have changed",
	0x3f, 0x01, "microcode has been changed",
	0x3f, 0x02, "changed operating definition",
	0x3f, 0x03, "inquiry data has changed",
	0x3f, 0x04, "component device attached",
	0x3f, 0x05, "device identifier changed",
	0x3f, 0x06, "redundancy group created or modified",
	0x3f, 0x07, "redundancy group deleted",
	0x3f, 0x08, "spare created or modified",
	0x3f, 0x09, "spare deleted",
	0x3f, 0x0a, "volume set created or modified",
	0x3f, 0x0b, "volume set deleted",
	0x3f, 0x0c, "volume set deassigned",
	0x3f, 0x0d, "volume set reassigned",
	0x3f, 0x0e, "reported LUNs data has changed",
	0x3f, 0x0f, "echo buffer overwritten",
	0x3f, 0x10, "medium loadable",
	0x3f, 0x11, "medium auxiliary memory accessible",
	0x40, 0x00, "ram failure",
	0x41, 0x00, "data path failure",
	0x42, 0x00, "power-on or self-test failure",
	0x43, 0x00, "message error",
	0x44, 0x00, "internal target failure",
	0x45, 0x00, "select or reselect failure",
	0x46, 0x00, "unsuccessful soft reset",
	0x47, 0x00, "scsi parity error",
	0x47, 0x01, "data phase crc error detected",
	0x47, 0x02, "scsi parity error detected during st data phase",
	0x47, 0x03, "information unit iucrc error detected",
	0x47, 0x04, "asynchronous information protection error detected",
	0x47, 0x05, "protocol service crc error",
	0x47, 0x7f, "some commands cleared by iscsi protocol event",
	0x48, 0x00, "initiator detected error message received",
	0x49, 0x00, "invalid message error",
	0x4a, 0x00, "command phase error",
	0x4b, 0x00, "data phase error",
	0x4b, 0x01, "invalid target port transfer tag received",
	0x4b, 0x02, "too much write data",
	0x4b, 0x03, "ack/nak timeout",
	0x4b, 0x04, "nak received",
	0x4b, 0x05, "data offset error",
	0x4c, 0x00, "logical unit failed self-configuration",
	0x4d, 0x00, "tagged overlapped commands (ASCQ = queue tag)",
	0x4e, 0x00, "overlapped commands attempted",
	0x50, 0x00, "write append error",
	0x50, 0x01, "data protect write append error",
	0x50, 0x95, "data protect write append error",
	0x51, 0x00, "erase failure",
	0x52, 0x00, "cartridge fault",
	0x53, 0x00, "media load or eject failed",
	0x53, 0x01, "unload tape failure",
	0x53, 0x02, "medium removal prevented",
	0x54, 0x00, "scsi to host system interface failure",
	0x55, 0x00, "system resource failure",
	0x55, 0x01, "system buffer full",
	0x55, 0x02, "insufficient reservation resources",
	0x55, 0x03, "insufficient resources",
	0x55, 0x04, "insufficient registration resources",
	0x55, 0x05, "insufficient access control resources",
	0x55, 0x06, "auxiliary memory out of space",
	0x57, 0x00, "unable to recover TOC",
	0x58, 0x00, "generation does not exist",
	0x59, 0x00, "updated block read",
	0x5a, 0x00, "operator request or state change input",
	0x5a, 0x01, "operator medium removal request",
	0x5a, 0x02, "operator selected write protect",
	0x5a, 0x03, "operator selected write permit",
	0x5b, 0x00, "log exception",
	0x5b, 0x01, "threshold condition met",
	0x5b, 0x02, "log counter at maximum",
	0x5b, 0x03, "log list codes exhausted",
	0x5c, 0x00, "RPL status change",
	0x5c, 0x01, "spindles synchronized",
	0x5c, 0x02, "spindles not synchronized",
	0x5d, 0x00, "drive operation marginal, service immediately"
		    " (failure prediction threshold exceeded)",
	0x5d, 0x01, "media failure prediction threshold exceeded",
	0x5d, 0x02, "LUN failure prediction threshold exceeded",
	0x5d, 0x03, "spare area exhaustion prediction threshold exceeded",
	0x5d, 0x10, "hardware impending failure general hard drive failure",
	0x5d, 0x11, "hardware impending failure drive error rate too high",
	0x5d, 0x12, "hardware impending failure data error rate too high",
	0x5d, 0x13, "hardware impending failure seek error rate too high",
	0x5d, 0x14, "hardware impending failure too many block reassigns",
	0x5d, 0x15, "hardware impending failure access times too high",
	0x5d, 0x16, "hardware impending failure start unit times too high",
	0x5d, 0x17, "hardware impending failure channel parametrics",
	0x5d, 0x18, "hardware impending failure controller detected",
	0x5d, 0x19, "hardware impending failure throughput performance",
	0x5d, 0x1a, "hardware impending failure seek time performance",
	0x5d, 0x1b, "hardware impending failure spin-up retry count",
	0x5d, 0x1c, "hardware impending failure drive calibration retry count",
	0x5d, 0x20, "controller impending failure general hard drive failure",
	0x5d, 0x21, "controller impending failure drive error rate too high",
	0x5d, 0x22, "controller impending failure data error rate too high",
	0x5d, 0x23, "controller impending failure seek error rate too high",
	0x5d, 0x24, "controller impending failure too many block reassigns",
	0x5d, 0x25, "controller impending failure access times too high",
	0x5d, 0x26, "controller impending failure start unit times too high",
	0x5d, 0x27, "controller impending failure channel parametrics",
	0x5d, 0x28, "controller impending failure controller detected",
	0x5d, 0x29, "controller impending failure throughput performance",
	0x5d, 0x2a, "controller impending failure seek time performance",
	0x5d, 0x2b, "controller impending failure spin-up retry count",
	0x5d, 0x2c, "controller impending failure drive calibration retry cnt",
	0x5d, 0x30, "data channel impending failure general hard drive failure",
	0x5d, 0x31, "data channel impending failure drive error rate too high",
	0x5d, 0x32, "data channel impending failure data error rate too high",
	0x5d, 0x33, "data channel impending failure seek error rate too high",
	0x5d, 0x34, "data channel impending failure too many block reassigns",
	0x5d, 0x35, "data channel impending failure access times too high",
	0x5d, 0x36, "data channel impending failure start unit times too high",
	0x5d, 0x37, "data channel impending failure channel parametrics",
	0x5d, 0x38, "data channel impending failure controller detected",
	0x5d, 0x39, "data channel impending failure throughput performance",
	0x5d, 0x3a, "data channel impending failure seek time performance",
	0x5d, 0x3b, "data channel impending failure spin-up retry count",
	0x5d, 0x3c, "data channel impending failure drive calibrate retry cnt",
	0x5d, 0x40, "servo impending failure general hard drive failure",
	0x5d, 0x41, "servo impending failure drive error rate too high",
	0x5d, 0x42, "servo impending failure data error rate too high",
	0x5d, 0x43, "servo impending failure seek error rate too high",
	0x5d, 0x44, "servo impending failure too many block reassigns",
	0x5d, 0x45, "servo impending failure access times too high",
	0x5d, 0x46, "servo impending failure start unit times too high",
	0x5d, 0x47, "servo impending failure channel parametrics",
	0x5d, 0x48, "servo impending failure controller detected",
	0x5d, 0x49, "servo impending failure throughput performance",
	0x5d, 0x4a, "servo impending failure seek time performance",
	0x5d, 0x4b, "servo impending failure spin-up retry count",
	0x5d, 0x4c, "servo impending failure drive calibration retry count",
	0x5d, 0x50, "spindle impending failure general hard drive failure",
	0x5d, 0x51, "spindle impending failure drive error rate too high",
	0x5d, 0x52, "spindle impending failure data error rate too high",
	0x5d, 0x53, "spindle impending failure seek error rate too high",
	0x5d, 0x54, "spindle impending failure too many block reassigns",
	0x5d, 0x55, "spindle impending failure access times too high",
	0x5d, 0x56, "spindle impending failure start unit times too high",
	0x5d, 0x57, "spindle impending failure channel parametrics",
	0x5d, 0x58, "spindle impending failure controller detected",
	0x5d, 0x59, "spindle impending failure throughput performance",
	0x5d, 0x5a, "spindle impending failure seek time performance",
	0x5d, 0x5b, "spindle impending failure spin-up retry count",
	0x5d, 0x5c, "spindle impending failure drive calibration retry count",
	0x5d, 0x60, "firmware impending failure general hard drive failure",
	0x5d, 0x61, "firmware impending failure drive error rate too high",
	0x5d, 0x62, "firmware impending failure data error rate too high",
	0x5d, 0x63, "firmware impending failure seek error rate too high",
	0x5d, 0x64, "firmware impending failure too many block reassigns",
	0x5d, 0x65, "firmware impending failure access times too high",
	0x5d, 0x66, "firmware impending failure start unit times too high",
	0x5d, 0x67, "firmware impending failure channel parametrics",
	0x5d, 0x68, "firmware impending failure controller detected",
	0x5d, 0x69, "firmware impending failure throughput performance",
	0x5d, 0x6a, "firmware impending failure seek time performance",
	0x5d, 0x6b, "firmware impending failure spin-up retry count",
	0x5d, 0x6c, "firmware impending failure drive calibration retry count",
	0x5d, 0xff, "failure prediction threshold exceeded (false)",
	0x5e, 0x00, "low power condition active",
	0x5e, 0x01, "idle condition activated by timer",
	0x5e, 0x02, "standby condition activated by timer",
	0x5e, 0x03, "idle condition activated by command",
	0x5e, 0x04, "standby condition activated by command",
	0x60, 0x00, "lamp failure",
	0x61, 0x00, "video aquisition error",
	0x62, 0x00, "scan head positioning error",
	0x63, 0x00, "end of user area encountered on this track",
	0x63, 0x01, "packet does not fit in available space",
	0x64, 0x00, "illegal mode for this track",
	0x64, 0x01, "invalid packet size",
	0x65, 0x00, "voltage fault",
	0x66, 0x00, "automatic document feeder cover up",
	0x67, 0x00, "configuration failure",
	0x67, 0x01, "configuration of incapable LUNs failed",
	0x67, 0x02, "add LUN failed",
	0x67, 0x03, "modification of LUN failed",
	0x67, 0x04, "exchange of LUN failed",
	0x67, 0x05, "remove of LUN failed",
	0x67, 0x06, "attachment of LUN failed",
	0x67, 0x07, "creation of LUN failed",
	0x67, 0x08, "assign failure occurred",
	0x67, 0x09, "multiply assigned LUN",
	0x67, 0x0a, "set target port groups command failed",
	0x68, 0x00, "logical unit not configured",
	0x69, 0x00, "data loss on logical unit",
	0x69, 0x01, "multiple LUN failures",
	0x69, 0x02, "parity/data mismatch",
	0x6a, 0x00, "informational, refer to log",
	0x6b, 0x00, "state change has occured",
	0x6b, 0x01, "redundancy level got better",
	0x6b, 0x02, "redundancy level got worse",
	0x6c, 0x00, "rebuild failure occured",
	0x6d, 0x00, "recalculate failure occured",
	0x6e, 0x00, "command to logical unit failed",
	0x6f, 0x00, "copy protect key exchange failure authentication failure",
	0x6f, 0x01, "copy protect key exchange failure key not present",
	0x6f, 0x02, "copy protect key exchange failure key not established",
	0x6f, 0x03, "read of scrambled sector without authentication",
	0x6f, 0x04, "media region code is mismatched to LUN region",
	0x6f, 0x05, "drive region must be permanent/region reset count error",
	0x70, 0xffff, "decompression exception short algorithm id of ASCQ",
	0x71, 0x00, "decompression exception long algorithm id",
	0x72, 0x00, "session fixation error",
	0x72, 0x01, "session fixation error writing lead-in",
	0x72, 0x02, "session fixation error writing lead-out",
	0x72, 0x03, "session fixation error - incomplete track in session",
	0x72, 0x04, "empty or partially written reserved track",
	0x72, 0x05, "no more track reservations allowed",
	0x73, 0x00, "cd control error",
	0x73, 0x01, "power calibration area almost full",
	0x73, 0x02, "power calibration area is full",
	0x73, 0x03, "power calibration area error",
	0x73, 0x04, "program memory area update failure",
	0x73, 0x05, "program memory area is full",
	0x73, 0x06, "rma/pma is almost full",
	0xffff, 0xffff, NULL
};

char *
scsi_esname(uint_t key, char *tmpstr)
{
	int i = 0;

	while (extended_sense_list[i].asc != 0xffff) {
		if (key == extended_sense_list[i].asc) {
			return ((char *)extended_sense_list[i].message);
		}
		i++;
	}
	return (sprintf(tmpstr, "<vendor unique code 0x%x>", key));
}

char *
scsi_asc_name(uint_t asc, uint_t ascq, char *tmpstr)
{
	int i = 0;

	while (extended_sense_list[i].asc != 0xffff) {
		if ((asc == extended_sense_list[i].asc) &&
		    ((ascq == extended_sense_list[i].ascq) ||
		    (extended_sense_list[i].ascq == 0xffff))) {
			return ((char *)extended_sense_list[i].message);
		}
		i++;
	}
	return (sprintf(tmpstr, "<vendor unique code 0x%x>", asc));
}

char *
scsi_sname(uchar_t sense_key)
{
	if (sense_key >= (uchar_t)(NUM_SENSE_KEYS+NUM_IMPL_SENSE_KEYS)) {
		return ("<unknown sense key>");
	} else {
		return (sense_keys[sense_key]);
	}
}


/*
 * Print a piece of inquiry data- cleaned up for non-printable characters.
 */

static void
inq_fill(char *p, int l, char *s)
{
	register unsigned i = 0;
	char c;

	if (!p)
		return;

	while (i++ < l) {
		/* clean string of non-printing chars */
		if ((c = *p++) < ' ' || c >= 0177) {
			c = ' ';
		}
		*s++ = c;
	}
	*s++ = 0;
}

static char *
scsi_asc_search(uint_t asc, uint_t ascq,
    struct scsi_asq_key_strings *list)
{
	int i = 0;

	while (list[i].asc != 0xffff) {
		if ((asc == list[i].asc) &&
		    ((ascq == list[i].ascq) ||
		    (list[i].ascq == 0xffff))) {
			return ((char *)list[i].message);
		}
		i++;
	}
	return (NULL);
}

static char *
scsi_asc_ascq_name(uint_t asc, uint_t ascq, char *tmpstr,
	struct scsi_asq_key_strings *list)
{
	char *message;

	if (list) {
		if (message = scsi_asc_search(asc, ascq, list)) {
			return (message);
		}
	}
	if (message = scsi_asc_search(asc, ascq, extended_sense_list)) {
		return (message);
	}

	return (sprintf(tmpstr, "<vendor unique code 0x%x>", asc));
}

/*
 * The first part/column of the error message will be at least this length.
 * This number has been calculated so that each line fits in 80 chars.
 */
#define	SCSI_ERRMSG_COLUMN_LEN	42
#define	SCSI_ERRMSG_BUF_LEN	256

void
scsi_vu_errmsg(struct scsi_device *devp, struct scsi_pkt *pkt, char *label,
    int severity, daddr_t blkno, daddr_t err_blkno,
    struct scsi_key_strings *cmdlist, struct scsi_extended_sense *sensep,
    struct scsi_asq_key_strings *asc_list,
    char *(*decode_fru)(struct scsi_device *, char *, int, uchar_t))
{
	uchar_t com;
	static char buf[SCSI_ERRMSG_BUF_LEN];
	static char buf1[SCSI_ERRMSG_BUF_LEN];
	static char tmpbuf[64];
	static char pad[SCSI_ERRMSG_COLUMN_LEN];
	dev_info_t *dev = devp->sd_dev;
	static char *error_classes[] = {
		"All", "Unknown", "Informational",
		"Recovered", "Retryable", "Fatal"
	};
	uchar_t sense_key, asc, ascq, fru_code;
	uchar_t *fru_code_ptr;
	int i, buflen;

	mutex_enter(&scsi_log_mutex);

	/*
	 * We need to put our space padding code because kernel version
	 * of sprintf(9F) doesn't support %-<number>s type of left alignment.
	 */
	for (i = 0; i < SCSI_ERRMSG_COLUMN_LEN; i++) {
		pad[i] = ' ';
	}

	bzero(buf, 256);
	com = ((union scsi_cdb *)pkt->pkt_cdbp)->scc_cmd;
	(void) sprintf(buf, "Error for Command: %s",
	    scsi_cmd_name(com, cmdlist, tmpbuf));
	buflen = strlen(buf);
	if (buflen < SCSI_ERRMSG_COLUMN_LEN) {
		pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = '\0';
		(void) sprintf(&buf[buflen], "%s Error Level: %s",
		    pad, error_classes[severity]);
		pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = ' ';
	} else {
		(void) sprintf(&buf[buflen], " Error Level: %s",
		    error_classes[severity]);
	}
	impl_scsi_log(dev, label, CE_WARN, buf);

	if (blkno != -1 || err_blkno != -1 &&
	    ((com & 0xf) == SCMD_READ) || ((com & 0xf) == SCMD_WRITE)) {
		bzero(buf, 256);
		(void) sprintf(buf, "Requested Block: %ld", blkno);
		buflen = strlen(buf);
		if (buflen < SCSI_ERRMSG_COLUMN_LEN) {
			pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = '\0';
			(void) sprintf(&buf[buflen], "%s Error Block: %ld\n",
			    pad, err_blkno);
			pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = ' ';
		} else {
			(void) sprintf(&buf[buflen], " Error Block: %ld\n",
			    err_blkno);
		}
		impl_scsi_log(dev, label, CE_CONT, buf);
	}

	bzero(buf, 256);
	(void) strcpy(buf, "Vendor: ");
	inq_fill(devp->sd_inq->inq_vid, 8, &buf[strlen(buf)]);
	buflen = strlen(buf);
	if (buflen < SCSI_ERRMSG_COLUMN_LEN) {
		pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = '\0';
		(void) sprintf(&buf[strlen(buf)], "%s Serial Number: ", pad);
		pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = ' ';
	} else {
		(void) sprintf(&buf[strlen(buf)], " Serial Number: ");
	}
	inq_fill(devp->sd_inq->inq_serial, 12, &buf[strlen(buf)]);
	impl_scsi_log(dev, label, CE_CONT, "%s\n", buf);

	if (sensep) {
		sense_key = scsi_sense_key((uint8_t *)sensep);
		asc = scsi_sense_asc((uint8_t *)sensep);
		ascq = scsi_sense_ascq((uint8_t *)sensep);
		scsi_ext_sense_fields((uint8_t *)sensep, SENSE_LENGTH,
		    NULL, NULL, &fru_code_ptr, NULL, NULL);
		fru_code = (fru_code_ptr ? *fru_code_ptr : 0);

		bzero(buf, 256);
		(void) sprintf(buf, "Sense Key: %s\n",
		    sense_keys[sense_key]);
		impl_scsi_log(dev, label, CE_CONT, buf);

		bzero(buf, 256);
		if ((fru_code != 0) &&
		    (decode_fru != NULL)) {
			(*decode_fru)(devp, buf, SCSI_ERRMSG_BUF_LEN,
			    fru_code);
			if (buf[0] != NULL) {
				bzero(buf1, 256);
				(void) sprintf(&buf1[strlen(buf1)],
				    "ASC: 0x%x (%s)", asc,
				    scsi_asc_ascq_name(asc, ascq,
					tmpbuf, asc_list));
				buflen = strlen(buf1);
				if (buflen < SCSI_ERRMSG_COLUMN_LEN) {
				    pad[SCSI_ERRMSG_COLUMN_LEN - buflen] = '\0';
				    (void) sprintf(&buf1[buflen],
				    "%s ASCQ: 0x%x", pad, ascq);
				} else {
				    (void) sprintf(&buf1[buflen], " ASCQ: 0x%x",
					ascq);
				}
				impl_scsi_log(dev,
					label, CE_CONT, "%s\n", buf1);
				impl_scsi_log(dev, label, CE_CONT,
					"FRU: 0x%x (%s)\n",
						fru_code, buf);
				mutex_exit(&scsi_log_mutex);
				return;
			}
		}
		(void) sprintf(&buf[strlen(buf)],
		    "ASC: 0x%x (%s), ASCQ: 0x%x, FRU: 0x%x",
		    asc, scsi_asc_ascq_name(asc, ascq, tmpbuf, asc_list),
		    ascq, fru_code);
		impl_scsi_log(dev, label, CE_CONT, "%s\n", buf);
	}
	mutex_exit(&scsi_log_mutex);
}

void
scsi_errmsg(struct scsi_device *devp, struct scsi_pkt *pkt, char *label,
    int severity, daddr_t blkno, daddr_t err_blkno,
    struct scsi_key_strings *cmdlist, struct scsi_extended_sense *sensep)
{
	scsi_vu_errmsg(devp, pkt, label, severity, blkno,
		err_blkno, cmdlist, sensep, NULL, NULL);
}

/*PRINTFLIKE4*/
void
scsi_log(dev_info_t *dev, char *label, uint_t level,
    const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	mutex_enter(&scsi_log_mutex);
	v_scsi_log(dev, label, level, fmt, ap);
	mutex_exit(&scsi_log_mutex);
	va_end(ap);
}

/*PRINTFLIKE4*/
static void
impl_scsi_log(dev_info_t *dev, char *label, uint_t level,
    const char *fmt, ...)
{
	va_list ap;

	ASSERT(mutex_owned(&scsi_log_mutex));

	va_start(ap, fmt);
	v_scsi_log(dev, label, level, fmt, ap);
	va_end(ap);
}


char *ddi_pathname(dev_info_t *dip, char *path);

/*PRINTFLIKE4*/
static void
v_scsi_log(dev_info_t *dev, char *label, uint_t level,
    const char *fmt, va_list ap)
{
	static char name[256];
	int log_only = 0;
	int boot_only = 0;
	int console_only = 0;

	ASSERT(mutex_owned(&scsi_log_mutex));

	if (dev) {
		if (level == CE_PANIC || level == CE_WARN ||
		    level == CE_NOTE) {
			(void) sprintf(name, "%s (%s%d):\n",
				ddi_pathname(dev, scsi_log_buffer),
				label, ddi_get_instance(dev));
		} else if (level >= (uint_t)SCSI_DEBUG) {
			(void) sprintf(name,
			    "%s%d:", label, ddi_get_instance(dev));
		} else {
			name[0] = '\0';
		}
	} else {
		(void) sprintf(name, "%s:", label);
	}

	(void) vsprintf(scsi_log_buffer, fmt, ap);

	switch (scsi_log_buffer[0]) {
	case '!':
		log_only = 1;
		break;
	case '?':
		boot_only = 1;
		break;
	case '^':
		console_only = 1;
		break;
	}

	switch (level) {
	case CE_NOTE:
		level = CE_CONT;
		/* FALLTHROUGH */
	case CE_CONT:
	case CE_WARN:
	case CE_PANIC:
		if (boot_only) {
			cmn_err(level, "?%s\t%s", name,
				&scsi_log_buffer[1]);
		} else if (console_only) {
			cmn_err(level, "^%s\t%s", name,
				&scsi_log_buffer[1]);
		} else if (log_only) {
			cmn_err(level, "!%s\t%s", name,
				&scsi_log_buffer[1]);
		} else {
			cmn_err(level, "%s\t%s", name,
				scsi_log_buffer);
		}
		break;
	case (uint_t)SCSI_DEBUG:
	default:
		cmn_err(CE_CONT, "^DEBUG: %s\t%s", name,
				scsi_log_buffer);
		break;
	}
}

int
scsi_get_device_type_scsi_options(dev_info_t *dip,
    struct scsi_device *devp, int default_scsi_options)
{

	caddr_t config_list	= NULL;
	int options		= default_scsi_options;
	struct scsi_inquiry  *inq = devp->sd_inq;
	caddr_t vidptr, datanameptr;
	int	vidlen, dupletlen;
	int config_list_len, len;

	/*
	 * look up the device-type-scsi-options-list and walk thru
	 * the list
	 * compare the vendor ids of the earlier inquiry command and
	 * with those vids in the list
	 * if there is a match, lookup the scsi-options value
	 */
	if (ddi_getlongprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "device-type-scsi-options-list",
	    (caddr_t)&config_list, &config_list_len) == DDI_PROP_SUCCESS) {

		/*
		 * Compare vids in each duplet - if it matches, get value for
		 * dataname and then lookup scsi_options
		 * dupletlen is calculated later.
		 */
		for (len = config_list_len, vidptr = config_list; len > 0;
		    vidptr += dupletlen, len -= dupletlen) {

			vidlen = strlen(vidptr);
			datanameptr = vidptr + vidlen + 1;

			if ((vidlen != 0) &&
			    bcmp(inq->inq_vid, vidptr, vidlen) == 0) {
				/*
				 * get the data list
				 */
				options = ddi_prop_get_int(DDI_DEV_T_ANY,
				    dip, 0,
				    datanameptr, default_scsi_options);
				break;
			}
			dupletlen = vidlen + strlen(datanameptr) + 2;
		}
		kmem_free(config_list, config_list_len);
	}

	return (options);
}

/*
 * Functions for format-neutral sense data functions
 */

int
scsi_validate_sense(uint8_t *sense_buffer, int sense_buf_len, int *flags)
{
	int result;
	struct scsi_extended_sense *es =
	    (struct scsi_extended_sense *)sense_buffer;

	/*
	 * Init flags if present
	 */
	if (flags != NULL) {
		*flags = 0;
	}

	/*
	 * Check response code (Solaris breaks this into a 3-bit class
	 * and 4-bit code field.
	 */
	if ((es->es_class != CLASS_EXTENDED_SENSE) ||
	    ((es->es_code != CODE_FMT_FIXED_CURRENT) &&
		(es->es_code != CODE_FMT_FIXED_DEFERRED) &&
		(es->es_code != CODE_FMT_DESCR_CURRENT) &&
		(es->es_code != CODE_FMT_DESCR_DEFERRED))) {
		/*
		 * Sense data (if there's actually anything here) is not
		 * in a format we can handle).
		 */
		return (SENSE_UNUSABLE);
	}

	/*
	 * Check if this is deferred sense
	 */
	if ((flags != NULL) &&
	    ((es->es_code == CODE_FMT_FIXED_DEFERRED) ||
		(es->es_code == CODE_FMT_DESCR_DEFERRED))) {
		*flags |= SNS_BUF_DEFERRED;
	}

	/*
	 * Make sure length is OK
	 */
	if (es->es_code == CODE_FMT_FIXED_CURRENT ||
	    es->es_code == CODE_FMT_FIXED_DEFERRED) {
		/*
		 * We can get by with a buffer that only includes the key,
		 * asc, and ascq.  In reality the minimum length we should
		 * ever see is 18 bytes.
		 */
		if ((sense_buf_len < MIN_FIXED_SENSE_LEN) ||
		    ((es->es_add_len + ADDL_SENSE_ADJUST) <
			MIN_FIXED_SENSE_LEN)) {
			result = SENSE_UNUSABLE;
		} else {
			/*
			 * The es_add_len field contains the number of sense
			 * data bytes that follow the es_add_len field.
			 */
			if ((flags != NULL) &&
			    (sense_buf_len <
				(es->es_add_len + ADDL_SENSE_ADJUST))) {
				*flags |= SNS_BUF_OVERFLOW;
			}

			result = SENSE_FIXED_FORMAT;
		}
	} else {
		struct scsi_descr_sense_hdr *ds =
		    (struct scsi_descr_sense_hdr *)sense_buffer;

		/*
		 * For descriptor format we need at least the descriptor
		 * header
		 */
		if (sense_buf_len < sizeof (struct scsi_descr_sense_hdr)) {
			result = SENSE_UNUSABLE;
		} else {
			/*
			 * Check for overflow
			 */
			if ((flags != NULL) &&
			    (sense_buf_len <
				(ds->ds_addl_sense_length + sizeof (*ds)))) {
				*flags |= SNS_BUF_OVERFLOW;
			}

			result = SENSE_DESCR_FORMAT;
		}
	}

	return (result);
}


uint8_t
scsi_sense_key(uint8_t *sense_buffer)
{
	uint8_t skey;
	if (SCSI_IS_DESCR_SENSE(sense_buffer)) {
		struct scsi_descr_sense_hdr *sdsp =
		    (struct scsi_descr_sense_hdr *)sense_buffer;
		skey = sdsp->ds_key;
	} else {
		struct scsi_extended_sense *ext_sensep =
		    (struct scsi_extended_sense *)sense_buffer;
		skey = ext_sensep->es_key;
	}
	return (skey);
}

uint8_t
scsi_sense_asc(uint8_t *sense_buffer)
{
	uint8_t asc;
	if (SCSI_IS_DESCR_SENSE(sense_buffer)) {
		struct scsi_descr_sense_hdr *sdsp =
		    (struct scsi_descr_sense_hdr *)sense_buffer;
		asc = sdsp->ds_add_code;
	} else {
		struct scsi_extended_sense *ext_sensep =
		    (struct scsi_extended_sense *)sense_buffer;
		asc = ext_sensep->es_add_code;
	}
	return (asc);
}

uint8_t
scsi_sense_ascq(uint8_t *sense_buffer)
{
	uint8_t ascq;
	if (SCSI_IS_DESCR_SENSE(sense_buffer)) {
		struct scsi_descr_sense_hdr *sdsp =
		    (struct scsi_descr_sense_hdr *)sense_buffer;
		ascq = sdsp->ds_qual_code;
	} else {
		struct scsi_extended_sense *ext_sensep =
		    (struct scsi_extended_sense *)sense_buffer;
		ascq = ext_sensep->es_qual_code;
	}
	return (ascq);
}

void scsi_ext_sense_fields(uint8_t *sense_buffer, int sense_buf_len,
    uint8_t **information, uint8_t **cmd_spec_info, uint8_t **fru_code,
    uint8_t **sk_specific, uint8_t **stream_flags)
{
	int sense_fmt;

	/*
	 * Sanity check sense data and determine the format
	 */
	sense_fmt = scsi_validate_sense(sense_buffer, sense_buf_len, NULL);

	/*
	 * Initialize any requested data to 0
	 */
	if (information) {
		*information = NULL;
	}
	if (cmd_spec_info) {
		*cmd_spec_info = NULL;
	}
	if (fru_code) {
		*fru_code = NULL;
	}
	if (sk_specific) {
		*sk_specific = NULL;
	}
	if (stream_flags) {
		*stream_flags = NULL;
	}

	if (sense_fmt == SENSE_DESCR_FORMAT) {
		struct scsi_descr_template *sdt = NULL;

		while (scsi_get_next_descr(sense_buffer,
		    sense_buf_len, &sdt) != -1) {
			switch (sdt->sdt_descr_type) {
			case DESCR_INFORMATION: {
				struct scsi_information_sense_descr *isd =
				    (struct scsi_information_sense_descr *)
				    sdt;
				if (information) {
					*information =
					    &isd->isd_information[0];
				}
				break;
			}
			case DESCR_COMMAND_SPECIFIC: {
				struct scsi_cmd_specific_sense_descr *csd =
				    (struct scsi_cmd_specific_sense_descr *)
				    sdt;
				if (cmd_spec_info) {
					*cmd_spec_info =
					    &csd->css_cmd_specific_info[0];
				}
				break;
			}
			case DESCR_SENSE_KEY_SPECIFIC: {
				struct scsi_sk_specific_sense_descr *ssd =
				    (struct scsi_sk_specific_sense_descr *)
				    sdt;
				if (sk_specific) {
					*sk_specific =
					    (uint8_t *)&ssd->sss_data;
				}
				break;
			}
			case DESCR_FRU: {
				struct scsi_fru_sense_descr *fsd =
				    (struct scsi_fru_sense_descr *)
				    sdt;
				if (fru_code) {
					*fru_code = &fsd->fs_fru_code;
				}
				break;
			}
			case DESCR_STREAM_COMMANDS: {
				struct scsi_stream_cmd_sense_descr *strsd =
				    (struct scsi_stream_cmd_sense_descr *)
				    sdt;
				if (stream_flags) {
					*stream_flags =
					    (uint8_t *)&strsd->scs_data;
				}
				break;
			}
			case DESCR_BLOCK_COMMANDS: {
				struct scsi_block_cmd_sense_descr *bsd =
				    (struct scsi_block_cmd_sense_descr *)
				    sdt;
				/*
				 * The "Block Command" sense descriptor
				 * contains an ili bit that we can store
				 * in the stream specific data if it is
				 * available.  We shouldn't see both
				 * a block command and a stream command
				 * descriptor in the same collection
				 * of sense data.
				 */
				if (stream_flags) {
					/*
					 * Can't take an address of a bitfield,
					 * but the flags are just after the
					 * bcs_reserved field.
					 */
					*stream_flags =
					    (uint8_t *)&bsd->bcs_reserved + 1;
				}
				break;
			}
			}
		}
	} else {
		struct scsi_extended_sense *es =
		    (struct scsi_extended_sense *)sense_buffer;

		/* Get data from fixed sense buffer */
		if (information && es->es_valid) {
			*information = &es->es_info_1;
		}
		if (cmd_spec_info && es->es_valid) {
			*cmd_spec_info = &es->es_cmd_info[0];
		}
		if (fru_code) {
			*fru_code = &es->es_fru_code;
		}
		if (sk_specific) {
			*sk_specific = &es->es_skey_specific[0];
		}
		if (stream_flags) {
			/*
			 * Can't take the address of a bit field,
			 * but the stream flags are located just after
			 * the es_segnum field;
			 */
			*stream_flags = &es->es_segnum + 1;
		}
	}
}

boolean_t
scsi_sense_info_uint64(uint8_t *sense_buffer, int sense_buf_len,
    uint64_t *information)
{
	boolean_t valid;
	int sense_fmt;

	ASSERT(sense_buffer != NULL);
	ASSERT(information != NULL);

	/* Validate sense data and get format */
	sense_fmt = scsi_validate_sense(sense_buffer, sense_buf_len, NULL);

	if (sense_fmt == SENSE_UNUSABLE) {
		/* Information is not valid */
		valid = 0;
	} else if (sense_fmt == SENSE_FIXED_FORMAT) {
		struct scsi_extended_sense *es =
		    (struct scsi_extended_sense *)sense_buffer;

		*information = (uint64_t)SCSI_READ32(&es->es_info_1);

		valid = es->es_valid;
	} else {
		/* Sense data is descriptor format */
		struct scsi_information_sense_descr *isd;

		isd = (struct scsi_information_sense_descr *)
		    scsi_find_sense_descr(sense_buffer, sense_buf_len,
			DESCR_INFORMATION);

		if (isd) {
			*information = SCSI_READ64(isd->isd_information);
			valid = 1;
		} else {
			valid = 0;
		}
	}

	return (valid);
}

boolean_t
scsi_sense_cmdspecific_uint64(uint8_t *sense_buffer, int sense_buf_len,
    uint64_t *cmd_specific_info)
{
	boolean_t valid;
	int sense_fmt;

	ASSERT(sense_buffer != NULL);
	ASSERT(cmd_specific_info != NULL);

	/* Validate sense data and get format */
	sense_fmt = scsi_validate_sense(sense_buffer, sense_buf_len, NULL);

	if (sense_fmt == SENSE_UNUSABLE) {
		/* Command specific info is not valid */
		valid = 0;
	} else if (sense_fmt == SENSE_FIXED_FORMAT) {
		struct scsi_extended_sense *es =
		    (struct scsi_extended_sense *)sense_buffer;

		*cmd_specific_info = (uint64_t)SCSI_READ32(es->es_cmd_info);

		valid = es->es_valid;
	} else {
		/* Sense data is descriptor format */
		struct scsi_cmd_specific_sense_descr *c;

		c = (struct scsi_cmd_specific_sense_descr *)
		    scsi_find_sense_descr(sense_buffer, sense_buf_len,
			DESCR_COMMAND_SPECIFIC);

		if (c) {
			valid = 1;
			*cmd_specific_info =
			    SCSI_READ64(c->css_cmd_specific_info);
		} else {
			valid = 0;
		}
	}

	return (valid);
}

uint8_t *
scsi_find_sense_descr(uint8_t *sdsp, int sense_buf_len, int req_descr_type)
{
	struct scsi_descr_template *sdt = NULL;

	while (scsi_get_next_descr(sdsp, sense_buf_len, &sdt) != -1) {
		ASSERT(sdt != NULL);
		if (sdt->sdt_descr_type == req_descr_type) {
			/* Found requested descriptor type */
			break;
		}
	}

	return ((uint8_t *)sdt);
}

/*
 * Sense Descriptor format is:
 *
 * <Descriptor type> <Descriptor length> <Descriptor data> ...
 *
 * 2 must be added to the descriptor length value to get the
 * total descriptor length sense the stored length does not
 * include the "type" and "additional length" fields.
 */

#define	NEXT_DESCR_PTR(ndp_descr) \
	((struct scsi_descr_template *)(((uint8_t *)(ndp_descr)) + \
	    ((ndp_descr)->sdt_addl_length + \
	    sizeof (struct scsi_descr_template))))

static int
scsi_get_next_descr(uint8_t *sense_buffer,
    int sense_buf_len, struct scsi_descr_template **descrpp)
{
	struct scsi_descr_sense_hdr *sdsp =
	    (struct scsi_descr_sense_hdr *)sense_buffer;
	struct scsi_descr_template *cur_descr;
	boolean_t find_first;
	int valid_sense_length;

	ASSERT(descrpp != NULL);
	find_first = (*descrpp == NULL);

	/*
	 * If no descriptor is passed in then return the first
	 * descriptor
	 */
	if (find_first) {
		/*
		 * The first descriptor will immediately follow the header
		 * (Pointer arithmetic)
		 */
		cur_descr = (struct scsi_descr_template *)(sdsp+1);
	} else {
		cur_descr = *descrpp;
		ASSERT(cur_descr > (struct scsi_descr_template *)sdsp);
	}

	/* Assume no more descriptors are available */
	*descrpp = NULL;

	/*
	 * Calculate the amount of valid sense data -- make sure the length
	 * byte in this descriptor lies within the valid sense data.
	 */
	valid_sense_length =
	    min((sizeof (struct scsi_descr_sense_hdr) +
	    sdsp->ds_addl_sense_length),
	    sense_buf_len);

	/*
	 * Make sure this descriptor is complete (either the first
	 * descriptor or the descriptor passed in)
	 */
	if (scsi_validate_descr(sdsp, valid_sense_length, cur_descr) !=
	    DESCR_GOOD) {
		return (-1);
	}

	/*
	 * If we were looking for the first descriptor go ahead and return it
	 */
	if (find_first) {
		*descrpp = cur_descr;
		return ((*descrpp)->sdt_descr_type);
	}

	/*
	 * Get pointer to next descriptor
	 */
	cur_descr = NEXT_DESCR_PTR(cur_descr);

	/*
	 * Make sure this descriptor is also complete.
	 */
	if (scsi_validate_descr(sdsp, valid_sense_length, cur_descr) !=
	    DESCR_GOOD) {
		return (-1);
	}

	*descrpp = (struct scsi_descr_template *)cur_descr;
	return ((*descrpp)->sdt_descr_type);
}

static int
scsi_validate_descr(struct scsi_descr_sense_hdr *sdsp,
    int valid_sense_length, struct scsi_descr_template *descrp)
{
	int descr_offset, next_descr_offset;

	/*
	 * Make sure length is present
	 */
	descr_offset = (uint8_t *)descrp - (uint8_t *)sdsp;
	if (descr_offset + sizeof (struct scsi_descr_template) >
	    valid_sense_length) {
		return (DESCR_PARTIAL);
	}

	/*
	 * Check if length is 0 (no more descriptors)
	 */
	if (descrp->sdt_addl_length == 0) {
		return (DESCR_END);
	}

	/*
	 * Make sure the rest of the descriptor is present
	 */
	next_descr_offset =
	    (uint8_t *)NEXT_DESCR_PTR(descrp) - (uint8_t *)sdsp;
	if (next_descr_offset > valid_sense_length) {
		return (DESCR_PARTIAL);
	}

	return (DESCR_GOOD);
}
