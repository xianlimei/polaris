/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 *	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
#pragma ident	"@(#)machrel.c	1.79	06/01/24 SMI"

#include	<string.h>
#include	<stdio.h>
#include	<sys/elf_386.h>
#include	"debug.h"
#include	"reloc.h"
#include	"msg.h"
#include	"_libld.h"

Word
init_rel(Rel_desc *reld, void *reloc)
{
	Rel *	rel = (Rel *)reloc;

	/* LINTED */
	reld->rel_rtype = (Word)ELF_R_TYPE(rel->r_info);
	reld->rel_roffset = rel->r_offset;
	reld->rel_raddend = 0;
	reld->rel_typedata = 0;

	return ((Word)ELF_R_SYM(rel->r_info));
}

void
mach_eflags(Ehdr *ehdr, Ofl_desc *ofl)
{
	ofl->ofl_e_flags |= ehdr->e_flags;
}

void
mach_make_dynamic(Ofl_desc *ofl, size_t *cnt)
{
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ)) {
		/*
		 * Create this entry if we are going to create a PLT table.
		 */
		if (ofl->ofl_pltcnt)
			(*cnt)++;		/* DT_PLTGOT */
	}
}

void
mach_update_odynamic(Ofl_desc * ofl, Dyn ** dyn)
{
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ)) {
		if (ofl->ofl_pltcnt) {
			(*dyn)->d_tag = DT_PLTGOT;
			(*dyn)->d_un.d_ptr = fillin_gotplt2(ofl);
			(*dyn)++;
		}
	}
}

Xword
calc_plt_addr(Sym_desc *sdp, Ofl_desc *ofl)
{
	Xword	value;

	value = (Xword)(ofl->ofl_osplt->os_shdr->sh_addr) +
	    M_PLT_RESERVSZ + ((sdp->sd_aux->sa_PLTndx - 1) * M_PLT_ENTSIZE);
	return (value);
}

/*
 *  Build a single plt entry - code is:
 *	if (building a.out)
 *		JMP	*got_off
 *	else
 *		JMP	*got_off@GOT(%ebx)
 *	PUSHL	&rel_off
 *	JMP	-n(%pc)		# -n is pcrel offset to first plt entry
 *
 *	The got_off@GOT entry gets filled with the address of the PUSHL,
 *	so the first pass through the plt jumps back here, jumping
 *	in turn to the first plt entry, which jumps to the dynamic
 *	linker.	 The dynamic linker then patches the GOT, rerouting
 *	future plt calls to the proper destination.
 */
static void
plt_entry(Ofl_desc * ofl, Word rel_off, Sym_desc * sdp)
{
	uchar_t		*pltent, *gotent;
	Sword		plt_off;
	Word		got_off;

	got_off = sdp->sd_aux->sa_PLTGOTndx * M_GOT_ENTSIZE;
	plt_off = M_PLT_RESERVSZ + ((sdp->sd_aux->sa_PLTndx - 1) *
	    M_PLT_ENTSIZE);
	pltent = (uchar_t *)(ofl->ofl_osplt->os_outdata->d_buf) + plt_off;
	gotent = (uchar_t *)(ofl->ofl_osgot->os_outdata->d_buf) + got_off;

	/*
	 * Fill in the got entry with the address of the next instruction.
	 */
	/* LINTED */
	*(Word *)gotent = ofl->ofl_osplt->os_shdr->sh_addr + plt_off +
	    M_PLT_INSSIZE;

	if (!(ofl->ofl_flags & FLG_OF_SHAROBJ)) {
		pltent[0] = M_SPECIAL_INST;
		pltent[1] = M_JMP_DISP_IND;
		pltent += 2;
		/* LINTED */
		*(Word *)pltent = (Word)(ofl->ofl_osgot->os_shdr->sh_addr +
			got_off);
	} else {
		pltent[0] = M_SPECIAL_INST;
		pltent[1] = M_JMP_REG_DISP_IND;
		pltent += 2;
		/* LINTED */
		*(Word *)pltent = (Word)got_off;
	}
	pltent += 4;

	pltent[0] = M_INST_PUSHL;
	pltent++;
	/* LINTED */
	*(Word *)pltent = (Word)rel_off;
	pltent += 4;

	plt_off = -(plt_off + 16);	/* JMP, PUSHL, JMP take 16 bytes */
	pltent[0] = M_INST_JMP;
	pltent++;
	/* LINTED */
	*(Word *)pltent = (Word)plt_off;
}

