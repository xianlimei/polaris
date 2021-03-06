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
# ident	"@(#)Makefile	1.5	06/07/11 SMI"
# 

PROG= bootadm

i386_DATA= filelist.ramdisk filelist.safe

SBINLINKS= $(PROG)

OBJS = bootadm.o
SRCS = $(OBJS:.o=.c)

include ../Makefile.com

ROOTBOOTSOLARISDATA= $($(MACH)_DATA:%=$(ROOTBOOTSOLARIS)/%)
ROOTUSRSBINLINKS= $(SBINLINKS:%=$(ROOTUSRSBIN)/%)

$(ROOTBOOTSOLARISDATA) :=	OWNER = root
$(ROOTBOOTSOLARISDATA) :=	GROUP = sys
$(ROOTBOOTSOLARISDATA) :=	FILEMODE = 0644

.KEEP_STATE:

LDLIBS +=	-lnvpair

# Writing into string literals is incorrect.  We need to match gcc's
# behavior, which causes us to take SIGSEGV on such a write.
CFLAGS +=	$(XSTRCONST)
CPPFLAGS +=	-D_FILE_OFFSET_BITS=64

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(ROOTUSRSBINLINKS):
	-$(RM) $@; $(SYMLINK) ../../sbin/$(@F) $@

install: all $(ROOTSBINPROG) $(ROOTBOOTSOLARISDATA) .WAIT $(ROOTUSRSBINLINKS)

clean:

_msg:

lint:	lint_PROG

include ../Makefile.targ
