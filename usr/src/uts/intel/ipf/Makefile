#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile	1.6	06/07/14 SMI"
#
# uts/intel/ipf/Makefile
#
#	This makefile drives the production of the ipf driver 
#	kernel module.
#
#	intel architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= ipf
OBJECTS		= $(IPF_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(IPF_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/inet/ipf

#
#	Include common rules.
#
include $(UTSBASE)/intel/Makefile.intel

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

MINOR=	    echo $(RELEASE) | cut -d. -f2
CPPFLAGS += -DIPFILTER_LKM -DIPFILTER_LOG -DIPFILTER_LOOKUP -DUSE_INET6
CPPFLAGS += -DSUNDDI -DSOLARIS2=$(MINOR:sh) -DIRE_ILL_CN
LDFLAGS += -dy -Ndrv/ip -Ndrv/pfil -Nmisc/md5

INC_PATH += -I$(UTSBASE)/common/inet/ipf

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

#
#	Include common targets.
#
include $(UTSBASE)/intel/Makefile.targ
