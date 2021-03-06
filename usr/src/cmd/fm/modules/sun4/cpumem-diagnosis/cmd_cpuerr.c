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

#pragma ident	"@(#)cmd_cpuerr.c	1.10	06/06/28 SMI"

/*
 * Ereport-handling routines for CPU errors
 */

#include <cmd_cpu.h>
#include <cmd.h>

#include <strings.h>
#include <string.h>
#include <errno.h>
#include <fm/fmd_api.h>
#include <sys/fm/protocol.h>
#include <sys/async.h>
#ifdef sun4u
#include <sys/fm/cpu/UltraSPARC-III.h>
#include <cmd_opl.h>
#endif

/*
 * We follow the same algorithm for handling all L1$, TLB, and L2/L3 cache
 * tag events so we can have one common routine into which each handler
 * calls.  The two tests of (strcmp(serdnm, "") != 0) are used to eliminate
 * the need for a separate macro for UEs which override SERD engine
 * counting CEs leading to same fault.
 */
/*ARGSUSED9*/
static cmd_evdisp_t
cmd_cpuerr_common(fmd_hdl_t *hdl, fmd_event_t *ep, cmd_cpu_t *cpu,
    cmd_case_t *cc, cmd_ptrsubtype_t pstype, const char *serdnm,
    const char *serdn, const char *serdt, const char *fltnm,
    cmd_errcl_t clcode)
{
	const char *uuid;

	if (cc->cc_cp != NULL && fmd_case_solved(hdl, cc->cc_cp))
		return (CMD_EVD_REDUND);

	if (cc->cc_cp == NULL) {
		cc->cc_cp = cmd_case_create(hdl, &cpu->cpu_header, pstype,
		    &uuid);
		if (strcmp(serdnm, "") != 0) {
			cc->cc_serdnm = cmd_cpu_serdnm_create(hdl, cpu,
			    serdnm);
			fmd_serd_create(hdl, cc->cc_serdnm,
			    fmd_prop_get_int32(hdl, serdn),
			    fmd_prop_get_int64(hdl, serdt));
		}
	}

	if (strcmp(serdnm, "") != 0) {
		fmd_hdl_debug(hdl, "adding event to %s\n", cc->cc_serdnm);
		if (fmd_serd_record(hdl, cc->cc_serdnm, ep) == FMD_B_FALSE)
			return (CMD_EVD_OK); /* serd engine hasn't fired yet */

		fmd_case_add_serd(hdl, cc->cc_cp, cc->cc_serdnm);
	} else {
		fmd_hdl_debug(hdl,
		    "destroying existing %s state for class %x\n",
		    cc->cc_serdnm, clcode);
		fmd_serd_destroy(hdl, cc->cc_serdnm);
		fmd_hdl_strfree(hdl, cc->cc_serdnm);
		cc->cc_serdnm = NULL;
		fmd_case_reset(hdl, cc->cc_cp);
		fmd_case_add_ereport(hdl, cc->cc_cp, ep);
	}

	cmd_cpu_create_faultlist(hdl, cc->cc_cp, cpu, fltnm, NULL, 100);

	fmd_case_solve(hdl, cc->cc_cp);

	return (CMD_EVD_OK);
}

#define	CMD_CPU_SIMPLEHANDLER(name, casenm, ptr, ntname, fltname)	\
cmd_evdisp_t								\
cmd_##name(fmd_hdl_t *hdl, fmd_event_t *ep, nvlist_t *nvl,		\
    const char *class, cmd_errcl_t clcode)				\
{									\
	uint8_t level = clcode & CMD_ERRCL_LEVEL_EXTRACT;		\
	cmd_cpu_t *cpu;							\
									\
	clcode &= CMD_ERRCL_LEVEL_MASK;					\
	if ((cpu = cmd_cpu_lookup_from_detector(hdl, nvl, class,	\
	    level)) == NULL || cpu->cpu_faulting)			\
		return (CMD_EVD_UNUSED);				\
									\
	return (cmd_cpuerr_common(hdl, ep, cpu, &cpu->cpu_##casenm,	\
	    ptr, ntname, ntname "_n", ntname "_t", fltname, clcode));	\
}

