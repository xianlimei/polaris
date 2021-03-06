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
 * ident	"@(#)Solaris_ActiveProjectProperties.java	1.2	05/06/08 SMI"
 *
 * Copyright (c) 2001 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Solaris_ActiveProjectProperties.java
 */

package com.sun.wbem.solarisprovider.srm;

/**
 * Defines property names of the Solaris_ActiveProjectProperties class and
 * the corresponding keys in the RDS protocol
 */
public interface Solaris_ActiveProjectProperties {
    /** The name of the project name property */
    static final String PROJECTNAME =	"ProjectName";
    static final String PROJECTNAME_KEY =	"prj_name";
    /** The name of the project ID property */
    static final String PROJECTID =	"ProjectID";
    static final String PROJECTID_KEY =	"prj_id";
}
