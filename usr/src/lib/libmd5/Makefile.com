#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)Makefile.com	1.10	06/08/03 SMI"
#

LIBRARY= libmd5.a
VERS= .1

include ../../Makefile.lib
include ../../Makefile.rootfs

DYNFLAGS +=     -F libmd.so.1

LIBS =          $(DYNLIB) $(LINTLIB)

SRCDIR =        ../common
$(LINTLIB) :=   SRCS = $(SRCDIR)/llib-lmd5

MAPFILES +=	$(MAPFILE-FLTR)

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Also, since there are no OBJECTS, turn
# off CTF.

BUILD.SO=       $(LD) -o $@ -G $(DYNFLAGS)
CTFMERGE_LIB=   :

.KEEP_STATE:

all: $(LIBS)

lint:

include ../../Makefile.targ
