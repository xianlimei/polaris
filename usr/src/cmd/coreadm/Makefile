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
#ident	"@(#)Makefile	1.5	05/06/08 SMI"
#

PROG=		coreadm
MANIFEST=	coreadm.xml

include ../Makefile.cmd

ROOTMANIFESTDIR=	$(ROOTSVCSYSTEM)
$(ROOTMANIFEST)		:= FILEMODE= 444

$(ROOTMANIFESTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

CFLAGS += $(CCVERBOSE)
LDLIBS += -lproc

all: $(PROG)

install: all $(ROOTPROG) $(ROOTMANIFEST)

clean:

check:	$(CHKMANIFEST)

lint:	lint_PROG

include ../Makefile.targ
