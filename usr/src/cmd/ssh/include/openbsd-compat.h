/* $Id: openbsd-compat.h,v 1.17 2002/09/12 00:33:02 djm Exp $ */

#ifndef	_OPENBSD_COMPAT_H
#define	_OPENBSD_COMPAT_H

#pragma ident	"@(#)openbsd-compat.h	1.4	03/11/19 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

/* OpenBSD function replacements */
#include "bindresvport.h"
#include "getcwd.h"
#include "realpath.h"
#include "rresvport.h"
#include "strlcpy.h"
#include "strlcat.h"
#include "strmode.h"
#include "mktemp.h"
#include "daemon.h"
#include "dirname.h"
#include "base64.h"
#include "sigact.h"
#include "inet_ntoa.h"
#include "inet_ntop.h"
#include "strsep.h"
#include "setproctitle.h"
#include "getgrouplist.h"
#include "glob.h"
#include "readpassphrase.h"
#include "getopt.h"

/* Home grown routines */
#include "bsd-arc4random.h"
#include "bsd-getpeereid.h"
#include "bsd-misc.h"
#include "bsd-snprintf.h"
#include "bsd-waitpid.h"

/* rfc2553 socket API replacements */
#include "fake-getaddrinfo.h"
#include "fake-getnameinfo.h"
#include "fake-socket.h"

/* Routines for a single OS platform */
#include "bsd-cray.h"
#include "port-irix.h"
#include "port-aix.h"

#ifdef __cplusplus
}
#endif

#endif /* _OPENBSD_COMPAT_H */
