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
 * ident	"@(#)ServiceLocationEnumeration.java	1.3	05/06/08 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)ServiceLocationEnumeration.java	1.3	06/08/05
//  ServiceLocationEnumeration.java:
//  Author:           James Kempf
//  Created On:       Wed May 13 17:38:01 1998
//  Last Modified By: James Kempf
//  Last Modified On: Tue May 26 13:15:57 1998
//  Update Count:     5
//

package com.sun.slp;

import java.util.*;

/**
 * The ServiceLocationEnumeration interface extends Enumeration
 * with a method that can throw an exception.
 *
 * @version 1.3 05/06/11
 * @author James Kempf
 */

public interface ServiceLocationEnumeration extends Enumeration {

    Object next() throws ServiceLocationException;

}
