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
# ident	"@(#)Makefile	1.14	05/06/08 SMI"
#
#	Makefile for nispasswd daemon
#
# cmd/rpcsvc/nis/rpc.nispasswdd/Makefile
#

PROG = rpc.nispasswdd

DERIVED_FILES = nispasswd_xdr.c

SRCS = rpc.nispasswdd.c npd_svc.c npd_cache.c npd_svcsubr.c npd_ypfwd.c $(DERIVED_FILES)

OBJS = $(SRCS:%.c=%.o)

include $(SRC)/cmd/Makefile.cmd

PROTOCOL_DIR = $(ROOT)/usr/include/rpcsvc
LDLIBS += -lnsl

# rpcgen flags
RPCGENFLAGS= -c -C -M

CPPFLAGS += -I$(SRC)/lib/libnsl/include  -I$(SRC)/lib/nsswitch/nisplus/common \
	-DDEBUG -D_REENTRANT

#
LINTFLAGS= -L$(SRC)/lib/libnsl -c
LINTOUT= lint.out

.KEEP_STATE:

all := TARGET= all
install := TARGET= install
clean := TARGET= clean
lint := TARGET= lint

all: $(DERIVED_FILES) $(PROG)
install: all $(ROOTUSRSBINPROG)
clean:
	$(RM) core $(DERIVED_FILES) $(OBJS) $(PROG)
lint:
	$(LINT.c) $(SRCS) $(LDLIBS) > $(LINTOUT) 2>&1

#
# build rules
#
$(PROG): $(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(DERIVED_FILES) : $(PROTOCOL_DIR)/nispasswd.x
	$(RPCGEN) $(RPCGENFLAGS) $(PROTOCOL_DIR)/nispasswd.x > $(DERIVED_FILES)

include $(SRC)/cmd/Makefile.targ
