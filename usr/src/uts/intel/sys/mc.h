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
 *
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_MC_H
#define	_SYS_MC_H

#pragma ident	"@(#)mc.h	1.1	06/02/11 SMI"

/*
 * Public interfaces exposed by the memory controller driver
 */

#include <sys/cpuvar.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	MC_UNUM_NAMLEN		192
#define	MC_UNUM_NDIMM		8

typedef struct mc_unum {
	int unum_board;
	int unum_chip;
	int unum_mc;
	int unum_cs;
	uint64_t unum_offset;
	int unum_dimms[MC_UNUM_NDIMM];
} mc_unum_t;

#define	MC_AMD_DEV_OFFSET	24	/* node ID + offset == PCI dev num */

#define	MC_IOC			(0x4d43 << 16)
#define	MC_IOC_SNAPSHOT_INFO	(MC_IOC | 1)
#define	MC_IOC_SNAPSHOT		(MC_IOC | 2)

/*
 * Prior to requesting a copy of the snapshot, consumers are advised to request
 * information regarding the snapshot.  An mc_snapshot_info_t will be returned,
 * containing the snapshot size as well as the snapshot generation number.  Note
 * that, due to the potentially dynamic nature of the system, the snapshot may
 * change at any time.  As such, the information in the mc_snapshot_info_t may
 * be out of date by the time it is used.  The generation number is used to
 * track snapshot changes.  That is, the generation number will be updated each
 * time the source data for the snapshot is updated.  The consumer should not
 * attach any meaning to the magnitude of a generation number change, and pay
 * attention only to the fact that the number has changed.
 */
typedef struct mc_snapshot_info {
	uint32_t mcs_size;	/* snapshot size */
	uint_t mcs_gen;		/* snapshot generation number */
} mc_snapshot_info_t;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_MC_H */
