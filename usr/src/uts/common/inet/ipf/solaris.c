/*
 * Copyright (C) 1993-2001, 2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/* #pragma ident   "@(#)solaris.c	1.12 6/5/96 (C) 1995 Darren Reed"*/
#pragma ident "@(#)$Id: solaris.c,v 2.73.2.6 2005/07/13 21:40:47 darrenr Exp $"

#pragma ident	"@(#)solaris.c	1.8	06/07/14 SMI"

#include <sys/systm.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/cred.h>
#include <sys/dditypes.h>
#include <sys/stream.h>
#include <sys/poll.h>
#include <sys/autoconf.h>
#include <sys/byteorder.h>
#include <sys/socket.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/kstat.h>
#include <sys/sockio.h>
#include <net/if.h>
#if SOLARIS2 >= 6
# include <net/if_types.h>
#endif
#include <net/af.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include "netinet/ip_compat.h"
#include "netinet/ipl.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_auth.h"
#include "netinet/ip_state.h"


extern	struct	filterstats	frstats[];
extern	int	fr_running;
extern	int	fr_flags;
extern	int	iplwrite __P((dev_t, struct uio *, cred_t *));

extern ipnat_t *nat_list;

static	int	ipf_getinfo __P((dev_info_t *, ddi_info_cmd_t,
				 void *, void **));
#if SOLARIS2 < 10
static	int	ipf_identify __P((dev_info_t *));
#endif
static	int	ipf_attach __P((dev_info_t *, ddi_attach_cmd_t));
static	int	ipf_detach __P((dev_info_t *, ddi_detach_cmd_t));
static	int	fr_qifsync __P((ip_t *, int, void *, int, void *, mblk_t **));
static	int	ipf_property_update __P((dev_info_t *));
static	char	*ipf_devfiles[] = { IPL_NAME, IPNAT_NAME, IPSTATE_NAME,
				    IPAUTH_NAME, IPSYNC_NAME, IPSCAN_NAME,
				    IPLOOKUP_NAME, NULL };


#if SOLARIS2 >= 7
extern	timeout_id_t	fr_timer_id;
#else
extern	int		fr_timer_id;
#endif

static struct cb_ops ipf_cb_ops = {
	iplopen,
	iplclose,
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	iplread,
	iplwrite,	/* write */
	iplioctl,	/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,
	NULL,
	D_MTSAFE,
#if SOLARIS2 > 4
	CB_REV,
	nodev,		/* aread */
	nodev,		/* awrite */
#endif
};

static struct dev_ops ipf_ops = {
	DEVO_REV,
	0,
	ipf_getinfo,
#if SOLARIS2 >= 10
	nulldev,
#else
	ipf_identify,
#endif
	nulldev,
	ipf_attach,
	ipf_detach,
	nodev,		/* reset */
	&ipf_cb_ops,
	(struct bus_ops *)0
};

extern struct mod_ops mod_driverops;
static struct modldrv iplmod = {
	&mod_driverops, IPL_VERSION, &ipf_ops };
static struct modlinkage modlink1 = { MODREV_1, &iplmod, NULL };

#if SOLARIS2 >= 6
static	size_t	hdrsizes[57][2] = {
	{ 0, 0 },
	{ IFT_OTHER, 0 },
	{ IFT_1822, 0 },
	{ IFT_HDH1822, 0 },
	{ IFT_X25DDN, 0 },
	{ IFT_X25, 0 },
	{ IFT_ETHER, 14 },
	{ IFT_ISO88023, 0 },
	{ IFT_ISO88024, 0 },
	{ IFT_ISO88025, 0 },
	{ IFT_ISO88026, 0 },
	{ IFT_STARLAN, 0 },
	{ IFT_P10, 0 },
	{ IFT_P80, 0 },
	{ IFT_HY, 0 },
	{ IFT_FDDI, 24 },
	{ IFT_LAPB, 0 },
	{ IFT_SDLC, 0 },
	{ IFT_T1, 0 },
	{ IFT_CEPT, 0 },
	{ IFT_ISDNBASIC, 0 },
	{ IFT_ISDNPRIMARY, 0 },
	{ IFT_PTPSERIAL, 0 },
	{ IFT_PPP, 0 },
	{ IFT_LOOP, 0 },
	{ IFT_EON, 0 },
	{ IFT_XETHER, 0 },
	{ IFT_NSIP, 0 },
	{ IFT_SLIP, 0 },
	{ IFT_ULTRA, 0 },
	{ IFT_DS3, 0 },
	{ IFT_SIP, 0 },
	{ IFT_FRELAY, 0 },
	{ IFT_RS232, 0 },
	{ IFT_PARA, 0 },
	{ IFT_ARCNET, 0 },
	{ IFT_ARCNETPLUS, 0 },
	{ IFT_ATM, 0 },
	{ IFT_MIOX25, 0 },
	{ IFT_SONET, 0 },
	{ IFT_X25PLE, 0 },
	{ IFT_ISO88022LLC, 0 },
	{ IFT_LOCALTALK, 0 },
	{ IFT_SMDSDXI, 0 },
	{ IFT_FRELAYDCE, 0 },
	{ IFT_V35, 0 },
	{ IFT_HSSI, 0 },
	{ IFT_HIPPI, 0 },
	{ IFT_MODEM, 0 },
	{ IFT_AAL5, 0 },
	{ IFT_SONETPATH, 0 },
	{ IFT_SONETVT, 0 },
	{ IFT_SMDSICIP, 0 },
	{ IFT_PROPVIRTUAL, 0 },
	{ IFT_PROPMUX, 0 },
};
#endif /* SOLARIS2 >= 6 */

