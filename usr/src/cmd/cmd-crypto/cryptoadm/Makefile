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
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)Makefile	1.10	05/06/08 SMI"
#

PROG = cryptoadm

OBJS =  cryptoadm.o \
	adm_uef.o \
	adm_kef.o \
	adm_kef_ioctl.o \
	adm_kef_util.o \
	adm_util.o \
	start_stop.o \
	adm_metaslot.o

SRCS = $(OBJS:.o=.c)

include ../../Makefile.cmd 

POFILES = $(OBJS:%.o=%.po)
POFILE = $(PROG)_all.po

CFLAGS += $(CCVERBOSE)

LDLIBS += -lcryptoutil
LDFLAGS += $(ZLAZYLOAD)

.KEEP_STATE:

all:    $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS) $(DYNFLAGS)
	$(POST_PROCESS)

$(POFILE): $(POFILES)
	$(RM) $@; cat $(POFILES) > $@

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) -f $(OBJS) $(PROG)

lint:   lint_SRCS

include ../../Makefile.targ
