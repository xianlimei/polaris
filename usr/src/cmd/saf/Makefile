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
# ident	"@(#)Makefile	1.18	05/06/08 SMI"
#

PROG= sac sacadm pmadm

include ../Makefile.cmd

MANIFEST = sac.xml

ROOTMANIFESTDIR = $(ROOTSVCSYSTEM)

SACOBJ= sac.o readtab.o global.o log.o misc.o
SACOBJS= $(SACOBJ) util1.o
SACADMOBJ= sacadm.o admutil.o
SACADMOBJS= $(SACADMOBJ) util2.o
PMADMOBJ= pmadm.o admutil.o
PMADMOBJS= $(PMADMOBJ) util2.o
SACSRC= $(SACOBJ:%.o=%.c)
SACSRCS= $(SACSRC) util.c
SACADMSRC= $(SACADMOBJ:%.o=%.c)
SACADMSRCS= $(SACADMSRC) util.c
PMADMSRC= $(PMADMOBJ:%.o=%.c)
PMADMSRCS= $(PMADMSRC) util.c
OBJS= $(SACOBJS) $(SACADMOBJS) $(PMADMOBJS)

LIBSAFD = $(ROOTLIB)/saf
DIRS= $(LIBSAFD)
SACF= sac
ADMF= sacadm pmadm
ROOTSACF= $(SACF:%=$(LIBSAFD)/%)
ROOTADMF= $(ADMF:%=$(ROOTUSRSBIN)/%)

LDLIBS += -lnsl
LINTFLAGS += -b -x
OWNER= root
GROUP= sys

util1.o := CPPFLAGS += -DSAC
util2.o := CPPFLAGS += -USAC

sac :=	LDLIBS += -lpam

$(ROOTUSRSBIN)/sacadm :=	FILEMODE =	04755

$(ROOTSVCSYSTEM)/sac.xml := OWNER = root
$(ROOTSVCSYSTEM)/sac.xml := GROUP = sys
$(ROOTSVCSYSTEM)/sac.xml := FILEMODE = 0444

$(LIBSAFD)/% : %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(PROG) 

sac: $(SACOBJS)
	$(LINK.c) $(SACOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

sacadm: $(SACADMOBJS)
	$(LINK.c) $(SACADMOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

pmadm: $(PMADMOBJS)
	$(LINK.c) $(PMADMOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

util1.o: util.c
	$(COMPILE.c) -o $@ util.c

util2.o: util.c
	$(COMPILE.c) -o $@ util.c

install: all .WAIT $(ROOTSACF) $(ROOTADMF) $(ROOTMANIFEST)

# Don't install dirs already installed by Targetdirs
#$(DIRS):
#	$(INS.dir)

check:	$(CHKMANIFEST)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SACSRCS)
	$(LINT.c) $(SACADMSRCS)
	$(LINT.c) $(PMADMSRCS)

include ../Makefile.targ
