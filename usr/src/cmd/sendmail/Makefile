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
#ident	"@(#)Makefile	1.44	05/06/08 SMI"
#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# cmd/sendmail/Makefile
#
# Makefile for top level sendmail source directory
#

# static libraries (db, libsm, libsmdb, libsmutil) must be built before
# src and aux
SUBDIRS=	cf lib db libsm libsmdb libsmutil .WAIT src aux

include 	../Makefile.cmd

ROOTDIRS=	$(ROOT)/var/spool/mqueue $(ROOT)/var/spool/clientmqueue
$(ROOTDIRS):= 	OWNER = root
$(ROOTDIRS):=	DIRMODE = 0750
$(ROOT)/var/spool/clientmqueue:=	OWNER = smmsp
$(ROOT)/var/spool/clientmqueue:=	GROUP = smmsp
$(ROOT)/var/spool/clientmqueue:=	DIRMODE = 0770

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint

.KEEP_STATE:

all clean clobber lint:	$(SUBDIRS)

install: 	$(ROOTDIRS) $(SUBDIRS) $(ROOTMANIFEST) $(ROOTSVCSH)

.PARALLEL:	$(SUBDIRS)

$(ROOTDIRS):
	$(INS.dir)

$(SUBDIRS): 	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
