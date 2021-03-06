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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_IB_IBTL_IBTL_TYPES_H
#define	_SYS_IB_IBTL_IBTL_TYPES_H

#pragma ident	"@(#)ibtl_types.h	1.10	06/01/26 SMI"

/*
 * ibtl_types.h
 *
 * All common IBTL defined types. These are common data types
 * that are shared by the IBTI and IBCI interfaces, it is only included
 * by ibti.h and ibci.h
 */
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ib/ib_types.h>
#include <sys/ib/ibtl/ibtl_status.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Define Internal IBTL handles
 */
typedef	struct	ibtl_clnt_s	*ibt_clnt_hdl_t;    /* ibt_attach() */
typedef	struct	ibtl_hca_s	*ibt_hca_hdl_t;	    /* ibt_open_hca() */
typedef	struct	ibtl_channel_s	*ibt_channel_hdl_t; /* alloc_rc|ud_channel() */
typedef	struct	ibtl_srq_s	*ibt_srq_hdl_t;	    /* ibt_alloc_srq() */
typedef	struct	ibtl_cq_s	*ibt_cq_hdl_t;	    /* ibt_alloc_cq() */
typedef	struct	ibcm_svc_info_s	*ibt_srv_hdl_t;	    /* ibt_register_service() */
typedef	struct	ibcm_svc_bind_s	*ibt_sbind_hdl_t;   /* ibt_bind_service() */

typedef	struct	ibc_fmr_pool_s	*ibt_fmr_pool_hdl_t; /* ibt_create_fmr_pool() */
typedef	struct	ibc_ma_s	*ibt_ma_hdl_t;	    /* ibt_map_mem_area() */
typedef	struct	ibc_pd_s	*ibt_pd_hdl_t;	    /* ibt_alloc_pd() */
typedef	struct	ibc_sched_s	*ibt_sched_hdl_t;   /* ibt_alloc_cq_sched() */
typedef	struct	ibc_mr_s	*ibt_mr_hdl_t;	    /* ibt_register_mr() */
typedef	struct	ibc_mw_s	*ibt_mw_hdl_t;	    /* ibt_alloc_mw() */
typedef	struct	ibt_ud_dest_s	*ibt_ud_dest_hdl_t; /* UD dest handle */
typedef	struct	ibc_ah_s	*ibt_ah_hdl_t;	    /* ibt_alloc_ah() */
typedef struct	ibtl_eec_s	*ibt_eec_hdl_t;
typedef	struct	ibt_rd_dest_s	*ibt_rd_dest_hdl_t;	/* Reserved for */
							/* Future use */

/*
 * Some General Types.
 */
typedef uint32_t	ibt_lkey_t;		/* L_Key */
typedef uint32_t	ibt_rkey_t;		/* R_Key */
typedef uint64_t	ibt_wrid_t;		/* Client assigned WR ID */
typedef uint32_t	ibt_immed_t;		/* WR Immediate Data */
typedef uint64_t	ibt_atom_arg_t;		/* WR Atomic Operation arg */
typedef	uint_t		ibt_cq_handler_id_t;	/* Event handler ID */

/*
 * IBT selector type, used when looking up/requesting either an
 * MTU, Pkt lifetime, or Static rate.
 * The interpretation of IBT_BEST depends on the attribute being selected.
 */
typedef enum ibt_selector_e {
	IBT_GT		= 0,	/* Greater than */
	IBT_LT		= 1,	/* Less than */
	IBT_EQU		= 2,	/* Equal to */
	IBT_BEST	= 3	/* Best */
} ibt_selector_t;


/*
 * Static rate definitions.
 */
typedef enum ibt_srate_e {
	IBT_SRATE_NOT_SPECIFIED	= 0,
	IBT_SRATE_1X		= 2,
	IBT_SRATE_4X		= 3,
	IBT_SRATE_12X		= 4
} ibt_srate_t;

/*
 * Static rate request type.
 */
typedef struct ibt_srate_req_s {
	ibt_srate_t	r_srate;	/* Requested srate */
	ibt_selector_t	r_selector;	/* Qualifier for r_srate */
} ibt_srate_req_t;

/*
 * Packet Life Time Request Type.
 */
typedef struct ibt_pkt_lt_req_s {
	clock_t		p_pkt_lt;	/* Requested Packet Life Time */
	ibt_selector_t	p_selector;	/* Qualifier for p_pkt_lt */
} ibt_pkt_lt_req_t;

/*
 * Queue size struct.
 */
typedef struct ibt_queue_sizes_s {
	uint_t	qs_sq;		/* SendQ size. */
	uint_t	qs_rq;		/* RecvQ size. */
} ibt_queue_sizes_t;

/*
 * Channel sizes struct, used by functions that allocate/query RC or UD
 * channels.
 */
typedef struct ibt_chan_sizes_s {
	uint_t	cs_sq;		/* SendQ size. */
	uint_t	cs_rq;		/* ReceiveQ size. */
	uint_t	cs_sq_sgl;	/* Max SGL elements in a SQ WR. */
	uint_t	cs_rq_sgl;	/* Max SGL elements in a RQ Wr. */
} ibt_chan_sizes_t;

/*
 * Shared Queue size struct.
 */
typedef struct ibt_srq_sizes_s {
	uint_t	srq_wr_sz;
	uint_t	srq_sgl_sz;
} ibt_srq_sizes_t;

/*
 * SRQ Modify Flags
 */
typedef enum ibt_srq_modify_flags_e {
	IBT_SRQ_SET_NOTHING		= 0,
	IBT_SRQ_SET_SIZE		= (1 << 1),
	IBT_SRQ_SET_LIMIT		= (1 << 2)
} ibt_srq_modify_flags_t;


/*
 * Execution flags, indicates if the function should block or not.
 * Note: in some cases, e.g., a NULL rc_cm_handler, IBT_NONBLOCKING
 * will not have an effect, and the thread will block.
 * IBT_NOCALLBACKS is valid for ibt_close_rc_channel only.
 */
typedef enum ibt_execution_mode_e {
	IBT_BLOCKING	= 0,	/* Block */
	IBT_NONBLOCKING	= 1,	/* Return as soon as possible */
	IBT_NOCALLBACKS	= 2	/* cm_handler is not invoked after */
				/* ibt_close_rc_channel returns */
} ibt_execution_mode_t;

/*
 * Memory window alloc flags
 */
typedef enum ibt_mw_flags_e {
	IBT_MW_SLEEP		= 0,		/* Can block */
	IBT_MW_NOSLEEP		= (1 << 0),	/* Can't block */
	IBT_MW_USER_MAP		= (1 << 1),
	IBT_MW_DEFER_ALLOC	= (1 << 2),
	IBT_MW_TYPE_1		= (1 << 3),
	IBT_MW_TYPE_2		= (1 << 4)
} ibt_mw_flags_t;

/*
 * PD alloc flags
 */
typedef enum ibt_pd_flags_e {
	IBT_PD_NO_FLAGS		= 0,
	IBT_PD_USER_MAP		= (1 << 0),
	IBT_PD_DEFER_ALLOC	= (1 << 1)
} ibt_pd_flags_t;

/*
 * UD Dest alloc flags
 */
typedef enum ibt_ud_dest_flags_e {
	IBT_UD_DEST_NO_FLAGS	= 0,
	IBT_UD_DEST_USER_MAP	= (1 << 0),
	IBT_UD_DEST_DEFER_ALLOC	= (1 << 1)
} ibt_ud_dest_flags_t;

/*
 * SRQ alloc flags
 */
