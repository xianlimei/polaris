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
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
#pragma ident	"@(#)relocate.c	1.137	06/08/29 SMI"

/*
 * set-up for relocations
 */
#include	<string.h>
#include	<stdio.h>
#include	<alloca.h>
#include	<reloc.h>
#include	<debug.h>
#include	"msg.h"
#include	"_libld.h"

/*
 * Structure to hold copy relocation items.
 */
typedef struct copy_rel {
	Sym_desc *	copyrel_symd;	/* symbol descriptor to be copied */
	Addr 		copyrel_stval;	/* Original symbol value */
} Copy_rel;

/*
 * For each copy relocation symbol, determine if the symbol is:
 * 	1) to be *disp* relocated at runtime
 *	2) a reference symbol for *disp* relocation
 *	3) possibly *disp* relocated at ld time.
 *
 * The first and the second are serious errors.
 */
static void
is_disp_copied(Ofl_desc *ofl, Copy_rel *cpy)
{
	Ifl_desc	*ifl = cpy->copyrel_symd->sd_file;
	Sym_desc	*sdp = cpy->copyrel_symd;
	Is_desc		*irel;
	Addr		symaddr = cpy->copyrel_stval;
	Listnode	*lnp1;

	/*
	 * This symbol may not be *disp* relocated at run time, but could
	 * already have been *disp* relocated when the shared object was
	 * created.  Warn the user.
	 */
	if ((ifl->ifl_flags & FLG_IF_DISPDONE) &&
	    (ofl->ofl_flags & FLG_OF_VERBOSE))
		eprintf(ofl->ofl_lml, ERR_WARNING, MSG_INTL(MSG_REL_DISPREL2),
		    conv_reloc_type(ifl->ifl_ehdr->e_machine, M_R_COPY, 0),
		    ifl->ifl_name, demangle(sdp->sd_name));

	if ((ifl->ifl_flags & FLG_IF_DISPPEND) == 0)
		return;

	/*
	 * Traverse the input relocation sections.
	 */
	for (LIST_TRAVERSE(&ifl->ifl_relsect, lnp1, irel)) {
		Sym_desc	*rsdp;
		Is_desc		*trel;
		Rel		*rend, *reloc;
		Xword		rsize, entsize;

		trel = ifl->ifl_isdesc[irel->is_shdr->sh_info];
		rsize = irel->is_shdr->sh_size;
		entsize = irel->is_shdr->sh_entsize;
		reloc = (Rel *)irel->is_indata->d_buf;

		/*
		 * Decide entry size
		 */
		if ((entsize == 0) || (entsize > rsize)) {
			if (irel->is_shdr->sh_type == SHT_RELA)
				entsize = sizeof (Rela);
			else
				entsize = sizeof (Rel);
		}

		/*
		 * Traverse the relocation entries.
		 */
		for (rend = (Rel *)((uintptr_t)reloc + (uintptr_t)rsize);
		    reloc < rend;
		    reloc = (Rel *)((uintptr_t)reloc + (uintptr_t)entsize)) {
			const char	*str;
			Word		rstndx;

			if (IS_PC_RELATIVE(ELF_R_TYPE(reloc->r_info)) == 0)
				continue;

			/*
			 * First, check if this symbol is reference symbol
			 * for this relocation entry.
			 */
			rstndx = (Word) ELF_R_SYM(reloc->r_info);
			rsdp = ifl->ifl_oldndx[rstndx];
			if (rsdp == sdp) {
				if ((str = demangle(rsdp->sd_name)) !=
				    rsdp->sd_name) {
					char	*_str = alloca(strlen(str) + 1);
					(void) strcpy(_str, str);
					str = (const char *)_str;
				}
				eprintf(ofl->ofl_lml,
				    ERR_WARNING, MSG_INTL(MSG_REL_DISPREL1),
				    conv_reloc_type(ifl->ifl_ehdr->e_machine,
				    (uint_t)ELF_R_TYPE(reloc->r_info), 0),
				    ifl->ifl_name, str,
				    MSG_INTL(MSG_STR_UNKNOWN),
				    EC_XWORD(reloc->r_offset),
				    demangle(sdp->sd_name));
			}

			/*
			 * Then check if this relocation entry is relocating
			 * this symbol.
			 */
			if ((sdp->sd_isc != trel) ||
			    (reloc->r_offset < symaddr) ||
			    (reloc->r_offset >=
			    (symaddr + sdp->sd_sym->st_size)))
				continue;

			/*
			 * This symbol is truely *disp* relocated, so should
			 * really be fixed by user.
			 */
			if ((str = demangle(sdp->sd_name)) != sdp->sd_name) {
				char	*_str = alloca(strlen(str) + 1);
				(void) strcpy(_str, str);
				str = (const char *)_str;
			}
			eprintf(ofl->ofl_lml, ERR_WARNING,
			    MSG_INTL(MSG_REL_DISPREL1),
			    conv_reloc_type(ifl->ifl_ehdr->e_machine,
			    (uint_t)ELF_R_TYPE(reloc->r_info), 0),
			    ifl->ifl_name, demangle(rsdp->sd_name), str,
			    EC_XWORD(reloc->r_offset), str);
		}
	}
}

/*
 * The number of symbols provided by some objects can be very large.  Use a
 * binary search to match the associated value to a symbol table entry.
 */
static int
disp_bsearch(const void *key, const void *array)
{
	Addr		kvalue, avalue;
	Ssv_desc	*ssvp = (Ssv_desc *)array;

	kvalue = *((Addr *)key);
	avalue = ssvp->ssv_value;

	if (avalue > kvalue)
		return (-1);
	if ((avalue < kvalue) &&
	    ((avalue + ssvp->ssv_sdp->sd_sym->st_size) <= kvalue))
		return (1);
	return (0);
}

/*
 * Given a sorted list of symbols, look for a symbol in which the relocation
 * offset falls between the [sym.st_value - sym.st_value + sym.st_size].  Since
 * the symbol list is maintained in sorted order,  we can bail once the
 * relocation offset becomes less than the symbol values.  The symbol is
 * returned for use in error diagnostics.
 */
static Sym_desc *
disp_scansyms(Ifl_desc * ifl, Rel_desc *rld, Boolean rlocal, int inspect,
    Ofl_desc *ofl)
{
	Sym_desc	*tsdp, *rsdp;
	Sym		*rsym, *tsym;
	Ssv_desc	*ssvp;
	uchar_t		rtype, ttype;
	Addr		value;

	/*
	 * Sorted symbol values have been uniquified by adding their associated
	 * section offset.  Uniquify the relocation offset by adding its
	 * associated section offset, and search for the symbol.
	 */
	value = rld->rel_roffset;
	if (rld->rel_isdesc->is_shdr)
		value += rld->rel_isdesc->is_shdr->sh_offset;

	if ((ssvp = bsearch((void *)&value, (void *)ifl->ifl_sortsyms,
	    ifl->ifl_sortcnt, sizeof (Ssv_desc), &disp_bsearch)) != 0)
		tsdp = ssvp->ssv_sdp;
	else
		tsdp = 0;

	if (inspect)
		return (tsdp);

	/*
	 * Determine the relocation reference symbol and its type.
	 */
	rsdp = rld->rel_sym;
	rsym = rsdp->sd_sym;
	rtype = ELF_ST_TYPE(rsym->st_info);

	/*
	 * If there is no target symbol to match the relocation offset, then the
	 * offset is effectively local data.  If the relocation symbol is global
	 * data we have a potential for this displacement relocation to be
	 * invalidated should the global symbol be copied.
	 */
	if (tsdp == 0) {
		if ((rlocal == TRUE) ||
		    ((rtype != STT_OBJECT) && (rtype != STT_SECTION)))
		return (tsdp);
	} else {
		/*
		 * If both symbols are local, no copy relocations can occur to
		 * either symbol.
		 */
		if ((rlocal == TRUE) && ((tsdp->sd_flags1 & FLG_SY1_LOCL) ||
		    ((ofl->ofl_flags & FLG_OF_AUTOLCL) &&
		    (tsdp->sd_flags1 & FLG_SY1_GLOB) == 0) ||
		    (ELF_ST_BIND(tsdp->sd_sym->st_info) != STB_GLOBAL)))
			return (tsdp);

		/*
		 * Determine the relocation target symbols type.
		 */
		tsym = tsdp->sd_sym;
		ttype = ELF_ST_TYPE(tsym->st_info);

		/*
		 * If the reference symbol is local, and the target isn't a
		 * data element, then no copy relocations can occur to either
		 * symbol.  Note, this catches pc-relative relocations against
		 * the _GLOBAL_OFFSET_TABLE_, which is effectively treated as
		 * a local symbol.
		 */
		if ((rlocal == TRUE) && (ttype != STT_OBJECT) &&
		    (ttype != STT_SECTION))
			return (tsdp);

		/*
		 * Finally, one of the symbols must reference a data element.
		 */
		if ((rtype != STT_OBJECT) && (rtype != STT_SECTION) &&
		    (ttype != STT_OBJECT) && (ttype != STT_SECTION))
			return (tsdp);
	}

	/*
	 * We have two global symbols, at least one of which is a data item.
	 * The last case where a displacement relocation can be ignored, is
	 * if the reference symbol is included in the target symbol.
	 */
	value = rsym->st_value;
	value += rld->rel_raddend;

	if ((rld->rel_roffset >= value) &&
	    (rld->rel_roffset < (value + rsym->st_size)))
		return (tsdp);

	/*
	 * We have a displacement relocation that could be compromised by a
	 * copy relocation of one of the associated data items.
	 */
	rld->rel_flags |= FLG_REL_DISP;
	return (tsdp);
}

void
ld_disp_errmsg(const char *msg, Rel_desc *rsp, Ofl_desc *ofl)
{
	Sym_desc	*sdp;
	const char	*str;
	Ifl_desc	*ifl = rsp->rel_isdesc->is_file;

	if ((sdp = disp_scansyms(ifl, rsp, 0, 1, ofl)) != 0)
		str = demangle(sdp->sd_name);
	else
		str = MSG_INTL(MSG_STR_UNKNOWN);

	eprintf(ofl->ofl_lml, ERR_WARNING, msg,
	    conv_reloc_type(ifl->ifl_ehdr->e_machine, rsp->rel_rtype, 0),
	    ifl->ifl_name, rsp->rel_sname, str, EC_OFF(rsp->rel_roffset));
}

