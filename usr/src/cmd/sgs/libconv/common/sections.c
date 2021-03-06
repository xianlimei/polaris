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
#pragma ident	"@(#)sections.c	1.38	06/08/29 SMI"

/*
 * String conversion routines for section attributes.
 */
#include	<string.h>
#include	<sys/param.h>
#include	<sys/elf_SPARC.h>
#include	<sys/elf_amd64.h>
#include	<_conv.h>
#include	<sections_msg.h>



/* Instantiate a local copy of conv_map2str() from _conv.h */
DEFINE_conv_map2str



static const Msg secs[SHT_NUM] = {
	MSG_SHT_NULL,		MSG_SHT_PROGBITS,	MSG_SHT_SYMTAB,
	MSG_SHT_STRTAB,		MSG_SHT_RELA,		MSG_SHT_HASH,
	MSG_SHT_DYNAMIC,	MSG_SHT_NOTE,		MSG_SHT_NOBITS,
	MSG_SHT_REL,		MSG_SHT_SHLIB,		MSG_SHT_DYNSYM,
	MSG_SHT_UNKNOWN12,	MSG_SHT_UNKNOWN13,	MSG_SHT_INIT_ARRAY,
	MSG_SHT_FINI_ARRAY,	MSG_SHT_PREINIT_ARRAY,	MSG_SHT_GROUP,
	MSG_SHT_SYMTAB_SHNDX
};
static const Msg secs_alt[SHT_NUM] = {
	MSG_SHT_NULL_ALT,	MSG_SHT_PROGBITS_ALT,	MSG_SHT_SYMTAB_ALT,
	MSG_SHT_STRTAB_ALT,	MSG_SHT_RELA_ALT,	MSG_SHT_HASH_ALT,
	MSG_SHT_DYNAMIC_ALT,	MSG_SHT_NOTE_ALT,	MSG_SHT_NOBITS_ALT,
	MSG_SHT_REL_ALT,	MSG_SHT_SHLIB_ALT,	MSG_SHT_DYNSYM_ALT,
	MSG_SHT_UNKNOWN12,	MSG_SHT_UNKNOWN13,	MSG_SHT_INIT_ARRAY_ALT,
	MSG_SHT_FINI_ARRAY_ALT,	MSG_SHT_PREINIT_ARRAY_ALT, MSG_SHT_GROUP_ALT,
	MSG_SHT_SYMTAB_SHNDX_ALT
};
#if	(SHT_NUM != (SHT_SYMTAB_SHNDX + 1))
#error	"SHT_NUM has grown"
#endif

static const Msg usecs[SHT_HISUNW - SHT_LOSUNW + 1] = {
	MSG_SHT_SUNW_dof,		MSG_SHT_SUNW_cap,
	MSG_SHT_SUNW_SIGNATURE,		MSG_SHT_SUNW_ANNOTATE,
	MSG_SHT_SUNW_DEBUGSTR,		MSG_SHT_SUNW_DEBUG,
	MSG_SHT_SUNW_move,		MSG_SHT_SUNW_COMDAT,
	MSG_SHT_SUNW_syminfo,		MSG_SHT_SUNW_verdef,
	MSG_SHT_SUNW_verneed,		MSG_SHT_SUNW_versym
};
static const Msg usecs_alt[SHT_HISUNW - SHT_LOSUNW + 1] = {
	MSG_SHT_SUNW_dof_ALT,		MSG_SHT_SUNW_cap_ALT,
	MSG_SHT_SUNW_SIGNATURE_ALT,	MSG_SHT_SUNW_ANNOTATE_ALT,
	MSG_SHT_SUNW_DEBUGSTR_ALT,	MSG_SHT_SUNW_DEBUG_ALT,
	MSG_SHT_SUNW_move_ALT,		MSG_SHT_SUNW_COMDAT_ALT,
	MSG_SHT_SUNW_syminfo_ALT,	MSG_SHT_SUNW_verdef_ALT,
	MSG_SHT_SUNW_verneed_ALT,	MSG_SHT_SUNW_versym_ALT
};
#if	(SHT_LOSUNW != SHT_SUNW_dof)
#error	"SHT_LOSUNW has moved"
#endif