typedef enum ibt_srq_flags_e {
	IBT_SRQ_NO_FLAGS	= 0,
	IBT_SRQ_USER_MAP	= (1 << 0),
	IBT_SRQ_DEFER_ALLOC	= (1 << 1)
} ibt_srq_flags_t;

/*
 * ibt_alloc_lkey() alloc flags
 */
typedef enum ibt_lkey_flags_e {
	IBT_KEY_NO_FLAGS	= 0,
	IBT_KEY_REMOTE		= (1 << 0)
} ibt_lkey_flags_t;

/*
 *  RNR NAK retry counts.
 */
typedef enum ibt_rnr_retry_cnt_e {
	IBT_RNR_NO_RETRY	= 0x0,	/* Don't retry, fail on first timeout */
	IBT_RNR_RETRY_1		= 0x1,	/* Retry once */
	IBT_RNR_RETRY_2		= 0x2,	/* Retry twice */
	IBT_RNR_RETRY_3		= 0x3,	/* Retry three times */
	IBT_RNR_RETRY_4		= 0x4,	/* Retry four times */
	IBT_RNR_RETRY_5		= 0x5,	/* Retry five times */
	IBT_RNR_RETRY_6		= 0x6,	/* Retry six times */
	IBT_RNR_INFINITE_RETRY	= 0x7	/* Retry forever */
} ibt_rnr_retry_cnt_t;

/*
 * Valid values for RNR NAK timer fields, part of a channel's context.
 */
typedef enum ibt_rnr_nak_time_e {
	IBT_RNR_NAK_655ms	= 0x0,
	IBT_RNR_NAK_10us	= 0x1,
	IBT_RNR_NAK_20us	= 0x2,
	IBT_RNR_NAK_30us	= 0x3,
	IBT_RNR_NAK_40us	= 0x4,
	IBT_RNR_NAK_60us	= 0x5,
	IBT_RNR_NAK_80us	= 0x6,
	IBT_RNR_NAK_120us	= 0x7,
	IBT_RNR_NAK_160us	= 0x8,
	IBT_RNR_NAK_240us	= 0x9,
	IBT_RNR_NAK_320us	= 0xA,
	IBT_RNR_NAK_480us	= 0xB,
	IBT_RNR_NAK_640us	= 0xC,
	IBT_RNR_NAK_960us	= 0xD,
	IBT_RNR_NAK_1280us	= 0xE,
	IBT_RNR_NAK_1920us	= 0xF,
	IBT_RNR_NAK_2560us	= 0x10,
	IBT_RNR_NAK_3840us	= 0x11,
	IBT_RNR_NAK_5120us	= 0x12,
	IBT_RNR_NAK_7680us	= 0x13,
	IBT_RNR_NAK_10ms	= 0x14,
	IBT_RNR_NAK_15ms	= 0x15,
	IBT_RNR_NAK_20ms	= 0x16,
	IBT_RNR_NAK_31ms	= 0x17,
	IBT_RNR_NAK_41ms	= 0x18,
	IBT_RNR_NAK_61ms	= 0x19,
	IBT_RNR_NAK_82ms	= 0x1A,
	IBT_RNR_NAK_123ms	= 0x1B,
	IBT_RNR_NAK_164ms	= 0x1C,
	IBT_RNR_NAK_246ms	= 0x1D,
	IBT_RNR_NAK_328ms	= 0x1E,
	IBT_RNR_NAK_492ms	= 0x1F
} ibt_rnr_nak_time_t;

/*
 * The definition of HCA capabilities etc as a bitfield.
 */
typedef enum ibt_hca_flags_e {
	IBT_HCA_NO_FLAGS	= 0,

	IBT_HCA_RD		= 1 << 0,
	IBT_HCA_UD_MULTICAST	= 1 << 1,
	IBT_HCA_RAW_MULTICAST	= 1 << 2,

	IBT_HCA_ATOMICS_HCA	= 1 << 3,
	IBT_HCA_ATOMICS_GLOBAL	= 1 << 4,

	IBT_HCA_RESIZE_CHAN	= 1 << 5,	/* Is resize supported? */
	IBT_HCA_AUTO_PATH_MIG	= 1 << 6,	/* Is APM supported? */
	IBT_HCA_SQD_SQD_PORT	= 1 << 7,	/* Can change physical port */
						/* on transit from SQD to SQD */
	IBT_HCA_PKEY_CNTR	= 1 << 8,
	IBT_HCA_QKEY_CNTR	= 1 << 9,
	IBT_HCA_AH_PORT_CHECK	= 1 << 10,	/* HCA checks AH port match */
						/* in UD WRs */
	IBT_HCA_PORT_UP		= 1 << 11,	/* PortActive event supported */
	IBT_HCA_INIT_TYPE	= 1 << 12,	/* InitType supported */
	IBT_HCA_SI_GUID		= 1 << 13,	/* System Image GUID */
						/* supported */
	IBT_HCA_SHUTDOWN_PORT	= 1 << 14,	/* ShutdownPort supported */
	IBT_HCA_RNR_NAK		= 1 << 15,	/* RNR-NAK supported for RC */
	IBT_HCA_CURRENT_QP_STATE = 1 << 16,	/* Does modify_qp support */
						/* checking of current state? */
	IBT_HCA_SRQ 		= 1 << 17,	/* Shared Receive Queue */
	IBT_HCA_RESIZE_SRQ	= 1 << 18,	/* Is resize SRQ supported? */
	IBT_HCA_BASE_MEM_MGT	= 1 << 19,	/* Base memory mgt supported? */
	IBT_HCA_MULT_PAGE_SZ_MR	= 1 << 20,	/* Support of multiple page */
						/* sizes per memory region? */
	IBT_HCA_BLOCK_LIST	= 1 << 21,	/* Block list physical buffer */
						/* lists supported? */
	IBT_HCA_ZERO_BASED_VA	= 1 << 22,	/* Zero Based Virtual */
						/* Addresses supported? */
	IBT_HCA_LOCAL_INVAL_FENCE = 1 << 23,	/* Local invalidate fencing? */
	IBT_HCA_BASE_QUEUE_MGT	= 1 << 24,	/* Base Queue Mgt supported? */
	IBT_HCA_CKSUM_FULL	= 1 << 25,	/* Checksum offload supported */
	IBT_HCA_MEM_WIN_TYPE_2B	= 1 << 26,	/* Type 2B memory windows */
	IBT_HCA_PHYS_BUF_BLOCK	= 1 << 27,	/* Block mode phys buf lists */
	IBT_HCA_FMR		= 1 << 28	/* FMR Support */
} ibt_hca_flags_t;

/*
 * The definition of HCA page size capabilities as a bitfield
 */
typedef enum ibt_page_sizes_e {
	IBT_PAGE_4K		= 0x1 << 2,
	IBT_PAGE_8K		= 0x1 << 3,
	IBT_PAGE_16K		= 0x1 << 4,
	IBT_PAGE_32K		= 0x1 << 5,
	IBT_PAGE_64K		= 0x1 << 6,
	IBT_PAGE_128K		= 0x1 << 7,
	IBT_PAGE_256K		= 0x1 << 8,
	IBT_PAGE_512K		= 0x1 << 9,
	IBT_PAGE_1M		= 0x1 << 10,
	IBT_PAGE_2M		= 0x1 << 11,
	IBT_PAGE_4M		= 0x1 << 12,
	IBT_PAGE_8M		= 0x1 << 13,
	IBT_PAGE_16M		= 0x1 << 14,
	IBT_PAGE_32M		= 0x1 << 15,
	IBT_PAGE_64M		= 0x1 << 16,
	IBT_PAGE_128M		= 0x1 << 17,
	IBT_PAGE_256M		= 0x1 << 18,
	IBT_PAGE_512M		= 0x1 << 19,
	IBT_PAGE_1G		= 0x1 << 20,
	IBT_PAGE_2G		= 0x1 << 21,
	IBT_PAGE_4G		= 0x1 << 22,
	IBT_PAGE_8G		= 0x1 << 23,
	IBT_PAGE_16G		= 0x1 << 24
} ibt_page_sizes_t;