/*
 * qsort(3C) comparison routine used for the disp_sortsyms().
 */
static int
disp_qsort(const void * s1, const void * s2)
{
	Ssv_desc	*ssvp1 = ((Ssv_desc *)s1);
	Ssv_desc	*ssvp2 = ((Ssv_desc *)s2);
	Addr		val1 = ssvp1->ssv_value;
	Addr		val2 = ssvp2->ssv_value;

	if (val1 > val2)
		return (1);
	if (val1 < val2)
		return (-1);
	return (0);
}

/*
 * Determine whether a displacement relocation is between a local and global
 * symbol pair.  One symbol is used to perform the relocation, and the other
 * is the destination offset of the relocation.
 */
static uintptr_t
disp_inspect(Ofl_desc *ofl, Rel_desc *rld, Boolean rlocal)
{
	Is_desc		*isp = rld->rel_isdesc;
	Ifl_desc	*ifl = rld->rel_isdesc->is_file;

	/*
	 * If the input files symbols haven't been sorted yet, do so.
	 */
	if (ifl->ifl_sortsyms == 0) {
		Word	ondx, nndx;

		if ((ifl->ifl_sortsyms = libld_malloc((ifl->ifl_symscnt + 1) *
		    sizeof (Ssv_desc))) == 0)
			return (S_ERROR);

		for (ondx = 0, nndx = 0; ondx < ifl->ifl_symscnt; ondx++) {
			Sym_desc	*sdp;
			Addr		value;

			/*
			 * As symbol resolution has already occurred, various
			 * symbols from this object may have been satisfied
			 * from other objects.  Only select symbols from this
			 * object.  For the displacement test, we only really
			 * need to observe data definitions, however, later as
			 * part of providing warning disgnostics, relating the
			 * relocation offset to a symbol is desirable.  Thus,
			 * collect all symbols that define a memory area.
			 */
			if (((sdp = ifl->ifl_oldndx[ondx]) == 0) ||
			    (sdp->sd_sym->st_shndx == SHN_UNDEF) ||
			    (sdp->sd_sym->st_shndx >= SHN_LORESERVE) ||
			    (sdp->sd_ref != REF_REL_NEED) ||
			    (sdp->sd_file != ifl) ||
			    (sdp->sd_sym->st_size == 0))
				continue;

			/*
			 * As a further optimization for later checking, mark
			 * this section if this a global data definition.
			 */
			if (sdp->sd_isc && (ondx >= ifl->ifl_locscnt))
				sdp->sd_isc->is_flags |= FLG_IS_GDATADEF;

			/*
			 * Capture the symbol.  Within relocatable objects, a
			 * symbols value is its offset within its associated
			 * section.  Add the section offset to this value to
			 * uniquify the symbol.
			 */
			value = sdp->sd_sym->st_value;
			if (sdp->sd_isc && sdp->sd_isc->is_shdr)
				value += sdp->sd_isc->is_shdr->sh_offset;

			ifl->ifl_sortsyms[nndx].ssv_value = value;
			ifl->ifl_sortsyms[nndx].ssv_sdp = sdp;
			nndx++;
		}

		/*
		 * Sort the list based on the symbols value (address).
		 */
		if ((ifl->ifl_sortcnt = nndx) != 0)
			qsort(ifl->ifl_sortsyms, nndx, sizeof (Ssv_desc),
			    &disp_qsort);
	}

	/*
	 * If the reference symbol is local, and the section being relocated
	 * contains no global definitions, neither can be the target of a copy
	 * relocation.
	 */
	if ((rlocal == FALSE) && ((isp->is_flags & FLG_IS_GDATADEF) == 0))
		return (1);

	/*
	 * Otherwise determine whether this relocation symbol and its offset
	 * could be candidates for a copy relocation.
	 */
	if (ifl->ifl_sortcnt)
		(void) disp_scansyms(ifl, rld, rlocal, 0, ofl);
	return (1);
}

/*
 * Add an active relocation record.
 */
uintptr_t
ld_add_actrel(Word flags, Rel_desc *rsp, Ofl_desc *ofl)
{
	Rel_desc	*arsp;
	Rel_cache	*rcp;

	/*
	 * If no relocation cache structures are available, allocate a new
	 * one and link it into the bucket list.
	 */
	if ((ofl->ofl_actrels.tail == 0) ||
	    ((rcp = (Rel_cache *)ofl->ofl_actrels.tail->data) == 0) ||
	    ((arsp = rcp->rc_free) == rcp->rc_end)) {
		static size_t	nextsize = 0;
		size_t		size;

		/*
		 * Typically, when generating an executable or shared object
		 * there will be an active relocation for every input
		 * relocation.
		 */
		if (nextsize == 0) {
			if ((ofl->ofl_flags & FLG_OF_RELOBJ) == 0) {
				if ((size = ofl->ofl_relocincnt) == 0)
					size = REL_LAIDESCNO;
				if (size > REL_HAIDESCNO)
					nextsize = REL_HAIDESCNO;
				else
					nextsize = REL_LAIDESCNO;
			} else
				nextsize = size = REL_HAIDESCNO;
		} else
			size = nextsize;

		size = size * sizeof (Rel_desc);

		if (((rcp = libld_malloc(sizeof (Rel_cache) + size)) == 0) ||
		    (list_appendc(&ofl->ofl_actrels, rcp) == 0))
			return (S_ERROR);

		/* LINTED */
		rcp->rc_free = arsp = (Rel_desc *)(rcp + 1);
		/* LINTED */
		rcp->rc_end = (Rel_desc *)((char *)rcp->rc_free + size);
	}

	*arsp = *rsp;
	arsp->rel_flags |= flags;

	rcp->rc_free++;
	ofl->ofl_actrelscnt++;

	/*
	 * Any GOT relocation reference requires the creation of a .got table.
	 * Most references to a .got require a .got entry,  which is accounted
	 * for with the ofl_gotcnt counter.  However, some references are
	 * relative to the .got table, but require no .got entry.  This test
	 * insures a .got is created regardless of the type of reference.
	 */
	if (IS_GOT_REQUIRED(arsp->rel_rtype))
		ofl->ofl_flags |= FLG_OF_BLDGOT;

	/*
	 * If this is a displacement relocation generate a warning.
	 */
	if (arsp->rel_flags & FLG_REL_DISP) {
		ofl->ofl_dtflags_1 |= DF_1_DISPRELDNE;

		if (ofl->ofl_flags & FLG_OF_VERBOSE)
			ld_disp_errmsg(MSG_INTL(MSG_REL_DISPREL3), arsp, ofl);
	}

	DBG_CALL(Dbg_reloc_ars_entry(ofl->ofl_lml, ELF_DBG_LD,
	    arsp->rel_isdesc->is_shdr->sh_type, M_MACH, arsp));
	return (1);
}

uintptr_t
ld_reloc_GOT_relative(Boolean local, Rel_desc *rsp, Ofl_desc *ofl)
{
	Sym_desc	*sdp;
	Word		flags = ofl->ofl_flags;
	Gotndx		*gnp;

	sdp = rsp->rel_sym;

	/*
	 * If this is the first time we've seen this symbol in a GOT
	 * relocation we need to assign it a GOT token.  Once we've got
	 * all of the GOT's assigned we can assign the actual indexes.
	 */
	if ((gnp = ld_find_gotndx(&(sdp->sd_GOTndxs), GOT_REF_GENERIC,
	    ofl, rsp)) == 0) {
		Word	rtype = rsp->rel_rtype;

		if (ld_assign_got_ndx(&(sdp->sd_GOTndxs), 0, GOT_REF_GENERIC,
		    ofl, rsp, sdp) == S_ERROR)
			return (S_ERROR);

		/*
		 * Now we initialize the GOT table entry.
		 *
		 * Pseudo code to describe the the decisions below:
		 *
		 * If (local)
		 * then
		 *	enter symbol value in GOT table entry
		 *	if (Shared Object)
		 *	then
		 *		create Relative relocation against symbol
		 *	fi
		 * else
		 *	clear GOT table entry
		 *	create a GLOB_DAT relocation against symbol
		 * fi
		 */
		if (local == TRUE) {
			if (flags & FLG_OF_SHAROBJ) {
				if (ld_add_actrel((FLG_REL_GOT | FLG_REL_GOTCL),
				    rsp, ofl) == S_ERROR)
					return (S_ERROR);

				/*
				 * Add a RELATIVE relocation if this is
				 * anything but a ABS symbol.
				 */
				if ((((sdp->sd_flags & FLG_SY_SPECSEC) == 0) ||
				    (sdp->sd_sym->st_shndx != SHN_ABS)) ||
				    (sdp->sd_aux && sdp->sd_aux->sa_symspec)) {
					rsp->rel_rtype = M_R_RELATIVE;
					if (ld_add_outrel((FLG_REL_GOT |
					    FLG_REL_ADVAL), rsp,
					    ofl) == S_ERROR)
						return (S_ERROR);
					rsp->rel_rtype = rtype;
				}
			} else {
				if (ld_add_actrel(FLG_REL_GOT, rsp,
				    ofl) == S_ERROR)
					return (S_ERROR);
			}
		} else {
			rsp->rel_rtype = M_R_GLOB_DAT;
			if (ld_add_outrel(FLG_REL_GOT, rsp, ofl) == S_ERROR)
				return (S_ERROR);
			rsp->rel_rtype = rtype;
		}
	} else {
		if (ld_assign_got_ndx(&(sdp->sd_GOTndxs), gnp, GOT_REF_GENERIC,
		    ofl, rsp, sdp) == S_ERROR)
			return (S_ERROR);
	}

	/*
	 * Perform relocation to GOT table entry.
	 */
	return (ld_add_actrel(NULL, rsp, ofl));
}


/*
 * Perform relocations for PLT's
 */