CMD_CPU_SIMPLEHANDLER(txce, l2tag, CMD_PTR_CPU_L2TAG, "l2tag", "l2cachetag")
CMD_CPU_SIMPLEHANDLER(l3_thce, l3tag, CMD_PTR_CPU_L3TAG, "l3tag", "l3cachetag")
CMD_CPU_SIMPLEHANDLER(icache, icache, CMD_PTR_CPU_ICACHE, "icache", "icache")
CMD_CPU_SIMPLEHANDLER(dcache, dcache, CMD_PTR_CPU_DCACHE, "dcache", "dcache")
CMD_CPU_SIMPLEHANDLER(pcache, pcache, CMD_PTR_CPU_PCACHE, "pcache", "pcache")
CMD_CPU_SIMPLEHANDLER(itlb, itlb, CMD_PTR_CPU_ITLB, "itlb", "itlb")
CMD_CPU_SIMPLEHANDLER(dtlb, dtlb, CMD_PTR_CPU_DTLB, "dtlb", "dtlb")
CMD_CPU_SIMPLEHANDLER(irc, ireg, CMD_PTR_CPU_IREG, "ireg", "ireg")
CMD_CPU_SIMPLEHANDLER(frc, freg, CMD_PTR_CPU_FREG, "freg", "freg")
CMD_CPU_SIMPLEHANDLER(mau, mau, CMD_PTR_CPU_MAU, "mau", "mau")

CMD_CPU_SIMPLEHANDLER(fpu, fpu, CMD_PTR_CPU_FPU, "", "fpu")
CMD_CPU_SIMPLEHANDLER(l2ctl, l2ctl, CMD_PTR_CPU_L2CTL, "", "l2ctl")
CMD_CPU_SIMPLEHANDLER(iru, ireg, CMD_PTR_CPU_IREG, "", "ireg")
CMD_CPU_SIMPLEHANDLER(fru, freg, CMD_PTR_CPU_FREG, "", "freg")


#ifdef sun4u
/*
 * The following macro handles UEs or CPU errors.
 * It handles the error cases in which there is with or
 * without "resource".
 *
 * If the "fltname" "core" is to be generated, the sibling CPUs
 * within the core will be added to the suspect list.
 * If the "fltname" "chip" is to be generated, the sibling CPUs
 * within the chip will be added to the suspect list.
 * If the "fltname" "strand" is to be generated, the strand
 * itself will be in the suspect list.
 */
#define	CMD_OPL_UEHANDLER(name, casenm, ptr, fltname, has_rsrc)		\
cmd_evdisp_t								\
cmd_##name(fmd_hdl_t *hdl, fmd_event_t *ep, nvlist_t *nvl,		\
    const char *class, cmd_errcl_t clcode)				\
{									\
	cmd_cpu_t *cpu;							\
	cmd_case_t *cc;							\
	cmd_evdisp_t rc;						\
	nvlist_t  *rsrc = NULL;						\
	uint8_t cpumask, version = 1;					\
	uint8_t lookup_rsrc = has_rsrc;					\
									\
	fmd_hdl_debug(hdl,						\
	    "Enter cmd_opl_ue_cpu for class %x\n", clcode);		\
									\
	if (lookup_rsrc) {						\
		if (nvlist_lookup_nvlist(nvl,				\
		    FM_EREPORT_PAYLOAD_NAME_RESOURCE, &rsrc) != 0)	\
			return (CMD_EVD_BAD);				\
									\
		if ((cpu = cmd_cpu_lookup(hdl, rsrc, CPU_EREPORT_STRING,\
		    CMD_CPU_LEVEL_THREAD)) == NULL ||			\
		    cpu->cpu_faulting)					\
			return (CMD_EVD_UNUSED);			\
	} else {							\
		if ((cpu = cmd_cpu_lookup_from_detector(hdl, nvl, class,\
		    CMD_CPU_LEVEL_THREAD)) == NULL || cpu->cpu_faulting)\
			return (CMD_EVD_UNUSED);			\
									\
		(void) nvlist_lookup_nvlist(nvl,			\
		    FM_EREPORT_DETECTOR, &rsrc);			\
	}								\
									\
	if (nvlist_lookup_uint8(rsrc, FM_VERSION, &version) != 0 ||	\
	    version > FM_CPU_SCHEME_VERSION ||				\
	    nvlist_lookup_uint8(rsrc, FM_FMRI_CPU_MASK, &cpumask) != 0)	\
		return (CMD_EVD_BAD);					\
									\
	cc = &cpu->cpu_##casenm;					\
	rc = cmd_opl_ue_cpu(hdl, ep, class, fltname,			\
	    ptr, cpu, cc, cpumask);					\
	return (rc);							\
}

/*
 * CPU errors without resource
 */
