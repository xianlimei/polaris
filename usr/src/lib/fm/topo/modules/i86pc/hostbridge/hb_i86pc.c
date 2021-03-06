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

#pragma ident	"@(#)hb_i86pc.c	1.3	06/05/09 SMI"

#include <fm/topo_mod.h>
#include <libdevinfo.h>
#include <strings.h>
#include "pcibus.h"
#include "hostbridge.h"
#include "did.h"
#include "util.h"

static int
hb_process(tnode_t *ptn, topo_instance_t hbi, di_node_t bn, did_hash_t *didhash,
    di_prom_handle_t promtree, topo_mod_t *mod)
{
	tnode_t *hb;

	if (did_create(didhash, bn, 0, hbi, NO_RC, TRUST_BDF, promtree) == NULL)
		return (-1);
	if ((hb = pcihostbridge_declare(ptn, bn, hbi, didhash,
	    promtree, mod)) == NULL)
		return (-1);
	return (topo_mod_enumerate(mod, hb, PCI_BUS, PCI_BUS, 0,
	    MAX_HB_BUSES));
}

static int
rc_process(tnode_t *ptn, topo_instance_t hbi, di_node_t bn, did_hash_t *didhash,
    di_prom_handle_t promtree, topo_mod_t *mod)
{
	tnode_t *hb;
	tnode_t *rc;

	if (did_create(didhash, bn, 0, hbi, hbi, TRUST_BDF, promtree) == NULL)
		return (-1);
	if ((hb = pciexhostbridge_declare(ptn, bn, hbi, didhash, promtree, mod))
	    == NULL)
		return (-1);
	if ((rc = pciexrc_declare(hb, bn, hbi, didhash, promtree, mod)) == NULL)
		return (-1);
	return (topo_mod_enumerate(mod,
	    rc, PCI_BUS, PCIEX_BUS, 0, MAX_HB_BUSES));
}


int
pci_hostbridges_find(tnode_t *ptn, did_hash_t *didhash,
    di_prom_handle_t promtree, topo_mod_t *mod)
{
	di_node_t devtree;
	di_node_t pnode;
	di_node_t cnode;
	int hbcnt = 0;

	/* Scan for buses, top-level devinfo nodes with the right driver */
	devtree = di_init("/", DINFOCPYALL);
	if (devtree == DI_NODE_NIL) {
		topo_mod_dprintf(mod, "devinfo init failed.");
		topo_node_range_destroy(ptn, HOSTBRIDGE);
		return (0);
	}

	pnode = di_drv_first_node(PCI, devtree);
	while (pnode != DI_NODE_NIL) {
		if (hb_process(ptn, hbcnt++, pnode, didhash, promtree, mod)
		    < 0) {
			di_fini(devtree);
			topo_node_range_destroy(ptn, HOSTBRIDGE);
			return (topo_mod_seterrno(mod, EMOD_PARTIAL_ENUM));
		}
		pnode = di_drv_next_node(pnode);
	}

	pnode = di_drv_first_node(NPE, devtree);
	while (pnode != DI_NODE_NIL) {
		for (cnode = di_child_node(pnode); cnode != DI_NODE_NIL;
		    cnode = di_sibling_node(cnode)) {
			if (di_driver_name(cnode) == NULL)
				continue;
			if (strcmp(di_driver_name(cnode), PCI_PCI) == 0) {
				if (hb_process(ptn, hbcnt++, cnode, didhash,
				    promtree, mod) < 0) {
					di_fini(devtree);
					topo_node_range_destroy(ptn,
					    HOSTBRIDGE);
					return (topo_mod_seterrno(mod,
					    EMOD_PARTIAL_ENUM));
				}
			}
			if (strcmp(di_driver_name(cnode), PCIE_PCI) == 0) {
				if (rc_process(ptn, hbcnt++, cnode, didhash,
				    promtree, mod) < 0) {
					di_fini(devtree);
					topo_node_range_destroy(ptn,
					    HOSTBRIDGE);
					return (topo_mod_seterrno(mod,
					    EMOD_PARTIAL_ENUM));
				}
			}
		}
		pnode = di_drv_next_node(pnode);
	}
	di_fini(devtree);
	return (0);
}

/*ARGSUSED*/
int
platform_hb_enum(tnode_t *parent, const char *name,
    topo_instance_t imin, topo_instance_t imax, did_hash_t *didhash,
    di_prom_handle_t promtree, topo_mod_t *mod)
{
	return (pci_hostbridges_find(parent, didhash, promtree, mod));
}

/*ARGSUSED*/
int
platform_hb_label(tnode_t *node, nvlist_t *in, nvlist_t **out, topo_mod_t *mod)
{
	return (labelmethod_inherit(mod, node, in, out));
}