uintptr_t
ld_reloc_plt(Rel_desc *rsp, Ofl_desc *ofl)
{
	Sym_desc	*sdp = rsp->rel_sym;

#if	defined(__i386) || defined(__amd64)
#if	defined(_ELF64)
	/*
	 * AMD64 TLS code sequences do not use a unique TLS relocation to
	 * reference the __tls_get_addr() function call.
	 */
	if ((ofl->ofl_flags & FLG_OF_EXEC) &&
	    (strcmp(sdp->sd_name, MSG_ORIG(MSG_SYM_TLSGETADDR_U)) == 0)) {
		return (ld_add_actrel(FLG_REL_TLSFIX, rsp, ofl));
	}
#else
	/*
	 * GNUC IA32 TLS code sequences do not use a unique TLS relocation to
	 * reference the ___tls_get_addr() function call.
	 */
	if ((ofl->ofl_flags & FLG_OF_EXEC) &&
	    (strcmp(sdp->sd_name, MSG_ORIG(MSG_SYM_TLSGETADDR_UU)) == 0)) {
		return (ld_add_actrel(FLG_REL_TLSFIX, rsp, ofl));
	}
#endif
#endif	/* __i386 && __amd64 */

	/*
	 * if (not PLT yet assigned)
	 * then
	 *	assign PLT index to symbol
	 *	build output JMP_SLOT relocation
	 * fi
	 */
	if (sdp->sd_aux->sa_PLTndx == 0) {
		Word	ortype = rsp->rel_rtype;

		ld_assign_plt_ndx(sdp, ofl);

		/*
		 * If this symbol is binding to a LAZYLOADED object then
		 * set the LAZYLD symbol flag.
		 */
		if ((sdp->sd_aux->sa_bindto &&
		    (sdp->sd_aux->sa_bindto->ifl_flags & FLG_IF_LAZYLD)) ||
		    (sdp->sd_file &&
		    (sdp->sd_file->ifl_flags & FLG_IF_LAZYLD)))
			sdp->sd_flags |= FLG_SY_LAZYLD;

		rsp->rel_rtype = M_R_JMP_SLOT;
		if (ld_add_outrel(FLG_REL_PLT, rsp, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;
	}

	/*
	 * Perform relocation to PLT table entry.
	 */
	if ((ofl->ofl_flags & FLG_OF_SHAROBJ) &&
	    IS_ADD_RELATIVE(rsp->rel_rtype)) {
		Word	ortype	= rsp->rel_rtype;

		rsp->rel_rtype = M_R_RELATIVE;
		if (ld_add_outrel(FLG_REL_ADVAL, rsp, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;
		return (1);
	} else
		return (ld_add_actrel(NULL, rsp, ofl));
}

/*
 * process GLOBAL undefined and ref_dyn_need symbols.
 */
static uintptr_t
reloc_exec(Rel_desc *rsp, Ofl_desc *ofl)
{
	Sym_desc	*_sdp, *sdp = rsp->rel_sym;
	Sym_aux		*sap = sdp->sd_aux;
	Sym		*sym = sdp->sd_sym;
	Addr		stval;

	/*
	 * Reference is to a function so simply create a plt entry for it.
	 */
	if (ELF_ST_TYPE(sym->st_info) == STT_FUNC)
		return (ld_reloc_plt(rsp, ofl));

	/*
	 * Catch absolutes - these may cause a text relocation.
	 */
	if ((sdp->sd_flags & FLG_SY_SPECSEC) && (sym->st_shndx == SHN_ABS)) {
		if ((ofl->ofl_flags1 & FLG_OF1_ABSEXEC) == 0)
			return (ld_add_outrel(NULL, rsp, ofl));

		/*
		 * If -zabsexec is set then promote the ABSOLUTE symbol to
		 * current the current object and perform the relocation now.
		 */
		sdp->sd_ref = REF_REL_NEED;
		return (ld_add_actrel(NULL, rsp, ofl));
	}

	/*
	 * If the relocation is against a writable section simply compute the
	 * necessary output relocation.  As an optimization, if the symbol has
	 * already been transformed into a copy relocation then we can perform
	 * the relocation directly (copy relocations should only be generated
	 * for references from the text segment and these relocations are
	 * normally carried out before we get to the data segment relocations).
	 */
	if ((ELF_ST_TYPE(sym->st_info) == STT_OBJECT) &&
	    (rsp->rel_osdesc->os_shdr->sh_flags & SHF_WRITE)) {
		if (sdp->sd_flags & FLG_SY_MVTOCOMM)
			return (ld_add_actrel(NULL, rsp, ofl));
		else
			return (ld_add_outrel(NULL, rsp, ofl));
	}

	/*
	 * If the reference isn't to an object (normally because a .type
	 * directive hasn't defined in some assembler source), then simply apply
	 * a generic relocation (this has a tendency to result in text
	 * relocations).
	 */
	if (ELF_ST_TYPE(sym->st_info) != STT_OBJECT) {
		eprintf(ofl->ofl_lml, ERR_WARNING, MSG_INTL(MSG_REL_UNEXPSYM),
		    conv_sym_info_type(sdp->sd_file->ifl_ehdr->e_machine,
		    ELF_ST_TYPE(sym->st_info), 0),
		    rsp->rel_isdesc->is_file->ifl_name,
		    demangle(rsp->rel_sname), sdp->sd_file->ifl_name);
		return (ld_add_outrel(NULL, rsp, ofl));
	}

	/*
	 * Prepare for generating a copy relocation.
	 *
	 * If this symbol is one of an alias pair, we need to insure both
	 * symbols become part of the output (the strong symbol will be used to
	 * maintain the symbols state).  And, if we did raise the precedence of
	 * a symbol we need to check and see if this is a weak symbol.  If it is
	 * we want to use it's strong counter part.
	 *
	 * The results of this logic should be:
	 *	rel_usym: assigned to strong
	 *	 rel_sym: assigned to symbol to perform
	 *		  copy_reloc against (weak or strong).
	 */
	if (sap->sa_linkndx) {
		_sdp = sdp->sd_file->ifl_oldndx[sap->sa_linkndx];

		if (_sdp->sd_ref < sdp->sd_ref) {
			_sdp->sd_ref = sdp->sd_ref;
			_sdp->sd_flags |= FLG_SY_REFRSD;

			/*
			 * As we're going to replicate a symbol from a shared
			 * object, retain its correct binding status.
			 */
			if (ELF_ST_BIND(_sdp->sd_sym->st_info) == STB_GLOBAL)
				_sdp->sd_flags |= FLG_SY_GLOBREF;

		} else if (_sdp->sd_ref > sdp->sd_ref) {
			sdp->sd_ref = _sdp->sd_ref;
			sdp->sd_flags |= FLG_SY_REFRSD;

			/*
			 * As we're going to replicate a symbol from a shared
			 * object, retain its correct binding status.
			 */
			if (ELF_ST_BIND(sym->st_info) == STB_GLOBAL)
				sdp->sd_flags |= FLG_SY_GLOBREF;
		}

		/*
		 * If this is a weak symbol then we want to move the strong
		 * symbol into local .bss.  If there is a copy_reloc to be
		 * performed, that should still occur against the WEAK symbol.
		 */
		if ((ELF_ST_BIND(sdp->sd_sym->st_info) == STB_WEAK) ||
		    (sdp->sd_flags & FLG_SY_WEAKDEF))
			rsp->rel_usym = _sdp;
	} else
		_sdp = 0;

	/*
	 * If the reference is to an object then allocate space for the object
	 * within the executables .bss.  Relocations will now be performed from
	 * this new location.  If the original shared objects data is
	 * initialized, then generate a copy relocation that will copy the data
	 * to the executables .bss at runtime.
	 */
	if (!(rsp->rel_usym->sd_flags & FLG_SY_MVTOCOMM)) {
		Word	rtype = rsp->rel_rtype;
		Copy_rel *cpy_rel;

		/*
		 * Indicate that the symbol(s) against which we're relocating
		 * have been moved to the executables common.  Also, insure that
		 * the symbol(s) remain marked as global, as the shared object
		 * from which they are copied must be able to relocate to the
		 * new common location within the executable.
		 *
		 * Note that even though a new symbol has been generated in the
		 * output files' .bss, the symbol must remain REF_DYN_NEED and
		 * not be promoted to REF_REL_NEED.  sym_validate() still needs
		 * to carry out a number of checks against the symbols binding
		 * that are triggered by the REF_DYN_NEED state.
		 */
		sdp->sd_flags |= FLG_SY_MVTOCOMM;
		sdp->sd_flags1 |= FLG_SY1_GLOB;
		sdp->sd_flags1 &= ~FLG_SY1_LOCL;
		sdp->sd_sym->st_other &= ~MSK_SYM_VISIBILITY;
		if (_sdp) {
			_sdp->sd_flags |= FLG_SY_MVTOCOMM;
			_sdp->sd_flags1 |= FLG_SY1_GLOB;
			_sdp->sd_flags1 &= ~FLG_SY1_LOCL;
			_sdp->sd_sym->st_other &= ~MSK_SYM_VISIBILITY;

			/*
			 * Make sure the symbol has a reference in case of any
			 * error diagnostics against it (perhaps this belongs
			 * to a version that isn't allowable for this build).
			 * The resulting diagnostic (see sym_undef_entry())
			 * might seem a little bogus, as the symbol hasn't
			 * really been referenced by this file, but has been
			 * promoted as a consequence of its alias reference.
			 */
			if (!(_sdp->sd_aux->sa_rfile))
				_sdp->sd_aux->sa_rfile = sdp->sd_aux->sa_rfile;
		}

		/*
		 * Assign the symbol to the bss and insure sufficient alignment
		 * (we don't know the real alignment so we have to make the
		 * worst case guess).
		 */
		_sdp = rsp->rel_usym;
		stval = _sdp->sd_sym->st_value;
		if (ld_sym_copy(_sdp) == S_ERROR)
			return (S_ERROR);
		_sdp->sd_shndx = _sdp->sd_sym->st_shndx = SHN_COMMON;
		_sdp->sd_flags |= FLG_SY_SPECSEC;
		_sdp->sd_sym->st_value =
		    (_sdp->sd_sym->st_size < (M_WORD_ALIGN * 2)) ?
		    M_WORD_ALIGN : M_WORD_ALIGN * 2;

		/*
		 * Whether or not the symbol references initialized
		 * data we generate a copy relocation - this differs
		 * from the past where we would not create the COPY_RELOC
		 * if we were binding against .bss.  This is done
		 * for *two* reasons.
		 *
		 *  o If the symbol in the shared object changes to
		 *    a initialized data - we need the COPY to pick it
		 *    up.
		 *  o without the COPY RELOC we can't tell that the
		 *    symbol from the COPY'd object has been moved
		 *    and all bindings to it should bind here.
		 */

		/*
		 * Keep this symbol in the copy relocation list
		 * to check the validity later.
		 */
		if ((cpy_rel = libld_malloc(sizeof (Copy_rel))) == 0)
			return (S_ERROR);
		cpy_rel->copyrel_symd = _sdp;
		cpy_rel->copyrel_stval = stval;
		if (list_appendc(&ofl->ofl_copyrels, cpy_rel) == 0)
			return (S_ERROR);

		rsp->rel_rtype = M_R_COPY;
		if (ld_add_outrel(FLG_REL_BSS, rsp, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = rtype;

		/*
		 * If this symbol is a protected symbol, warn it.
		 */
		if (_sdp->sd_flags & FLG_SY_PROT)
			eprintf(ofl->ofl_lml, ERR_WARNING,
			MSG_INTL(MSG_REL_COPY),
			conv_reloc_type(_sdp->sd_file->ifl_ehdr->e_machine,
			M_R_COPY, 0), _sdp->sd_file->ifl_name, _sdp->sd_name);
		DBG_CALL(Dbg_syms_reloc(ofl, sdp));
	}
	return (ld_add_actrel(NULL, rsp, ofl));
}

/*
 * All relocations should have been handled by the other routines.  This
 * routine is here as a catch all, if we do enter it we've goofed - but
 * we'll try and to the best we can.
 */
static uintptr_t
reloc_generic(Rel_desc *rsp, Ofl_desc *ofl)
{
	Ifl_desc	*ifl = rsp->rel_isdesc->is_file;

	eprintf(ofl->ofl_lml, ERR_WARNING, MSG_INTL(MSG_REL_UNEXPREL),
	    conv_reloc_type(ifl->ifl_ehdr->e_machine, rsp->rel_rtype, 0),
	    ifl->ifl_name, demangle(rsp->rel_sname));

	/*
	 * If building a shared object then put the relocation off
	 * until runtime.
	 */
	if (ofl->ofl_flags & FLG_OF_SHAROBJ)
		return (ld_add_outrel(NULL, rsp, ofl));

	/*
	 * Otherwise process relocation now.
	 */
	return (ld_add_actrel(NULL, rsp, ofl));
}

/*
 * Process relocations when building a relocatable object.  Typically, there
 * aren't many relocations that can be caught at this point, most are simply
 * passed through to the output relocatable object.
 */
static uintptr_t
reloc_relobj(Boolean local, Rel_desc *rsp, Ofl_desc *ofl)
{
	Word		rtype = rsp->rel_rtype;
	Sym_desc	*sdp = rsp->rel_sym;
	Is_desc		*isp = rsp->rel_isdesc;
	Word		oflags = NULL;

	/*
	 * Determine if we can do any relocations at this point.  We can if:
	 *
	 *	this is local_symbol and a non-GOT relocation, and
	 *	the relocation is pc-relative, and
	 *	the relocation is against a symbol in same section
	 */
	if (local && !IS_GOT_RELATIVE(rtype) && !IS_GOT_BASED(rtype) &&
	    !IS_GOT_PC(rtype) && IS_PC_RELATIVE(rtype) &&
	    ((sdp->sd_isc) && (sdp->sd_isc->is_osdesc == isp->is_osdesc)))
		return (ld_add_actrel(NULL, rsp, ofl));

	/*
	 * If -zredlocsym is in effect, translate all local symbol relocations
	 * to be against against section symbols, since section symbols are
	 * the only symbols which will be added to the .symtab.
	 */
	if (local && (((ofl->ofl_flags1 & FLG_OF1_REDLSYM) &&
	    (ELF_ST_BIND(sdp->sd_sym->st_info) == STB_LOCAL)) ||
	    ((sdp->sd_flags1 & FLG_SY1_ELIM) &&
	    (ofl->ofl_flags & FLG_OF_PROCRED)))) {
		/*
		 * But if this is PIC code, don't allow it for now.
		 */
		if (IS_GOT_RELATIVE(rsp->rel_rtype)) {
			Ifl_desc	*ifl = rsp->rel_isdesc->is_file;

			eprintf(ofl->ofl_lml, ERR_FATAL,
			    MSG_INTL(MSG_REL_PICREDLOC),
			    demangle(rsp->rel_sname), ifl->ifl_name,
			    conv_reloc_type(ifl->ifl_ehdr->e_machine,
			    rsp->rel_rtype, 0));
			return (S_ERROR);
		}

		/*
		 * Indicate that this relocation should be processed the same
		 * as a section symbol.  For SPARC and AMD64 (Rela), indicate
		 * that the addend also needs to be applied to this relocation.
		 */
#if	(defined(__i386) || defined(__amd64)) && !defined(_ELF64)
		oflags = FLG_REL_SCNNDX;
#else
		oflags = FLG_REL_SCNNDX | FLG_REL_ADVAL;
#endif
	}

#if	(defined(__i386) || defined(__amd64)) && !defined(_ELF64)
	/*
	 * Intel (Rel) relocations do not contain an addend.  Any addend is
	 * contained within the file at the location identified by the
	 * relocation offset.  Therefore, if we're processing a section symbol,
	 * or a -zredlocsym relocation (that basically transforms a local symbol
	 * reference into a section reference), perform an active relocation to
	 * propagate any addend.
	 */
	if ((ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION) ||
	    (oflags == FLG_REL_SCNNDX))
		if (ld_add_actrel(NULL, rsp, ofl) == S_ERROR)
			return (S_ERROR);
#endif
	return (ld_add_outrel(oflags, rsp, ofl));
}

/*
 * Perform any generic TLS validations before passing control to machine
 * specific routines.  At this point we know we are dealing with an executable
 * or shared object - relocatable objects have already been processed.
 */
static uintptr_t
reloc_TLS(Boolean local, Rel_desc *rsp, Ofl_desc *ofl)
{
	Word		rtype = rsp->rel_rtype, flags = ofl->ofl_flags;
	Ifl_desc	*ifl = rsp->rel_isdesc->is_file;
	Half		mach = ifl->ifl_ehdr->e_machine;
	Sym_desc	*sdp = rsp->rel_sym;
	unsigned char	type;

	/*
	 * All TLS relocations are illegal in a static executable.
	 */
	if ((flags & (FLG_OF_STATIC | FLG_OF_EXEC)) ==
	    (FLG_OF_STATIC | FLG_OF_EXEC)) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_REL_TLSSTAT),
		    conv_reloc_type(mach, rtype, 0), ifl->ifl_name,
		    demangle(rsp->rel_sname));
		return (S_ERROR);
	}

	/*
	 * Any TLS relocation must be against a STT_TLS symbol, all others
	 * are illegal.
	 */
	if ((type = ELF_ST_TYPE(sdp->sd_sym->st_info)) != STT_TLS) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_REL_TLSBADSYM),
		    conv_reloc_type(mach, rtype, 0), ifl->ifl_name,
		    demangle(rsp->rel_sname),
		    conv_sym_info_type(mach, type, 0));
		return (S_ERROR);
	}

	/*
	 * A dynamic executable can not use the LD or LE reference models to
	 * reference an external symbol.  A shared object can not use the LD
	 * reference model to reference an external symbol.
	 */
	if (!local && (IS_TLS_LD(rtype) ||
	    ((flags & FLG_OF_EXEC) && IS_TLS_LE(rtype)))) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_REL_TLSBND),
		    conv_reloc_type(mach, rtype, 0), ifl->ifl_name,
		    demangle(rsp->rel_sname), sdp->sd_file->ifl_name);
		return (S_ERROR);
	}

	/*
	 * The TLS LE model is only allowed for dynamic executables.  The TLS IE
	 * model is allowed for shared objects, but this model has restrictions.
	 * This model can only be used freely in dependencies that are loaded
	 * immediately as part of process initialization.  However, during the
	 * initial runtime handshake with libc that establishes the thread
	 * pointer, a small backup TLS reservation is created.  This area can
	 * be used by objects that are loaded after threads are initialized.
	 * However, this area is limited in size and may have already been
	 * used.  This area is intended for specialized applications, and does
	 * not provide the degree of flexibility dynamic TLS can offer.  Under
	 * -z verbose indicate this restriction to the user.
	 */
	if ((flags & FLG_OF_EXEC) == 0) {
		if (IS_TLS_LE(rtype)) {
			eprintf(ofl->ofl_lml, ERR_FATAL,
			    MSG_INTL(MSG_REL_TLSLE),
			    conv_reloc_type(mach, rtype, 0), ifl->ifl_name,
			    demangle(rsp->rel_sname));
			return (S_ERROR);

		} else if ((IS_TLS_IE(rtype)) && (flags & FLG_OF_VERBOSE)) {
			eprintf(ofl->ofl_lml, ERR_WARNING,
			    MSG_INTL(MSG_REL_TLSIE),
			    conv_reloc_type(mach, rtype, 0), ifl->ifl_name,
			    demangle(rsp->rel_sname));
		}
	}

	return (ld_reloc_TLS(local, rsp, ofl));
}