static dev_info_t *ipf_dev_info = NULL;

static const filter_kstats_t ipf_kstat_tmp = {
	{ "pass",			KSTAT_DATA_ULONG },
	{ "block",			KSTAT_DATA_ULONG },
	{ "nomatch",			KSTAT_DATA_ULONG },
	{ "short",			KSTAT_DATA_ULONG },
	{ "pass, logged",		KSTAT_DATA_ULONG },
	{ "block, logged",		KSTAT_DATA_ULONG },
	{ "nomatch, logged",		KSTAT_DATA_ULONG },
	{ "logged",			KSTAT_DATA_ULONG },
	{ "skip",			KSTAT_DATA_ULONG },
	{ "return sent",		KSTAT_DATA_ULONG },
	{ "acct",			KSTAT_DATA_ULONG },
	{ "bad frag state alloc",	KSTAT_DATA_ULONG },
	{ "new frag state kept",	KSTAT_DATA_ULONG },
	{ "new frag state compl. pkt",	KSTAT_DATA_ULONG },
	{ "bad pkt state alloc",	KSTAT_DATA_ULONG },
	{ "new pkt kept state",		KSTAT_DATA_ULONG },
	{ "cachehit",			KSTAT_DATA_ULONG },
	{ "tcp cksum bad",		KSTAT_DATA_ULONG },
	{{ "pullup ok",			KSTAT_DATA_ULONG },
	{ "pullup nok",			KSTAT_DATA_ULONG }},
	{ "src != route",		KSTAT_DATA_ULONG },
	{ "ttl invalid",		KSTAT_DATA_ULONG },
	{ "bad ip pkt",			KSTAT_DATA_ULONG },
	{ "ipv6 pkt",			KSTAT_DATA_ULONG },
	{ "dropped:pps ceiling",	KSTAT_DATA_ULONG },
	{ "ip upd. fail",		KSTAT_DATA_ULONG }
};

kstat_t		*ipf_kstatp[2] = {NULL, NULL};
static int	ipf_kstat_update(kstat_t *ksp, int rwflag);

static void
ipf_kstat_init(void)
{
	int 	i;

	for (i = 0; i < 2; i++) {
		ipf_kstatp[i] = kstat_create("ipf", 0,
			(i==0)?"inbound":"outbound",
			"net",
			KSTAT_TYPE_NAMED,
			sizeof (filter_kstats_t) / sizeof (kstat_named_t),
			0);
		if (ipf_kstatp[i] != NULL) {
			bcopy(&ipf_kstat_tmp, ipf_kstatp[i]->ks_data,
				sizeof (filter_kstats_t));
			ipf_kstatp[i]->ks_update = ipf_kstat_update;
			ipf_kstatp[i]->ks_private = &frstats[i];
			kstat_install(ipf_kstatp[i]);
		}
	}

#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: ipf_kstat_init() installed 0x%x, 0x%x",
		ipf_kstatp[0], ipf_kstatp[1]);
#endif
}

static void
ipf_kstat_fini(void)
{
	int i;
	for (i = 0; i < 2; i++) {
		if (ipf_kstatp[i] != NULL) {
			kstat_delete(ipf_kstatp[i]);
			ipf_kstatp[i] = NULL;
		}
	}
}

