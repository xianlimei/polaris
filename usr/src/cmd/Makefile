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
# ident	"@(#)Makefile	1.358	06/08/25 SMI"

include ../Makefile.master

#
#	Note that the commands 'agents', 'lp', 'perl', and 'man' are first in
#	the list, violating alphabetical order.  This is because they are very
#	long-running and should be given the most wall-clock time for a
#	parallel build.
#
#	Commands in the FIRST_SUBDIRS list are built before starting the build
#	of other commands.  Currently this includes only 'isaexec' and
#	'platexec'.  This is necessary because $(ROOT)/usr/lib/isaexec or
#	$(ROOT)/usr/lib/platexec must exist when some other commands are built
#	because their 'make install' creates a hard link to one of them.
#
#	Commands are listed one per line so that TeamWare can auto-merge most
#	changes.
#

FIRST_SUBDIRS=		\
	isaexec		\
	platexec

COMMON_SUBDIRS=		\
	agents		\
	availdevs	\
	lp		\
	perl		\
	man		\
	Adm		\
	abi		\
	adbgen		\
	acct		\
	acctadm		\
	arch		\
	asa		\
	audio		\
	auths		\
	autopush	\
	awk		\
	awk_xpg4	\
	backup		\
	banner		\
	bart		\
	basename	\
	bc		\
	bdiff		\
	bfs		\
	bnu		\
	boot		\
	busstat		\
	cal		\
	calendar	\
	captoinfo	\
	cat		\
	cdrw		\
	cfgadm		\
	checkeq		\
	checknr		\
	chgrp		\
	chmod		\
	chown		\
	chroot		\
	cksum		\
	clear		\
	clinfo		\
	cmd-crypto	\
	cmd-inet	\
	cmp		\
	col		\
	comm		\
	compress	\
	consadm		\
	coreadm		\
	cpio		\
	cpc		\
	cron		\
	crypt		\
	csh		\
	csplit		\
	ctrun		\
	ctstat		\
	ctwatch		\
	cut		\
	datadm		\
	date		\
	dc		\
	dd		\
	deroff		\
	des		\
	devfsadm	\
	syseventd	\
	devctl		\
	devinfo		\
	devmgmt		\
	dfs.cmds	\
	diff		\
	diff3		\
	diffmk		\
	dircmp		\
	dirname		\
	dis		\
	diskmgtd	\
	dispadmin	\
	dladm		\
	dmesg		\
	dtrace		\
	du		\
	dumpadm		\
	dumpcs		\
	echo		\
	ed		\
	eeprom		\
	egrep		\
	emul64ioctl	\
	enhance		\
	env		\
	eqn		\
	expand		\
	expr		\
	exstr		\
	factor		\
	false		\
	fdetach		\
	fdformat	\
	fdisk		\
	filesync	\
	fgrep		\
	file		\
	find		\
	fm		\
	fmli		\
	fmt		\
	fmthard		\
	fmtmsg		\
	fold		\
	format		\
	fs.d		\
	fstyp		\
	fuser		\
	gcore		\
	gencat		\
	geniconvtbl	\
	genmsg		\
	getconf		\
	getdevpolicy	\
	getent		\
	getfacl		\
	getmajor	\
	getopt		\
	gettext		\
	gettxt		\
	graph		\
	grep		\
	grep_xpg4	\
	groups		\
	grpck		\
	gss		\
	halt		\
	head		\
	hostid		\
	hostname	\
	id		\
	infocmp		\
	init		\
	initpkg		\
	install.d	\
	intrd		\
	intrstat	\
	ipcrm		\
	ipcs		\
	ipf		\
	isainfo		\
	isalist		\
	iscsi		\
	java		\
	join		\
	kbd		\
	keyserv		\
	killall		\
	krb5		\
	kstat		\
	last		\
	lastcomm	\
	ldap		\
	ldapcachemgr	\
	line		\
	link		\
	listen		\
	loadkeys	\
	locale		\
	lockstat	\
	locator		\
	lofiadm		\
	logadm		\
	logger		\
	login		\
	logins		\
	logname		\
	look		\
	ls		\
	lvm		\
	mach		\
	machid		\
	mail		\
	mailx		\
	makekey		\
	mdb		\
	mesg		\
	mkdir		\
	mkfifo		\
	mkfile		\
	mkmsgs		\
	mknod		\
	mkpwdict	\
	mktemp		\
	modload		\
	more		\
	msgfmt		\
	msgid		\
	mt		\
	mv		\
	mvdir		\
	netadm		\
	netfiles	\
	newform		\
	newgrp		\
	news		\
	newtask		\
	nice		\
	nispasswd	\
	nl		\
	nlsadmin	\
	nohup		\
	nsadmin		\
	nscd		\
	oamuser		\
	oawk		\
	openssl		\
	pack		\
	pagesize	\
	passmgmt	\
	passwd		\
	paste		\
	pathchk		\
	pbind		\
	pcidr		\
	pcmciad		\
	pfexec		\
	pgrep		\
	picl		\
	plimit		\
	pools		\
	power		\
	ppgsz		\
	pg		\
	plockstat	\
	pr		\
	prctl		\
	print		\
	priocntl	\
	profiles	\
	projadd		\
	projects	\
	prstat		\
	prtconf		\
	prtdiag		\
	prtvtoc		\
	ps		\
	psradm		\
	psrinfo		\
	psrset		\
	ptools		\
	pwck		\
	pwconv		\
	pwd		\
	raidctl		\
	ramdiskadm	\
	rcap		\
	rcm_daemon	\
	rctladm		\
	refer		\
	regcmp		\
	renice		\
	rexd		\
	rm		\
	rmdir		\
	rmformat	\
	rmt		\
	roles		\
	rpcbind		\
	rpcgen		\
	rpcinfo		\
	rpcsvc		\
	rpld		\
	runat		\
	sa		\
	saf		\
	savecore	\
	script		\
	sdiff		\
	sendmail	\
	setfacl		\
	setmnt		\
	setpgrp		\
	setuname	\
	sgs		\
	sh		\
	sleep		\
	smbios		\
	smserverd	\
	soelim		\
	sort		\
	spell		\
	spline		\
	split		\
	srchtxt		\
	ssh		\
	stat		\
	streams		\
	strings		\
	su		\
	sulogin		\
	sum		\
	sunpc		\
	svc		\
	swap		\
	sync		\
	sysdef		\
	syseventadm	\
	syslogd		\
	tabs		\
	tar		\
	tbl		\
	tcopy		\
	tcpd		\
	tee		\
	terminfo	\
	th_tools	\
	tic		\
	time		\
	tip		\
	tnf		\
	touch		\
	tplot		\
	tput		\
	trapstat	\
	troff		\
	true		\
	truss		\
	tty		\
	ttymon		\
	uadmin		\
	ul		\
	uname		\
	uniq		\
	units		\
	unlink		\
	unpack		\
	users		\
	utmp_update	\
	utmpd		\
	valtools	\
	vgrind		\
	vi		\
	volmgt		\
	w		\
	wall		\
	wbem		\
	wc		\
	which		\
	who		\
	whodo		\
	wracct		\
	write		\
	xntpd		\
	xargs		\
	xstr		\
	yes		\
	ypcmd		\
	yppasswd	\
	zdb		\
	zdump		\
	zfs		\
	zic		\
	zinject		\
	zlogin		\
	zoneadm		\
	zoneadmd	\
	zonecfg		\
	zonename	\
	zpool		\
	ztest