uintptr_t
ld_process_sym_reloc(Ofl_desc *ofl, Rel_desc *reld, Rel *reloc, Is_desc *isp,
    const char *isname)
{
	Word		rtype = reld->rel_rtype;
	Word		flags = ofl->ofl_flags;
	Sym_desc	*sdp = reld->rel_sym;
	Sym_aux		*sap;
	Boolean		local;

	DBG_CALL(Dbg_reloc_in(ofl->ofl_lml, ELF_DBG_LD, M_MACH, M_REL_SHT_TYPE,
	    (void *)reloc, isname, reld->rel_sname));

	/*
	 * Indicate this symbol is being used for relocation and therefore must
	 * have its output address updated accordingly (refer to update_osym()).
	 */
	sdp->sd_flags |= FLG_SY_UPREQD;

	/*
	 * Indicate the section this symbol is defined in has been referenced,
	 * therefor it *is not* a candidate for elimination.
	 */
	if (sdp->sd_isc) {
		sdp->sd_isc->is_flags |= FLG_IS_SECTREF;
		sdp->sd_isc->is_file->ifl_flags |= FLG_IF_FILEREF;
	}

	reld->rel_usym = sdp;

	/*
	 * Determine if this symbol is actually an alias to another symbol.  If
	 * so, and the alias is not REF_DYN_SEEN, set rel_usym to point to the
	 * weak symbols strong counter-part.  The one exception is if the
	 * FLG_SY_MVTOCOMM flag is set on the weak symbol.  If this is the case,
	 * the strong is only here because of its promotion, and the weak symbol
	 * should still be used for the relocation reference (see reloc_exec()).
	 */
	sap = sdp->sd_aux;
	if (sap && sap->sa_linkndx &&
	    ((ELF_ST_BIND(sdp->sd_sym->st_info) == STB_WEAK) ||
	    (sdp->sd_flags & FLG_SY_WEAKDEF)) &&
	    (!(sdp->sd_flags & FLG_SY_MVTOCOMM))) {
		Sym_desc *	_sdp;

		_sdp = sdp->sd_file->ifl_oldndx[sap->sa_linkndx];
		if (_sdp->sd_ref != REF_DYN_SEEN)
			reld->rel_usym = _sdp;
	}

	/*
	 * Determine whether this symbol should be bound locally or not.
	 * Symbols will be bound locally if one of the following is true:
	 *
	 *  o	the symbol is of type STB_LOCAL
	 *
	 *  o	the output image is not a relocatable object and the
	 *	relocation is relative to the .got
	 *
	 *  o	the section being relocated is of type SHT_SUNW_dof;
	 *	these sections are bound to the functions in the
	 *	containing object and don't care about interpositioning
	 *
	 *  o	the symbol has been reduced (scoped to a local or
	 *	symbolic) and reductions are being processed
	 *
	 *  o	the -Bsymbolic flag is in use when building a shared
	 *	object or an executable (fixed address) is being created
	 *
	 *  o	Building an executable and the symbol is defined
	 *	in the executable.
	 *
	 *  o	the relocation is against a segment which will not
	 *	be loaded into memory.  If that is the case we need
	 *	to resolve the relocation now because ld.so.1 won't
	 *	be able to.
	 */
	local = FALSE;
	if (ELF_ST_BIND(sdp->sd_sym->st_info) == STB_LOCAL) {
		local = TRUE;
	} else if (!(reld->rel_flags & FLG_REL_LOAD)) {
		local = TRUE;
	} else if (sdp->sd_sym->st_shndx != SHN_UNDEF) {
		if (reld->rel_isdesc &&
		    reld->rel_isdesc->is_shdr->sh_type == SHT_SUNW_dof) {
			local = TRUE;
		} else if (!(flags & FLG_OF_RELOBJ) &&
		    (IS_LOCALBND(rtype) || IS_SEG_RELATIVE(rtype))) {
			local = TRUE;
		} else if (sdp->sd_ref == REF_REL_NEED) {
			if ((sdp->sd_flags1 & (FLG_SY1_LOCL | FLG_SY1_PROT)))
				local = TRUE;
			else if (flags & (FLG_OF_SYMBOLIC | FLG_OF_EXEC))
				local = TRUE;
		}
	}

	/*
	 * If this is a PC_RELATIVE relocation, the relocation could be
	 * compromised if the relocated address is later used as a copy
	 * relocated symbol (PSARC 1999/636, bugid 4187211).  Scan the input
	 * files symbol table to cross reference this relocation offset.
	 */
	if ((ofl->ofl_flags & FLG_OF_SHAROBJ) && IS_PC_RELATIVE(rtype) &&
	    (IS_GOT_PC(rtype) == 0) && (IS_PLT(rtype) == 0)) {
		if (disp_inspect(ofl, reld, local) == S_ERROR)
			return (S_ERROR);
	}

	/*
	 * GOT based relocations must bind to the object being built - since
	 * they are relevant to the current GOT.  If not building a relocatable
	 * object - give a appropriate error message.
	 */
	if (!local && !(flags & FLG_OF_RELOBJ) && IS_GOT_BASED(rtype)) {
		Ifl_desc	*ifl = reld->rel_isdesc->is_file;

		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_REL_BADGOTBASED),
		    conv_reloc_type(ifl->ifl_ehdr->e_machine, rtype, 0),
		    ifl->ifl_name, demangle(sdp->sd_name));
		return (S_ERROR);
	}

	/*
	 * TLS symbols can only have TLS relocations.
	 */
	if ((ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_TLS) &&
	    (IS_TLS_INS(rtype) == 0)) {
		/*
		 * The above test is relaxed if the target section is
		 * non-allocable.
		 */
		if (reld->rel_osdesc->os_shdr->sh_flags & SHF_ALLOC) {
			Ifl_desc	*ifl = reld->rel_isdesc->is_file;

			eprintf(ofl->ofl_lml, ERR_FATAL,
			    MSG_INTL(MSG_REL_BADTLS),
			    conv_reloc_type(ifl->ifl_ehdr->e_machine,
			    rtype, 0), ifl->ifl_name, demangle(sdp->sd_name));
			return (S_ERROR);
		}
	}

	/*
	 * Select the relocation to perform.
	 */
	if (IS_REGISTER(rtype))
		return (ld_reloc_register(reld, isp, ofl));

	if (flags & FLG_OF_RELOBJ)
		return (reloc_relobj(local, reld, ofl));

	if (IS_TLS_INS(rtype))
		return (reloc_TLS(local, reld, ofl));

	if (IS_GOT_INS(rtype))
		return (ld_reloc_GOTOP(local, reld, ofl));

	if (IS_GOT_RELATIVE(rtype))
		return (ld_reloc_GOT_relative(local, reld, ofl));

	if (local)
		return (ld_reloc_local(reld, ofl));

	if ((IS_PLT(rtype)) && ((flags & FLG_OF_BFLAG) == 0))
		return (ld_reloc_plt(reld, ofl));

	if ((sdp->sd_ref == REF_REL_NEED) ||
	    (flags & FLG_OF_BFLAG) || (flags & FLG_OF_SHAROBJ) ||
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_NOTYPE))
		return (ld_add_outrel(NULL, reld, ofl));

	if (sdp->sd_ref == REF_DYN_NEED)
		return (reloc_exec(reld, ofl));

	/*
	 * IS_NOT_REL(rtype)
	 */
	return (reloc_generic(reld, ofl));
}

