#! /usr/bin/sh
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
# ident	"@(#)svm.save.sh	1.2	05/06/08 SMI"
#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Save existing clone SVM configuation before differential flash installation.

# directory where we save original clone config
SAVE_DIR=${FLASH_DIR}/flash/svm

if [ "${FLASH_TYPE}" = "DIFFERENTIAL" ]; then
	mkdir -p ${SAVE_DIR}
	cp ${FLASH_ROOT}/kernel/drv/md.conf ${SAVE_DIR}
	cp ${FLASH_ROOT}/etc/lvm/devpath ${SAVE_DIR}
	cp ${FLASH_ROOT}/etc/lvm/md.cf ${SAVE_DIR}
	cp ${FLASH_ROOT}/etc/lvm/md.ctlrmap ${SAVE_DIR}
	cp ${FLASH_ROOT}/etc/lvm/md.tab ${SAVE_DIR}
	cp ${FLASH_ROOT}/etc/lvm/mddb.cf ${SAVE_DIR}
	cp ${FLASH_ROOT}/etc/lvm/runtime.cf ${SAVE_DIR}
	cp ${FLASH_ROOT}/etc/system ${SAVE_DIR}
fi

exit 0