$(CLOSED_BUILD)COMMON_SUBDIRS += \
	$(CLOSED)/cmd/dodatadm	\
	$(CLOSED)/cmd/iconv	\
	$(CLOSED)/cmd/ksh	\
	$(CLOSED)/cmd/llc2	\
	$(CLOSED)/cmd/localedef	\
	$(CLOSED)/cmd/more_xpg4	\
	$(CLOSED)/cmd/mtst	\
	$(CLOSED)/cmd/od	\
	$(CLOSED)/cmd/patch	\
	$(CLOSED)/cmd/pax	\
	$(CLOSED)/cmd/pcitool	\
	$(CLOSED)/cmd/printf	\
	$(CLOSED)/cmd/sed	\
	$(CLOSED)/cmd/sed_xpg4	\
	$(CLOSED)/cmd/smartcard	\
	$(CLOSED)/cmd/tail	\
	$(CLOSED)/cmd/tr	\
	$(CLOSED)/cmd/tr_xpg4

i386_SUBDIRS=		\
	addbadsec	\
	biosdev		\
	diskscan	\
	OWconfig	\
	rtc

sparc_SUBDIRS=		\
	cvcd		\
	dcs		\
	drd		\
	fruadm		\
	oplhpd		\
	prtdscp		\
	prtfru		\
	scadm		\
	sckmd		\
	sf880drd	\
	stmsboot	\
	vntsd		\
	wrsmconf	\
	wrsmstat