/*
 * Generate relocation descriptor and dispatch
 */
static uintptr_t
process_reld(Ofl_desc *ofl, Is_desc *isp, Rel_desc *reld, Word rsndx,
    Rel *reloc)
{
	Ifl_desc	*ifl = isp->is_file;
	Word		rtype = reld->rel_rtype;
	Sym_desc	*sdp;

	/*
	 * Make sure the relocation is in the valid range.
	 */
	if (rtype >= M_R_NUM) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_REL_INVALRELT),
		    ifl->ifl_name, isp->is_name, rtype);
		return (S_ERROR);
	}

	ofl->ofl_entrelscnt++;

	/*
	 * Special case: a register symbol associated with symbol index 0 is
	 * initialized (i.e., relocated) to a constant from the r_addend field
	 * rather than from a symbol value.
	 */
	if (IS_REGISTER(rtype) && (rsndx == 0)) {
		reld->rel_sym = 0;
		reld->rel_sname = MSG_ORIG(MSG_STR_EMPTY);

		DBG_CALL(Dbg_reloc_in(ofl->ofl_lml, ELF_DBG_LD, M_MACH,
		    isp->is_shdr->sh_type, (void *)reloc, isp->is_name,
		    reld->rel_sname));
		return (ld_reloc_register(reld, isp, ofl));
	}

	/*
	 * Determine whether we're dealing with a named symbol.  Note, bogus
	 * relocations can result in a null symbol descriptor (sdp), the error
	 * condition should be caught below after determining whether a valid
	 * symbol name exists.
	 */
	sdp = ifl->ifl_oldndx[rsndx];
	if (sdp != NULL && sdp->sd_name && *sdp->sd_name)
		reld->rel_sname = sdp->sd_name;
	else {
		static char *strunknown;

		if (strunknown == 0)
			strunknown = (char *)MSG_INTL(MSG_STR_UNKNOWN);
		reld->rel_sname = strunknown;
	}

	/*
	 * If for some reason we have a null relocation record issue a
	 * warning and continue (the compiler folks can get into this
	 * state some time).  Normal users should never see this error.
	 */
	if (rtype == M_R_NONE) {
		DBG_CALL(Dbg_reloc_in(ofl->ofl_lml, ELF_DBG_LD, M_MACH,
		    M_REL_SHT_TYPE, (void *)reloc, isp->is_name,
		    reld->rel_sname));
		eprintf(ofl->ofl_lml, ERR_WARNING, MSG_INTL(MSG_REL_NULL),
		    ifl->ifl_name, isp->is_name);
		return (1);
	}

	if (((ofl->ofl_flags & FLG_OF_RELOBJ) == 0) && IS_NOTSUP(rtype)) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_REL_NOTSUP),
		    conv_reloc_type(ifl->ifl_ehdr->e_machine, rtype, 0),
		    ifl->ifl_name, isp->is_name);
		return (S_ERROR);
	}

	/*
	 * If we are here, we know that the relocation requires reference
	 * symbol. If no symbol is assigned, this is a fatal error.
	 */
	if (sdp == NULL) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_REL_NOSYMBOL),
		    conv_reloc_type(ifl->ifl_ehdr->e_machine, rtype, 0),
		    isp->is_name, ifl->ifl_name, EC_XWORD(reloc->r_offset));
		return (S_ERROR);
	}

	if (sdp->sd_flags1 & FLG_SY1_IGNORE)
		return (1);

	/*
	 * If this symbol is part of a DISCARDED section attempt to find another
	 * definition.
	 */
	if (sdp->sd_flags & FLG_SY_ISDISC) {
		Sym_desc *	nsdp;

		if ((reld->rel_sname != sdp->sd_name) ||
		    (ELF_ST_BIND(sdp->sd_sym->st_info) == STB_LOCAL) ||
		    ((nsdp = ld_sym_find(sdp->sd_name, SYM_NOHASH, 0,
		    ofl)) == 0)) {
			eprintf(ofl->ofl_lml, ERR_FATAL,
			    MSG_INTL(MSG_REL_SYMDISC), ifl->ifl_name,
			    isp->is_name, demangle(sdp->sd_name),
			    sdp->sd_isc->is_name);
			return (S_ERROR);
		}
		ifl->ifl_oldndx[rsndx] = sdp = nsdp;
	}

	/*
	 * If this is a global symbol, determine whether its visibility needs
	 * adjusting.
	 */
	if (sdp->sd_aux && ((sdp->sd_flags & FLG_SY_VISIBLE) == 0))
		ld_sym_adjust_vis(sdp, ofl);

	/*
	 * Ignore any relocation against a section that will not be in the
	 * output file (has been stripped).
	 */
	if ((sdp->sd_isc == 0) &&
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION))
		return (1);

	/*
	 * If the input section exists, but the section has not been associated
	 * to an output section, then this is a little suspicious.
	 */
	if (sdp->sd_isc && (sdp->sd_isc->is_osdesc == 0) &&
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION)) {
		eprintf(ofl->ofl_lml, ERR_WARNING, MSG_INTL(MSG_RELINVSEC),
		    conv_reloc_type(ifl->ifl_ehdr->e_machine, rtype, 0),
		    ifl->ifl_name, isp->is_name, sdp->sd_isc->is_name);
		return (1);
	}

	/*
	 * If the symbol for this relocation is invalid (which should have
	 * generated a message during symbol processing), or the relocation
	 * record's symbol reference is in any other way invalid, then it's
	 * about time we gave up.
	 */
	if ((sdp->sd_flags & FLG_SY_INVALID) || (rsndx == 0) ||
	    (rsndx >= ifl->ifl_symscnt)) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_REL_UNKNWSYM),
		    conv_reloc_type(ifl->ifl_ehdr->e_machine, rtype, 0),
		    ifl->ifl_name, isp->is_name, demangle(reld->rel_sname),
		    EC_XWORD(reloc->r_offset), EC_WORD(rsndx));
		return (S_ERROR);
	}

	reld->rel_sym = sdp;
	return (ld_process_sym_reloc(ofl, reld, reloc, isp, isp->is_name));
}

