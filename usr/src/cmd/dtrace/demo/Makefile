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
#pragma ident	"@(#)Makefile	1.3	05/06/08 SMI"

include ../../Makefile.cmd

DFILES = \
	applicat.d \
	badopen.d \
	begin.d \
	callout.d \
	clause.d \
	clear.d \
	countdown.d \
	counter.d \
	dateprof.d \
	delay.d \
	denorm.d \
	end.d \
	error.d \
	errorpath.d \
	find.d \
	firebird.d \
	hello.d \
	howlong.d \
	interp.d \
	interval.d \
	intr.d \
	iocpu.d \
	iosnoop.d \
	iothrough.d \
	iotime.d \
	iprb.d \
	kstat.d \
	ksyms.d \
	libc.d \
	lquantize.d \
	lwptime.d \
	normalize.d \
	nscd.d \
	pri.d \
	printa.d \
	pritime.d \
	prof.d \
	profpri.d \
	progtime.d \
	putnext.d \
	qlen.d \
	qtime.d \
	renormalize.d \
	restest.d \
	ring.d \
	rtime.d \
	rwinfo.d \
	rwtime.d \
	sig.d \
	soffice.d \
	spec.d \
	specopen.d \
	ssd.d \
	syscall.d \
	tick.d \
	ticktime.d \
	time.d \
	tracewrite.d \
	trunc.d \
	trussrw.d \
	userfunc.d \
	whatfor.d \
	whatlock.d \
	where.d \
	whererun.d \
	whoexec.d \
	whofor.d \
	whoio.d \
	whopreempt.d \
	whoqueue.d \
	whosteal.d \
	whowrite.d \
	writes.d \
	writesbycmd.d \
	writesbycmdfd.d \
	writetime.d \
	writetimeq.d \
	xioctl.d \
	xterm.d \
	xwork.d

DEMOFILES = \
	$(DFILES) \
	index.html \
	sunlogo.gif

MKDEMO	= mkdemo
CLEANFILES += $(DFILES) $(MKDEMO)

ROOTDEMODIR = $(ROOT)/usr/demo/dtrace
ROOTDEMOFILES = $(DEMOFILES:%=$(ROOTDEMODIR)/%)

all:	$(DEMOFILES)

clobber clean:
	-$(RM) $(CLEANFILES)

clean.lint install_h lint:

install: $(ROOTDEMOFILES)

$(ROOTDEMODIR):
	$(INS.dir)

$(ROOTDEMODIR)/%: %
	$(INS.file)

%.html:	$(DFILES) $(MKDEMO)	
	./$(MKDEMO) $@

%.d:	$(MKDEMO)	
	./$(MKDEMO) $@

%:	%.pl
	$(CP) $< $@
	$(CHMOD) 755 $@

$(ROOTDEMOFILES): $(ROOTDEMODIR)

$(ROOTDEMODIR) := OWNER = root
$(ROOTDEMODIR) := GROUP = bin
$(ROOTDEMODIR) := DIRMODE = 755

$(ROOTDEMOFILES) := OWNER = root
$(ROOTDEMOFILES) := GROUP = bin
$(ROOTDEMOFILES) := FILEMODE = 644
