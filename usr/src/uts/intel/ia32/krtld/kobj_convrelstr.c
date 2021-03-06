/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
#pragma ident	"@(#)kobj_convrelstr.c	1.8	06/03/07 SMI"

#include	<sys/types.h>
#include	"reloc.h"

static const char	*rels[R_386_NUM] = {
	"R_386_NONE",			"R_386_32",
	"R_386_PC32",			"R_386_GOT32",
	"R_386_PLT32",			"R_386_COPY",
	"R_386_GLOB_DAT",		"R_386_JMP_SLOT",
	"R_386_RELATIVE",		"R_386_GOTOFF",
	"R_386_GOTPC",			"R_386_32PLT",
	"R_386_TLS_GD_PLT",		"R_386_TLS_LDM_PLT",
	"R_386_TLS_TPOFF",		"R_386_TLS_IE",
	"R_386_TLS_GOTIE",		"R_386_TLS_LE",
	"R_386_TLS_GD",			"R_386_TLS_LDM",
	"R_386_16",			"R_386_PC16",
	"R_386_8",			"R_386_PC8",
	"R_386_UNKNOWN24",		"R_386_UNKNOWN25",
	"R_386_UNKNOWN26",		"R_386_UNKNOWN27",
	"R_386_UNKNOWN28",		"R_386_UNKNOWN29",
	"R_386_UNKNOWN30",		"R_386_UNKNOWN31",
	"R_386_TLS_LDO_32",		"R_386_UNKNOWN33",
	"R_386_UNKNOWN34",		"R_386_TLS_DTPMOD32",
	"R_386_TLS_DTPOFF32",		"R_386_UNKNOWN37"
};

#if	(R_386_NUM != (R_386_UNKNOWN37 + 1))
#error	"R_386_NUM has grown"
#endif

/*
 * This is a 'stub' of the orignal version defined in liblddbg.so.  This stub
 * returns the 'int string' of the relocation in question instead of converting
 * the relocation to it's full syntax.
 */
const char *
conv_reloc_386_type(Word type)
{
	static char 	strbuf[32];
	int		ndx = 31;

	if (type < R_386_NUM)
		return (rels[type]);

	strbuf[ndx--] = '\0';
	do {
		strbuf[ndx--] = '0' + (type % 10);
		type = type / 10;
	} while ((ndx >= (int)0) && (type > (Word)0));

	return (&strbuf[ndx + 1]);
}
