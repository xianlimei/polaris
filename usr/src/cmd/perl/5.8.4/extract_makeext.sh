#!/bin/ksh -p
#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)extract_makeext.sh	1.1	04/06/09 SMI"
#
# The perl extension build mechanism works by generating a Makefile from a
# template Makefile.PL file in the extension directory.  Unfortunately this
# generated Makefile has a dependency against both the original Makefile.PL as
# well as config.sh and config.h.  If triggered, the rule for this dependency
# rebuilds the Makefile from Makefile.PL and exits the make by calling 'false'.
# This behaviour is unacceptable in the context of ON, where the build must
# succeed wherever possible.  This script generates a makefile that mimics
# that generated by the Makefile.PL, so that we can ensure that the extension
# make will always pass the relevant dependency checks, and will build
# sucessfully.  The output of this script is a combination of parts of make_ext
# and the dependencies from the various module Makefiles.  Note also that the
# generated rules unset VERSION in the environment before calling make, to avoid
# a clash between the Solaris and Perl uses of this variable.
#

if  [ -z "$1" -o ! -r "$1" ]; then
	printf 'No config.sh file specified\n'
	exit 1
fi

# Pull in config.sh.
set -e
. $1
set +e

printf '# This file was automatically generated from config.sh by %s\n\n' \
    $(basename $0)

# Boilerplate, mostly the same as distrib/Makefile.
cat <<'EOF'
include ../../../../Makefile.cmd
include ../../Makefile.perlcfg

PERL_MM_ARGS += INSTALLDIRS=perl LIBPERL_A=libperl.so

EOF

# Define some useful macros.
printf 'PERL_STATIC_EXT = '
for ext in $static_ext
do
	printf '%s ' $ext
done
printf '\n'

printf 'PERL_DYNAMIC_EXT = '
for ext in $dynamic_ext
do
	printf '%s ' $ext
done
printf '\n'

printf 'PERL_NONXS_EXT = '
for ext in $nonxs_ext
do
	printf '%s ' $ext
done
printf '\n'

printf 'PERL_EXT = $(PERL_STATIC_EXT) $(PERL_DYNAMIC_EXT) $(PERL_NONXS_EXT)\n'
printf 'PERL_EXT_MAKEFILES = $(PERL_EXT:%%=%%/Makefile)\n\n'

# Define the all, clean and clobber rules
cat <<'EOF'
all: $(PERL_EXT)

.PARALLEL: $(PERL_EXT_MAKEFILES) $(PERL_EXT)

clean:
	-@ $(PERL_MM_ENV); \
	for ext in $(PERL_EXT); do \
		( \
		cd $$ext; pwd; \
		[ -f Makefile.old ] && mf=Makefile.old; \
		[ -f Makefile ] && mf=Makefile; \
		[ ! -z "$$mf" ] && $(MAKE) -f $$mf clean; \
		) \
	done

clobber:
	-@ $(PERL_MM_ENV); \
	for ext in $(PERL_EXT); do \
		( \
		cd $$ext; pwd; \
		[ -f Makefile.old ] && mf=Makefile.old; \
		[ -f Makefile ] && mf=Makefile; \
		[ ! -z "$$mf" ] && $(MAKE) -f $$mf realclean; \
		) \
	done

EOF

# Generic Makefile.PL pattern matching rule.
printf '%%/Makefile: %%/Makefile.PL $(PERL_CONFIGDEP)\n'
printf '\t@ cd $(@D); pwd; '
printf '$(PERL_MM_ENV); $(MINIPERL) $(<F) $(PERL_MM_ARGS) >/dev/null 2>&1\n\n'

# Static extensions.
for ext in $static_ext
do
	printf '%s: %s/Makefile FRC\n' $ext $ext
	printf '\t@ cd $@; pwd; '
	printf '$(PERL_MM_ENV); $(MAKE) LINKTYPE=static CCCDLFLAGS= all\n\n'
done

# Dynamic extensions.
for ext in $dynamic_ext
do
	printf '%s: %s/Makefile FRC\n' $ext $ext
	printf '\t@ cd $@; pwd; '
	printf '$(PERL_MM_ENV); $(MAKE) LINKTYPE=dynamic all\n\n'
done

# Non-XS extensions.
for ext in $nonxs_ext
do
	printf '%s: %s/Makefile FRC\n' $ext $ext
	printf '\t@ cd $@; pwd '
	printf '$(PERL_MM_ENV); $(MAKE) all\n\n'
done

printf 'FRC:\n'