static int
ipf_kstat_update(kstat_t *ksp, int rwflag)
{
	filter_kstats_t	*fkp;
	filterstats_t	*fsp;

	if (rwflag == KSTAT_WRITE)
		return (EACCES);

	fkp = ksp->ks_data;
	fsp = ksp->ks_private;

	fkp->fks_pass.value.ul		= fsp->fr_pass;
	fkp->fks_block.value.ul		= fsp->fr_block;
	fkp->fks_nom.value.ul		= fsp->fr_nom;
	fkp->fks_short.value.ul		= fsp->fr_short;
	fkp->fks_ppkl.value.ul		= fsp->fr_ppkl;
	fkp->fks_bpkl.value.ul		= fsp->fr_bpkl;
	fkp->fks_npkl.value.ul		= fsp->fr_npkl;
	fkp->fks_pkl.value.ul		= fsp->fr_pkl;
	fkp->fks_skip.value.ul		= fsp->fr_skip;
	fkp->fks_ret.value.ul		= fsp->fr_ret;
	fkp->fks_acct.value.ul		= fsp->fr_acct;
	fkp->fks_bnfr.value.ul		= fsp->fr_bnfr;
	fkp->fks_nfr.value.ul		= fsp->fr_nfr;
	fkp->fks_cfr.value.ul		= fsp->fr_cfr;
	fkp->fks_bads.value.ul		= fsp->fr_bads;
	fkp->fks_ads.value.ul		= fsp->fr_ads;
	fkp->fks_chit.value.ul		= fsp->fr_chit;
	fkp->fks_tcpbad.value.ul 	= fsp->fr_tcpbad;
	fkp->fks_pull[0].value.ul 	= fsp->fr_pull[0];
	fkp->fks_pull[1].value.ul 	= fsp->fr_pull[1];
	fkp->fks_badsrc.value.ul 	= fsp->fr_badsrc;
	fkp->fks_badttl.value.ul 	= fsp->fr_badttl;
	fkp->fks_bad.value.ul		= fsp->fr_bad;
	fkp->fks_ipv6.value.ul		= fsp->fr_ipv6;
	fkp->fks_ppshit.value.ul 	= fsp->fr_ppshit;
	fkp->fks_ipud.value.ul		= fsp->fr_ipud;

	return (0);
}

int _init()
{
	int ipfinst;

	ipf_kstat_init();
	ipfinst = mod_install(&modlink1);
	if (ipfinst != 0)
		ipf_kstat_fini();
#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: _init() = %d", ipfinst);
#endif
	return ipfinst;
}


int _fini(void)
{
	int ipfinst;

	ipfinst = mod_remove(&modlink1);
#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: _fini() = %d", ipfinst);
#endif
	if (ipfinst == 0)
		ipf_kstat_fini();
	return ipfinst;
}


int _info(modinfop)
struct modinfo *modinfop;
{
	int ipfinst;

	ipfinst = mod_info(&modlink1, modinfop);
#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: _info(%x) = %x", modinfop, ipfinst);
#endif
	return ipfinst;
}


#if SOLARIS2 < 10
static int ipf_identify(dip)
dev_info_t *dip;
{
# ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: ipf_identify(%x)", dip);
# endif
	if (strcmp(ddi_get_name(dip), "ipf") == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}
#endif


static int ipf_attach(dip, cmd)
dev_info_t *dip;
ddi_attach_cmd_t cmd;
{
	char *s;
	int i;
	int instance;

#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: ipf_attach(%x,%x)", dip, cmd);
#endif

	if ((pfilinterface != PFIL_INTERFACE) || (PFIL_INTERFACE < 2000000)) {
		cmn_err(CE_NOTE, "pfilinterface(%d) != %d\n", pfilinterface,
			PFIL_INTERFACE);
		return EINVAL;
	}

	switch (cmd)
	{
	case DDI_ATTACH:
		instance = ddi_get_instance(dip);
		/* Only one instance of ipf (instance 0) can be attached. */
		if (instance > 0)
			return DDI_FAILURE;
		if (fr_running != 0)
			return DDI_FAILURE;

#ifdef	IPFDEBUG
		cmn_err(CE_NOTE, "IP Filter: attach ipf instance %d", instance);
#endif

		(void) ipf_property_update(dip);

		for (i = 0; ((s = ipf_devfiles[i]) != NULL); i++) {
			s = strrchr(s, '/');
			if (s == NULL)
				continue;
			s++;
			if (ddi_create_minor_node(dip, s, S_IFCHR, i,
						  DDI_PSEUDO, 0) ==
			    DDI_FAILURE) {
				ddi_remove_minor_node(dip, NULL);
				goto attach_failed;
			}
		}

		ipf_dev_info = dip;
		/*
		 * Initialize mutex's
		 */
		RWLOCK_INIT(&ipf_global, "ipf filter load/unload mutex");
		RWLOCK_INIT(&ipf_mutex, "ipf filter rwlock");
		RWLOCK_INIT(&ipf_frcache, "ipf cache rwlock");

		/*
		 * Lock people out while we set things up.
		 */
		WRITE_ENTER(&ipf_global);
		if ((fr_running != 0) || (iplattach() == -1)) {
			RWLOCK_EXIT(&ipf_global);
			goto attach_failed;
		}

		if (pfil_add_hook(fr_check, PFIL_IN|PFIL_OUT, &pfh_inet4))
			cmn_err(CE_WARN, "IP Filter: %s(pfh_inet4) failed",
				"pfil_add_hook");
#ifdef USE_INET6
		if (pfil_add_hook(fr_check, PFIL_IN|PFIL_OUT, &pfh_inet6))
			cmn_err(CE_WARN, "IP Filter: %s(pfh_inet6) failed",
				"pfil_add_hook");
#endif
		if (pfil_add_hook(fr_qifsync, PFIL_IN|PFIL_OUT, &pfh_sync))
			cmn_err(CE_WARN, "IP Filter: %s(pfh_sync) failed",
				"pfil_add_hook");

		fr_timer_id = timeout(fr_slowtimer, NULL,
				      drv_usectohz(500000));

		fr_running = 1;

		RWLOCK_EXIT(&ipf_global);

		cmn_err(CE_CONT, "!%s, running.\n", ipfilter_version);

		return DDI_SUCCESS;
		/* NOTREACHED */
	default:
		break;
	}

attach_failed:
#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: failed to attach\n");
#endif
	/*
	 * Use our own detach routine to toss
	 * away any stuff we allocated above.
	 */
	(void) ipf_detach(dip, DDI_DETACH);
	return DDI_FAILURE;
}


static int ipf_detach(dip, cmd)
dev_info_t *dip;
ddi_detach_cmd_t cmd;
{
	int i;

#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: ipf_detach(%x,%x)", dip, cmd);
#endif
	switch (cmd) {
	case DDI_DETACH:
		if (fr_refcnt != 0)
			return DDI_FAILURE;

		if (fr_running == -2 || fr_running == 0)
			break;
		/*
		 * Make sure we're the only one's modifying things.  With
		 * this lock others should just fall out of the loop.
		 */
		WRITE_ENTER(&ipf_global);
		if (fr_running <= 0) {
			RWLOCK_EXIT(&ipf_global);
			return DDI_FAILURE;
		}
		fr_running = -2;

		if (pfil_remove_hook(fr_check, PFIL_IN|PFIL_OUT, &pfh_inet4))
			cmn_err(CE_WARN, "IP Filter: %s(pfh_inet4) failed",
				"pfil_remove_hook");
#ifdef USE_INET6
		if (pfil_remove_hook(fr_check, PFIL_IN|PFIL_OUT, &pfh_inet6))
			cmn_err(CE_WARN, "IP Filter: %s(pfh_inet6) failed",
				"pfil_add_hook");
#endif
		if (pfil_remove_hook(fr_qifsync, PFIL_IN|PFIL_OUT, &pfh_sync))
			cmn_err(CE_WARN, "IP Filter: %s(pfh_sync) failed",
				"pfil_remove_hook");

		RWLOCK_EXIT(&ipf_global);

		if (fr_timer_id != 0) {
			(void) untimeout(fr_timer_id);
			fr_timer_id = 0;
		}

		/*
		 * Undo what we did in ipf_attach, freeing resources
		 * and removing things we installed.  The system
		 * framework guarantees we are not active with this devinfo
		 * node in any other entry points at this time.
		 */
		ddi_prop_remove_all(dip);
		i = ddi_get_instance(dip);
		ddi_remove_minor_node(dip, NULL);
		if (i > 0) {
			cmn_err(CE_CONT, "IP Filter: still attached (%d)\n", i);
			return DDI_FAILURE;
		}

		WRITE_ENTER(&ipf_global);
		if (!ipldetach()) {
			RWLOCK_EXIT(&ipf_global);
			RW_DESTROY(&ipf_mutex);
			RW_DESTROY(&ipf_frcache);
			RW_DESTROY(&ipf_global);
			cmn_err(CE_CONT, "!%s detached.\n", ipfilter_version);
			return (DDI_SUCCESS);
		}
		RWLOCK_EXIT(&ipf_global);
		break;
	default:
		break;
	}
	cmn_err(CE_NOTE, "IP Filter: failed to detach\n");
	return DDI_FAILURE;
}


/*ARGSUSED*/
static int ipf_getinfo(dip, infocmd, arg, result)
dev_info_t *dip;
ddi_info_cmd_t infocmd;
void *arg, **result;
{
	int error;

	if (fr_running <= 0)
		return DDI_FAILURE;
	error = DDI_FAILURE;
#ifdef	IPFDEBUG
	cmn_err(CE_NOTE, "IP Filter: ipf_getinfo(%x,%x,%x)", dip, infocmd, arg);
#endif
	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = ipf_dev_info;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		break;
	}
	return (error);
}


/*
 * look for bad consistancies between the list of interfaces the filter knows
 * about and those which are currently configured.
 */
/*ARGSUSED*/
static int fr_qifsync(ip, hlen, il, out, qif, mp)
ip_t *ip;
int hlen;
void *il;
int out;
void *qif;
mblk_t **mp;
{

	frsync(qif);
	/*
	 * Resync. any NAT `connections' using this interface and its IP #.
	 */
	fr_natsync(qif);
	fr_statesync(qif);
	return 0;
}


/*
 * look for bad consistancies between the list of interfaces the filter knows
 * about and those which are currently configured.
 */
int ipfsync()
{
	frsync(NULL);
	return 0;
}


/*
 * Fetch configuration file values that have been entered into the ipf.conf
 * driver file.
 */
static int ipf_property_update(dip)
dev_info_t *dip;
{
	ipftuneable_t *ipft;
	int64_t *i64p;
	char *name;
	u_int one;
	int *i32p;
	int err;

#ifdef DDI_NO_AUTODETACH
	if (ddi_prop_update_int(DDI_DEV_T_NONE, dip,
				DDI_NO_AUTODETACH, 1) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "!updating DDI_NO_AUTODETACH failed");
		return DDI_FAILURE;
	}
#else
	if (ddi_prop_update_int(DDI_DEV_T_NONE, dip,
				"ddi-no-autodetach", 1) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "!updating ddi-no-autodetach failed");
		return DDI_FAILURE;
	}
#endif

	err = DDI_SUCCESS;
	ipft = ipf_tuneables;
	for (ipft = ipf_tuneables; (name = ipft->ipft_name) != NULL; ipft++) {
		one = 1;
		switch (ipft->ipft_sz)
		{
		case 4 :
			i32p = NULL;
			err = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
							0, name, &i32p, &one);
			if (err == DDI_PROP_NOT_FOUND)
				continue;
#ifdef	IPFDEBUG
			cmn_err(CE_CONT, "IP Filter: lookup_int(%s) = %d\n",
				name, err);
#endif
			if (err != DDI_PROP_SUCCESS)
				return err;
			if (*i32p >= ipft->ipft_min && *i32p <= ipft->ipft_max)
				*ipft->ipft_pint = *i32p;
			else
				err = DDI_PROP_CANNOT_DECODE;
			ddi_prop_free(i32p);
			break;

#if SOLARIS2 > 8
		case 8 :
			i64p = NULL;
			err = ddi_prop_lookup_int64_array(DDI_DEV_T_ANY, dip,
							  0, name, &i64p, &one);
			if (err == DDI_PROP_NOT_FOUND)
				continue;
# ifdef	IPFDEBUG
			cmn_err(CE_CONT, "IP Filter: lookup_int64(%s) = %d\n",
				name, err);
# endif
			if (err != DDI_PROP_SUCCESS)
				return err;
			if (*i64p >= ipft->ipft_min && *i64p <= ipft->ipft_max)
				*ipft->ipft_pint = *i64p;
			else
				err = DDI_PROP_CANNOT_DECODE;
			ddi_prop_free(i64p);
			break;
#endif

		default :
			break;
		}
		if (err != DDI_SUCCESS)
			break;
	}

	return err;
}
