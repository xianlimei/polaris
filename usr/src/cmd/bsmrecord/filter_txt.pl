#!/usr/perl5/bin/perl -w
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
# Copyright (c) 2001 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)filter_txt.pl	1.2	05/06/08 SMI"

# local script to process audit_record_attr.txt -> audit_record_attr
#
# comments in the source file may start with "#" or "##" in any
# column.  Those with double hash are retained as comments (but with a
# single "#") in the destination and the others are removed.  Because
# of the comment removal, any sequence of more than one line of blank
# lines is also removed.

use strict;
require 5.005;

my $blankCount = 1;	# not zero is a kludge to avoid making the first
			# line of the output a blank line.
while (<>) {
        s/(?<!#)#(?!#).*//;
	if (/^\s*$/) {
		$blankCount++ ;
		next if ($blankCount > 1);
	} else {
		$blankCount = 0;
	}
	s/##/#/;
	print;
}