static uintptr_t
reloc_section(Ofl_desc *ofl, Is_desc *isect, Is_desc *rsect, Os_desc *osect)
{
	Rel		*rend;		/* end of relocation section data */
	Rel		*reloc;		/* current relocation entry */
	Xword		rsize;		/* size of relocation section data */
	Xword		entsize;	/* size of relocation entry */
	Rel_desc	reld;		/* relocation descriptor */
	Shdr *		shdr;
	Word		flags = 0;

	shdr = rsect->is_shdr;
	rsize = shdr->sh_size;
	reloc = (Rel *)rsect->is_indata->d_buf;

	/*
	 * Decide entry size.
	 */
	if (((entsize = shdr->sh_entsize) == 0) || (entsize > rsize)) {
		if (shdr->sh_type == SHT_RELA)
			entsize = sizeof (Rela);
		else
			entsize = sizeof (Rel);
	}

	/*
	 * Build up the basic information in for the Rel_desc structure.
	 */
	reld.rel_osdesc = osect;
	reld.rel_isdesc = isect;
	reld.rel_move = 0;

	if ((ofl->ofl_flags & FLG_OF_RELOBJ) ||
	    (osect && (osect->os_sgdesc->sg_phdr.p_type == PT_LOAD)))
		flags |= FLG_REL_LOAD;

	if (shdr->sh_info == 0)
		flags |= FLG_REL_NOINFO;

	DBG_CALL(Dbg_reloc_proc(ofl->ofl_lml, osect, isect, rsect));

	for (rend = (Rel *)((uintptr_t)reloc + (uintptr_t)rsize);
	    reloc < rend;
	    reloc = (Rel *)((uintptr_t)reloc + (uintptr_t)entsize)) {
		Word	rsndx;

		/*
		 * Initialize the relocation record information and process
		 * the individual relocation.  Reinitialize the flags to
		 * insure we don't carry any state over from the previous
		 * relocation records processing.
		 */
		reld.rel_flags = flags;
		rsndx = ld_init_rel(&reld, (void *)reloc);

		if (process_reld(ofl, rsect, &reld, rsndx, reloc) == S_ERROR)
			return (S_ERROR);
	}
	return (1);
}

static uintptr_t
reloc_segments(int wr_flag, Ofl_desc *ofl)
{
	Listnode	*lnp1;
	Sg_desc		*sgp;
	Is_desc		*isp;

	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp)) {
		Os_desc	**ospp;
		Aliste	off;

		if ((sgp->sg_phdr.p_flags & PF_W) != wr_flag)
			continue;

		for (ALIST_TRAVERSE(sgp->sg_osdescs, off, ospp)) {
			Is_desc		*risp;
			Listnode	*lnp3;
			Os_desc		*osp = *ospp;

			osp->os_szoutrels = 0;
			for (LIST_TRAVERSE(&(osp->os_relisdescs), lnp3, risp)) {
				Word	indx;

				/*
				 * Determine the input section that this
				 * relocation information refers to.
				 */
				indx = risp->is_shdr->sh_info;
				isp = risp->is_file->ifl_isdesc[indx];

				/*
				 * Do not process relocations against sections
				 * which are being discarded (COMDAT)
				 */
				if (isp->is_flags & FLG_IS_DISCARD)
					continue;

				if (reloc_section(ofl, isp, risp, osp) ==
				    S_ERROR)
					return (S_ERROR);
			}

			/*
			 * Check for relocations against non-writable
			 * allocatable sections.
			 */
			if ((osp->os_szoutrels) &&
			    (sgp->sg_phdr.p_type == PT_LOAD) &&
			    ((sgp->sg_phdr.p_flags & PF_W) == 0)) {
				ofl->ofl_flags |= FLG_OF_TEXTREL;
				ofl->ofl_dtflags |= DF_TEXTREL;
			}
		}
	}

	return (1);
}

/*
 * Move Section related function
 * Get move entry
 */
static Move *
get_move_entry(Is_desc *rsect, Xword roffset)
{
	Ifl_desc	*ifile = rsect->is_file;
	Shdr		*rshdr = rsect->is_shdr;
	Is_desc		*misp;
	Shdr		*mshdr;
	Xword 		midx;
	Move		*ret;

	/*
	 * Set info for the target move section
	 */
	misp = ifile->ifl_isdesc[rshdr->sh_info];
	mshdr = (ifile->ifl_isdesc[rshdr->sh_info])->is_shdr;

	if (mshdr->sh_entsize == 0)
		return ((Move *)0);
	midx = roffset / mshdr->sh_entsize;

	ret = (Move *)misp->is_indata->d_buf;
	ret += midx;

	/*
	 * If this is an illgal entry, retun NULL.
	 */
	if ((midx * mshdr->sh_entsize) >= mshdr->sh_size)
		return ((Move *)0);
	return (ret);
}

/*
 * Relocation against Move Table.
 */
static uintptr_t
process_movereloc(Ofl_desc *ofl, Is_desc *rsect)
{
	Ifl_desc	*file = rsect->is_file;
	Rel		*rend, *reloc;
	Xword 		rsize, entsize;
	static Rel_desc reld_zero;
	Rel_desc 	reld;

	rsize = rsect->is_shdr->sh_size;
	reloc = (Rel *)rsect->is_indata->d_buf;

	reld = reld_zero;

	/*
	 * Decide entry size
	 */
	entsize = rsect->is_shdr->sh_entsize;
	if ((entsize == 0) ||
	    (entsize > rsect->is_shdr->sh_size)) {
		if (rsect->is_shdr->sh_type == SHT_RELA)
			entsize = sizeof (Rela);
		else
			entsize = sizeof (Rel);
	}

	/*
	 * Go through the relocation entries.
	 */
	for (rend = (Rel *)((uintptr_t)reloc + (uintptr_t)rsize);
	    reloc < rend;
	    reloc = (Rel *)((uintptr_t)reloc + (uintptr_t)entsize)) {
		Sym_desc *	psdp;
		Move *		mp;
		Word		rsndx;

		/*
		 * Initialize the relocation record information.
		 */
		reld.rel_flags = FLG_REL_LOAD;
		rsndx = ld_init_rel(&reld, (void *)reloc);

		if (((mp = get_move_entry(rsect, reloc->r_offset)) == 0) ||
		    ((reld.rel_move = libld_malloc(sizeof (Mv_desc))) == 0))
			return (S_ERROR);

		psdp = file->ifl_oldndx[ELF_M_SYM(mp->m_info)];
		reld.rel_move->mvd_move = mp;
		reld.rel_move->mvd_sym = psdp;

		if (psdp->sd_flags & FLG_SY_PAREXPN) {
			int	_num, num;

			reld.rel_osdesc = ofl->ofl_issunwdata1->is_osdesc;
			reld.rel_isdesc = ofl->ofl_issunwdata1;
			reld.rel_roffset = mp->m_poffset;

			for (num = mp->m_repeat, _num = 0; _num < num; _num++) {
				reld.rel_roffset +=
					/* LINTED */
					(_num * ELF_M_SIZE(mp->m_info));
				/*
				 * Generate Reld
				 */
				if (process_reld(ofl,
				    rsect, &reld, rsndx, reloc) == S_ERROR)
					return (S_ERROR);
			}
		} else {
			/*
			 * Generate Reld
			 */
			reld.rel_flags |= FLG_REL_MOVETAB;
			reld.rel_osdesc = ofl->ofl_osmove;
			reld.rel_isdesc =
				ofl->ofl_osmove->os_isdescs.head->data;
			if (process_reld(ofl,
			    rsect, &reld, rsndx, reloc) == S_ERROR)
				return (S_ERROR);
		}
	}
	return (1);
}

/*
 * This function is similar to reloc_init().
 *
 * This function is called when the SHT_SUNW_move table is expanded
 * and there were relocation against the SHT_SUNW_move section.
 */
static uintptr_t
reloc_movesections(Ofl_desc *ofl)
{
	Listnode	*lnp1;
	Is_desc		*risp;

	/*
	 * Generate/Expand relocation entries
	 */
	for (LIST_TRAVERSE(&ofl->ofl_mvrelisdescs, lnp1, risp)) {
		if (process_movereloc(ofl, risp) == S_ERROR)
			return (S_ERROR);
	}

	return (1);
}

/*
 * Count the number of output relocation entries, global offset table entries,
 * and procedure linkage table entries.  This function searches the segment and
 * outsect lists and passes each input reloc section to process_reloc().
 * It allocates space for any output relocations needed.  And builds up
 * the relocation structures for later processing.
 */