CMD_OPL_UEHANDLER(oplinv_urg, opl_inv_urg, CMD_PTR_CPU_UGESR_INV_URG, "core", 0)
CMD_OPL_UEHANDLER(oplcre, opl_cre, CMD_PTR_CPU_UGESR_CRE, "core", 0)
CMD_OPL_UEHANDLER(opltsb_ctx, opl_tsb_ctx, CMD_PTR_CPU_UGESR_TSB_CTX, "core", 0)
CMD_OPL_UEHANDLER(opltsbp, opl_tsbp, CMD_PTR_CPU_UGESR_TSBP, "core", 0)
CMD_OPL_UEHANDLER(oplpstate, opl_pstate, CMD_PTR_CPU_UGESR_PSTATE, "core", 0)
CMD_OPL_UEHANDLER(opltstate, opl_tstate, CMD_PTR_CPU_UGESR_TSTATE, "core", 0)
CMD_OPL_UEHANDLER(opliug_f, opl_iug_f, CMD_PTR_CPU_UGESR_IUG_F, "core", 0)
CMD_OPL_UEHANDLER(opliug_r, opl_iug_r, CMD_PTR_CPU_UGESR_IUG_R, "core", 0)
CMD_OPL_UEHANDLER(oplsdc, opl_sdc, CMD_PTR_CPU_UGESR_SDC, "chip", 0)
CMD_OPL_UEHANDLER(oplwdt, opl_wdt, CMD_PTR_CPU_UGESR_WDT, "core", 0)
CMD_OPL_UEHANDLER(opldtlb, opl_dtlb, CMD_PTR_CPU_UGESR_DTLB, "core", 0)
CMD_OPL_UEHANDLER(oplitlb, opl_itlb, CMD_PTR_CPU_UGESR_ITLB, "core", 0)
CMD_OPL_UEHANDLER(oplcore_err, opl_core_err, CMD_PTR_CPU_UGESR_CORE_ERR,
"core", 0)
CMD_OPL_UEHANDLER(opldae, opl_dae, CMD_PTR_CPU_UGESR_DAE, "core", 0)
CMD_OPL_UEHANDLER(opliae, opl_iae, CMD_PTR_CPU_UGESR_IAE, "core", 0)
CMD_OPL_UEHANDLER(opluge, opl_uge, CMD_PTR_CPU_UGESR_UGE, "core", 0)

/*
 * UEs with resource
 */
CMD_OPL_UEHANDLER(oplinv_sfsr, opl_invsfsr, CMD_PTR_CPU_INV_SFSR, "strand", 1)
CMD_OPL_UEHANDLER(opluecpu_detcpu, oplue_detcpu, CMD_PTR_CPU_UE_DET_CPU,
"chip", 1)
CMD_OPL_UEHANDLER(opluecpu_detio, oplue_detio, CMD_PTR_CPU_UE_DET_IO, "chip", 1)
CMD_OPL_UEHANDLER(oplmtlb, opl_mtlb, CMD_PTR_CPU_MTLB, "core", 1)
CMD_OPL_UEHANDLER(opltlbp, opl_tlbp, CMD_PTR_CPU_TLBP, "core", 1)
#endif	/* sun4u */

typedef struct errdata {
	cmd_serd_t *ed_serd;
	const char *ed_fltnm;
	const cmd_ptrsubtype_t ed_pst;
} errdata_t;

static const errdata_t l3errdata =
	{ &cmd.cmd_l3data_serd, "l3cachedata", CMD_PTR_CPU_L3DATA  };
static const errdata_t l2errdata =
	{ &cmd.cmd_l2data_serd, "l2cachedata", CMD_PTR_CPU_L2DATA };