const char *
conv_sec_type(Half mach, Word sec, int fmt_flags)
{
	static char	string[CONV_INV_STRSIZE];

	if (sec < SHT_NUM) {
		return (conv_map2str(string, sizeof (string), sec, fmt_flags,
			ARRAY_NELTS(secs), secs, secs_alt, NULL));
	} else if ((sec >= SHT_LOSUNW) && (sec <= SHT_HISUNW)) {
		return (conv_map2str(string, sizeof (string), sec - SHT_LOSUNW,
			fmt_flags, ARRAY_NELTS(usecs), usecs, usecs_alt, NULL));
	} else if ((sec >= SHT_LOPROC) && (sec <= SHT_HIPROC)) {
		switch (mach) {
		case EM_SPARC:
		case EM_SPARC32PLUS:
		case EM_SPARCV9:
			if (sec == SHT_SPARC_GOTDATA) {
				return (fmt_flags & CONV_FMT_ALTDUMP)
					? MSG_ORIG(MSG_SHT_SPARC_GOTDATA_ALT)
					: MSG_ORIG(MSG_SHT_SPARC_GOTDATA);
			}
			break;
		case EM_AMD64:
			if (sec == SHT_AMD64_UNWIND) {
				return (fmt_flags & CONV_FMT_ALTDUMP)
					? MSG_ORIG(MSG_SHT_AMD64_UNWIND_ALT)
					: MSG_ORIG(MSG_SHT_AMD64_UNWIND);
			}
		}
	}

	/* If we get here, it's an unknown type */
	return (conv_invalid_val(string, CONV_INV_STRSIZE, sec, fmt_flags));
}

#define	FLAGSZ	CONV_EXPN_FIELD_DEF_PREFIX_SIZE + \
		MSG_SHF_WRITE_SIZE	+ CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_ALLOC_SIZE	+ CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_EXECINSTR_SIZE	+ CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_MERGE_SIZE	+ CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_STRINGS_SIZE	+ CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_INFO_LINK_SIZE	+ CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_LINK_ORDER_SIZE	+ CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_OS_NONCONFORMING_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_GROUP_SIZE	+ CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_TLS_SIZE	+ CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_EXCLUDE_SIZE	+ CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_ORDERED_SIZE	+ CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		MSG_SHF_AMD64_LARGE_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE + \
		CONV_INV_STRSIZE + CONV_EXPN_FIELD_DEF_SUFFIX_SIZE

const char *
conv_sec_flags(Xword flags)
{
	static	char	string[FLAGSZ];
	static Val_desc vda[] = {
		{ SHF_WRITE,		MSG_ORIG(MSG_SHF_WRITE) },
		{ SHF_ALLOC,		MSG_ORIG(MSG_SHF_ALLOC) },
		{ SHF_EXECINSTR,	MSG_ORIG(MSG_SHF_EXECINSTR) },
		{ SHF_MERGE,		MSG_ORIG(MSG_SHF_MERGE) },
		{ SHF_STRINGS,		MSG_ORIG(MSG_SHF_STRINGS) },
		{ SHF_INFO_LINK,	MSG_ORIG(MSG_SHF_INFO_LINK) },
		{ SHF_LINK_ORDER,	MSG_ORIG(MSG_SHF_LINK_ORDER) },
		{ SHF_OS_NONCONFORMING,	MSG_ORIG(MSG_SHF_OS_NONCONFORMING) },
		{ SHF_GROUP,		MSG_ORIG(MSG_SHF_GROUP) },
		{ SHF_TLS,		MSG_ORIG(MSG_SHF_TLS) },
		{ SHF_EXCLUDE,		MSG_ORIG(MSG_SHF_EXCLUDE) },
		{ SHF_ORDERED,		MSG_ORIG(MSG_SHF_ORDERED) },
		{ SHF_AMD64_LARGE,	MSG_ORIG(MSG_SHF_AMD64_LARGE) },
		{ 0,			0 }
	};
	static CONV_EXPN_FIELD_ARG conv_arg = { string, sizeof (string), vda };

	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));

	conv_arg.oflags = conv_arg.rflags = flags;
	(void) conv_expn_field(&conv_arg);

	return ((const char *)string);
}

const char *
conv_sec_linkinfo(Word info, Xword flags)
{
	static	char	string[CONV_INV_STRSIZE];

	if (flags & ALL_SHF_ORDER) {
		if (info == SHN_BEFORE)
			return (MSG_ORIG(MSG_SHN_BEFORE));
		else if (info == SHN_AFTER)
			return (MSG_ORIG(MSG_SHN_AFTER));
	}
	(void) conv_invalid_val(string, CONV_INV_STRSIZE, info, 1);
	return ((const char *)string);
}
