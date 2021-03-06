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
# ident	"@(#)Makefile	1.3	06/07/29 SMI"
#

include			../Makefile.lib

SUBDIRS =	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
_msg :=		TARGET= _msg
package :=	TARGET= package

LIBRARY=	libsldap.a
TEXT_DOMAIN =	SUNW_OST_OSLIB
XGETFLAGS=
POFILE=		$(LIBRARY:.a=.po)
POFILES=	generic.po

.KEEP_STATE:

all clean clobber delete install lint catalog package: $(SUBDIRS)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

_msg:	$(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(RM) generic.po
	cp $(POFILE) $(MSGDOMAIN)

$(POFILE): $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

$(MSGDOMAIN):
	$(INS.dir)

$(POFILES):
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS)  common/*.[ch]*
	sed "/^domain/d" < messages.po  > $@
	$(RM) messages.po

FRC:

# EXPORT DELETE START
EXPORT_SRC:
	$(RM) -f common/ns_crypt.c+ Makefile+
	sed -e "/EXPORT DELETE START/,/EXPORT DELETE END/d" < \
		common/ns_crypt.c > common/ns_crypt.c+
	$(MV) common/ns_crypt.c+ common/ns_crypt.c
	sed -e "/^# EXPORT DELETE START/,/^# EXPORT DELETE END/d" \
		< Makefile > Makefile+
	$(MV) Makefile+ Makefile
	$(CHMOD) 444 Makefile common/ns_crypt.c

# This target cleans up the SCCS files as well as the plain src.
EXPORT_WS:
	$(RM) -f common/ns_crypt.c+ Makefile+

	sccs edit common/ns_crypt.c
	sed -e "/EXPORT DELETE START/,/EXPORT DELETE END/d" \
		< ns_crypt.c > common/ns_crypt.c+
	sccs unedit common/ns_crypt.c
	$(RM) -f common/ns_crypt.c ns_crypt.c
	$(RM) -f common/SCCS/s.ns_crypt.c
	$(MV) common/ns_crypt.c+ common/ns_crypt.c
	sccs create common/ns_crypt.c
	$(RM) common/,ns_crypt.c

	sccs edit Makefile
	sed -e "/^# EXPORT DELETE START/,/^# EXPORT DELETE END/d" \
		< Makefile > Makefile+
	sccs unedit Makefile
	$(RM) -f Makefile
	$(RM) -f SCCS/s.Makefile
	$(MV) Makefile+ Makefile
	sccs create Makefile
	$(RM) ,Makefile

# EXPORT DELETE END
