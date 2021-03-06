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
# ident	"@(#)Makefile.com	1.2	06/08/01 SMI"
#

LIBRARY =	libmapid.a
VERS	=	.1
OBJECTS =	mapid.o

include $(SRC)/lib/Makefile.lib

LIBS	=	$(DYNLIB) $(LINTLIB)

#
# This library will be installed w/all other nfs
# binaries in /usr/lib/nfs, so define it as such.
#
ROOTLIBDIR   =	$(ROOT)/usr/lib/nfs
ROOTSLINK32  =	$(ROOTLIBDIR)/32

$(ROOTSLINK32):	$(ROOTLIBDIR)
	$(SYMLINK) . $@

#
# SRCS is defined to be $(OBJECTS:%.o=$(SRCDIR)/%.c)
#
SRCDIR	=	../common
$(LINTLIB) :=	SRCS = $(SRCDIR)/$(LINTSRC)

LDLIBS	+=	-lresolv -lcmd -lc

CFLAGS	+=	$(CCVERBOSE)
CPPFLAGS+=	-I$(SRCDIR) -D_REENTRANT

.KEEP_STATE:

all:

lint: lintcheck

include $(SRC)/lib/Makefile.targ