uintptr_t
ld_reloc_init(Ofl_desc *ofl)
{
	Listnode	*lnp;
	Is_desc		*isp;

	/*
	 * At this point we have finished processing all input symbols.  Make
	 * sure we add any absolute (internal) symbols before continuing with
	 * any relocation processing.
	 */
	if (ld_sym_spec(ofl) == S_ERROR)
		return (S_ERROR);

	ofl->ofl_gotcnt = M_GOT_XNumber;

	/*
	 * First process all of the relocations against NON-writable
	 * segments followed by relocations against the writeable segments.
	 *
	 * This separation is so that when the writable segments are processed
	 * we know whether or not a COPYRELOC will be produced for any symbols.
	 * If relocations aren't processed in this order, a COPYRELOC and a
	 * regular relocation can be produced against the same symbol.  The
	 * regular relocation would be redundant.
	 */
	if (reloc_segments(0, ofl) == S_ERROR)
		return (S_ERROR);

	if (reloc_segments(PF_W, ofl) == S_ERROR)
		return (S_ERROR);

	/*
	 * Process any extra relocations.  These are relocation sections that
	 * have a NULL sh_info.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_extrarels, lnp, isp)) {
		if (reloc_section(ofl, NULL, isp, NULL) == S_ERROR)
			return (S_ERROR);
	}

	/*
	 * If there were relocation against move table,
	 * process the relocation sections.
	 */
	if (reloc_movesections(ofl) == S_ERROR)
		return (S_ERROR);

	/*
	 * Now all the relocations are pre-processed,
	 * check the validity of copy relocations.
	 */
	if (ofl->ofl_copyrels.head != 0) {
		Copy_rel *	cpyrel;

		for (LIST_TRAVERSE(&ofl->ofl_copyrels, lnp, cpyrel)) {
			Sym_desc *	sdp;

			sdp = cpyrel->copyrel_symd;
			/*
			 * If there were no displacement relocation
			 * in this file, don't worry about it.
			 */
			if (sdp->sd_file->ifl_flags &
			    (FLG_IF_DISPPEND | FLG_IF_DISPDONE))
				is_disp_copied(ofl, cpyrel);
		}
	}

	/*
	 * GOT sections are created for dynamic executables and shared objects
	 * if the FLG_OF_BLDGOT is set, or explicit reference has been made to
	 * a GOT symbol.
	 */
	if (((ofl->ofl_flags & FLG_OF_RELOBJ) == 0) &&
	    ((ofl->ofl_flags & FLG_OF_BLDGOT) ||
	    (ld_sym_find(MSG_ORIG(MSG_SYM_GOFTBL),
	    SYM_NOHASH, 0, ofl) != 0) ||
	    (ld_sym_find(MSG_ORIG(MSG_SYM_GOFTBL_U),
	    SYM_NOHASH, 0, ofl) != 0))) {
		if (ld_make_got(ofl) == S_ERROR)
			return (S_ERROR);

#if	defined(sparc) || defined(__sparcv9)
		if (ld_allocate_got(ofl) == S_ERROR)
			return (S_ERROR);
#elif	defined(i386) || defined(__amd64)
/* nothing to do */
#else
#error Unknown architecture!
#endif
	}

	return (1);
}

/*
 * Simple comparison routine to be used by qsort() for
 * the sorting of the output relocation list.
 *
 * The reloc_compare() routine results in a relocation
 * table which is located on:
 *
 *	file referenced (NEEDED NDX)
 *	referenced symbol
 *	relocation offset
 *
 * This provides the most efficient traversal of the relocation
 * table at run-time.
 */
static int
reloc_compare(Reloc_list *i, Reloc_list *j)
{

	/*
	 * first - sort on neededndx
	 */
	if (i->rl_key1 > j->rl_key1)
		return (1);
	if (i->rl_key1 < j->rl_key1)
		return (-1);

	/*
	 * Then sort on symbol
	 */
	if ((uintptr_t)i->rl_key2 > (uintptr_t)j->rl_key2)
		return (1);
	if ((uintptr_t)i->rl_key2 < (uintptr_t)j->rl_key2)
		return (-1);

	/*
	 * i->key2 == j->key2
	 *
	 * At this point we fall back to key2 (offsets) to
	 * sort the output relocations.  Ideally this will
	 * make for the most efficient processing of these
	 * relocations at run-time.
	 */
	if (i->rl_key3 > j->rl_key3)
		return (1);
	if (i->rl_key3 < j->rl_key3)
		return (-1);
	return (0);
}

static uintptr_t
do_sorted_outrelocs(Ofl_desc *ofl)
{
	Rel_desc	*orsp;
	Rel_cache	*rcp;
	Listnode	*lnp;
	Reloc_list	*sorted_list;
	Word		index = 0;
	int		debug = 0;
	uintptr_t	error = 1;

	if ((sorted_list = libld_malloc((size_t)(sizeof (Reloc_list) *
	    ofl->ofl_reloccnt))) == NULL)
		return (S_ERROR);

	/*
	 * All but the PLT output relocations are sorted in the output file
	 * based upon their sym_desc.  By doing this multiple relocations
	 * against the same symbol are grouped together, thus when the object
	 * is later relocated by ld.so.1 it will take advantage of the symbol
	 * cache that ld.so.1 has.  This can significantly reduce the runtime
	 * relocation cost of a dynamic object.
	 *
	 * PLT relocations are not sorted because the order of the PLT
	 * relocations is used by ld.so.1 to determine what symbol a PLT
	 * relocation is against.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_outrels, lnp, rcp)) {
		/*LINTED*/
		for (orsp = (Rel_desc *)(rcp + 1);
		    orsp < rcp->rc_free; orsp++) {
			if (debug == 0) {
				DBG_CALL(Dbg_reloc_dooutrel(ofl->ofl_lml,
				    M_REL_SHT_TYPE));
				debug = 1;
			}

			/*
			 * If it's a PLT relocation we output it now in the
			 * order that it was originally processed.
			 */
			if (orsp->rel_flags & FLG_REL_PLT) {
				if (ld_perform_outreloc(orsp, ofl) == S_ERROR)
					error = S_ERROR;
				continue;
			}

			if ((orsp->rel_rtype == M_R_RELATIVE) ||
			    (orsp->rel_rtype == M_R_REGISTER)) {
				sorted_list[index].rl_key1 = 0;
				sorted_list[index].rl_key2 =
				    /* LINTED */
				    (Sym_desc *)(uintptr_t)orsp->rel_rtype;
			} else {
				sorted_list[index].rl_key1 =
				    orsp->rel_sym->sd_file->ifl_neededndx;
				sorted_list[index].rl_key2 =
				    orsp->rel_sym;
			}

			if (orsp->rel_flags & FLG_REL_GOT)
				sorted_list[index].rl_key3 =
					ld_calc_got_offset(orsp, ofl);
			else {
				if (orsp->rel_rtype == M_R_REGISTER)
					sorted_list[index].rl_key3 = 0;
				else {
					sorted_list[index].rl_key3 =
						orsp->rel_roffset +
						(Xword)_elf_getxoff(orsp->
						rel_isdesc->
						is_indata) +
						orsp->rel_isdesc->is_osdesc->
						os_shdr->sh_addr;
				}
			}

			sorted_list[index++].rl_rsp = orsp;
		}
	}

	qsort(sorted_list, (size_t)ofl->ofl_reloccnt, sizeof (Reloc_list),
		(int (*)(const void *, const void *))reloc_compare);

	/*
	 * All output relocations have now been sorted, go through
	 * and process each relocation.
	 */
	for (index = 0; index < ofl->ofl_reloccnt; index++) {
		if (ld_perform_outreloc(sorted_list[index].rl_rsp, ofl) ==
		    S_ERROR)
			error = S_ERROR;
	}

	return (error);
}

/*
 * Process relocations.  Finds every input relocation section for each output
 * section and invokes reloc_sec() to relocate that section.
 */
uintptr_t
ld_reloc_process(Ofl_desc *ofl)
{
	Listnode	*lnp1;
	Sg_desc		*sgp;
	Os_desc		*osp;
	Word		ndx = 0, flags = ofl->ofl_flags;
	Shdr		*shdr;

	/*
	 * Determine the index of the symbol table that will be referenced by
	 * the relocation entries.
	 */
	if ((flags & (FLG_OF_DYNAMIC|FLG_OF_RELOBJ)) == FLG_OF_DYNAMIC)
		/* LINTED */
		ndx = (Word)elf_ndxscn(ofl->ofl_osdynsym->os_scn);
	else if (!(flags & FLG_OF_STRIP) || (flags & FLG_OF_RELOBJ))
		/* LINTED */
		ndx = (Word)elf_ndxscn(ofl->ofl_ossymtab->os_scn);

	/*
	 * Re-initialize counters. These are used to provide relocation
	 * offsets within the output buffers.
	 */
	ofl->ofl_relocpltsz = 0;
	ofl->ofl_relocgotsz = 0;
	ofl->ofl_relocbsssz = 0;

	/*
	 * Now that the output file is created and symbol update has occurred,
	 * process the relocations collected in process_reloc().
	 */
	if (do_sorted_outrelocs(ofl) == S_ERROR)
		return (S_ERROR);

	if (ld_do_activerelocs(ofl) == S_ERROR)
		return (S_ERROR);

	if ((ofl->ofl_flags1 & FLG_OF1_RELCNT) == 0) {
		/*
		 * Process the relocation sections:
		 *
		 *  o	for each relocation section generated for the output
		 *	image update its shdr information to reflect the
		 *	symbol table it needs (sh_link) and the section to
		 *	which the relocation must be applied (sh_info).
		 */
		for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp)) {
			Os_desc **ospp;
			Aliste	off;

			for (ALIST_TRAVERSE(sgp->sg_osdescs, off, ospp)) {
				osp = *ospp;

				if (osp->os_relosdesc == 0)
					continue;

				shdr = osp->os_relosdesc->os_shdr;
				shdr->sh_link = ndx;
				/* LINTED */
				shdr->sh_info = (Word)elf_ndxscn(osp->os_scn);
			}
		}

		/*
		 * Since the .rel[a] section is not tied to any specific
		 * section, we'd not have found it above.
		 */
		if ((osp = ofl->ofl_osrel) != NULL) {
			shdr = osp->os_shdr;
			shdr->sh_link = ndx;
			shdr->sh_info = 0;
		}
	} else {
		/*
		 * We only have two relocation sections here, (PLT's,
		 * coalesced) so just hit them directly instead of stepping
		 * over the output sections.
		 */
		if ((osp = ofl->ofl_osrelhead) != NULL) {
			shdr = osp->os_shdr;
			shdr->sh_link = ndx;
			shdr->sh_info = 0;
		}
		if (((osp = ofl->ofl_osplt) != NULL) && osp->os_relosdesc) {
			shdr = osp->os_relosdesc->os_shdr;
			shdr->sh_link = ndx;
			/* LINTED */
			shdr->sh_info = (Word)elf_ndxscn(osp->os_scn);
		}
	}

	/*
	 * If the -z text option was given, and we have output relocations
	 * against a non-writable, allocatable section, issue a diagnostic and
	 * return (the actual entries that caused this error would have been
	 * output during the relocating section phase).
	 */
	if ((flags & (FLG_OF_PURETXT | FLG_OF_TEXTREL)) ==
	    (FLG_OF_PURETXT | FLG_OF_TEXTREL)) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_REL_REMAIN_3));
		return (S_ERROR);
	}

	/*
	 * Finally, initialize the first got entry with the address of the
	 * .dynamic section (_DYNAMIC).
	 */
	if (flags & FLG_OF_DYNAMIC) {
		if (ld_fillin_gotplt(ofl) == S_ERROR)
			return (S_ERROR);
	}

	/*
	 * Now that any GOT information has been written, display the debugging
	 * information if required.
	 */
	if ((osp = ofl->ofl_osgot) != NULL)
		DBG_CALL(Dbg_got_display(ofl, osp->os_shdr->sh_addr, 1));

	return (1);
}

