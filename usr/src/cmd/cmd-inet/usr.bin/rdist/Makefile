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
# ident	"@(#)Makefile	1.15	05/06/08 SMI"
#
# cmd/cmd-inet/usr.bin/rdist/Makefile

PROG=	rdist
OBJS=	docmd.o expand.o lookup.o main.o server.o
KCMDOBJS=	kcmd.o
YOBJS=	gram.o
SRCS=	$(OBJS:.o=.c) $(CMDINETCOMMONDIR)/$(KCMDOBJS:.o=.c)
YSRCS=	$(YOBJS:.o=.y)

include ../../../Makefile.cmd
include ../../Makefile.cmd-inet

CPPFLAGS +=	-DSYSV -DSTRNET -DBSD_COMP
LDLIBS +=	-lsocket -lnsl
LDFLAGS +=	$(ZLAZYLOAD)

include  $(SRC)/lib/gss_mechs/mech_krb5/Makefile.mech_krb5
LDFLAGS += $(KRUNPATH) -L$(ROOT)$(KLIBDIR_DO) -L$(ROOT)$(KLIBDIR_GL)
LDLIBS +=  -lnsl -lmech_krb5
CPPFLAGS += -I. -I$(CMDINETCOMMONDIR) \
		-I$(SRC)/lib/gss_mechs/mech_krb5 \
		-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
		-I$(SRC)/lib/gss_mechs/mech_krb5/include \
		-I$(SRC)/lib/gss_mechs/mech_krb5/include/krb5

ROOTSYMLINK=	$(ROOT)/usr/ucb/$(PROG)

# conditional assignments
$(ROOTPROG) :=      FILEMODE=       04555
$(ROOTPROG) :=      OWNER=          root

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS) $(KCMDOBJS) $(YOBJS)
	$(LINK.c) $(OBJS) $(KCMDOBJS) $(YOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG) $(ROOTSYMLINK)

$(ROOTSYMLINK):
	$(RM) $@; $(SYMLINK) ../bin/$(PROG) $@

clean:
	$(RM) $(OBJS) $(KCMDOBJS) $(YOBJS)

lint:
	$(YACC.y) $(YSRCS)
	$(LINT.c) $(SRCS) y.tab.c $(LDLIBS)
	$(RM) y.tab.c


include ../../../Makefile.targ