#
# Commands that are messaged.  Note that 'lp' and 'man' come first
# (see previous comment about 'lp' and 'man').
#
MSGSUBDIRS=		\
	lp		\
	man		\
	abi		\
	acctadm		\
	allocate	\
	asa		\
	audio		\
	audit		\
	auditd		\
	auths		\
	autopush	\
	awk		\
	awk_xpg4	\
	backup		\
	banner		\
	bart		\
	basename	\
	bnu		\
	bsmconv		\
	bsmrecord	\
	bsmunconv	\
	busstat		\
	cal		\
	cat		\
	cdrw		\
	cfgadm		\
	checkeq		\
	checknr		\
	chgrp		\
	chmod		\
	chown		\
	cksum		\
	cmd-crypto	\
	cmd-inet	\
	cmp		\
	col		\
	comm		\
	compress	\
	consadm		\
	coreadm		\
	cpio		\
	cpc		\
	cron		\
	csh		\
	csplit		\
	ctrun		\
	ctstat		\
	ctwatch		\
	cut		\
	datadm		\
	date		\
	dc		\
	dcs		\
	dd		\
	deroff		\
	devfsadm	\
	diff		\
	diffmk		\
	dladm		\
	du		\
	dumpcs		\
	ed		\
	env		\
	eqn		\
	expand		\
	expr		\
	fgrep		\
	file		\
	filesync	\
	find		\
	fm		\
	fold		\
	fs.d		\
	geniconvtbl	\
	genmsg		\
	getconf		\
	getent		\
	gettext		\
	gettxt		\
	grep		\
	grep_xpg4	\
	grpck		\
	gss		\
	halt		\
	head		\
	hostname	\
	id		\
	isaexec		\
	iscsi		\
	join		\
	krb5		\
	kstat		\
	last		\
	ldap		\
	ldapcachemgr	\
	locale		\
	lofiadm		\
	logadm		\
	logger		\
	logins		\
	logname		\
	ls		\
	lvm		\
	mailx		\
	mesg		\
	mkdir		\
	mkpwdict	\
	mktemp		\
	more		\
	msgfmt		\
	mv		\
	newgrp		\
	newtask		\
	nice		\
	nispasswd	\
	nohup		\
	oawk		\
	pack		\
	passwd		\
	passmgmt	\
	paste		\
	pathchk		\
	pfexec		\
	pg		\
	pgrep		\
	picl            \
	pools		\
	power		\
	pr		\
	praudit		\
	print		\
	profiles	\
	projadd		\
	projects	\
	prstat		\
	prtdiag		\
	ps		\
	psrinfo		\
	pwconv		\
	pwd		\
	raidctl		\
	ramdiskadm	\
	rcap		\
	rcm_daemon	\
	refer		\
	regcmp		\
	renice		\
	roles		\
	rm		\
	rmdir		\
	rmformat	\
	scadm		\
	script		\
	sdiff		\
	sgs		\
	sh		\
	sleep		\
	sort		\
	split		\
	ssh		\
	stat		\
	strings		\
	su		\
	sum		\
	svc		\
	swap		\
	syseventadm	\
	syseventd	\
	tabs		\
	tar		\
	tbl		\
	tee		\
	time		\
	tnf		\
	touch		\
	tput		\
	troff		\
	tty		\
	ttymon		\
	ul		\
	uname		\
	uniq		\
	units		\
	unlink		\
	unpack		\
	valtools	\
	vgrind		\
	vi		\
	volmgt		\
	w		\
	wbem		\
	wc		\
	who		\
	whodo		\
	wracct		\
	write		\
	xargs		\
	yppasswd	\
	zdump		\
	zfs		\
	zic		\
	zlogin		\
	zoneadm		\
	zoneadmd	\
	zonecfg		\
	zonename	\
	zpool

$(CLOSED_BUILD)MSGSUBDIRS += \
	$(CLOSED)/cmd/iconv	\
	$(CLOSED)/cmd/ksh	\
	$(CLOSED)/cmd/llc2	\
	$(CLOSED)/cmd/localedef	\
	$(CLOSED)/cmd/more_xpg4	\
	$(CLOSED)/cmd/od	\
	$(CLOSED)/cmd/patch	\
	$(CLOSED)/cmd/pax	\
	$(CLOSED)/cmd/printf	\
	$(CLOSED)/cmd/sed	\
	$(CLOSED)/cmd/sed_xpg4	\
	$(CLOSED)/cmd/tail	\
	$(CLOSED)/cmd/tr	\
	$(CLOSED)/cmd/tr_xpg4

sparc_MSGSUBDIRS=	\
	fruadm		\
	prtdscp		\
	prtfru		\
	stmsboot	\
	vntsd		\
	wrsmconf	\
	wrsmstat

#
# commands that use dcgettext for localized time, LC_TIME
#
DCSUBDIRS=		\
	cal		\
	cfgadm		\
	diff		\
	ls		\
	pr		\
	tar		\
	w		\
	who		\
	whodo		\
	write