uintptr_t
perform_outreloc(Rel_desc * orsp, Ofl_desc * ofl)
{
	Os_desc *	relosp, * osp = 0;
	Word		ndx, roffset, value;
	Rel		rea;
	char		*relbits;
	Sym_desc *	sdp, * psym = (Sym_desc *)0;
	int		sectmoved = 0;

	sdp = orsp->rel_sym;

	/*
	 * If the section this relocation is against has been discarded
	 * (-zignore), then also discard (skip) the relocation itself.
	 */
	if (orsp->rel_isdesc && ((orsp->rel_flags &
	    (FLG_REL_GOT | FLG_REL_BSS | FLG_REL_PLT | FLG_REL_NOINFO)) == 0) &&
	    (orsp->rel_isdesc->is_flags & FLG_IS_DISCARD)) {
		DBG_CALL(Dbg_reloc_discard(M_MACH, orsp));
		return (1);
	}

	/*
	 * If this is a relocation against a move table, or expanded move
	 * table, adjust the relocation entries.
	 */
	if (orsp->rel_move)
		adj_movereloc(ofl, orsp);

	/*
	 * If this is a relocation against a section using a partial initialized
	 * symbol, adjust the embedded symbol info.
	 *
	 * The second argument of the am_I_partial() is the value stored at the
	 * target address relocation is going to be applied.
	 */
	if (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION) {
		if (ofl->ofl_parsym.head &&
		    (sdp->sd_isc->is_flags & FLG_IS_RELUPD) &&
		    /* LINTED */
		    (psym = am_I_partial(orsp, *(Xword *)
		    ((uchar_t *)(orsp->rel_isdesc->is_indata->d_buf) +
		    orsp->rel_roffset)))) {
			DBG_CALL(Dbg_move_outsctadj(psym));
			sectmoved = 1;
		    }
	}

	value = sdp->sd_sym->st_value;

	if (orsp->rel_flags & FLG_REL_GOT) {
		osp = ofl->ofl_osgot;
		roffset = (Word)calc_got_offset(orsp, ofl);
	} else if (orsp->rel_flags & FLG_REL_PLT) {
		/*
		 * Note that relocations for PLT's actually
		 * cause a relocation againt the GOT.
		 */
		osp = ofl->ofl_osplt;
		roffset = (Word) (ofl->ofl_osgot->os_shdr->sh_addr) +
		    sdp->sd_aux->sa_PLTGOTndx * M_GOT_ENTSIZE;

		plt_entry(ofl, osp->os_relosdesc->os_szoutrels, sdp);

	} else if (orsp->rel_flags & FLG_REL_BSS) {
		/*
		 * This must be a R_386_COPY.  For these set the roffset to
		 * point to the new symbols location.
		 */
		osp = ofl->ofl_isbss->is_osdesc;
		roffset = (Word)value;
	} else {
		osp = orsp->rel_osdesc;

		/*
		 * Calculate virtual offset of reference point; equals offset
		 * into section + vaddr of section for loadable sections, or
		 * offset plus section displacement for nonloadable sections.
		 */
		roffset = orsp->rel_roffset +
		    (Off)_elf_getxoff(orsp->rel_isdesc->is_indata);
		if (!(ofl->ofl_flags & FLG_OF_RELOBJ))
			roffset += orsp->rel_isdesc->is_osdesc->
			    os_shdr->sh_addr;
	}

	if ((osp == 0) || ((relosp = osp->os_relosdesc) == 0))
		relosp = ofl->ofl_osrel;

	/*
	 * Assign the symbols index for the output relocation.  If the
	 * relocation refers to a SECTION symbol then it's index is based upon
	 * the output sections symbols index.  Otherwise the index can be
	 * derived from the symbols index itself.
	 */
	if (orsp->rel_rtype == R_386_RELATIVE)
		ndx = STN_UNDEF;
	else if ((orsp->rel_flags & FLG_REL_SCNNDX) ||
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION)) {
		if (sectmoved == 0) {
			/*
			 * Check for a null input section. This can
			 * occur if this relocation references a symbol
			 * generated by sym_add_sym().
			 */
			if ((sdp->sd_isc != 0) &&
			    (sdp->sd_isc->is_osdesc != 0))
				ndx = sdp->sd_isc->is_osdesc->os_scnsymndx;
			else
				ndx = sdp->sd_shndx;
		} else
			ndx = ofl->ofl_sunwdata1ndx;
	} else
		ndx = sdp->sd_symndx;

	relbits = (char *)relosp->os_outdata->d_buf;

	rea.r_info = ELF_R_INFO(ndx, orsp->rel_rtype);
	rea.r_offset = roffset;
	DBG_CALL(Dbg_reloc_out(M_MACH, SHT_REL, &rea, orsp->rel_sname,
	    relosp->os_name));

	/*
	 * Assert we haven't walked off the end of our relocation table.
	 */
	assert(relosp->os_szoutrels <= relosp->os_shdr->sh_size);

	(void) memcpy((relbits + relosp->os_szoutrels),
	    (char *)&rea, sizeof (Rel));
	relosp->os_szoutrels += sizeof (Rel);

	/*
	 * Determine if this relocation is against a non-writable, allocatable
	 * section.  If so we may need to provide a text relocation diagnostic.
	 * Note that relocations against the .plt (R_386_JMP_SLOT) actually
	 * result in modifications to the .got.
	 */
	if (orsp->rel_rtype == R_386_JMP_SLOT)
		osp = ofl->ofl_osgot;

	reloc_remain_entry(orsp, osp, ofl);
	return (1);
}

/*
 * i386 Instructions for TLS processing
 */
static uchar_t tlsinstr_gd_ie[] = {
	/*
	 * 0x00	movl %gs:0x0, %eax
	 */
	0x65, 0xa1, 0x00, 0x00, 0x00, 0x00,
	/*
	 * 0x06	addl x(%eax), %eax
	 * 0x0c ...
	 */
	0x03, 0x80, 0x00, 0x00, 0x00, 0x00
};

static uchar_t tlsinstr_gd_le[] = {
	/*
	 * 0x00 movl %gs:0x0, %eax
	 */
	0x65, 0xa1, 0x00, 0x00, 0x00, 0x00,
	/*
	 * 0x06 addl $0x0, %eax
	 */
	0x05, 0x00, 0x00, 0x00, 0x00,
	/*
	 * 0x0b nop
	 * 0x0c
	 */
	0x90
};

static uchar_t tlsinstr_gd_ie_movgs[] = {
	/*
	 *	movl %gs:0x0,%eax
	 */
	0x65, 0xa1, 0x00, 0x00, 0x00, 00
};

#define	TLS_GD_IE_MOV	0x8b	/* movl opcode */
#define	TLS_GD_IE_POP	0x58	/* popl + reg */

#define	TLS_GD_LE_MOVL	0xb8	/* movl + reg */

#define	TLS_NOP		0x90	/* NOP instruction */

#define	MODRM_MSK_MOD	0xc0
#define	MODRM_MSK_RO	0x38
#define	MODRM_MSK_RM	0x07

#define	SIB_MSK_SS	0xc0
#define	SIB_MSK_IND	0x38
#define	SIB_MSK_BS	0x07


