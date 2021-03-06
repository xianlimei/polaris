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
#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# uts/sparc/usbsksp/Makefile

#pragma ident	"@(#)Makefile	1.2	05/12/29 SMI"

#
#	This makefile drives the production of the Keyspan USB Serial
#	Adapter driver.
#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= usbsksp
OBJECTS		= $(USBSER_KEYSPAN_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(USBSER_KEYSPAN_OBJS:%.o=$(LINTS_DIR)/%.ln)
WARLOCK_OK	= $(MODULE).ok
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

# 
# lint pass one enforcement 
# 
CFLAGS += $(CCVERBOSE)

LDFLAGS         += -dy -Nmisc/usba -Nmisc/usbser -Nmisc/usbs49_fw

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

.KEEP_STATE:

all:		$(ALL_DEPS)

def:		$(DEF_DEPS)

clean:		$(CLEAN_DEPS); \
                $(RM) $(WARLOCK_OUT) $(WARLOCK_OK)

clobber:	$(CLOBBER_DEPS); \
                $(RM) $(WARLOCK_OUT) $(WARLOCK_OK)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

#
#	Defines for local commands.
#
WLCC		= wlcc
TOUCH		= touch
WARLOCK		= warlock
SCCS		= sccs
TEST		= test

#
#	warlock
#
WARLOCK_OUT	= usbser_keyspan.ll keyspan_dsd.ll \
		  keyspan_pipe.ll
WARLOCK_CMD	= usbser_keyspan.wlcmd

USBSER_FILES = $(USBSER_OBJS:%.o=../usbser/%.ll)
USBA_FILES = $(USBA_OBJS:%.o=../usba/%.ll)
UHCI_FILES = $(UHCI_OBJS:%.o=../uhci/%.ll)
OHCI_FILES = $(OHCI_OBJS:%.o=../ohci/%.ll)
EHCI_FILES = $(EHCI_OBJS:%.o=../ehci/%.ll)

warlock: $(WARLOCK_OK) warlock_with_usba

%.wlcmd:
	$(TEST) -f $@ || $(SCCS) get $@

$(WARLOCK_OK): warlock_with_usbser
	$(TOUCH) $@

%.ll: $(UTSBASE)/common/io/usb/clients/usbser/usbser_keyspan/%.c
	$(WLCC) $(CPPFLAGS) -DDEBUG -o $@ $<

warlock_with_usbser: $(WARLOCK_OUT) usbser_files warlock_ddi.files
	$(WARLOCK) -c $(WARLOCK_CMD) $(WARLOCK_OUT) \
		$(USBSER_FILES) -l ../warlock/ddi_dki_impl.ll

usbser_files:
	@cd ../usbser; pwd; $(MAKE) usbser.ll

warlock_with_usba: $(WARLOCK_CMD) $(WARLOCK_OUT) usbser_files \
	usba_files ohci_files uhci_files ehci_files warlock_ddi.files
	$(WARLOCK) -c usbser_keyspan_with_usba.wlcmd \
		$(USBA_FILES) $(OHCI_FILES) $(EHCI_FILES) $(UHCI_FILES) \
		$(USBSER_FILES) \
		$(WARLOCK_OUT) -l ../warlock/ddi_dki_impl.ll

usba_files:
	@cd ../usba;pwd; $(MAKE) warlock

uhci_files:
	@cd ../uhci;pwd; $(MAKE) warlock

ohci_files:
	@cd ../ohci;pwd; $(MAKE) warlock

ehci_files:
	@cd ../ehci;pwd; $(MAKE) warlock

warlock_ddi.files:
	cd ../warlock; pwd; $(MAKE) warlock
