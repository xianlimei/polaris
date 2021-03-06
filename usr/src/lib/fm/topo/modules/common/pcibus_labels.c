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

#pragma ident	"@(#)pcibus_labels.c	1.4	06/05/09 SMI"

#include <libnvpair.h>
#include <fm/topo_mod.h>
#include <assert.h>
#include <string.h>

#include "topo_error.h"
#include "did.h"
#include "pcibus.h"
#include "pcibus_labels.h"

extern slotnm_rewrite_t *Slot_Rewrites;
extern physlot_names_t *Physlot_Names;
extern missing_names_t *Missing_Names;

static const char *
pci_physslot_name_lookup(char *platform, did_t *dp)
{
	const char *rlabel = NULL;
	int n, p, i;

	if ((n = did_physslot(dp)) < 0 || Physlot_Names == NULL)
		return (NULL);

	for (p = 0; p < Physlot_Names->psn_nplats; p++) {
		if (strcmp(Physlot_Names->psn_names[p].pnm_platform,
		    platform) != 0)
			continue;
		for (i = 0; i < Physlot_Names->psn_names[p].pnm_nnames; i++) {
			physnm_t ps;
			ps = Physlot_Names->psn_names[p].pnm_names[i];
			if (ps.ps_num == n) {
				rlabel = ps.ps_label;
				break;
			}
		}
		break;
	}
	return (rlabel);
}

static const char *
pci_slotname_rewrite(char *platform, const char *label)
{
	const char *rlabel = label;
	int s, i;

	if (Slot_Rewrites == NULL)
		return (rlabel);

	for (s = 0; s < Slot_Rewrites->srw_nplats; s++) {
		if (strcmp(Slot_Rewrites->srw_platrewrites[s].prw_platform,
		    platform) != 0)
			continue;
		for (i = 0;
		    i < Slot_Rewrites->srw_platrewrites[s].prw_nrewrites;
		    i++) {
			slot_rwd_t rw;
			rw = Slot_Rewrites->srw_platrewrites[s].prw_rewrites[i];
			if (strcmp(rw.srw_obp, label) == 0) {
				rlabel = rw.srw_new;
				break;
			}
		}
		break;
	}
	assert(rlabel != NULL);
	return (rlabel);
}

static const char *
pci_missing_match(char *platform, did_t *dp, topo_mod_t *mod)
{
	const char *rlabel = NULL;
	int board, bridge, rc, bus, dev;
	int p, i;

	if (Missing_Names == NULL)
		return (NULL);
	bridge = did_bridge(dp);
	board = did_board(dp);
	rc = did_rc(dp);
	did_BDF(dp, &bus, &dev, NULL);

	topo_mod_dprintf(mod, "Missing a name for %d, %d, %d, %d, %d ?\n",
	    board, bridge, rc, bus, dev);

	for (p = 0; p < Missing_Names->mn_nplats; p++) {
		if (strcmp(Missing_Names->mn_names[p].pdl_platform,
		    platform) != 0)
			continue;
		for (i = 0; i < Missing_Names->mn_names[p].pdl_nnames; i++) {
			devlab_t m;
			m = Missing_Names->mn_names[p].pdl_names[i];
			if (m.dl_board == board && m.dl_bridge == bridge &&
			    m.dl_rc == rc && m.dl_bus == bus &&
			    m.dl_dev == dev) {
				rlabel = m.dl_label;
				break;
			}
		}
		break;
	}
	return (rlabel);
}

static const char *
pci_slotname_lookup(tnode_t *node, did_t *dp, topo_mod_t *mod)
{
	const char *l;
	char *plat;
	int err;
	int d;

	if (topo_prop_get_string(node,
	    TOPO_PGROUP_SYSTEM, TOPO_PROP_PLATFORM, &plat, &err) < 0) {
		(void) topo_mod_seterrno(mod, err);
		return (NULL);
	}
	did_BDF(dp, NULL, &d, NULL);
	if ((l = pci_physslot_name_lookup(plat, dp)) == NULL)
		if ((l = did_label(dp, d)) != NULL) {
			l = pci_slotname_rewrite(plat, l);
		} else {
			l = pci_missing_match(plat, dp, mod);
		}
	topo_mod_strfree(mod, plat);
	return (l);
}

int
pci_label_cmn(tnode_t *node, nvlist_t *in, nvlist_t **out, topo_mod_t *mod)
{
	uint64_t ptr;
	const char *l;
	did_t *dp;
	char *nm;
	int err;

	/*
	 * If it's not a device or a PCI-express bus (which could potentially
	 * represent a slot, and therefore we might need to capture its slot
	 * name information), just inherit any label from our parent
	 */
	*out = NULL;
	nm = topo_node_name(node);
	if (strcmp(nm, PCI_DEVICE) != 0 && strcmp(nm, PCIEX_DEVICE) != 0 &&
	    strcmp(nm, PCIEX_BUS) != 0) {
		if (topo_node_label_set(node, NULL, &err) < 0)
			if (err != ETOPO_PROP_NOENT)
				return (topo_mod_seterrno(mod, err));
		return (0);
	}

	if (nvlist_lookup_uint64(in, TOPO_METH_LABEL_ARG_NVL, &ptr) != 0) {
		topo_mod_dprintf(mod,
		    "label method argument not found.\n");
		return (-1);
	}
	dp = (did_t *)(uintptr_t)ptr;

	/*
	 * Is there a slotname associated with the device?
	 */
	if ((l = pci_slotname_lookup(node, dp, mod)) != NULL) {
		nvlist_t *rnvl;

		if (topo_mod_nvalloc(mod, &rnvl, NV_UNIQUE_NAME) != 0 ||
		    nvlist_add_string(rnvl, TOPO_METH_LABEL_RET_STR, l) != 0)
			return (topo_mod_seterrno(mod, EMOD_FMRI_NVL));
		*out = rnvl;
		return (0);
	} else {
		if (topo_node_label_set(node, NULL, &err) < 0)
			if (err != ETOPO_PROP_NOENT)
				return (topo_mod_seterrno(mod, err));
		return (0);
	}
}