Fixupret
tls_fixups(Rel_desc *arsp)
{
	Sym_desc	*sdp = arsp->rel_sym;
	Word		rtype = arsp->rel_rtype;
	uchar_t		*offset, r1, r2;

	offset = (uchar_t *)((uintptr_t)arsp->rel_roffset +
	    (uintptr_t)_elf_getxoff(arsp->rel_isdesc->is_indata) +
	    (uintptr_t)arsp->rel_osdesc->os_outdata->d_buf);

	if (sdp->sd_ref == REF_DYN_NEED) {
		/*
		 * IE reference model
		 */
		switch (rtype) {
		case R_386_TLS_GD:
			/*
			 * Transition:
			 *	0x0 leal x@tlsgd(,r1,1), %eax
			 *	0x7 call ___tls_get_addr
			 *	0xc
			 * To:
			 *	0x0 movl %gs:0, %eax
			 *	0x6 addl x@gotntpoff(r1), %eax
			 */
			DBG_CALL(Dbg_reloc_transition(M_MACH,
				rtype,
				R_386_TLS_GOTIE,
				arsp->rel_roffset,
				sdp->sd_name));
			arsp->rel_rtype = R_386_TLS_GOTIE;
			arsp->rel_roffset += 5;
			/*
			 * Addjust 'offset' to beginning of instruction
			 * sequence.
			 */
			offset -= 3;
			r1 = (offset[2] & SIB_MSK_IND) >> 3;
			(void) memcpy(offset, tlsinstr_gd_ie,
				sizeof (tlsinstr_gd_ie));
			/*
			 * set register %r1 into the addl
			 * instruction.
			 */
			offset[0x7] |= r1;
			return (FIX_RELOC);
		case R_386_TLS_GD_PLT:
			/*
			 * Fixup done via the TLS_GD relocation
			 */
			DBG_CALL(Dbg_reloc_transition(M_MACH,
				rtype,
				R_386_NONE,
				arsp->rel_roffset,
				sdp->sd_name));
			return (FIX_DONE);
		}
	}

	/*
	 * LE reference model
	 */
	switch (rtype) {
	case R_386_TLS_GD:
		/*
		 * Transition:
		 *	0x0 leal x@tlsgd(,r1,1), %eax
		 *	0x7 call ___tls_get_addr
		 *	0xc
		 * To:
		 *	0x0 movl %gs:0, %eax
		 *	0x6 addl $x@ntpoff, %eax
		 *	0xb nop
		 *	0xc
		 */
		DBG_CALL(Dbg_reloc_transition(M_MACH,
			rtype,
			R_386_TLS_LE,
			arsp->rel_roffset,
			sdp->sd_name));

		arsp->rel_rtype = R_386_TLS_LE;
		arsp->rel_roffset += 4;
		/*
		 * Addjust 'offset' to beginning of instruction
		 * sequence.
		 */
		offset -= 3;
		(void) memcpy(offset, tlsinstr_gd_le,
			sizeof (tlsinstr_gd_le));
		return (FIX_RELOC);
	case R_386_TLS_GD_PLT:
	case R_386_PLT32:
		/*
		 * Fixup done via the TLS_GD relocation
		 */
		DBG_CALL(Dbg_reloc_transition(M_MACH,
			rtype,
			R_386_NONE,
			arsp->rel_roffset,
			sdp->sd_name));
		return (FIX_DONE);
	case R_386_TLS_LDM_PLT:
		DBG_CALL(Dbg_reloc_transition(M_MACH,
			rtype,
			R_386_NONE,
			arsp->rel_roffset,
			sdp->sd_name));
		/*
		 * Transition:
		 *	call __tls_get_addr()
		 * to:
		 *	nop
		 *	nop
		 *	nop
		 *	nop
		 *	nop
		 */
		*(offset - 1) = TLS_NOP;
		*(offset) = TLS_NOP;
		*(offset + 1) = TLS_NOP;
		*(offset + 2) = TLS_NOP;
		*(offset + 3) = TLS_NOP;
		return (FIX_DONE);
	case R_386_TLS_LDM:
		DBG_CALL(Dbg_reloc_transition(M_MACH,
			rtype,
			R_386_NONE,
			arsp->rel_roffset,
			sdp->sd_name));
		/*
		 * Transition:
		 *
		 *  0x00 leal x1@tlsldm(%ebx), %eax
		 *  0x06 call ___tls_get_addr
		 *
		 * to:
		 *
		 *  0x00 movl %gs:0, %eax
		 */
		(void) memcpy(offset - 2, tlsinstr_gd_ie_movgs,
			sizeof (tlsinstr_gd_ie_movgs));
		return (FIX_DONE);
	case R_386_TLS_LDO_32:
		/*
		 *
		 *  Instructions:
		 *
		 *  0x10 leal x1@dtpoff(%eax), %edx	R_386_TLS_LDO_32
		 *		to
		 *  0x10 leal x1@ntpoff(%eax), %edx	R_386_TLS_LE
		 *
		 */
		offset -= 2;

		DBG_CALL(Dbg_reloc_transition(M_MACH,
			rtype,
			R_386_TLS_LE,
			arsp->rel_roffset,
			sdp->sd_name));
		arsp->rel_rtype = R_386_TLS_LE;
		return (FIX_RELOC);
	case R_386_TLS_GOTIE:
		/*
		 * These transitions are a little different than the
		 * others, in that we could have multiple instructions
		 * pointed to by a single relocation.  Depending upon the
		 * instruction, we perform a different code transition.
		 *
		 * Here's the known transitions:
		 *
		 *  1) movl foo@gotntpoff(%reg1), %reg2
		 *	0x8b, 0x80 | (reg2 << 3) | reg1, foo@gotntpoff
		 *
		 *  2) addl foo@gotntpoff(%reg1), %reg2
		 *	0x03, 0x80 | (reg2 << 3) | reg1, foo@gotntpoff
		 *
		 *  Transitions IE -> LE
		 *
		 *  1) movl $foo@ntpoff, %reg2
		 *	0xc7, 0xc0 | reg2, foo@ntpoff
		 *
		 *  2) addl $foo@ntpoff, %reg2
		 *	0x81, 0xc0 | reg2, foo@ntpoff
		 *
		 *
		 * Note: reg1 != 4 (%esp)
		 */
		DBG_CALL(Dbg_reloc_transition(M_MACH,
			rtype,
			R_386_TLS_LE,
			arsp->rel_roffset,
			sdp->sd_name));
		arsp->rel_rtype = R_386_TLS_LE;
		offset -= 2;
		r2 = (offset[1] & MODRM_MSK_RO) >> 3;
		if (offset[0] == 0x8b) {
			/* case 1 above */
			offset[0] = 0xc7;	/* movl */
			offset[1] = 0xc0 | r2;
			return (FIX_RELOC);
		}

		if (offset[0] == 0x03) {
			/* case 2 above */
			assert(offset[0] == 0x03);
			offset[0] = 0x81;	/* addl */
			offset[1] = 0xc0 | r2;
			return (FIX_RELOC);
		}

		/*
		 * Unexpected instruction sequence - fatal error.
		 */
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_BADTLSINS),
		    conv_reloc_386_type_str(arsp->rel_rtype),
		    arsp->rel_isdesc->is_file->ifl_name,
		    demangle(arsp->rel_sname), arsp->rel_isdesc->is_name,
		    EC_OFF(arsp->rel_roffset));
		return (FIX_ERROR);
	case R_386_TLS_IE:
		/*
		 * These transitions are a little different than the
		 * others, in that we could have multiple instructions
		 * pointed to by a single relocation.  Depending upon the
		 * instruction, we perform a different code transition.
		 *
		 * Here's the known transitions:
		 *  1) movl foo@indntpoff, %eax
		 *	0xa1, foo@indntpoff
		 *
		 *  2) movl foo@indntpoff, %eax
		 *	0x8b, 0x05 | (reg << 3), foo@gotntpoff
		 *
		 *  3) addl foo@indntpoff, %eax
		 *	0x03, 0x05 | (reg << 3), foo@gotntpoff
		 *
		 *  Transitions IE -> LE
		 *
		 *  1) movl $foo@ntpoff, %eax
		 *	0xb8, foo@ntpoff
		 *
		 *  2) movl $foo@ntpoff, %reg
		 *	0xc7, 0xc0 | reg, foo@ntpoff
		 *
		 *  3) addl $foo@ntpoff, %reg
		 *	0x81, 0xc0 | reg, foo@ntpoff
		 */
		arsp->rel_rtype = R_386_TLS_LE;
		offset--;
		if (offset[0] == 0xa1) {
			/* case 1 above */
			offset[0] = 0xb8;	/*  movl */
			return (FIX_RELOC);
		}

		offset--;
		if (offset[0] == 0x8b) {
			/* case 2 above */
			r2 = (offset[1] & MODRM_MSK_RO) >> 3;
			offset[0] = 0xc7;	/* movl */
			offset[1] = 0xc0 | r2;
			return (FIX_RELOC);
		}
		if (offset[0] == 0x03) {
			/* case 3 above */
			r2 = (offset[1] & MODRM_MSK_RO) >> 3;
			offset[0] = 0x81;	/* addl */
			offset[1] = 0xc0 | r2;
			return (FIX_RELOC);
		}
		/*
		 * Unexpected instruction sequence - fatal error.
		 */
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_BADTLSINS),
		    conv_reloc_386_type_str(arsp->rel_rtype),
		    arsp->rel_isdesc->is_file->ifl_name,
		    demangle(arsp->rel_sname), arsp->rel_isdesc->is_name,
		    EC_OFF(arsp->rel_roffset));
		return (FIX_ERROR);
	}
	return (FIX_RELOC);
}

