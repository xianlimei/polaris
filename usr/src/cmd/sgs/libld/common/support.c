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

#pragma ident	"@(#)support.c	1.17	06/08/29 SMI"

#include	<stdio.h>
#include	<dlfcn.h>
#include	<libelf.h>
#include	<link.h>
#include	<debug.h>
#include	"msg.h"
#include	"_libld.h"

/*
 * Table which defines the default functions to be called by the library
 * SUPPORT (-S <libname>).  These functions can be redefined by the
 * ld_support_loadso() routine.
 */
static Support_list support[LDS_NUM] = {
	{MSG_ORIG(MSG_SUP_VERSION),	{ 0, 0 }},	/* LDS_VERSION */
	{MSG_ORIG(MSG_SUP_INPUT_DONE),	{ 0, 0 }},	/* LDS_INPUT_DONE */
#if	defined(_ELF64)
	{MSG_ORIG(MSG_SUP_START_64),	{ 0, 0 }},	/* LDS_START64 */
	{MSG_ORIG(MSG_SUP_ATEXIT_64),	{ 0, 0 }},	/* LDS_ATEXIT64 */
	{MSG_ORIG(MSG_SUP_FILE_64),	{ 0, 0 }},	/* LDS_FILE64 */
	{MSG_ORIG(MSG_SUP_INP_SECTION_64), { 0, 0 }},	/* LDS_INP_SECTION64 */
	{MSG_ORIG(MSG_SUP_SECTION_64),	{ 0, 0 }}	/* LDS_SECTION64 */
#else	/* Elf32 */
	{MSG_ORIG(MSG_SUP_START),	{ 0, 0 }},	/* LDS_START */
	{MSG_ORIG(MSG_SUP_ATEXIT),	{ 0, 0 }},	/* LDS_ATEXIT */
	{MSG_ORIG(MSG_SUP_FILE),	{ 0, 0 }},	/* LDS_FILE */
	{MSG_ORIG(MSG_SUP_INP_SECTION),	{ 0, 0 }},	/* LDS_INP_SECTION */
	{MSG_ORIG(MSG_SUP_SECTION),	{ 0, 0 }}	/* LDS_SECTION */
#endif
};

/*
 * Loads in a support shared object specified using the SGS_SUPPORT environment
 * variable or the -S ld option, and determines which interface functions are
 * provided by that object.
 */
uintptr_t
ld_sup_loadso(Ofl_desc *ofl, const char *obj)
{
	void		*handle;
	void		(*fptr)();
	Func_list	*flp;
	int 		i;
	uint_t		ver_level;

	/*
	 * Load the required support library.  If we are unable to load it fail
	 * with a fatal error.
	 */
	if ((handle = dlopen(obj, RTLD_LAZY)) == NULL) {
		eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_SUP_NOLOAD),
		    obj, dlerror());
		return (S_ERROR);
	}

	ver_level = LD_SUP_VERSION1;
	for (i = 0; i < LDS_NUM; i++) {
		if (fptr = (void (*)())dlsym(handle, support[i].sup_name)) {

			if ((flp = libld_malloc(sizeof (Func_list))) == NULL)
				return (S_ERROR);

			flp->fl_obj = obj;
			flp->fl_fptr = fptr;
			DBG_CALL(Dbg_support_load(ofl->ofl_lml, obj,
			    support[i].sup_name));

			if (i == LDS_VERSION) {
				DBG_CALL(Dbg_support_action(ofl->ofl_lml,
				    flp->fl_obj, support[LDS_VERSION].sup_name,
				    LDS_VERSION, 0));
				ver_level = ((uint_t(*)())
				    flp->fl_fptr)(LD_SUP_VCURRENT);
				if ((ver_level == LD_SUP_VNONE) ||
				    (ver_level > LD_SUP_VCURRENT)) {
					eprintf(ofl->ofl_lml, ERR_FATAL,
					    MSG_INTL(MSG_SUP_BADVERSION),
					    LD_SUP_VCURRENT, ver_level);
					(void) dlclose(handle);
					return (S_ERROR);
				}

			}
			flp->fl_version = ver_level;
			if (list_appendc(&support[i].sup_funcs, flp) == 0)
				return (S_ERROR);
		}
	}
	return (1);
}

/*
 * Wrapper routines for the ld support library calls.
 */
void
ld_sup_start(Ofl_desc *ofl, const Half etype, const char *caller)
{
	Func_list	*flp;
	Listnode	*lnp;

	for (LIST_TRAVERSE(&support[LDS_START].sup_funcs, lnp, flp)) {
		DBG_CALL(Dbg_support_action(ofl->ofl_lml, flp->fl_obj,
		    support[LDS_START].sup_name, LDS_START, ofl->ofl_name));
		(*flp->fl_fptr)(ofl->ofl_name, etype, caller);
	}
}

void
ld_sup_atexit(Ofl_desc *ofl, int ecode)
{
	Func_list	*flp;
	Listnode	*lnp;

	for (LIST_TRAVERSE(&support[LDS_ATEXIT].sup_funcs, lnp, flp)) {
		DBG_CALL(Dbg_support_action(ofl->ofl_lml, flp->fl_obj,
		    support[LDS_ATEXIT].sup_name, LDS_ATEXIT, 0));
		(*flp->fl_fptr)(ecode);
	}
}

void
ld_sup_file(Ofl_desc *ofl, const char *ifile, const Elf_Kind ekind, int flags,
    Elf *elf)
{
	Func_list	*flp;
	Listnode	*lnp;

	for (LIST_TRAVERSE(&support[LDS_FILE].sup_funcs, lnp, flp)) {
		int	_flags = 0;

		if (!(flags & FLG_IF_CMDLINE))
			_flags |= LD_SUP_DERIVED;
		if (!(flags & FLG_IF_NEEDED))
			_flags |= LD_SUP_INHERITED;
		if (flags & FLG_IF_EXTRACT)
			_flags |= LD_SUP_EXTRACTED;

		DBG_CALL(Dbg_support_action(ofl->ofl_lml, flp->fl_obj,
		    support[LDS_FILE].sup_name, LDS_FILE, ifile));
		(*flp->fl_fptr)(ifile, ekind, _flags, elf);
	}
}

uintptr_t
ld_sup_input_section(Ofl_desc *ofl, Ifl_desc *ifl, const char *sname,
    Shdr **oshdr, Word ndx, Elf_Scn *scn, Elf *elf)
{
	Func_list	*flp;
	Listnode	*lnp;
	uint_t		flags = 0;
	Elf_Data	*data = NULL;
	Shdr		*nshdr = *oshdr;

	for (LIST_TRAVERSE(&support[LDS_INP_SECTION].sup_funcs, lnp, flp)) {
		/*
		 * This interface was introduced in VERSION2 - so only call it
		 * for libraries reporting support for version 2 or above.
		 */
		if (flp->fl_version < LD_SUP_VERSION2)
			continue;
		if ((data == NULL) &&
		    ((data = elf_getdata(scn, NULL)) == NULL)) {
			eprintf(ofl->ofl_lml, ERR_ELF,
			    MSG_INTL(MSG_ELF_GETDATA), ifl->ifl_name);
			ofl->ofl_flags |= FLG_OF_FATAL;
			return (S_ERROR);
		}

		DBG_CALL(Dbg_support_action(ofl->ofl_lml, flp->fl_obj,
		    support[LDS_INP_SECTION].sup_name, LDS_INP_SECTION, sname));
		(*flp->fl_fptr)(sname, &nshdr, ndx, data, elf, &flags);
	}

	/*
	 * If the section header has been re-allocated (known to occur with
	 * libCCexcept.so), then diagnose the section header difference and
	 * return the new section header.
	 */
	if (nshdr != *oshdr) {
		Dbg_shdr_modified(ofl->ofl_lml, ifl->ifl_ehdr->e_machine,
		    *oshdr, nshdr, sname);
		*oshdr = nshdr;
	}
	return (0);
}

void
ld_sup_section(Ofl_desc *ofl, const char *scn, Shdr *shdr, Word ndx,
    Elf_Data *data, Elf *elf)
{
	Func_list	*flp;
	Listnode	*lnp;

	for (LIST_TRAVERSE(&support[LDS_SECTION].sup_funcs, lnp, flp)) {
		DBG_CALL(Dbg_support_action(ofl->ofl_lml, flp->fl_obj,
		    support[LDS_SECTION].sup_name, LDS_SECTION, scn));
		(*flp->fl_fptr)(scn, shdr, ndx, data, elf);
	}
}

void
ld_sup_input_done(Ofl_desc *ofl)
{
	Func_list	*flp;
	Listnode	*lnp;
	uint_t		flags = 0;

	for (LIST_TRAVERSE(&support[LDS_INPUT_DONE].sup_funcs, lnp, flp)) {
		/*
		 * This interface was introduced in VERSION2 - so only call it
		 * for libraries reporting support for version 2 or above.
		 */
		if (flp->fl_version < LD_SUP_VERSION2)
			continue;
		DBG_CALL(Dbg_support_action(ofl->ofl_lml, flp->fl_obj,
		    support[LDS_INPUT_DONE].sup_name, LDS_INPUT_DONE, 0));
		(*flp->fl_fptr)(&flags);
	}
}