/*
 * Memory Window Type.
 */
typedef enum ibt_mem_win_type_e {
	IBT_MEM_WIN_TYPE_NOT_DEFINED	= 0,
	IBT_MEM_WIN_TYPE_1		= (1 << 0),
	IBT_MEM_WIN_TYPE_2		= (1 << 1)
} ibt_mem_win_type_t;

/*
 * HCA attributes.
 * Contains all HCA static attributes.
 */
typedef struct ibt_hca_attr_s {
	ibt_hca_flags_t	hca_flags;		/* HCA capabilities etc */

	/* device/version inconsistency w/ NodeInfo and IOControllerProfile */
	uint32_t	hca_vendor_id:24;	/* 24 bit Vendor ID */
	uint16_t	hca_device_id;
	uint32_t	hca_version_id;

	uint_t		hca_max_chans;		/* Max Chans supported */
	uint_t		hca_max_chan_sz;	/* Max outstanding WRs on any */
						/* channel */

	uint_t		hca_max_sgl;		/* Max SGL entries per WR */

	uint_t		hca_max_cq;		/* Max num of CQs supported  */
	uint_t		hca_max_cq_sz;		/* Max capacity of each CQ */

	ibt_page_sizes_t	hca_page_sz;	/* Bit mask of page sizes */

	uint_t		hca_max_memr;		/* Max num of HCA mem regions */
	ib_memlen_t	hca_max_memr_len;	/* Largest block, in bytes of */
						/* mem that can be registered */
	uint_t		hca_max_mem_win;	/* Max Memory windows in HCA */

	uint_t		hca_max_rsc; 		/* Max Responder Resources of */
						/* this HCA for RDMAR/Atomics */
						/* with this HCA as target. */
	uint8_t		hca_max_rdma_in_chan;	/* Max RDMAR/Atomics in per */
						/* chan this HCA as target. */
	uint8_t		hca_max_rdma_out_chan;	/* Max RDMA Reads/Atomics out */
						/* per channel by this HCA */
	uint_t		hca_max_ipv6_chan;	/* Max IPV6 channels in HCA */
	uint_t		hca_max_ether_chan;	/* Max Ether channels in HCA */

	uint_t		hca_max_mcg_chans;	/* Max number of channels */
						/* that can join multicast */
						/* groups */
	uint_t		hca_max_mcg;		/* Max multicast groups */
	uint_t		hca_max_chan_per_mcg;	/* Max number of channels per */
						/* Multicast group in HCA */

	uint16_t	hca_max_partitions;	/* Max partitions in HCA */
	uint8_t		hca_nports;		/* Number of physical ports */
	ib_guid_t	hca_node_guid;		/* Node GUID */

	ib_time_t	hca_local_ack_delay;

	uint_t		hca_max_port_sgid_tbl_sz;
	uint16_t	hca_max_port_pkey_tbl_sz;
	uint_t		hca_max_pd;		/* Max# of Protection Domains */
	ib_guid_t	hca_si_guid;		/* Optional System Image GUID */
	uint_t		hca_hca_max_ci_priv_sz;
	uint_t		hca_chan_max_ci_priv_sz;
	uint_t		hca_cq_max_ci_priv_sz;
	uint_t		hca_pd_max_ci_priv_sz;
	uint_t		hca_mr_max_ci_priv_sz;
	uint_t		hca_mw_max_ci_priv_sz;
	uint_t		hca_ud_dest_max_ci_priv_sz;
	uint_t		hca_cq_sched_max_ci_priv_sz;
	uint_t		hca_max_ud_dest;
	uint_t		hca_opaque2;
	uint_t		hca_opaque3;
	uint_t		hca_opaque4;
	uint8_t		hca_opaque5;
	uint8_t		hca_opaque6;
	uint_t		hca_opaque7;
	uint_t		hca_opaque8;
	uint_t		hca_max_srqs;		/* Max SRQs supported */
	uint_t		hca_max_srqs_sz;	/* Max outstanding WRs on any */
						/* SRQ */
	uint_t		hca_max_srq_sgl;	/* Max SGL entries per SRQ WR */
	uint_t		hca_max_phys_buf_list_sz;
	size_t		hca_block_sz_lo;	/* Range of block sizes */
	size_t		hca_block_sz_hi;	/* supported by the HCA */
	uint_t		hca_max_cq_handlers;
	ibt_lkey_t	hca_reserved_lkey;
	uint_t		hca_max_fmrs;		/* Max FMR Supported */
	uint_t		hca_opaque9;
} ibt_hca_attr_t;

/*
 * HCA Port link states.
 */
typedef enum ibt_port_state_e {
	IBT_PORT_DOWN	= 1,
	IBT_PORT_INIT,
	IBT_PORT_ARM,
	IBT_PORT_ACTIVE
} ibt_port_state_t;

/*
 * HCA Port capabilities as a bitfield.
 */
typedef enum ibt_port_caps_e {
	IBT_PORT_CAP_NO_FLAGS		= 0,
	IBT_PORT_CAP_SM			= 1 << 0,	/* SM port */
	IBT_PORT_CAP_SM_DISABLED	= 1 << 1,
	IBT_PORT_CAP_SNMP_TUNNEL	= 1 << 2,	/* SNMP Tunneling */
	IBT_PORT_CAP_DM			= 1 << 3,	/* DM supported */
	IBT_PORT_CAP_VENDOR		= 1 << 4	/* Vendor Class */
} ibt_port_caps_t;


/*
 * HCA port attributes structure definition. The number of ports per HCA
 * can be found from the "ibt_hca_attr_t" structure.
 *
 * p_pkey_tbl is a pointer to an array of ib_pkey_t, members are
 * accessed as:
 *		hca_portinfo->p_pkey_tbl[i]
 *
 * Where 0 <= i < hca_portinfo.p_pkey_tbl_sz
 *
 * Similarly p_sgid_tbl is a pointer to an array of ib_gid_t.
 *
 * The Query Port function - ibt_query_hca_ports() allocates the memory
 * required for the ibt_hca_portinfo_t struct as well as the memory
 * required for the SGID and P_Key tables. The memory is freed by calling
 * ibt_free_portinfo().
 */
typedef struct ibt_hca_portinfo_s {
	ib_lid_t		p_opaque1;	/* Base LID of port */
	ib_qkey_cntr_t		p_qkey_violations; /* Bad Q_Key cnt */
	ib_pkey_cntr_t		p_pkey_violations; /* Optional bad P_Key cnt */
	uint8_t			p_sm_sl:4;	/* SM Service level */
	ib_lid_t		p_sm_lid;	/* SM LID */
	ibt_port_state_t	p_linkstate;	/* Port state */
	uint8_t			p_port_num;
	ib_mtu_t		p_mtu;		/* Max transfer unit - pkt */
	uint8_t			p_lmc:3;	/* Local mask control */
	ib_gid_t		*p_sgid_tbl;	/* SGID Table */
	uint_t			p_sgid_tbl_sz;	/* Size of SGID table */
	uint16_t		p_pkey_tbl_sz;	/* Size of P_Key table */
	uint16_t		p_def_pkey_ix;	/* default pkey index for TI */
	ib_pkey_t		*p_pkey_tbl;	/* P_Key table */
	uint8_t			p_max_vl;	/* Max num of virtual lanes */
	uint8_t			p_init_type_reply; /* Optional InitTypeReply */
	ib_time_t		p_subnet_timeout; /* Max Subnet Timeout */
	ibt_port_caps_t		p_capabilities;	/* Port Capabilities */
	uint32_t		p_msg_sz;	/* Max message size */
} ibt_hca_portinfo_t;