/*ARGSUSED*/
static void
cmd_xxu_hdlr(fmd_hdl_t *hdl, cmd_xr_t *xr, fmd_event_t *ep)
{
	int isl3 = CMD_ERRCL_ISL3XXCU(xr->xr_clcode);
	const errdata_t *ed = isl3 ? &l3errdata : &l2errdata;
	cmd_cpu_t *cpu = xr->xr_cpu;
	cmd_case_t *cc = isl3 ? &cpu->cpu_l3data : &cpu->cpu_l2data;
	const char *uuid;
	nvlist_t *rsrc = NULL;

	if (cpu->cpu_faulting) {
		CMD_STAT_BUMP(xxu_retr_flt);
		return;
	}

	if (xr->xr_afar_status != AFLT_STAT_VALID) {
		fmd_hdl_debug(hdl, "xxU dropped, afar not VALID\n");
		return;
	}

	if (cmd_cpu_synd_check(xr->xr_synd) < 0) {
		fmd_hdl_debug(hdl, "xxU/LDxU dropped due to syndrome\n");
		return;
	}

#ifdef sun4u
	/*
	 * UE cache needed for sun4u only, because sun4u doesn't poison
	 * uncorrectable data loaded into L2/L3 cache.
	 */
	if (cmd_cpu_uec_match(xr->xr_cpu, xr->xr_afar)) {
		fmd_hdl_debug(hdl, "ue matched in UE cache\n");
		CMD_STAT_BUMP(xxu_ue_match);
		return;
	}
#endif /* sun4u */

	/*
	 * We didn't match in the UE cache.  We don't need to sleep for UE
	 * arrival, as we've already slept once for the train match.
	 */

	if (cc->cc_cp == NULL) {
		cc->cc_cp = cmd_case_create(hdl, &cpu->cpu_header, ed->ed_pst,
		    &uuid);
	} else if (cc->cc_serdnm != NULL) {
		fmd_hdl_debug(hdl, "destroying existing %s state\n",
		    cc->cc_serdnm);

		fmd_serd_destroy(hdl, cc->cc_serdnm);
		fmd_hdl_strfree(hdl, cc->cc_serdnm);
		cc->cc_serdnm = NULL;

		fmd_case_reset(hdl, cc->cc_cp);
	}

	if (xr->xr_rsrc_nvl != NULL && nvlist_dup(xr->xr_rsrc_nvl,
	    &rsrc, 0) != 0) {
		fmd_hdl_abort(hdl, "failed to duplicate resource FMRI for "
		    "%s fault", ed->ed_fltnm);
	}

	fmd_case_add_ereport(hdl, cc->cc_cp, ep);

	cmd_cpu_create_faultlist(hdl, cc->cc_cp, cpu, ed->ed_fltnm, rsrc, 100);
	fmd_case_solve(hdl, cc->cc_cp);
}

static void
cmd_xxc_hdlr(fmd_hdl_t *hdl, cmd_xr_t *xr, fmd_event_t *ep)
{
	int isl3 = CMD_ERRCL_ISL3XXCU(xr->xr_clcode);
	const errdata_t *ed = isl3 ? &l3errdata : &l2errdata;
	cmd_cpu_t *cpu = xr->xr_cpu;
	cmd_case_t *cc = isl3 ? &cpu->cpu_l3data : &cpu->cpu_l2data;
	const char *uuid;
	nvlist_t *rsrc = NULL;

	if (cpu->cpu_faulting || (cc->cc_cp != NULL &&
	    fmd_case_solved(hdl, cc->cc_cp)))
		return;

	if (cc->cc_cp == NULL) {
		cc->cc_cp = cmd_case_create(hdl, &cpu->cpu_header, ed->ed_pst,
		    &uuid);
		cc->cc_serdnm = cmd_cpu_serdnm_create(hdl, cpu,
		    ed->ed_serd->cs_name);

		fmd_serd_create(hdl, cc->cc_serdnm, ed->ed_serd->cs_n,
		    ed->ed_serd->cs_t);
	}

	fmd_hdl_debug(hdl, "adding event to %s\n", cc->cc_serdnm);

	if (fmd_serd_record(hdl, cc->cc_serdnm, ep) == FMD_B_FALSE)
		return; /* serd engine hasn't fired yet */

	if (xr->xr_rsrc_nvl != NULL && nvlist_dup(xr->xr_rsrc_nvl,
	    &rsrc, 0) != 0) {
		fmd_hdl_abort(hdl, "failed to duplicate resource FMRI for "
		    "%s fault", ed->ed_fltnm);
	}

	fmd_case_add_serd(hdl, cc->cc_cp, cc->cc_serdnm);
	cmd_cpu_create_faultlist(hdl, cc->cc_cp, cpu, ed->ed_fltnm, rsrc, 100);
	fmd_case_solve(hdl, cc->cc_cp);
}

/*
 * We're back from the timeout.  Check to see if this event was part of a train.
 * If it was, make sure to only process the cause of the train.  If not,
 * process the event directly.
 */
static void
cmd_xxcu_resolve(fmd_hdl_t *hdl, cmd_xr_t *xr, fmd_event_t *ep,
    cmd_xr_hdlr_f *hdlr)
{
	cmd_xxcu_trw_t *trw;
	cmd_errcl_t cause;

	if ((trw = cmd_trw_lookup(xr->xr_ena)) == NULL) {
		hdlr(hdl, xr, ep);
		return;
	}

	fmd_hdl_debug(hdl, "found waiter with mask 0x%08llx\n", trw->trw_mask);

	trw->trw_flags |= CMD_TRW_F_DELETING;

	if (trw->trw_flags & CMD_TRW_F_CAUSESEEN) {
		fmd_hdl_debug(hdl, "cause already seen -- discarding\n");
		goto done;
	}

	if ((cause = cmd_xxcu_train_match(trw->trw_mask)) == 0) {
		/*
		 * We didn't match in a train, so we're going to process each
		 * event individually.
		 */
		fmd_hdl_debug(hdl, "didn't match in a train\n");
		hdlr(hdl, xr, ep);
		goto done;
	}

	fmd_hdl_debug(hdl, "found a match for train.  cause is %llx, "
	    "this is %llx\n", cause, xr->xr_clcode);

	/*
	 * We've got a train match.  If this event is the cause of the train,
	 * process it.
	 */
	if (cause == xr->xr_clcode) {
		trw->trw_flags |= CMD_TRW_F_CAUSESEEN;
		hdlr(hdl, xr, ep);
	}

done:
	cmd_trw_deref(hdl, trw);
}

void
cmd_xxc_resolve(fmd_hdl_t *hdl, cmd_xr_t *xr, fmd_event_t *ep)
{
	cmd_xxcu_resolve(hdl, xr, ep, cmd_xxc_hdlr);
}

void
cmd_xxu_resolve(fmd_hdl_t *hdl, cmd_xr_t *xr, fmd_event_t *ep)
{
	cmd_xxcu_resolve(hdl, xr, ep, cmd_xxu_hdlr);
}

static cmd_evdisp_t
cmd_xxcu_initial(fmd_hdl_t *hdl, fmd_event_t *ep, nvlist_t *nvl,
    const char *class, cmd_errcl_t clcode, uint_t hdlrid)
{
	cmd_xxcu_trw_t *trw;
	cmd_case_t *cc;
	cmd_cpu_t *cpu;
	cmd_xr_t *xr;
	uint64_t ena;
	uint8_t level = clcode & CMD_ERRCL_LEVEL_EXTRACT;

	clcode &= CMD_ERRCL_LEVEL_MASK; /* keep level bits out of train masks */

	if ((cpu = cmd_cpu_lookup_from_detector(hdl, nvl, class,
	    level)) == NULL || cpu->cpu_faulting)
		return (CMD_EVD_UNUSED);

	cc = CMD_ERRCL_ISL2XXCU(clcode) ? &cpu->cpu_l2data : &cpu->cpu_l3data;
	if (cc->cc_cp != NULL && fmd_case_solved(hdl, cc->cc_cp))
		return (CMD_EVD_REDUND);

	(void) nvlist_lookup_uint64(nvl, FM_EREPORT_ENA, &ena);

	fmd_hdl_debug(hdl, "scheduling %s (%llx) for redelivery\n",
	    class, clcode);

	if ((trw = cmd_trw_lookup(ena)) == NULL) {
		if ((trw = cmd_trw_alloc(ena)) == NULL) {
			fmd_hdl_debug(hdl, "failed to get new trw\n");
			goto redeliver;
		}
	}

	if (trw->trw_flags & CMD_TRW_F_DELETING)
		goto redeliver;

	if (trw->trw_mask & clcode) {
		fmd_hdl_debug(hdl, "clcode %llx is already in trw "
		    "(mask %llx)\n", clcode, trw->trw_mask);
		return (CMD_EVD_UNUSED);
	}

	cmd_trw_ref(hdl, trw, clcode);

	fmd_hdl_debug(hdl, "trw rescheduled for train delivery\n");

redeliver:
	if ((xr = cmd_xr_create(hdl, ep, nvl, cpu, clcode)) == NULL)
		return (CMD_EVD_BAD);

	return (cmd_xr_reschedule(hdl, xr, hdlrid));
}

cmd_evdisp_t
cmd_xxu(fmd_hdl_t *hdl, fmd_event_t *ep, nvlist_t *nvl, const char *class,
    cmd_errcl_t clcode)
{
	return (cmd_xxcu_initial(hdl, ep, nvl, class, clcode, CMD_XR_HDLR_XXU));
}

cmd_evdisp_t
cmd_xxc(fmd_hdl_t *hdl, fmd_event_t *ep, nvlist_t *nvl, const char *class,
    cmd_errcl_t clcode)
{
	return (cmd_xxcu_initial(hdl, ep, nvl, class, clcode, CMD_XR_HDLR_XXC));
}

void
cmd_cpuerr_close(fmd_hdl_t *hdl, void *arg)
{
	cmd_cpu_destroy(hdl, arg);
}