uintptr_t
do_activerelocs(Ofl_desc *ofl)
{
	Rel_desc	*arsp;
	Rel_cache	*rcp;
	Listnode	*lnp;
	uintptr_t	return_code = 1;
	Word		flags = ofl->ofl_flags;
	Word		dtflags1 = ofl->ofl_dtflags_1;

	DBG_CALL(Dbg_reloc_doactiverel());
	/*
	 * Process active relocations.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_actrels, lnp, rcp)) {
		/* LINTED */
		for (arsp = (Rel_desc *)(rcp + 1);
		    arsp < rcp->rc_free; arsp++) {
			uchar_t		*addr;
			Xword 		value;
			Sym_desc	*sdp;
			const char	*ifl_name;
			Xword		refaddr;
			int		moved = 0;
			Gotref		gref;

			/*
			 * If the section this relocation is against has been
			 * discarded (-zignore), then discard (skip) the
			 * relocation itself.
			 */
			if ((arsp->rel_isdesc->is_flags & FLG_IS_DISCARD) &&
			    ((arsp->rel_flags &
			    (FLG_REL_GOT | FLG_REL_BSS |
			    FLG_REL_PLT | FLG_REL_NOINFO)) == 0)) {
				DBG_CALL(Dbg_reloc_discard(M_MACH, arsp));
				continue;
			}

			/*
			 * We deteremine what the 'got reference'
			 * model (if required) is at this point.  This
			 * needs to be done before tls_fixup() since
			 * it may 'transition' our instructions.
			 *
			 * The got table entries have already been assigned,
			 * and we bind to those initial entries.
			 */
			if (arsp->rel_flags & FLG_REL_DTLS)
				gref = GOT_REF_TLSGD;
			else if (arsp->rel_flags & FLG_REL_MTLS)
				gref = GOT_REF_TLSLD;
			else if (arsp->rel_flags & FLG_REL_STLS)
				gref = GOT_REF_TLSIE;
			else
				gref = GOT_REF_GENERIC;

			/*
			 * Perform any required TLS fixups.
			 */
			if (arsp->rel_flags & FLG_REL_TLSFIX) {
				Fixupret	ret;

				if ((ret = tls_fixups(arsp)) == FIX_ERROR)
					return (S_ERROR);
				if (ret == FIX_DONE)
					continue;
			}

			/*
			 * If this is a relocation against a move table, or
			 * expanded move table, adjust the relocation entries.
			 */
			if (arsp->rel_move)
				adj_movereloc(ofl, arsp);

			sdp = arsp->rel_sym;
			refaddr = arsp->rel_roffset +
			    (Off)_elf_getxoff(arsp->rel_isdesc->is_indata);

			if (arsp->rel_flags & FLG_REL_CLVAL)
				value = 0;
			else if (ELF_ST_TYPE(sdp->sd_sym->st_info) ==
			    STT_SECTION) {
				Sym_desc	*sym;

				/*
				 * The value for a symbol pointing to a SECTION
				 * is based off of that sections position.
				 *
				 * The second argument of the am_I_partial() is
				 * the value stored at the target address
				 * relocation is going to be applied.
				 */
				if ((sdp->sd_isc->is_flags & FLG_IS_RELUPD) &&
				    /* LINTED */
				    (sym = am_I_partial(arsp, *(Xword *)
				    ((uchar_t *)
				    arsp->rel_isdesc->is_indata->d_buf +
				    arsp->rel_roffset)))) {
					/*
					 * If the symbol is moved,
					 * adjust the value
					 */
					value = sym->sd_sym->st_value;
					moved = 1;
				} else {
					value = _elf_getxoff(
					    sdp->sd_isc->is_indata);
					if (sdp->sd_isc->is_shdr->sh_flags &
					    SHF_ALLOC)
						value += sdp->sd_isc->
						    is_osdesc->os_shdr->sh_addr;
				}
				if (sdp->sd_isc->is_shdr->sh_flags & SHF_TLS)
					value -= ofl->ofl_tlsphdr->p_vaddr;
			} else {
				/*
				 * else the value is the symbols value
				 */
				value = sdp->sd_sym->st_value;
			}

			/*
			 * Relocation against the GLOBAL_OFFSET_TABLE.
			 */
			if (arsp->rel_flags & FLG_REL_GOT)
				arsp->rel_osdesc = ofl->ofl_osgot;

			/*
			 * If loadable and not producing a relocatable object
			 * add the sections virtual address to the reference
			 * address.
			 */
			if ((arsp->rel_flags & FLG_REL_LOAD) &&
			    ((flags & FLG_OF_RELOBJ) == 0))
				refaddr += arsp->rel_isdesc->is_osdesc->
				    os_shdr->sh_addr;

			/*
			 * If this entry has a PLT assigned to it, it's
			 * value is actually the address of the PLT (and
			 * not the address of the function).
			 */
			if (IS_PLT(arsp->rel_rtype)) {
				if (sdp->sd_aux && sdp->sd_aux->sa_PLTndx)
					value = calc_plt_addr(sdp, ofl);
			}

			/*
			 * Determine whether the value needs further adjustment.
			 * Filter through the attributes of the relocation to
			 * determine what adjustment is required.  Note, many
			 * of the following cases are only applicable when a
			 * .got is present.  As a .got is not generated when a
			 * relocatable object is being built, any adjustments
			 * that require a .got need to be skipped.
			 */
			if ((arsp->rel_flags & FLG_REL_GOT) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				Xword		R1addr;
				uintptr_t	R2addr;
				Word		gotndx;
				Gotndx		*gnp;

				/*
				 * Perform relocation against GOT table.  Since
				 * this doesn't fit exactly into a relocation
				 * we place the appropriate byte in the GOT
				 * directly
				 *
				 * Calculate offset into GOT at which to apply
				 * the relocation.
				 */
				gnp = find_gotndx(&(sdp->sd_GOTndxs), gref,
				    ofl, 0);
				assert(gnp);

				if (arsp->rel_rtype == R_386_TLS_DTPOFF32)
					gotndx = gnp->gn_gotndx + 1;
				else
					gotndx = gnp->gn_gotndx;

				R1addr = (Xword)(gotndx * M_GOT_ENTSIZE);

				/*
				 * Add the GOTs data's offset.
				 */
				R2addr = R1addr + (uintptr_t)
				    arsp->rel_osdesc->os_outdata->d_buf;

				DBG_CALL(Dbg_reloc_doact(M_MACH,
				    arsp->rel_rtype, R1addr, value,
				    arsp->rel_sname, arsp->rel_osdesc));

				/*
				 * And do it.
				 */
				*(Xword *)R2addr = value;
				continue;

			} else if (IS_GOT_BASED(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				value -= ofl->ofl_osgot->os_shdr->sh_addr;

			} else if (IS_GOT_PC(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				value =
				    (Xword)(ofl->ofl_osgot->os_shdr->sh_addr) -
				    refaddr;

			} else if ((IS_PC_RELATIVE(arsp->rel_rtype)) &&
			    (((flags & FLG_OF_RELOBJ) == 0) ||
			    (arsp->rel_osdesc == sdp->sd_isc->is_osdesc))) {
				value -= refaddr;

			} else if (IS_TLS_INS(arsp->rel_rtype) &&
			    IS_GOT_RELATIVE(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				Gotndx	*gnp;

				gnp = find_gotndx(&(sdp->sd_GOTndxs), gref,
				    ofl, 0);
				assert(gnp);
				value = (Xword)gnp->gn_gotndx * M_GOT_ENTSIZE;
				if (arsp->rel_rtype == R_386_TLS_IE) {
					value +=
					    ofl->ofl_osgot->os_shdr->sh_addr;
				}

			} else if (IS_GOT_RELATIVE(arsp->rel_rtype) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				Gotndx *gnp;

				gnp = find_gotndx(&(sdp->sd_GOTndxs),
				    GOT_REF_GENERIC, ofl, 0);
				assert(gnp);
				value = (Xword)gnp->gn_gotndx * M_GOT_ENTSIZE;

			} else if ((arsp->rel_flags & FLG_REL_STLS) &&
			    ((flags & FLG_OF_RELOBJ) == 0)) {
				Xword	tlsstatsize;

				/*
				 * This is the LE TLS reference model.  Static
				 * offset is hard-coded.
				 */
				tlsstatsize =
				    S_ROUND(ofl->ofl_tlsphdr->p_memsz,
				    M_TLSSTATALIGN);
				value = tlsstatsize - value;

				/*
				 * Since this code is fixed up, it assumes a
				 * negative offset that can be added to the
				 * thread pointer.
				 */
				if ((arsp->rel_rtype == R_386_TLS_LDO_32) ||
				    (arsp->rel_rtype == R_386_TLS_LE))
					value = -value;
			}

			if (arsp->rel_isdesc->is_file)
				ifl_name = arsp->rel_isdesc->is_file->ifl_name;
			else
				ifl_name = MSG_INTL(MSG_STR_NULL);

			/*
			 * Make sure we have data to relocate.  Compiler and
			 * assembler developers have been known to generate
			 * relocations against invalid sections (normally .bss),
			 * so for their benefit give them sufficient information
			 * to help analyze the problem.  End users should never
			 * see this.
			 */
			if (arsp->rel_isdesc->is_indata->d_buf == 0) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_REL_EMPTYSEC),
				    conv_reloc_386_type_str(arsp->rel_rtype),
				    ifl_name, demangle(arsp->rel_sname),
				    arsp->rel_isdesc->is_name);
				return (S_ERROR);
			}

			/*
			 * Get the address of the data item we need to modify.
			 */
			addr = (uchar_t *)((uintptr_t)arsp->rel_roffset +
			    (uintptr_t)_elf_getxoff(arsp->rel_isdesc->
			    is_indata));

			DBG_CALL(Dbg_reloc_doact(M_MACH, arsp->rel_rtype,
			    (uintptr_t)addr, value, arsp->rel_sname,
			    arsp->rel_osdesc));
			addr += (uintptr_t)arsp->rel_osdesc->os_outdata->d_buf;

			if ((((uintptr_t)addr - (uintptr_t)ofl->ofl_ehdr) >
			    ofl->ofl_size) || (arsp->rel_roffset >
			    arsp->rel_osdesc->os_shdr->sh_size)) {
				int	class;

				if (((uintptr_t)addr -
				    (uintptr_t)ofl->ofl_ehdr) > ofl->ofl_size)
					class = ERR_FATAL;
				else
					class = ERR_WARNING;

				eprintf(class, MSG_INTL(MSG_REL_INVALOFFSET),
				    conv_reloc_386_type_str(arsp->rel_rtype),
				    ifl_name, arsp->rel_isdesc->is_name,
				    demangle(arsp->rel_sname),
				    EC_ADDR((uintptr_t)addr -
				    (uintptr_t)ofl->ofl_ehdr));

				if (class == ERR_FATAL) {
					return_code = S_ERROR;
					continue;
				}
			}

			/*
			 * The relocation is additive.  Ignore the previous
			 * symbol value if this local partial symbol is
			 * expanded.
			 */
			if (moved)
				value -= *addr;

			/*
			 * If '-z noreloc' is specified - skip the do_reloc
			 * stage.
			 */
			if ((flags & FLG_OF_RELOBJ) ||
			    !(dtflags1 & DF_1_NORELOC)) {
				if (do_reloc((uchar_t)arsp->rel_rtype,
				    addr, &value, arsp->rel_sname,
				    ifl_name) == 0)
					return_code = S_ERROR;
			}
		}
	}
	return (return_code);
}