/*
 * Modify HCA port attributes flags, specifies which HCA port
 * attributes to modify.
 */
typedef enum ibt_port_modify_flags_e {
	IBT_PORT_NO_FLAGS	= 0,

	IBT_PORT_RESET_QKEY	= 1 << 0,	/* Reset Q_Key violation */
						/* counter */
	IBT_PORT_RESET_SM	= 1 << 1,	/* SM */
	IBT_PORT_SET_SM		= 1 << 2,
	IBT_PORT_RESET_SNMP	= 1 << 3,	/* SNMP Tunneling */
	IBT_PORT_SET_SNMP	= 1 << 4,
	IBT_PORT_RESET_DEVMGT	= 1 << 5,	/* Device Management */
	IBT_PORT_SET_DEVMGT	= 1 << 6,
	IBT_PORT_RESET_VENDOR	= 1 << 7,	/* Vendor Class */
	IBT_PORT_SET_VENDOR	= 1 << 8,
	IBT_PORT_SHUTDOWN	= 1 << 9,	/* Shut down the port */
	IBT_PORT_SET_INIT_TYPE	= 1 << 10	/* InitTypeReply value */
} ibt_port_modify_flags_t;

/*
 * Modify HCA port InitType bit definitions, applicable only if
 * IBT_PORT_SET_INIT_TYPE modify flag (ibt_port_modify_flags_t) is set.
 */
#define	IBT_PINIT_NO_LOAD		0x1
#define	IBT_PINIT_PRESERVE_CONTENT	0x2
#define	IBT_PINIT_PRESERVE_PRESENCE	0x4
#define	IBT_PINIT_NO_RESUSCITATE	0x8


/*
 * Address vector definition.
 */
typedef struct ibt_adds_vect_s {
	ib_gid_t	av_dgid;	/* IPV6 dest GID in GRH */
	ib_gid_t	av_sgid;	/* SGID */
	ibt_srate_t	av_srate;	/* Max static rate */
	uint8_t		av_srvl:4;	/* Service level in LRH */
	uint_t		av_flow:20;	/* 20 bit Flow Label */
	uint8_t		av_tclass;	/* Traffic Class */
	uint8_t		av_hop;		/* Hop Limit */
	uint8_t		av_port_num;	/* Port number for UD */
	boolean_t	av_opaque1;
	ib_lid_t	av_opaque2;
	ib_path_bits_t	av_opaque3;
	uint32_t	av_opaque4;
} ibt_adds_vect_t;

typedef struct ibt_cep_path_s {
	ibt_adds_vect_t	cep_adds_vect;		/* Address Vector */
	uint16_t	cep_pkey_ix;		/* P_Key Index */
	uint8_t		cep_hca_port_num;	/* Port number for connected */
						/* channels.  A value of 0 */
						/* indicates an invalid path */
	ib_time_t	cep_cm_opaque1;
} ibt_cep_path_t;

/*
 * Channel Migration State.
 */
typedef enum ibt_cep_cmstate_e {
	IBT_STATE_NOT_SUPPORTED	= 0,
	IBT_STATE_MIGRATED	= 1,
	IBT_STATE_REARMED	= 2,
	IBT_STATE_ARMED		= 3
} ibt_cep_cmstate_t;

/*
 * Transport service type
 *
 * NOTE: this was converted from an enum to a uint8_t to save space.
 */
typedef uint8_t ibt_tran_srv_t;

#define	IBT_RC_SRV		0
#define	IBT_UC_SRV		1
#define	IBT_RD_SRV		2
#define	IBT_UD_SRV		3
#define	IBT_RAWIP_SRV		4
#define	IBT_RAWETHER_SRV	5

/*
 * Channel (QP/EEC) state definitions.
 */
typedef enum ibt_cep_state_e {
	IBT_STATE_RESET	= 0,		/* Reset */
	IBT_STATE_INIT,			/* Initialized */
	IBT_STATE_RTR,			/* Ready to Receive */
	IBT_STATE_RTS,			/* Ready to Send */
	IBT_STATE_SQD,			/* Send Queue Drained */
	IBT_STATE_SQE,			/* Send Queue Error */
	IBT_STATE_ERROR,		/* Error */
	IBT_STATE_SQDRAIN,		/* Send Queue Draining */
	IBT_STATE_NUM			/* Number of states */
} ibt_cep_state_t;


/*
 * Channel Attribute flags.
 */
typedef enum ibt_attr_flags_e {
	IBT_ALL_SIGNALED	= 0,	/* All sends signaled */
	IBT_WR_SIGNALED		= 1,	/* Signaled on a WR basis */
	IBT_FAST_REG_RES_LKEY	= (1 << 1)
} ibt_attr_flags_t;

/*
 * Channel End Point (CEP) Control Flags.
 */
typedef enum ibt_cep_flags_e {
	IBT_CEP_NO_FLAGS	= 0,		/* Enable Nothing */
	IBT_CEP_RDMA_RD		= (1 << 0),	/* Enable incoming RDMA RD's */
						/* RC & RD only */
	IBT_CEP_RDMA_WR		= (1 << 1),	/* Enable incoming RDMA WR's */
						/* RC & RD only */
	IBT_CEP_ATOMIC		= (1 << 2)	/* Enable incoming Atomics, */
						/* RC & RD only */
} ibt_cep_flags_t;

/*
 * Channel Modify Flags
 */
typedef enum ibt_cep_modify_flags_e {
	IBT_CEP_SET_NOTHING		= 0,
	IBT_CEP_SET_SQ_SIZE		= (1 << 1),
	IBT_CEP_SET_RQ_SIZE		= (1 << 2),

	IBT_CEP_SET_RDMA_R		= (1 << 3),
	IBT_CEP_SET_RDMA_W		= (1 << 4),
	IBT_CEP_SET_ATOMIC		= (1 << 5),

	IBT_CEP_SET_ALT_PATH		= (1 << 6),	/* Alternate Path */

	IBT_CEP_SET_ADDS_VECT		= (1 << 7),
	IBT_CEP_SET_PORT		= (1 << 8),
	IBT_CEP_SET_OPAQUE5		= (1 << 9),
	IBT_CEP_SET_RETRY		= (1 << 10),
	IBT_CEP_SET_RNR_NAK_RETRY 	= (1 << 11),
	IBT_CEP_SET_MIN_RNR_NAK		= (1 << 12),

	IBT_CEP_SET_QKEY		= (1 << 13),
	IBT_CEP_SET_RDMARA_OUT		= (1 << 14),
	IBT_CEP_SET_RDMARA_IN		= (1 << 15),

	IBT_CEP_SET_OPAQUE1		= (1 << 16),
	IBT_CEP_SET_OPAQUE2		= (1 << 17),
	IBT_CEP_SET_OPAQUE3		= (1 << 18),
	IBT_CEP_SET_OPAQUE4		= (1 << 19),
	IBT_CEP_SET_SQD_EVENT		= (1 << 20),
	IBT_CEP_SET_OPAQUE6		= (1 << 21),
	IBT_CEP_SET_OPAQUE7		= (1 << 22),
	IBT_CEP_SET_OPAQUE8		= (1 << 23)
} ibt_cep_modify_flags_t;

