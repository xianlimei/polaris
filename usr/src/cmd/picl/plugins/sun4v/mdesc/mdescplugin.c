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

#pragma ident	"@(#)mdescplugin.c	1.4	06/05/16 SMI"

/*
 * The MDESC picl plugin serves 2 different functionalities.
 * --The first is to look up certain CPU properties in the MDESC an to add
 * these properties in the already created CPU PICL nodes in the /platform
 * section of the tree.
 * --The second functionality is to create a /disk_discovery section of the
 * PICL tree which will have a disk node created for each disk node in the
 * machine description.
 */

#include "mdescplugin.h"

#pragma init(mdescplugin_register)	/* place in .init section */

picl_nodehdl_t	root_node;
md_t		*mdp;
mde_cookie_t	rootnode;

void mdescplugin_init(void);
void mdescplugin_fini(void);

extern int add_cpu_prop(picl_nodehdl_t node, void *args);
extern int disk_discovery(void);
extern md_t *mdesc_devinit(void);
extern void mdesc_devfini(md_t *mdp);
extern int update_devices(char *dev, int op);

picld_plugin_reg_t mdescplugin_reg = {
	PICLD_PLUGIN_VERSION_1,
	PICLD_PLUGIN_CRITICAL,
	"mdesc_plugin",
	mdescplugin_init,
	mdescplugin_fini
};

#define	DISK_FOUND 0x00
#define	DISK_NOT_FOUND 0x01

typedef struct disk_lookup {
	char *path;
	picl_nodehdl_t disk;
	int result;
} disk_lookup_t;

int
find_disk(picl_nodehdl_t node, void *args)
{
	disk_lookup_t *lookup  = (disk_lookup_t *)args;
	int status;
	char path[PICL_PROPNAMELEN_MAX];

	status = ptree_get_propval_by_name(node, "Path", (void *)&path,
	    PICL_PROPNAMELEN_MAX);
	if (status != PICL_SUCCESS) {
		return (PICL_WALK_CONTINUE);
	}

	if (strcmp(path, lookup->path) == 0) {
		lookup->disk = node;
		lookup->result = DISK_FOUND;
		return (PICL_WALK_TERMINATE);
	}

	return (PICL_WALK_CONTINUE);
}

/*
 * DR event handler
 * respond to the picl events:
 *      PICLEVENT_DR_AP_STATE_CHANGE
 */
static void
dr_handler(const char *ename, const void *earg, size_t size, void *cookie)
{
	nvlist_t	*nvlp = NULL;
	char		*dtype;
	char		*ap_id;
	char		*hint;


	if (strcmp(ename, PICLEVENT_DR_AP_STATE_CHANGE) != 0) {
		return;
	}

	if (nvlist_unpack((char *)earg, size, &nvlp, NULL)) {
		return;
	}

	if (nvlist_lookup_string(nvlp, PICLEVENTARG_DATA_TYPE, &dtype)) {
		nvlist_free(nvlp);
		return;
	}

	if (strcmp(dtype, PICLEVENTARG_PICLEVENT_DATA) != 0) {
		nvlist_free(nvlp);
		return;
	}

	if (nvlist_lookup_string(nvlp, PICLEVENTARG_AP_ID, &ap_id)) {
		nvlist_free(nvlp);
		return;
	}

	if (nvlist_lookup_string(nvlp, PICLEVENTARG_HINT, &hint)) {
		nvlist_free(nvlp);
		return;
	}

	mdp = mdesc_devinit();
	if (mdp == NULL) {
		nvlist_free(nvlp);
		return;
	}

	rootnode = md_root_node(mdp);

	if (strcmp(hint, DR_HINT_INSERT) == 0)
		(void) update_devices(ap_id, DEV_ADD);
	else if (strcmp(hint, DR_HINT_REMOVE) == 0)
		(void) update_devices(ap_id, DEV_REMOVE);

	mdesc_devfini(mdp);
	nvlist_free(nvlp);
}

/*
 * Discovery event handler
 * respond to the picl events:
 *      PICLEVENT_SYSEVENT_DEVICE_ADDED
 *      PICLEVENT_SYSEVENT_DEVICE_REMOVED
 */
static void
dsc_handler(const char *ename, const void *earg, size_t size, void *cookie)
{
	nvlist_t	*nvlp = NULL;
	char		*path;
	disk_lookup_t	lookup;
	int		status;

	/*
	 * retrieve the device's physical path from the event arg
	 * and determine which disk (if any) we are working with
	 */
	if (nvlist_unpack((char *)earg, size, &nvlp, NULL))
		return;
	if (nvlist_lookup_string(nvlp, "devfs-path", &path))
		return;

	lookup.path = strdup(path);
	lookup.disk = NULL;
	lookup.result = DISK_NOT_FOUND;

	status = ptree_walk_tree_by_class(root_node, "disk", (void *)&lookup,
	    find_disk);
	if (status != PICL_SUCCESS) {
		return;
	}

	if (lookup.result == DISK_FOUND) {
		if (strcmp(ename, PICLEVENT_SYSEVENT_DEVICE_ADDED) == 0)
			ptree_update_propval_by_name(lookup.disk, "State",
			    (void *)strdup(CONFIGURED), PICL_PROPNAMELEN_MAX);
		else if (strcmp(ename, PICLEVENT_SYSEVENT_DEVICE_REMOVED) == 0)
			ptree_update_propval_by_name(lookup.disk, "State",
			    (void *)strdup(UNCONFIGURED), PICL_PROPNAMELEN_MAX);
	}

	nvlist_free(nvlp);
}

void
mdescplugin_init(void)
{
	int		status;

	status = ptree_get_root(&root_node);
	if (status != PICL_SUCCESS) {
		return;
	}

	mdp = mdesc_devinit();
	if (mdp == NULL)
		return;

	rootnode = md_root_node(mdp);

	/*
	 * This is the start of the CPU property augmentation code.
	 * add_cpu_prop and the rest of the CPU code lives in cpu_prop_update.c
	 */
	status = ptree_walk_tree_by_class(root_node, "cpu", NULL, add_cpu_prop);
	if (status != PICL_SUCCESS) {
		return;
	}

	(void) disk_discovery();

	/*
	 * register dsc_handler for both "sysevent-device-added" and
	 * and for "sysevent-device-removed" PICL events
	 */
	(void) ptree_register_handler(PICLEVENT_SYSEVENT_DEVICE_ADDED,
	    dsc_handler, NULL);
	(void) ptree_register_handler(PICLEVENT_SYSEVENT_DEVICE_REMOVED,
	    dsc_handler, NULL);
	(void) ptree_register_handler(PICLEVENT_DR_AP_STATE_CHANGE,
	    dr_handler, NULL);

	mdesc_devfini(mdp);
}

void
mdescplugin_fini(void)
{
	/* unregister the event handler */
	(void) ptree_unregister_handler(PICLEVENT_SYSEVENT_DEVICE_ADDED,
	    dsc_handler, NULL);
	(void) ptree_unregister_handler(PICLEVENT_SYSEVENT_DEVICE_REMOVED,
	    dsc_handler, NULL);
	(void) ptree_unregister_handler(PICLEVENT_DR_AP_STATE_CHANGE,
	    dr_handler, NULL);
}

void
mdescplugin_register(void)
{
	picld_plugin_register(&mdescplugin_reg);
}