/*
 * Add an output relocation record.
 */
uintptr_t
add_outrel(Word flags, Rel_desc *rsp, Ofl_desc *ofl)
{
	Rel_desc	*orsp;
	Rel_cache	*rcp;
	Sym_desc	*sdp = rsp->rel_sym;

	/*
	 * Static executables *do not* want any relocations against them.
	 * Since our engine still creates relocations against a WEAK UNDEFINED
	 * symbol in a static executable, it's best to disable them here
	 * instead of through out the relocation code.
	 */
	if ((ofl->ofl_flags & (FLG_OF_STATIC | FLG_OF_EXEC)) ==
	    (FLG_OF_STATIC | FLG_OF_EXEC))
		return (1);

	/*
	 * If no relocation cache structures are available allocate
	 * a new one and link it into the cache list.
	 */
	if ((ofl->ofl_outrels.tail == 0) ||
	    ((rcp = (Rel_cache *)ofl->ofl_outrels.tail->data) == 0) ||
	    ((orsp = rcp->rc_free) == rcp->rc_end)) {
		static size_t	nextsize = 0;
		size_t		size;

		/*
		 * Output relocation numbers can vary considerably between
		 * building executables or shared objects (pic vs. non-pic),
		 * etc.  But, they typically aren't very large, so for these
		 * objects use a standard bucket size.  For building relocatable
		 * objects, typically there will be an output relocation for
		 * every input relocation.
		 */
		if (nextsize == 0) {
			if (ofl->ofl_flags & FLG_OF_RELOBJ) {
				if ((size = ofl->ofl_relocincnt) == 0)
					size = REL_LOIDESCNO;
				if (size > REL_HOIDESCNO)
					nextsize = REL_HOIDESCNO;
				else
					nextsize = REL_LOIDESCNO;
			} else
				nextsize = size = REL_HOIDESCNO;
		} else
			size = nextsize;

		size = size * sizeof (Rel_desc);

		if (((rcp = libld_malloc(sizeof (Rel_cache) + size)) == 0) ||
		    (list_appendc(&ofl->ofl_outrels, rcp) == 0))
			return (S_ERROR);

		/* LINTED */
		rcp->rc_free = orsp = (Rel_desc *)(rcp + 1);
		/* LINTED */
		rcp->rc_end = (Rel_desc *)((char *)rcp->rc_free + size);
	}

	/*
	 * If we are adding a output relocation against a section
	 * symbol (non-RELATIVE) then mark that section.  These sections
	 * will be added to the .dynsym symbol table.
	 */
	if (sdp && (rsp->rel_rtype != M_R_RELATIVE) &&
	    ((flags & FLG_REL_SCNNDX) ||
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION))) {

		/*
		 * If this is a COMMON symbol - no output section
		 * exists yet - (it's created as part of sym_validate()).
		 * So - we mark here that when it's created it should
		 * be tagged with the FLG_OS_OUTREL flag.
		 */
		if ((sdp->sd_flags & FLG_SY_SPECSEC) &&
		    (sdp->sd_shndx == SHN_COMMON)) {
			if (ELF_ST_TYPE(sdp->sd_sym->st_info) != STT_TLS)
				ofl->ofl_flags1 |= FLG_OF1_BSSOREL;
			else
				ofl->ofl_flags1 |= FLG_OF1_TLSOREL;
		} else {
			Os_desc	*osp = sdp->sd_isc->is_osdesc;

			if ((osp->os_flags & FLG_OS_OUTREL) == 0) {
				ofl->ofl_dynshdrcnt++;
				osp->os_flags |= FLG_OS_OUTREL;
			}
		}
	}

	*orsp = *rsp;
	orsp->rel_flags |= flags;

	rcp->rc_free++;
	ofl->ofl_outrelscnt++;

	if (flags & FLG_REL_GOT)
		ofl->ofl_relocgotsz += (Xword)sizeof (Rel);
	else if (flags & FLG_REL_PLT)
		ofl->ofl_relocpltsz += (Xword)sizeof (Rel);
	else if (flags & FLG_REL_BSS)
		ofl->ofl_relocbsssz += (Xword)sizeof (Rel);
	else if (flags & FLG_REL_NOINFO)
		ofl->ofl_relocrelsz += (Xword)sizeof (Rel);
	else
		orsp->rel_osdesc->os_szoutrels += (Xword)sizeof (Rel);

	if (orsp->rel_rtype == M_R_RELATIVE)
		ofl->ofl_relocrelcnt++;

	/*
	 * We don't perform sorting on PLT relocations because
	 * they have already been assigned a PLT index and if we
	 * were to sort them we would have to re-assign the plt indexes.
	 */
	if (!(flags & FLG_REL_PLT))
		ofl->ofl_reloccnt++;

	/*
	 * Insure a GLOBAL_OFFSET_TABLE is generated if required.
	 */
	if (IS_GOT_REQUIRED(orsp->rel_rtype))
		ofl->ofl_flags |= FLG_OF_BLDGOT;

	/*
	 * Identify and possibly warn of a displacement relocation.
	 */
	if (orsp->rel_flags & FLG_REL_DISP) {
		ofl->ofl_dtflags_1 |= DF_1_DISPRELPND;

		if (ofl->ofl_flags & FLG_OF_VERBOSE)
			disp_errmsg(MSG_INTL(MSG_REL_DISPREL4), orsp, ofl);
	}
	DBG_CALL(Dbg_reloc_ors_entry(M_MACH, orsp));
	return (1);
}