/*
 * CQ notify types.
 */
typedef enum ibt_cq_notify_flags_e {
	IBT_NEXT_COMPLETION	= 1,
	IBT_NEXT_SOLICITED	= 2
} ibt_cq_notify_flags_t;

/*
 * CQ types shared across TI and CI.
 */
typedef enum ibt_cq_flags_e {
	IBT_CQ_NO_FLAGS			= 0,
	IBT_CQ_HANDLER_IN_THREAD	= 1 << 0,	/* A thread calls the */
							/* CQ handler */
	IBT_CQ_USER_MAP			= 1 << 1,
	IBT_CQ_DEFER_ALLOC		= 1 << 2
} ibt_cq_flags_t;

/*
 * CQ types shared across TI and CI.
 */
typedef enum ibt_cq_sched_flags_e {
	IBT_CQS_NO_FLAGS	= 0,
	IBT_CQS_WARM_CACHE	= 1 << 0, /* run on same CPU */
	IBT_CQS_AFFINITY	= 1 << 1,
	IBT_CQS_SCHED_GROUP	= 1 << 2,
	IBT_CQS_USER_MAP	= 1 << 3,
	IBT_CQS_DEFER_ALLOC	= 1 << 4
} ibt_cq_sched_flags_t;

/*
 * Attributes when creating a Completion Queue.
 *
 * Note:
 *	The IBT_CQ_HANDLER_IN_THREAD cq_flags bit should be ignored by the CI.
 */
typedef struct ibt_cq_attr_s {
	uint_t			cq_size;
	ibt_sched_hdl_t		cq_sched;	/* 0 = no hint, */
						/* other = cq_sched value */
	ibt_cq_flags_t		cq_flags;
} ibt_cq_attr_t;

/*
 * Memory Management
 */

/* Memory management flags */
typedef enum ibt_mr_flags_e {
	IBT_MR_SLEEP			= 0,
	IBT_MR_NOSLEEP			= (1 << 1),
	IBT_MR_NONCOHERENT		= (1 << 2),
	IBT_MR_PHYS_IOVA		= (1 << 3),  /* ibt_(re)register_buf */

	/* Access control flags */
	IBT_MR_ENABLE_WINDOW_BIND	= (1 << 4),
	IBT_MR_ENABLE_LOCAL_WRITE	= (1 << 5),
	IBT_MR_ENABLE_REMOTE_READ	= (1 << 6),
	IBT_MR_ENABLE_REMOTE_WRITE	= (1 << 7),
	IBT_MR_ENABLE_REMOTE_ATOMIC	= (1 << 8),

	/* Reregister flags */
	IBT_MR_CHANGE_TRANSLATION	= (1 << 9),
	IBT_MR_CHANGE_ACCESS		= (1 << 10),
	IBT_MR_CHANGE_PD		= (1 << 11),

	/* Additional registration flags */
	IBT_MR_ZBVA			= (1 << 12),

	/* Additional physical registration flags */
	IBT_MR_CONSUMER_KEY		= (1 << 13)	/* Consumer owns key */
							/* portion of keys */
} ibt_mr_flags_t;


/* Memory Region attribute flags */
typedef enum ibt_mr_attr_flags_e {
	/* Access control flags */
	IBT_MR_WINDOW_BIND		= (1 << 0),
	IBT_MR_LOCAL_WRITE		= (1 << 1),
	IBT_MR_REMOTE_READ		= (1 << 2),
	IBT_MR_REMOTE_WRITE		= (1 << 3),
	IBT_MR_REMOTE_ATOMIC		= (1 << 4),
	IBT_MR_ZERO_BASED_VA		= (1 << 5),
	IBT_MR_CONSUMER_OWNED_KEY	= (1 << 6),
	IBT_MR_SHARED			= (1 << 7),
	IBT_MR_FMR			= (1 << 8)
} ibt_mr_attr_flags_t;

/* Memory region physical descriptor. */
typedef struct ibt_phys_buf_s {
	union {
		uint64_t	_p_ll;		/* 64 bit DMA address */
		uint32_t	_p_la[2];	/* 2 x 32 bit address */
	} _phys_buf;
	size_t	p_size;
} ibt_phys_buf_t;

#define	p_laddr		_phys_buf._p_ll
#ifdef	_LONG_LONG_HTOL
#define	p_notused	_phys_buf._p_la[0]
#define	p_addr		_phys_buf._p_la[1]
#else
#define	p_addr		_phys_buf._p_la[0]
#define	p_notused	_phys_buf._p_la[1]
#endif


/* Memory region descriptor. */
typedef struct ibt_mr_desc_s {
	ib_vaddr_t	md_vaddr;	/* IB virtual adds of memory */
	ibt_lkey_t	md_lkey;
	ibt_rkey_t	md_rkey;
	boolean_t	md_sync_required;
} ibt_mr_desc_t;

/* Physical Memory region descriptor. */
typedef struct ibt_pmr_desc_s {
	ib_vaddr_t	pmd_iova;	/* Returned I/O Virtual Address */
	ibt_lkey_t	pmd_lkey;
	ibt_rkey_t	pmd_rkey;
	uint_t 		pmd_phys_buf_list_sz;	/* Allocated Phys buf sz */
	boolean_t	pmd_sync_required;
} ibt_pmr_desc_t;

/* Memory region protection bounds. */
typedef struct ibt_mr_prot_bounds_s {
	ib_vaddr_t	pb_addr;	/* Beginning address */
	size_t		pb_len;		/* Length of protected region */
} ibt_mr_prot_bounds_t;

/* Memory Region (Re)Register attributes */
typedef struct ibt_mr_attr_s {
	ib_vaddr_t	mr_vaddr;	/* Virtual address to register */
	ib_memlen_t	mr_len;		/* Length of region to register */
	struct as	*mr_as;		/* A pointer to an address space */
					/* structure. This parameter should */
					/* be set to NULL, which implies */
					/* kernel address space. */
	ibt_mr_flags_t	mr_flags;
} ibt_mr_attr_t;

/* Physical Memory Region (Re)Register */
typedef struct ibt_pmr_attr_s {
	ib_vaddr_t	pmr_iova;	/* I/O virtual address requested by */
					/* client for the first byte of the */
					/* region */
	ib_memlen_t	pmr_len;	/* Length of region to register */
	ib_memlen_t	pmr_offset;	/* Offset of the regions starting */
					/* IOVA within the 1st physical */
					/* buffer */
	ibt_mr_flags_t	pmr_flags;
	ibt_lkey_t	pmr_lkey;	/* Reregister only */
	ibt_rkey_t	pmr_rkey;	/* Reregister only */
	uint8_t		pmr_key;	/* Key to use on new Lkey & Rkey */
	uint_t		pmr_num_buf;	/* Num of entries in the pmr_buf_list */
	size_t		pmr_buf_sz;
	ibt_phys_buf_t	*pmr_buf_list;	/* List of physical buffers accessed */
					/* as an array */
	ibt_ma_hdl_t	pmr_ma;		/* Memory handle used to obtain the */
					/* pmr_buf_list */
} ibt_pmr_attr_t;


/*
 * Memory Region (Re)Register attributes - used by ibt_register_shared_mr(),
 * ibt_register_buf() and ibt_reregister_buf().
 */
