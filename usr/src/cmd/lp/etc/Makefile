#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# cmd/lp/etc/Makefile
#
#ident	"@(#)Makefile	1.13	05/06/08 SMI"

include			../Makefile.lp

OTHERPROG =     Systems
 
ROOTOTHERPROG = $(OTHERPROG:%=$(ROOTETCLP)/%)
 
TXTS =          $(OTHERPROG)
 
$(ROOTOTHERPROG) :=     OWNER = lp
$(ROOTOTHERPROG) :=     GROUP = lp
$(ROOTOTHERPROG) :=     FILEMODE = 664

OWNER =			root
GROUP =			sys
FILEMODE =		0744

.KEEP_STATE:

install:		$(ROOTOTHERPROG)

clean clobber strip lint all:
