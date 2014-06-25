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
# ident	"@(#)Makefile.com	1.3	06/04/23 SMI"
#

OBJECTS	= init.o interface.o signal.o forth.o fcode.o interp.o debug.o  \
	  env.o print.o properties.o actions.o package.o instance.o  \
	  alarm.o interactive.o framebuffer.o font.o fb8.o extend.o tracing.o \
	  resource.o prims64.o mcookie.o log.o cleanup.o 
	
LIBRARY	= fcode.a

include ../../Makefile.efcode