typedef struct ibt_smr_attr_s {
	ib_vaddr_t		mr_vaddr;
	ibt_mr_flags_t		mr_flags;
	uint8_t			mr_key;		/* Only for physical */
						/* ibt_(Re)register_buf() */
	ibt_lkey_t		mr_lkey;	/* Only for physical */
	ibt_rkey_t		mr_rkey;	/* ibt_Reregister_buf() */
} ibt_smr_attr_t;

/*
 * key states.
 */
typedef enum ibt_key_state_e {
	IBT_KEY_INVALID	= 0,
	IBT_KEY_FREE,
	IBT_KEY_VALID
} ibt_key_state_t;

/* Memory region query attributes */
typedef struct ibt_mr_query_attr_s {
	ibt_lkey_t		mr_lkey;
	ibt_rkey_t		mr_rkey;
	ibt_mr_prot_bounds_t	mr_lbounds;	/* Actual local CI protection */
						/* bounds */
	ibt_mr_prot_bounds_t	mr_rbounds;	/* Actual remote CI */
						/* protection bounds */
	ibt_mr_attr_flags_t	mr_attr_flags;	/* Access rights etc. */
	ibt_pd_hdl_t		mr_pd;		/* Protection domain */
	boolean_t		mr_sync_required;
	ibt_key_state_t		mr_lkey_state;
	uint_t			mr_phys_buf_list_sz;
} ibt_mr_query_attr_t;

/* Memory window query attributes */
typedef struct ibt_mw_query_attr_s {
	ibt_pd_hdl_t		mw_pd;
	ibt_mem_win_type_t	mw_type;
	ibt_rkey_t		mw_rkey;
	ibt_key_state_t		mw_state;
} ibt_mw_query_attr_t;


/* Memory Region Sync Flags. */
#define	IBT_SYNC_READ	0x1	/* Make memory changes visible to incoming */
				/* RDMA reads */

#define	IBT_SYNC_WRITE	0x2	/* Make the affects of an incoming RDMA write */
				/* visible to the consumer */

/* Memory region sync args */
typedef struct ibt_mr_sync_s {
	ibt_mr_hdl_t	ms_handle;
	ib_vaddr_t	ms_vaddr;
	ib_memlen_t	ms_len;
	uint32_t	ms_flags;	/* IBT_SYNC_READ or  IBT_SYNC_WRITE */
} ibt_mr_sync_t;

/*
 * Flags for Virtual Address to HCA Physical Address translation.
 */
typedef enum ibt_va_flags_e {
	IBT_VA_SLEEP		= 0,
	IBT_VA_NOSLEEP		= (1 << 0),
	IBT_VA_NONCOHERENT	= (1 << 1),
	IBT_VA_FMR		= (1 << 2),
	IBT_VA_BLOCK_MODE	= (1 << 3),
	IBT_VA_BUF		= (1 << 4)
} ibt_va_flags_t;


/*  Address Translation parameters */
typedef struct ibt_va_attr_s {
	ib_vaddr_t	va_vaddr;	/* Virtual address to register */
	ib_memlen_t	va_len;		/* Length of region to register */
	struct as	*va_as;		/* A pointer to an address space */
					/* structure. */
	size_t		va_phys_buf_min;
	size_t		va_phys_buf_max;
	ibt_va_flags_t	va_flags;
	struct buf	*va_buf;
} ibt_va_attr_t;


/*
 * Fast Memory Registration (FMR) support.
 */

/* FMR flush function handler. */
typedef void (*ibt_fmr_flush_handler_t)(ibt_fmr_pool_hdl_t fmr_pool,
    void *fmr_func_arg);

/* FMR Pool create attributes. */
typedef struct ibt_fmr_pool_attr_s {
	uint_t			fmr_max_pages_per_fmr;
	uint_t			fmr_pool_size;
	uint_t			fmr_dirty_watermark;
	size_t			fmr_page_sz;
	boolean_t		fmr_cache;
	ibt_mr_flags_t		fmr_flags;
	ibt_fmr_flush_handler_t	fmr_func_hdlr;
	void			*fmr_func_arg;
} ibt_fmr_pool_attr_t;


/*
 * WORK REQUEST AND WORK REQUEST COMPLETION DEFINITIONS.
 */

/*
 * Work Request and Work Request Completion types - These types are used
 *   to indicate the type of work requests posted to a work queue
 *   or the type of completion received.  Immediate Data is indicated via
 *   ibt_wr_flags_t or ibt_wc_flags_t.
 *
 *   IBT_WRC_RECV and IBT_WRC_RECV_RDMAWI are only used as opcodes in the
 *   work completions.
 *
 * NOTE: this was converted from an enum to a uint8_t to save space.
 */
typedef uint8_t ibt_wrc_opcode_t;

#define	IBT_WRC_SEND		1	/* Send */
#define	IBT_WRC_RDMAR		2	/* RDMA Read */
#define	IBT_WRC_RDMAW		3	/* RDMA Write */
#define	IBT_WRC_CSWAP		4	/* Compare & Swap Atomic */
#define	IBT_WRC_FADD		5	/* Fetch & Add Atomic */
#define	IBT_WRC_BIND		6	/* Bind Memory Window */
#define	IBT_WRC_RECV		7	/* Receive */
#define	IBT_WRC_RECV_RDMAWI	8	/* Received RDMA Write w/ Immediate */
#define	IBT_WRC_FAST_REG_PMR	9	/* Fast Register Physical mem region */
#define	IBT_WRC_LOCAL_INVALIDATE 10


/*
 * Work Request Completion flags - These flags indicate what type
 *   of data is present in the Work Request Completion structure
 */
typedef uint8_t ibt_wc_flags_t;

#define	IBT_WC_NO_FLAGS			0
#define	IBT_WC_GRH_PRESENT		(1 << 0)
#define	IBT_WC_IMMED_DATA_PRESENT	(1 << 1)
#define	IBT_WC_RKEY_INVALIDATED		(1 << 2)
#define	IBT_WC_CKSUM_OK			(1 << 3)


/*
 * Work Request Completion - This structure encapsulates the information
 *   necessary to define a work request completion.
 */
typedef struct ibt_wc_s {
	ibt_wrid_t		wc_id;		/* Work Request Id */
	uint64_t		wc_fma_ena;	/* fault management err data */
	ib_msglen_t		wc_bytes_xfer;	/* Number of Bytes */
						/* Transferred */
	ibt_wc_flags_t		wc_flags;	/* WR Completion Flags */
	ibt_wrc_opcode_t	wc_type;	/* Operation Type */
	uint16_t		wc_cksum;	/* payload checksum */
	ibt_immed_t		wc_immed_data;	/* Immediate Data */
	uint32_t		wc_freed_rc;	/* Freed Resource Count */
	ibt_wc_status_t		wc_status;	/* Completion Status */
	uint8_t			wc_sl:4;	/* Remote SL */
	uint16_t		wc_ethertype;	/* Ethertype Field - RE */
	ib_lid_t		wc_opaque1;
	uint16_t		wc_opaque2;
	ib_qpn_t		wc_qpn;		/* Source QPN Datagram only */
	ib_eecn_t		wc_opaque3;
	ib_qpn_t		wc_local_qpn;
	ibt_rkey_t		wc_rkey;
	ib_path_bits_t		wc_opaque4;
} ibt_wc_t;


/*
 * WR Flags. Common for both RC and UD
 *
 * NOTE: this was converted from an enum to a uint8_t to save space.
 */
typedef uint8_t ibt_wr_flags_t;

