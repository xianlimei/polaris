#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)Makefile	1.4	04/02/15 SMI"
#

PROG= xntpdc

all: $(PROG) 

include ../../Makefile.cmd
include ../Makefile.cmd

.KEEP_STATE:

.PARALLEL:

CLOBBERFILES = $(ROOTUSRSBINPROG)

install: all $(ROOTUSRSBINPROG)

clean:

include ../../Makefile.targ

LDLIBS += ../$(LIBNTP_A) $(LIBS)

OBJS=	ntpdc.o ntpdc_ops.o version.o
SOURCE=	ntpdc.c ntpdc_ops.c

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

lint:
	$(LINT.c) -x -u $(DEFS) $(INCL) $(SOURCE) $(LDLIBS)

clean:
	$(RM) $(OBJS) $(PROG)