/*
 * If the -z text option was given, and we have output relocations against a
 * non-writable, allocatable section, issue a diagnostic. Print offending
 * symbols in tabular form similar to the way undefined symbols are presented.
 * Called from reloc_count().  The actual fatal error condition is triggered on
 * in reloc_process() above.
 *
 * Note.  For historic reasons -ztext is not a default option (however all OS
 * shared object builds use this option).  It can be argued that this option
 * should also be default when generating an a.out (see 1163979).  However, if
 * an a.out contains text relocations it is either because the user is creating
 * something pretty weird (they've used the -b or -znodefs options), or because
 * the library against which they're building wasn't constructed correctly (ie.
 * a function has a NOTYPE type, in which case the a.out won't generate an
 * associated plt).  In the latter case the builder of the a.out can't do
 * anything to fix the error - thus we've chosen not to give the user an error,
 * or warning, for this case.
 */
static void
reloc_remain_title(Ofl_desc *ofl, int warning)
{
	const char	*str1;

	if (warning)
		str1 = MSG_INTL(MSG_REL_RMN_ITM_13);
	else
		str1 = MSG_INTL(MSG_REL_RMN_ITM_11);

	eprintf(ofl->ofl_lml, ERR_NONE, MSG_INTL(MSG_REL_REMAIN_FMT_1),
		str1,
		MSG_INTL(MSG_REL_RMN_ITM_31),
		MSG_INTL(MSG_REL_RMN_ITM_12),
		MSG_INTL(MSG_REL_RMN_ITM_2),
		MSG_INTL(MSG_REL_RMN_ITM_32));

}

void
ld_reloc_remain_entry(Rel_desc *orsp, Os_desc *osp, Ofl_desc *ofl)
{
	static Boolean	reloc_title = TRUE;

	/*
	 * -ztextoff
	 */
	if (ofl->ofl_flags1 & FLG_OF1_TEXTOFF)
		return;

	/*
	 * Only give relocation errors against loadable read-only segments.
	 */
	if ((orsp->rel_rtype == M_R_REGISTER) || (!osp) ||
	    (osp->os_sgdesc->sg_phdr.p_type != PT_LOAD) ||
	    (osp->os_sgdesc->sg_phdr.p_flags & PF_W))
		return;

	/*
	 * If we are in -ztextwarn mode, it's a silent error if a relocation is
	 * due to a 'WEAK REFERENCE'.  This is because if the symbol is not
	 * provided at run-time we will not perform a text-relocation.
	 */
	if (((ofl->ofl_flags & FLG_OF_PURETXT) == 0) &&
	    (ELF_ST_BIND(orsp->rel_sym->sd_sym->st_info) == STB_WEAK) &&
	    (orsp->rel_sym->sd_sym->st_shndx == SHN_UNDEF))
		return;

	if (reloc_title) {
		/*
		 * If building with '-ztext' then emit a fatal error.  If
		 * building a executable then only emit a 'warning'.
		 */
		if (ofl->ofl_flags & FLG_OF_PURETXT)
			reloc_remain_title(ofl, 0);
		else
			reloc_remain_title(ofl, 1);
		reloc_title = FALSE;
	}

	eprintf(ofl->ofl_lml, ERR_NONE, MSG_INTL(MSG_REL_REMAIN_2),
	    demangle(orsp->rel_sname), EC_OFF(orsp->rel_roffset),
	    orsp->rel_isdesc->is_file->ifl_name);
}

/*
 * Generic encapsulation for generating a TLS got index.
 */
uintptr_t
ld_assign_got_TLS(Boolean local, Rel_desc *rsp, Ofl_desc *ofl, Sym_desc *sdp,
    Gotndx *gnp, Gotref gref, Word rflag, Word ortype, Word rtype1, Word rtype2)
{
	Word	rflags;

	if (ld_assign_got_ndx(&(sdp->sd_GOTndxs), gnp, gref, ofl,
	    rsp, sdp) == S_ERROR)
		return (S_ERROR);

	rflags = FLG_REL_GOT | rflag;
	if (local)
		rflags |= FLG_REL_SCNNDX;
	rsp->rel_rtype = rtype1;

	if (ld_add_outrel(rflags, rsp, ofl) == S_ERROR)
		return (S_ERROR);

	if (local && (gref == GOT_REF_TLSIE)) {
		/*
		 * If this is a local LE TLS symbol, then the symbol won't be
		 * available at runtime.  The value of the local symbol will
		 * be placed in the associated got entry, and the got
		 * relocation is reassigned to a section symbol.
		 */
		if (ld_add_actrel(rflags, rsp, ofl) == S_ERROR)
			return (S_ERROR);
	}

	if (rtype2) {
		rflags = FLG_REL_GOT | rflag;
		rsp->rel_rtype = rtype2;

		if (local) {
			if (ld_add_actrel(rflags, rsp, ofl) == S_ERROR)
				return (S_ERROR);
		} else {
			if (ld_add_outrel(rflags, rsp, ofl) == S_ERROR)
				return (S_ERROR);
		}
	}

	rsp->rel_rtype = ortype;

	return (1);
}

/*
 * Move Section related function
 */
static uintptr_t
newroffset_for_move(Sym_desc *symd,
	Move *mventry, Xword offset1, Xword *offset2)
{
	Psym_info	*psym = symd->sd_psyminfo;
	Mv_itm		*itm;
	Listnode	*lnp1;
	int 		found = 0;

	/*
	 * Search for matching move entry
	 */
	found = 0;
	for (LIST_TRAVERSE(&psym->psym_mvs, lnp1, itm)) {
		if (itm->mv_ientry == mventry) {
			found = 1;
			break;
		}
	}
	if (found == 0) {
		/*
		 * This should never happen.
		 */
		return (S_ERROR);
	}

	/*
	 * Update r_offset
	 */
	*offset2 = (Xword)((itm->mv_oidx - 1)*sizeof (Move) +
		offset1 % sizeof (Move));
	return (1);
}

void
ld_adj_movereloc(Ofl_desc *ofl, Rel_desc *arsp)
{
	Move		*move = arsp->rel_move->mvd_move;
	Sym_desc	*psdp = arsp->rel_move->mvd_sym;
	Xword		newoffset;

	if (arsp->rel_flags & FLG_REL_MOVETAB) {
		/*
		 * We are relocating the move table itself.
		 */
		(void) newroffset_for_move(psdp, move, arsp->rel_roffset,
		    &newoffset);
		DBG_CALL(Dbg_move_adjmovereloc(ofl->ofl_lml, arsp->rel_roffset,
		    newoffset, psdp->sd_name));
		arsp->rel_roffset = newoffset;
	} else {
		/*
		 * We are expanding the partial symbol.  So we are generating
		 * the relocation entry relocating the expanded partial symbol.
		 */
		arsp->rel_roffset += psdp->sd_sym->st_value -
		    ofl->ofl_issunwdata1->is_osdesc->os_shdr->sh_addr;
		DBG_CALL(Dbg_move_adjexpandreloc(ofl->ofl_lml,
		    arsp->rel_roffset, psdp->sd_name));
	}
}

/*
 * Partially Initialized Symbol Handling routines
 * For sparc architecture, the second argument is reld->rel_raddend.
 * For i386  acrchitecure, the second argument is the value stored
 *	at the relocation target address.
 */
Sym_desc *
ld_am_I_partial(Rel_desc *reld, Xword val)
{
	Ifl_desc *	ifile = reld->rel_sym->sd_isc->is_file;
	int 		nlocs = ifile->ifl_locscnt, i;

	for (i = 1; i < nlocs; i++) {
		Sym *		osym;
		Sym_desc *	symd = ifile->ifl_oldndx[i];

		if ((osym = symd->sd_osym) == 0)
			continue;
		if ((symd->sd_flags & FLG_SY_PAREXPN) == 0)
			continue;
		if ((osym->st_value <= val) &&
		    (osym->st_value + osym->st_size  > val))
			return (symd);
	}
	return ((Sym_desc *) 0);
}

/*
 * Because of the combinations of 32-bit lib providing 64-bit support, and
 * visa-versa, the use of krtld's dorelocs can result in differing message
 * requirements that make msg.c/msg.h creation and chkmsg "interesting".
 * Thus the actual message files contain a couple of entries to satisfy
 * each architectures build.  Here we add dummy calls to quieten chkmsg.
 *
 * chkmsg: MSG_INTL(MSG_REL_NOFIT)
 * chkmsg: MSG_INTL(MSG_REL_NONALIGN)
 */