#define	IBT_WR_NO_FLAGS		0
#define	IBT_WR_SEND_IMMED	(1 << 0)	/* Immediate Data Indicator */
#define	IBT_WR_SEND_SIGNAL	(1 << 1)	/* Signaled, if set */
#define	IBT_WR_SEND_FENCE	(1 << 2)	/* Fence Indicator */
#define	IBT_WR_SEND_SOLICIT	(1 << 3)	/* Solicited Event Indicator */
#define	IBT_WR_SEND_REMOTE_INVAL	(1 << 4) /* Remote Invalidate */
#define	IBT_WR_SEND_CKSUM	(1 << 5)	/* Checksum offload Indicator */

/*
 * Access control flags for Bind Memory Window operation,
 * applicable for RC/UC/RD only.
 *
 * If IBT_WR_BIND_WRITE or IBT_WR_BIND_ATOMIC is desired then
 * it is required that Memory Region should have Local Write Access.
 */
typedef enum ibt_bind_flags_e {
	IBT_WR_BIND_READ	= (1 << 0),	/* enable remote read */
	IBT_WR_BIND_WRITE	= (1 << 1),	/* enable remote write */
	IBT_WR_BIND_ATOMIC	= (1 << 2),	/* enable remote atomics */
	IBT_WR_BIND_ZBVA	= (1 << 3)	/* Zero Based Virtual Address */
} ibt_bind_flags_t;

/*
 * Data Segment for scatter-gather list
 *
 * SGL consists of an array of data segments and the length of the SGL.
 */
typedef struct ibt_wr_ds_s {
	ib_vaddr_t	ds_va;		/* Virtual Address */
	ibt_lkey_t	ds_key;		/* L_Key */
	ib_msglen_t	ds_len;		/* Length of DS */
} ibt_wr_ds_t;

/*
 * Bind Memory Window WR
 *
 * WR ID from ibt_send_wr_t applies here too, SWG_0038 errata.
 */
typedef struct ibt_wr_bind_s {
	ibt_bind_flags_t	bind_flags;
	ibt_rkey_t		bind_rkey;		/* Mem Window's R_key */
	ibt_lkey_t		bind_lkey;		/* Mem Region's L_Key */
	ibt_rkey_t		bind_rkey_out;		/* OUT: new R_Key */
	ibt_mr_hdl_t		bind_ibt_mr_hdl;	/* Mem Region handle */
	ibt_mw_hdl_t		bind_ibt_mw_hdl;	/* Mem Window handle */
	ib_vaddr_t		bind_va;		/* Virtual Address */
	ib_memlen_t		bind_len;		/* Length of Window */
} ibt_wr_bind_t;

/*
 * Atomic WR
 *
 * Operation type (compare & swap or fetch & add) in ibt_wrc_opcode_t.
 *
 * A copy of the original contents of the remote memory will be stored
 * in the local data segment described by wr_sgl within ibt_send_wr_t,
 * and wr_nds should be set to 1.
 *
 * Atomic operation operands:
 *   Compare & Swap Operation:
 *	atom_arg1 - Compare Operand
 *	atom_arg2 - Swap Operand
 *
 *   Fetch & Add Operation:
 *	atom_arg1 - Add Operand
 *	atom_arg2 - ignored
 */
typedef struct ibt_wr_atomic_s {
	ib_vaddr_t	atom_raddr;	/* Remote address. */
	ibt_atom_arg_t	atom_arg1;	/* operand #1 */
	ibt_atom_arg_t	atom_arg2;	/* operand #2 */
	ibt_rkey_t	atom_rkey;	/* R_Key. */
} ibt_wr_atomic_t;

/*
 * RDMA WR
 * Immediate Data indicator in ibt_wr_flags_t.
 */
typedef struct ibt_wr_rdma_s {
	ib_vaddr_t	rdma_raddr;	/* Remote address. */
	ibt_rkey_t	rdma_rkey;	/* R_Key. */
	ibt_immed_t	rdma_immed;	/* Immediate Data */
} ibt_wr_rdma_t;

/*
 * Fast Register Physical Memory Region Work Request.
 */
typedef struct ibt_wr_reg_pmr_s {
	ib_vaddr_t	pmr_iova;	/* I/O virtual address requested by */
					/* client for the first byte of the */
					/* region */
	ib_memlen_t	pmr_len;	/* Length of region to register */
	ib_memlen_t	pmr_offset;	/* Offset of the regions starting */
					/* IOVA within the 1st physical */
					/* buffer */
	ibt_mr_hdl_t	pmr_mr_hdl;
	ibt_phys_buf_t	*pmr_buf_list;	/* List of physical buffers accessed */
					/* as an array */
	uint_t		pmr_num_buf;	/* Num of entries in the pmr_buf_list */
	ibt_lkey_t	pmr_lkey;
	ibt_rkey_t	pmr_rkey;
	ibt_mr_flags_t	pmr_flags;
	uint8_t		pmr_key;	/* Key to use on new Lkey & Rkey */
} ibt_wr_reg_pmr_t;

/*
 * Local Invalidate.
 */
typedef struct ibt_wr_li_s {
	ibt_mr_hdl_t	li_mr_hdl;	/* Null for MW invalidates */
	ibt_mw_hdl_t	li_mw_hdl;	/* Null for MR invalidates */
	ibt_lkey_t	li_lkey;	/* Ignore for MW invalidates */
	ibt_rkey_t	li_rkey;
} ibt_wr_li_t;

/*
 * Reserved For Future Use.
 * Raw IPv6 Send WR
 */
typedef struct ibt_wr_ripv6_s {
	ib_lid_t	rip_dlid;	/* DLID */
	ib_path_bits_t  rip_slid_bits;	/* SLID path bits, SWG_0033 errata */
	uint8_t		rip_sl:4;	/* SL */
	ibt_srate_t	rip_rate;	/* Max Static Rate, SWG_0007 errata */
} ibt_wr_ripv6_t;

/*
 * Reserved For Future Use.
 * Raw Ethertype Send WR
 */
typedef struct ibt_wr_reth_s {
	ib_ethertype_t  reth_type;	/* Ethertype */
	ib_lid_t	reth_dlid;	/* DLID */
	ib_path_bits_t	reth_slid_bits;	/* SLID path bits, SWG_0033 errata */
	uint8_t		reth_sl:4;	/* SL */
	ibt_srate_t	reth_rate;	/* Max Static Rate, SWG_0007 errata */
} ibt_wr_reth_t;

/*
 * Reserved For future Use.
 * RD Send WR, Operation type in ibt_wrc_opcode_t.
 */
typedef struct ibt_wr_rd_s {
	ibt_rd_dest_hdl_t	rdwr_dest_hdl;
	union {
	    ibt_immed_t		send_immed;	/* IBT_WRC_SEND */
	    ibt_wr_rdma_t	rdma;		/* IBT_WRC_RDMAR */
						/* IBT_WRC_RDMAW */
	    ibt_wr_li_t		*li;		/* IBT_WRC_LOCAL_INVALIDATE */
	    ibt_wr_atomic_t	*atomic;	/* IBT_WRC_FADD */
						/* IBT_WRC_CSWAP */
	    ibt_wr_bind_t	*bind;		/* IBT_WRC_BIND */
	    ibt_wr_reg_pmr_t	*reg_pmr;	/* IBT_WRC_FAST_REG_PMR */
	} rdwr;
} ibt_wr_rd_t;

/*
 * Reserved For Future Use.
 * UC Send WR, Operation type in ibt_wrc_opcode_t, the only valid
 * ones are:
 *		IBT_WRC_SEND
 *		IBT_WRC_RDMAW
 *		IBT_WRC_BIND
 */
