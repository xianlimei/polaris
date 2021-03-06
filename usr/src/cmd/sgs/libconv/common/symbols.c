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
#pragma ident	"@(#)symbols.c	1.21	06/05/09 SMI"

/*
 * String conversion routines for symbol attributes.
 */
#include	<stdio.h>
#include	<sys/machelf.h>
#include	<sys/elf_SPARC.h>
#include	<sys/elf_amd64.h>
#include	"_conv.h"
#include	"symbols_msg.h"

const char *
conv_sym_other(uchar_t other)
{
	static char		string[CONV_INV_STRSIZE];
	static const char	visibility[4] = {
		'D',	/* STV_DEFAULT */
		'I',	/* STV_INTERNAL */
		'H',	/* STV_HIDDEN */
		'P'	/* STV_PROTECTED */
	};
	uint_t		vis = ELF_ST_VISIBILITY(other);
	uint_t		ndx = 0;

	string[ndx++] = visibility[vis];

	/*
	 * If unkown bits are present in stother - throw out a '?'
	 */
	if (other & ~MSK_SYM_VISIBILITY)
		string[ndx++] = '?';
	string[ndx++] = '\0';

	return (string);
}

const char *
conv_sym_info_type(Half mach, uchar_t type, int fmt_flags)
{
	static char		string[CONV_INV_STRSIZE];
	static const Msg	types[] = {
		MSG_STT_NOTYPE,		MSG_STT_OBJECT,		MSG_STT_FUNC,
		MSG_STT_SECTION,	MSG_STT_FILE,		MSG_STT_COMMON,
		MSG_STT_TLS
	};

	if (type < STT_NUM) {
		return (MSG_ORIG(types[type]));
	} else if (((mach == EM_SPARC) || (mach == EM_SPARC32PLUS) ||
	    (mach == EM_SPARCV9)) && (type == STT_SPARC_REGISTER)) {
		return (MSG_ORIG(MSG_STT_REGISTER));
	} else {
		return (conv_invalid_val(string, CONV_INV_STRSIZE,
			type, fmt_flags));
	}
}

const char *
conv_sym_info_bind(uchar_t bind, int fmt_flags)
{
	static char		string[CONV_INV_STRSIZE];
	static const Msg	binds[] = {
		MSG_STB_LOCAL,		MSG_STB_GLOBAL,		MSG_STB_WEAK
	};

	if (bind >= STB_NUM)
		return (conv_invalid_val(string, CONV_INV_STRSIZE,
			bind, fmt_flags));
	else
		return (MSG_ORIG(binds[bind]));
}

const char *
conv_sym_shndx(Half shndx)
{
	static	char	string[CONV_INV_STRSIZE];

	switch (shndx) {
	case SHN_UNDEF:
		return (MSG_ORIG(MSG_SHN_UNDEF));
	case SHN_SUNW_IGNORE:
		return (MSG_ORIG(MSG_SHN_SUNW_IGNORE));
	case SHN_ABS:
		return (MSG_ORIG(MSG_SHN_ABS));
	case SHN_COMMON:
		return (MSG_ORIG(MSG_SHN_COMMON));
	case SHN_AMD64_LCOMMON:
		return (MSG_ORIG(MSG_SHN_AMD64_LCOMMON));
	case SHN_AFTER:
		return (MSG_ORIG(MSG_SHN_AFTER));
	case SHN_BEFORE:
		return (MSG_ORIG(MSG_SHN_BEFORE));
	case SHN_XINDEX:
		return (MSG_ORIG(MSG_SHN_XINDEX));
	default:
		return (conv_invalid_val(string, CONV_INV_STRSIZE, shndx,
		    CONV_FMT_DECIMAL));
	}
}

const char *
conv_sym_value(Half mach, uchar_t type, Addr value)
{
	static char	string[CONV_INV_STRSIZE];

	if (((mach == EM_SPARC) || (mach == EM_SPARC32PLUS) ||
	    (mach == EM_SPARCV9)) && (type == STT_SPARC_REGISTER))
		return (conv_sym_SPARC_value(value, 0));

	(void) sprintf(string, MSG_ORIG(MSG_SYM_FMT_VAL), EC_ADDR(value));
	return (string);
}