/*
 * Stub routine since register symbols are not supported on i386.
 */
/* ARGSUSED */
uintptr_t
reloc_register(Rel_desc * rsp, Is_desc * isp, Ofl_desc * ofl)
{
	eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NOREG));
	return (S_ERROR);
}

/*
 * process relocation for a LOCAL symbol
 */
uintptr_t
reloc_local(Rel_desc * rsp, Ofl_desc * ofl)
{
	Word		flags = ofl->ofl_flags;
	Sym_desc	*sdp = rsp->rel_sym;
	Word		shndx = rsp->rel_sym->sd_shndx;

	/*
	 * if ((shared object) and (not pc relative relocation) and
	 *    (not against ABS symbol))
	 * then
	 *	build R_386_RELATIVE
	 * fi
	 */
	if ((flags & FLG_OF_SHAROBJ) && (rsp->rel_flags & FLG_REL_LOAD) &&
	    !(IS_PC_RELATIVE(rsp->rel_rtype)) &&
	    !(IS_GOT_BASED(rsp->rel_rtype)) &&
	    !(rsp->rel_isdesc != NULL &&
	    (rsp->rel_isdesc->is_shdr->sh_type == SHT_SUNW_dof)) &&
	    (((sdp->sd_flags & FLG_SY_SPECSEC) == 0) ||
	    (shndx != SHN_ABS) || (sdp->sd_aux && sdp->sd_aux->sa_symspec))) {
		Word	ortype = rsp->rel_rtype;

		rsp->rel_rtype = R_386_RELATIVE;
		if (add_outrel(NULL, rsp, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;
	}

	/*
	 * If the relocation is against a 'non-allocatable' section
	 * and we can not resolve it now - then give a warning
	 * message.
	 *
	 * We can not resolve the symbol if either:
	 *	a) it's undefined
	 *	b) it's defined in a shared library and a
	 *	   COPY relocation hasn't moved it to the executable
	 *
	 * Note: because we process all of the relocations against the
	 *	text segment before any others - we know whether
	 *	or not a copy relocation will be generated before
	 *	we get here (see reloc_init()->reloc_segments()).
	 */
	if (!(rsp->rel_flags & FLG_REL_LOAD) &&
	    ((shndx == SHN_UNDEF) ||
	    ((sdp->sd_ref == REF_DYN_NEED) &&
	    ((sdp->sd_flags & FLG_SY_MVTOCOMM) == 0)))) {
		/*
		 * If the relocation is against a SHT_SUNW_ANNOTATE
		 * section - then silently ignore that the relocation
		 * can not be resolved.
		 */
		if (rsp->rel_osdesc &&
		    (rsp->rel_osdesc->os_shdr->sh_type == SHT_SUNW_ANNOTATE))
			return (0);
		(void) eprintf(ERR_WARNING, MSG_INTL(MSG_REL_EXTERNSYM),
		    conv_reloc_386_type_str(rsp->rel_rtype),
		    rsp->rel_isdesc->is_file->ifl_name,
		    demangle(rsp->rel_sname), rsp->rel_osdesc->os_name);
		return (1);
	}

	/*
	 * Perform relocation.
	 */
	return (add_actrel(NULL, rsp, ofl));
}

uintptr_t
/* ARGSUSED */
reloc_GOTOP(Boolean local, Rel_desc * rsp, Ofl_desc * ofl)
{
	/*
	 * Stub routine for common code compatibility, we shouldn't
	 * actually get here on x86.
	 */
	assert(0);
	return (S_ERROR);
}

uintptr_t
reloc_TLS(Boolean local, Rel_desc * rsp, Ofl_desc * ofl)
{
	Word		rtype = rsp->rel_rtype;
	Sym_desc	*sdp = rsp->rel_sym;
	Word		flags = ofl->ofl_flags;
	Word		rflags;
	Gotndx		*gnp;

	/*
	 * all TLS relocations are illegal in a static executable.
	 */
	if ((ofl->ofl_flags & (FLG_OF_STATIC | FLG_OF_EXEC)) ==
	    (FLG_OF_STATIC | FLG_OF_EXEC)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_TLSSTAT),
		    conv_reloc_386_type_str(rsp->rel_rtype),
		    rsp->rel_isdesc->is_file->ifl_name,
		    demangle(rsp->rel_sname));
		return (S_ERROR);
	}

	/*
	 * Any TLS relocation must be against a STT_TLS symbol, all others
	 * are illegal.
	 */
	if (ELF_ST_TYPE(sdp->sd_sym->st_info) != STT_TLS) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_TLSBADSYM),
		    conv_reloc_386_type_str(rsp->rel_rtype),
		    rsp->rel_isdesc->is_file->ifl_name,
		    demangle(rsp->rel_sname),
		    conv_info_type_str(ofl->ofl_e_machine,
		    ELF_ST_TYPE(sdp->sd_sym->st_info)));
		return (S_ERROR);
	}

	/*
	 * We're a executable - use either the IE or LE
	 * access model.
	 */
	if (flags & FLG_OF_EXEC) {
		/*
		 * If we are using either IE or LE reference
		 * model set the DF_STATIC_TLS flag.
		 */
		ofl->ofl_dtflags |= DF_STATIC_TLS;

		if (!local) {
			Gotref	gref;
			/*
			 * IE access model
			 */
			/*
			 * It's not possible for LD or LE reference
			 * models to reference a symbol external to
			 * the current object.
			 */
			if (IS_TLS_LD(rtype) || IS_TLS_LE(rtype)) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_REL_TLSBND),
				    conv_reloc_386_type_str(rsp->rel_rtype),
				    rsp->rel_isdesc->is_file->ifl_name,
				    demangle(rsp->rel_sname),
				    sdp->sd_file->ifl_name);
				return (S_ERROR);
			}

			gref = GOT_REF_TLSIE;

			/*
			 * Assign a GOT entry for static TLS references
			 */
			if ((gnp = find_gotndx(&(sdp->sd_GOTndxs),
			    gref, ofl, 0)) == 0) {
				if (assign_gotndx(&(sdp->sd_GOTndxs),
				    gnp, gref, ofl, rsp, sdp) == S_ERROR)
					return (S_ERROR);
				rsp->rel_rtype = R_386_TLS_TPOFF;
				if (add_outrel((FLG_REL_GOT | FLG_REL_STLS),
				    rsp, ofl) == S_ERROR)
					return (S_ERROR);
				rsp->rel_rtype = rtype;
			}
			if (IS_TLS_IE(rtype))
				return (add_actrel(FLG_REL_STLS, rsp, ofl));

			/*
			 * If (GD or LD) reference models - fixups
			 * are required.
			 */
			return (add_actrel((FLG_REL_TLSFIX | FLG_REL_STLS),
			    rsp, ofl));
		}
		/*
		 * LE access model
		 */
		if (IS_TLS_LE(rtype) || (rtype == R_386_TLS_LDO_32))
			return (add_actrel(FLG_REL_STLS, rsp, ofl));

		return (add_actrel((FLG_REL_TLSFIX | FLG_REL_STLS), rsp, ofl));
	}

	/*
	 * Building a shared object
	 */

	/*
	 * Building a shared object - only GD & LD access models
	 * will work here.
	 */
	if (IS_TLS_IE(rtype) || IS_TLS_LE(rtype)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_TLSIE),
		    conv_reloc_386_type_str(rsp->rel_rtype),
		    rsp->rel_isdesc->is_file->ifl_name,
		    demangle(rsp->rel_sname));
		return (S_ERROR);
	}

	/*
	 * LD access mode can only bind to local symbols.
	 */
	if (!local && IS_TLS_LD(rtype)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_TLSBND),
		    conv_reloc_386_type_str(rsp->rel_rtype),
		    rsp->rel_isdesc->is_file->ifl_name,
		    demangle(rsp->rel_sname),
		    sdp->sd_file->ifl_name);
		return (S_ERROR);
	}


	if (IS_TLS_LD(rtype) && ((gnp = find_gotndx(&(sdp->sd_GOTndxs),
	    GOT_REF_TLSLD, ofl, 0)) == 0)) {
		if (assign_gotndx(&(sdp->sd_GOTndxs), gnp, GOT_REF_TLSLD,
		    ofl, rsp, sdp) == S_ERROR)
			return (S_ERROR);
		rflags = FLG_REL_GOT | FLG_REL_MTLS;
		if (local)
			rflags |= FLG_REL_SCNNDX;
		rsp->rel_rtype = R_386_TLS_DTPMOD32;
		if (add_outrel(rflags, rsp, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = rtype;
	} else if (IS_TLS_GD(rtype) && ((gnp = find_gotndx(&(sdp->sd_GOTndxs),
	    GOT_REF_TLSGD, ofl, 0)) == 0)) {
		if (assign_gotndx(&(sdp->sd_GOTndxs), gnp, GOT_REF_TLSGD,
		    ofl, rsp, sdp) == S_ERROR)
			return (S_ERROR);
		rflags = FLG_REL_GOT | FLG_REL_DTLS;
		if (local)
			rflags |= FLG_REL_SCNNDX;
		rsp->rel_rtype = R_386_TLS_DTPMOD32;
		if (add_outrel(rflags, rsp, ofl) == S_ERROR)
			return (S_ERROR);
		if (local == TRUE) {
			rsp->rel_rtype = R_386_TLS_DTPOFF32;
			if (add_actrel((FLG_REL_GOT | FLG_REL_DTLS), rsp,
			    ofl) == S_ERROR)
				return (S_ERROR);
		} else {
			rsp->rel_rtype = R_386_TLS_DTPOFF32;
			if (add_outrel((FLG_REL_GOT | FLG_REL_DTLS), rsp,
			    ofl) == S_ERROR)
				return (S_ERROR);
		}
		rsp->rel_rtype = rtype;
	}
	/*
	 * For GD/LD TLS reference - TLS_{GD,LD}_CALL, this will eventually
	 * cause a call to __tls_get_addr().  Let's convert this
	 * relocation to that symbol now, and prepare for the PLT magic.
	 */
	if ((rtype == R_386_TLS_GD_PLT) || (rtype == R_386_TLS_LDM_PLT)) {
		Sym_desc *	tlsgetsym;

		if ((tlsgetsym = sym_add_u(MSG_ORIG(MSG_SYM_TLSGETADDR_UU),
		    ofl)) == (Sym_desc *)S_ERROR)
			return (S_ERROR);
		rsp->rel_sym = tlsgetsym;
		rsp->rel_sname = tlsgetsym->sd_name;
		rsp->rel_rtype = R_386_PLT32;
		if (reloc_plt(rsp, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_sym = sdp;
		rsp->rel_sname = sdp->sd_name;
		rsp->rel_rtype = rtype;
		return (1);
	}

	if (IS_TLS_LD(rtype))
		return (add_actrel(FLG_REL_MTLS, rsp, ofl));

	return (add_actrel(FLG_REL_DTLS, rsp, ofl));
}

/* ARGSUSED3 */
Gotndx *
find_gotndx(List * lst, Gotref gref, Ofl_desc * ofl, Rel_desc * rdesc)
{
	Listnode *	lnp;
	Gotndx *	gnp;

	if ((gref == GOT_REF_TLSLD) && ofl->ofl_tlsldgotndx)
		return (ofl->ofl_tlsldgotndx);

	for (LIST_TRAVERSE(lst, lnp, gnp)) {
		if (gnp->gn_gotref == gref)
			return (gnp);
	}
	return ((Gotndx *)0);
}

Xword
calc_got_offset(Rel_desc * rdesc, Ofl_desc * ofl)
{
	Os_desc		*osp = ofl->ofl_osgot;
	Sym_desc	*sdp = rdesc->rel_sym;
	Xword		gotndx;
	Gotref		gref;
	Gotndx		*gnp;

	if (rdesc->rel_flags & FLG_REL_DTLS)
		gref = GOT_REF_TLSGD;
	else if (rdesc->rel_flags & FLG_REL_MTLS)
		gref = GOT_REF_TLSLD;
	else if (rdesc->rel_flags & FLG_REL_STLS)
		gref = GOT_REF_TLSIE;
	else
		gref = GOT_REF_GENERIC;

	gnp = find_gotndx(&(sdp->sd_GOTndxs), gref, ofl, 0);
	assert(gnp);

	gotndx = (Xword)gnp->gn_gotndx;

	if ((rdesc->rel_flags & FLG_REL_DTLS) &&
	    (rdesc->rel_rtype == R_386_TLS_DTPOFF32))
		gotndx++;

	return ((Xword)(osp->os_shdr->sh_addr + (gotndx * M_GOT_ENTSIZE)));
}


/* ARGSUSED4 */
uintptr_t
assign_gotndx(List * lst, Gotndx * pgnp, Gotref gref, Ofl_desc * ofl,
    Rel_desc * rsp, Sym_desc * sdp)
{
	Gotndx	*gnp;
	uint_t	gotents;

	if (pgnp)
		return (1);

	if ((gref == GOT_REF_TLSGD) || (gref == GOT_REF_TLSLD))
		gotents = 2;
	else
		gotents = 1;

	if ((gnp = libld_calloc(sizeof (Gotndx), 1)) == 0)
		return (S_ERROR);
	gnp->gn_gotndx = ofl->ofl_gotcnt;
	gnp->gn_gotref = gref;

	ofl->ofl_gotcnt += gotents;

	if (gref == GOT_REF_TLSLD) {
		ofl->ofl_tlsldgotndx = gnp;
		return (1);
	}

	if (list_appendc(lst, (void *)gnp) == 0)
		return (S_ERROR);

	return (1);
}


void
assign_plt_ndx(Sym_desc * sdp, Ofl_desc *ofl)
{
	sdp->sd_aux->sa_PLTndx = 1 + ofl->ofl_pltcnt++;
	sdp->sd_aux->sa_PLTGOTndx = ofl->ofl_gotcnt++;
	ofl->ofl_flags |= FLG_OF_BLDGOT;
}

/*
 * Initializes .got[0] with the _DYNAMIC symbol value.
 */
uintptr_t
fillin_gotplt1(Ofl_desc * ofl)
{
	if (ofl->ofl_osgot) {
		Sym_desc *	sdp;

		if ((sdp = sym_find(MSG_ORIG(MSG_SYM_DYNAMIC_U),
		    SYM_NOHASH, 0, ofl)) != NULL) {
			uchar_t	*genptr = ((uchar_t *)
			    ofl->ofl_osgot->os_outdata->d_buf +
			    (M_GOT_XDYNAMIC * M_GOT_ENTSIZE));
			/* LINTED */
			*(Word *)genptr = (Word)sdp->sd_sym->st_value;
		}
	}

	/*
	 * Fill in the reserved slot in the procedure linkage table the first
	 * entry is:
	 *  if (building a.out) {
	 *	PUSHL	got[1]		    # the address of the link map entry
	 *	JMP *	got[2]		    # the address of rtbinder
	 *  } else {
	 *	PUSHL	got[1]@GOT(%ebx)    # the address of the link map entry
	 *	JMP *	got[2]@GOT(%ebx)    # the address of rtbinder
	 *  }
	 */
	if ((ofl->ofl_flags & FLG_OF_DYNAMIC) && ofl->ofl_osplt) {
		uchar_t *pltent;

		pltent = (uchar_t *)ofl->ofl_osplt->os_outdata->d_buf;
		if (!(ofl->ofl_flags & FLG_OF_SHAROBJ)) {
			pltent[0] = M_SPECIAL_INST;
			pltent[1] = M_PUSHL_DISP;
			pltent += 2;
			/* LINTED */
			*(Word *)pltent = (Word)(ofl->ofl_osgot->os_shdr->
				sh_addr + M_GOT_XLINKMAP * M_GOT_ENTSIZE);
			pltent += 4;
			pltent[0] = M_SPECIAL_INST;
			pltent[1] = M_JMP_DISP_IND;
			pltent += 2;
			/* LINTED */
			*(Word *)pltent = (Word)(ofl->ofl_osgot->os_shdr->
				sh_addr + M_GOT_XRTLD * M_GOT_ENTSIZE);
		} else {
			pltent[0] = M_SPECIAL_INST;
			pltent[1] = M_PUSHL_REG_DISP;
			pltent += 2;
			/* LINTED */
			*(Word *)pltent = (Word)(M_GOT_XLINKMAP *
				M_GOT_ENTSIZE);
			pltent += 4;
			pltent[0] = M_SPECIAL_INST;
			pltent[1] = M_JMP_REG_DISP_IND;
			pltent += 2;
			/* LINTED */
			*(Word *)pltent = (Word)(M_GOT_XRTLD *
				M_GOT_ENTSIZE);
		}
	}
	return (1);
}

/*
 * Return got[0].
 */
Addr
fillin_gotplt2(Ofl_desc * ofl)
{
	if (ofl->ofl_osgot)
		return (ofl->ofl_osgot->os_shdr->sh_addr);
	else
		return (0);
}