typedef struct ibt_wr_uc_s {
	union {
	    ibt_immed_t		send_immed;	/* IBT_WRC_SEND */
	    ibt_wr_rdma_t	rdma;		/* IBT_WRC_RDMAW */
	    ibt_wr_li_t		*li;		/* IBT_WRC_LOCAL_INVALIDATE */
	    ibt_wr_bind_t	*bind;		/* IBT_WRC_BIND */
	    ibt_wr_reg_pmr_t	*reg_pmr;	/* IBT_WRC_FAST_REG_PMR */
	} ucwr;
} ibt_wr_uc_t;

/*
 * RC Send WR, Operation type in ibt_wrc_opcode_t.
 */
typedef struct ibt_wr_rc_s {
	union {
	    ibt_immed_t		send_immed;	/* IBT_WRC_SEND w/ immediate */
	    ibt_rkey_t		send_inval;	/* IBT_WRC_SEND w/ invalidate */
	    ibt_wr_rdma_t	rdma;		/* IBT_WRC_RDMAR */
						/* IBT_WRC_RDMAW */
	    ibt_wr_li_t		*li;		/* IBT_WRC_LOCAL_INVALIDATE */
	    ibt_wr_atomic_t	*atomic;	/* IBT_WRC_CSWAP */
						/* IBT_WRC_FADD */
	    ibt_wr_bind_t	*bind;		/* IBT_WRC_BIND */
	    ibt_wr_reg_pmr_t	*reg_pmr;	/* IBT_WRC_FAST_REG_PMR */
	} rcwr;
} ibt_wr_rc_t;

/*
 * UD Send WR, the only valid Operation is IBT_WRC_SEND.
 */
typedef struct ibt_wr_ud_s {
	ibt_immed_t		udwr_immed;
	ibt_ud_dest_hdl_t	udwr_dest;
} ibt_wr_ud_t;

/*
 * Send Work Request (WR) attributes structure.
 *
 * Operation type in ibt_wrc_opcode_t.
 * Immediate Data indicator in ibt_wr_flags_t.
 */
typedef struct ibt_send_wr_s {
	ibt_wrid_t		wr_id;		/* WR ID */
	ibt_wr_flags_t		wr_flags;	/* Work Request Flags. */
	ibt_tran_srv_t		wr_trans;	/* Transport Type. */
	ibt_wrc_opcode_t	wr_opcode;	/* Operation Type. */
	uint8_t			wr_rsvd;	/* maybe later */
	uint32_t		wr_nds;		/* Number of data segments */
						/* pointed to by wr_sgl */
	ibt_wr_ds_t		*wr_sgl;	/* SGL */
	union {
		ibt_wr_ud_t	ud;
		ibt_wr_rc_t	rc;
		ibt_wr_rd_t	rd;	/* Reserved For Future Use */
		ibt_wr_uc_t	uc;	/* Reserved For Future Use */
		ibt_wr_reth_t	reth;	/* Reserved For Future Use */
		ibt_wr_ripv6_t	ripv6;	/* Reserved For Future Use */
	} wr;				/* operation specific */
} ibt_send_wr_t;

/*
 * Receive Work Request (WR) attributes structure.
 */
typedef struct ibt_recv_wr_s {
	ibt_wrid_t		wr_id;		/* WR ID */
	uint32_t		wr_nds;		/* number of data segments */
						/* pointed to by wr_sgl */
	ibt_wr_ds_t		*wr_sgl;	/* SGL */
} ibt_recv_wr_t;


/*
 * Asynchronous Events and Errors.
 *
 * The following codes are not used in calls to ibc_async_handler, but
 * are used by IBTL to inform IBT clients of a significant event.
 *
 *  IBT_HCA_ATTACH_EVENT	- New HCA available.
 *  IBT_HCA_DETACH_EVENT	- HCA is requesting not to be used.
 *
 * ERRORs on a channel indicate that the channel has entered error state.
 * EVENTs on a channel indicate that the channel has not changed state.
 *
 */
typedef enum ibt_async_code_e {
	IBT_EVENT_PATH_MIGRATED			= 0x000001,
	IBT_EVENT_SQD				= 0x000002,
	IBT_EVENT_COM_EST			= 0x000004,
	IBT_ERROR_CATASTROPHIC_CHAN		= 0x000008,
	IBT_ERROR_INVALID_REQUEST_CHAN		= 0x000010,
	IBT_ERROR_ACCESS_VIOLATION_CHAN		= 0x000020,
	IBT_ERROR_PATH_MIGRATE_REQ		= 0x000040,

	IBT_ERROR_CQ				= 0x000080,

	IBT_EVENT_PORT_UP			= 0x000100,
	IBT_ERROR_PORT_DOWN			= 0x000200,
	IBT_ERROR_LOCAL_CATASTROPHIC		= 0x000400,

	IBT_HCA_ATTACH_EVENT			= 0x000800,
	IBT_HCA_DETACH_EVENT			= 0x001000,
	IBT_ASYNC_OPAQUE1			= 0x002000,
	IBT_ASYNC_OPAQUE2			= 0x004000,
	IBT_ASYNC_OPAQUE3			= 0x008000,
	IBT_ASYNC_OPAQUE4			= 0x010000,
	IBT_EVENT_LIMIT_REACHED_SRQ		= 0x020000,
	IBT_EVENT_EMPTY_CHAN			= 0x040000,
	IBT_ERROR_CATASTROPHIC_SRQ		= 0x080000
} ibt_async_code_t;


/*
 * ibt_ci_data_in() and ibt_ci_data_out() flags.
 */
typedef enum ibt_ci_data_flags_e {
	IBT_CI_NO_FLAGS		= 0,
	IBT_CI_COMPLETE_ALLOC	= (1 << 0)
} ibt_ci_data_flags_t;

/*
 * Used by ibt_ci_data_in() and ibt_ci_data_out() identifies the type of handle
 * mapping data is being obtained for.
 */
typedef enum ibt_object_type_e {
	IBT_HDL_HCA	=	1,
	IBT_HDL_CHANNEL,
	IBT_HDL_CQ,
	IBT_HDL_PD,
	IBT_HDL_MR,
	IBT_HDL_MW,
	IBT_HDL_UD_DEST,
	IBT_HDL_SCHED,
	IBT_HDL_OPAQUE1,
	IBT_HDL_OPAQUE2,
	IBT_HDL_SRQ
} ibt_object_type_t;

/*
 * Memory error handler data structures; code, and payload data.
 */
typedef enum ibt_mem_code_s {
	IBT_MEM_AREA	= 0x1,
	IBT_MEM_REGION	= 0x2
} ibt_mem_code_t;

typedef struct ibt_mem_data_s {
	uint64_t	ev_fma_ena;	/* FMA Error data */
	ibt_mr_hdl_t	ev_mr_hdl;	/* MR handle */
	ibt_ma_hdl_t	ev_ma_hdl;	/* MA handle */
} ibt_mem_data_t;

/*
 * Special case failure type.
 */
typedef enum ibt_failure_type_e {
	IBT_FAILURE_STANDARD	= 0,
	IBT_FAILURE_CI,
	IBT_FAILURE_IBMF,
	IBT_FAILURE_IBTL,
	IBT_FAILURE_IBCM,
	IBT_FAILURE_IBDM,
	IBT_FAILURE_IBSM
} ibt_failure_type_t;

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_IB_IBTL_IBTL_TYPES_H */
