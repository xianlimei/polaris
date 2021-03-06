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
#ident	"@(#)Makefile	1.23	06/06/07 SMI"
#
# cmd/lp/etc/Makefile
#

include			../Makefile.sp

ROOTPRINTLIB = 		$(ROOTLIB)/print
ROOTLIBLPBIN =		$(ROOTLIBLP)/bin

$(ROOTVARSPOOLPRINT)	:= OWNER=root

OWNER =			root
GROUP =			lp
FILEMODE =		0755

MSGFILES =              lpadmin
POFILE =                scripts.po

PROG =			conv_lp conv_lpd Makefile.yp
ROOTLIBPRINTPROG=	$(PROG:%=$(ROOTPRINTLIB)/%)
$(ROOTLIBPRINTPROG)	:= FILEMODE=0555
$(ROOTPRINTLIB)/Makefile.yp		:= FILEMODE=0444

USRSBINPROG=		lpsystem lpadmin
ROOTUSRSBINPROG=	$(USRSBINPROG:%=$(ROOTUSRSBIN)/%)
$(ROOTUSRSBINPROG)		:= FILEMODE=555

PCONF=	 		printers.conf
ROOTPCONF=		$(PCONF:%=$(ROOTETC)/%)
$(ROOTPCONF)		:= OWNER=root
$(ROOTPCONF)		:= GROUP=sys
$(ROOTPCONF)		:= FILEMODE=644

PPDPROGS=	 	getmakes getmodels getppdfile getppds ppdfilename2mmp	
ROOTPPDPROGS=		$(PPDPROGS:%=$(ROOTLIBLPBIN)/%)
$(ROOTPPDPROGS)		:= OWNER=root
$(ROOTPPDPROGS)		:= GROUP=lp
$(ROOTPPDPROGS)		:= FILEMODE=555

LIBLINKS=		$(ROOTLIB)/lpadmin $(ROOTLIB)/lpsystem


.KEEP_STATE:

all :			$(PROG) 

$(ROOTLIB)/print/% $(ROOTINIT_D)/%:	%
	$(INS.file)

$(ROOTLIBLPBIN)/%: %
	$(INS.file)

$(ROOTINIT_D) $(ROOTUSRSBIN) $(ROOTVARSPOOLPRINT):
	$(INS.dir)

$(ROOTLIB)/lpadmin:
	$(RM) $@; $(SYMLINK) ../sbin/lpadmin $@

$(ROOTLIB)/lpsystem:
	$(RM) $@; $(SYMLINK) ../sbin/lpsystem $@

$(ROOTLNKPROGS) :	$(ROOTSTARTPROG)
			$(RM) $@; $(LN) $(ROOTSTARTPROG) $@

_msg:   $(POFILE)

$(POFILE): $(MSGFILES)
	grep gettext $(MSGFILES) | tr '`' ' ' | sed -e "s/gettext \"/gettext \(\"/" | sed -e "s/$$/);/"  > $(POFILE).i
	$(XGETTEXT) -s $(POFILE).i
	$(RM) $@ $(POFILE).i
	mv messages.po $(POFILE)

#
# Create a message file to test with
#
_msg_test:
	grep gettext $(MSGFILES) | tr '`' ' ' | sed -e "s/gettext \"/gettext \(\"/" | sed -e "s/$$/);/"  > $(POFILE).i
	$(XGETTEXT) -s -m "xxx" $(POFILE).i
	$(RM) $@ $(POFILE).i
	mv messages.po $(POFILE)

install:		$(ROOTINIT_D) $(ROOTLNKPROGS) \
			$(ROOTLIBPRINTPROG) $(ROOTSTARTPROG) \
			$(ROOTUSRSBIN) $(ROOTUSRSBINPROG) \
			$(ROOTVARSPOOLPRINT) $(ROOTPCONF) \
			$(ROOTPPDPROGS) $(LIBLINKS)

$(SYMLINKS1):
	$(RM) $@; $(SYMLINK) ../sbin/$(SBINPROG1) $@

$(SYMLINKS2):
	$(RM) $@; $(SYMLINK) ../sbin/$(SBINPROG2) $@

clean:
	$(RM) $(POFILE)

clobber: clean

strip lint:
