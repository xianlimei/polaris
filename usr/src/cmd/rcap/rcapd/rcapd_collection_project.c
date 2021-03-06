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

#pragma ident	"@(#)rcapd_collection_project.c	1.2	05/06/08 SMI"

#include <procfs.h>
#include <project.h>
#include <stdlib.h>
#include <strings.h>
#include "rcapd.h"
#include "utils.h"

				/* absolute cap name */
#define	PJ_ABS_ATTR_NAME	"rcap.max-rss"
				/* round up to next y = 2^n */
#define	ROUNDUP(x, y)		(((x) + ((y) - 1)) & ~((y) - 1))

static rcid_t rc_proj_getidbypsinfo(psinfo_t *);

void
lcollection_set_type_project(void)
{
	rc_getidbypsinfo = rc_proj_getidbypsinfo;
}

static int
lcollection_update_project_cb(const struct project *proj, void *walk_data)
{
	void(*update_notification_cb)(char *, int, uint64_t, int) =
	    (void(*)(char *, int, uint64_t, int))walk_data;
	char *capattr_abs;
	char *end;
	int changes;
	int64_t max_rss;
	lcollection_t *lcol;

	capattr_abs = strstr(proj->pj_attr, PJ_ABS_ATTR_NAME "=");
	if (capattr_abs != NULL) {
		if (capattr_abs > proj->pj_attr)
			if (*(capattr_abs - 1) != ';') {
				/*
				 * PJ_ABS_ATTR_NAME only matched part
				 * of an attribute.
				 */
				return (0);
			}
		capattr_abs += strlen(PJ_ABS_ATTR_NAME "=");
		max_rss = ROUNDUP(strtoll(capattr_abs, &end, 10), 1024) / 1024;
		if (end == capattr_abs || *end != ';' && *end != 0)
			warn(gettext("%s %s: malformed %s value "
			    "'%s'\n"), rcfg.rcfg_mode_name, proj->pj_name,
			    PJ_ABS_ATTR_NAME, capattr_abs);
	} else
		max_rss = 0;

	lcol = lcollection_insert_update(proj->pj_projid, max_rss,
	    proj->pj_name, &changes);
	if (update_notification_cb != NULL)
		update_notification_cb(proj->pj_name, changes, max_rss, (lcol !=
		    NULL) ? lcol->lcol_mark : 0);

	return (0);
}

static int
lcollection_update_project_byid_cb(const projid_t id, void *walk_data)
{
	char buf[PROJECT_BUFSZ];
	struct project proj;

	if (getprojbyid(id, &proj, buf, sizeof (buf)) != NULL && proj.pj_attr !=
	    NULL)
		return (lcollection_update_project_cb(&proj, walk_data));

	return (0);
}

static int
lcollection_update_onceactive_cb(lcollection_t *lcol, void *walk_data)
{
	void(*update_notification_cb)(char *, int, uint64_t, int) =
	    (void(*)(char *, int, uint64_t, int))walk_data;

	return (lcollection_update_project_byid_cb(lcol->lcol_id,
	    (void *)update_notification_cb));
}

static int
project_walk_all(int(*cb)(const struct project *, void *), void *walk_data)
{
	char buf[PROJECT_BUFSZ];
	struct project proj;
	int res = 0;

	setprojent();
	while (getprojent(&proj, buf, sizeof (buf)) != NULL && res == 0)
		res = cb(&proj, walk_data);
	endprojent();

	return (res);
}

void
lcollection_update_project(lcollection_update_type_t ut,
    void(*update_notification_cb)(char *, int, uint64_t, int))
{
	switch (ut) {
	case LCU_ACTIVE_ONLY:
		/*
		 * Enumerate active projects.  This is much faster than
		 * enumerating all projects (as is done below, in the default
		 * case), and is done to efficiently and incrementally update
		 * lcollection with capped projects.  The default case performs
		 * the initialization.
		 */
		(void) project_walk(lcollection_update_project_byid_cb,
		    (void *)update_notification_cb);
		/*
		 * Enumerate once-active projects, including the active
		 * projects just enumerated, meaning active projects will be
		 * updated and marked twice.
		 */
		list_walk_collection(lcollection_update_onceactive_cb,
		    (void *)update_notification_cb);
		break;
	default:
		/*
		 * Enumerate all projects.
		 */
		(void) project_walk_all(lcollection_update_project_cb,
		    (void *)update_notification_cb);
	}
}

static rcid_t
rc_proj_getidbypsinfo(psinfo_t *psinfo)
{
	return (psinfo->pr_projid);
}
