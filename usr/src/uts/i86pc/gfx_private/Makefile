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
# uts/i86pc/gfx_private/Makefile
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile	1.1	05/11/17 SMI"
#
#	This makefile drives the production of the graphics private
#	interface kernel module.
#
#	i86pc platform dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= gfx_private
OBJECTS		= $(GFX_PRIVATE_OBJS:%=$(OBJS_DIR)/%) $(OBJS_DIR)/$(VGATEXT_FONT).o
LINTS		= $(GFX_PRIVATE_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)
VGATEXT_FONT	= 8859-1
VGATEXT_SRC	= $(UTSBASE)/intel/io/vgatext

#
#	dependency
#
LDFLAGS	+=      -dy -Nmisc/pci_autoconfig

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

# Custom targets and rules

$(OBJS_DIR)/$(VGATEXT_FONT).c:	$(VGATEXT_SRC)/$(VGATEXT_FONT).bdf
	awk -f $(VGATEXT_SRC)/bdf_to_c.awk	\
		$(VGATEXT_SRC)/$(VGATEXT_FONT).bdf > $@

$(OBJS_DIR)/$(VGATEXT_FONT).o:	$(OBJS_DIR)/$(VGATEXT_FONT).c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

#
#	Include common targets.
#
include $(UTSBASE)/i86pc/Makefile.targ