$(CLOSED_BUILD)DCSUBDIRS += \
	$(CLOSED)/cmd/pax

#
# commands that belong only to the basic security module
#
BSMSUBDIRS=		\
	allocate	\
	amt		\
	audit		\
	audit_warn	\
	auditconfig	\
	auditd		\
	auditreduce	\
	auditstat	\
	praudit		\
	bsmconv		\
	bsmrecord	\
	bsmunconv

#
# commands not owned by the systems group
#
BWOSDIRS=

#
# Commands providing manifests must offer a check target.  A recursive check
# target across all commands directories is not currently provided.
#
MANIFEST_TOPDIRS=			\
	auditd				\
	bnu				\
	consadm				\
	coreadm				\
	cron				\
	cvcd				\
	dispadmin			\
	drd				\
	dumpadm				\
	eeprom				\
	fm				\
	intrd				\
	ipf				\
	keyserv				\
	ldapcachemgr			\
	nscd				\
	oplhpd				\
	power				\
	rexd				\
	rpcbind				\
	rpcsvc				\
	saf				\
	sf880drd			\
	smserverd			\
	syseventd			\
	syslogd				\
	utmpd				\
	vntsd				\
	ypcmd				\
	zoneadmd

MANIFEST_SUBDIRS=			\
	boot/scripts			\
	cmd-crypto/scripts		\
	cmd-inet/usr.lib/in.chargend	\
	cmd-inet/usr.lib/in.daytimed	\
	cmd-inet/usr.lib/in.dhcpd	\
	cmd-inet/usr.lib/in.discardd	\
	cmd-inet/usr.lib/in.echod	\
	cmd-inet/usr.lib/in.timed	\
	cmd-inet/usr.lib/inetd		\
	cmd-inet/usr.lib/slpd		\
	cmd-inet/usr.sbin		\
	cmd-inet/usr.sbin/in.ftpd	\
	cmd-inet/usr.sbin/in.talkd	\
	dcs/sparc/sun4u			\
	fs.d/autofs			\
	fs.d/cachefs			\
	fs.d/nfs/svc			\
	gss/gssd			\
	krb5/kadmin/server		\
	krb5/krb5kdc			\
	krb5/kwarn			\
	krb5/slave			\
	lp/cmd/lpsched			\
	lvm/rpc.metad			\
	lvm/rpc.metamedd		\
	lvm/rpc.metamhd			\
	lvm/md_monitord			\
	lvm/metassist/sysfiles		\
	lvm/util			\
	picl/picld			\
	print/gateway			\
	print/lp			\
	rcap/rcapd			\
	rpcsvc/rpc.bootparamd		\
	sendmail/lib			\
	smartcard/daemon		\
	ssh/etc				\
	svc/milestone			\
	ypcmd/yppasswd			\
	ypcmd/ypupdated			\
	xntpd/xntpd

all :=		TARGET = all
install :=	TARGET = install
clean :=	TARGET = clean
clobber :=	TARGET = clobber
lint :=		TARGET = lint
check :=	TARGET = check
_msg :=		TARGET = _msg
_dc :=		TARGET = _dc

.KEEP_STATE:

SUBDIRS = $(COMMON_SUBDIRS) $($(MACH)_SUBDIRS)

.PARALLEL:	$(BWOSDIRS) $(SUBDIRS) $(MSGSUBDIRS) $(BSMSUBDIRS) \
		$(MANIFEST_SUBDIRS)

all install clean clobber lint: $(FIRST_SUBDIRS) .WAIT $(SUBDIRS) $(BSMSUBDIRS)

check: svccfg_check .WAIT $(MANIFEST_TOPDIRS) $(MANIFEST_SUBDIRS)

svccfg_check:
	@$(ECHO) "building requirements for svccfg check ..."; \
	(cd $(SRC)/cmd/svc/svccfg && pwd && $(MAKE) $(MFLAGS) native)

#
# The .WAIT directive works around an apparent bug in parallel make.
# Evidently make was getting the target _msg vs. _dc confused under
# some level of parallelization, causing some of the _dc objects
# not to be built.
#
_msg: $(MSGSUBDIRS) $($(MACH)_MSGSUBDIRS) .WAIT _dc

_dc: $(DCSUBDIRS)

#
# Dependencies
#
fs.d: fstyp

$(FIRST_SUBDIRS) $(BWOSDIRS) $(SUBDIRS) $(BSMSUBDIRS) $(MANIFEST_SUBDIRS): FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	else \
		true; \
	fi

FRC:
