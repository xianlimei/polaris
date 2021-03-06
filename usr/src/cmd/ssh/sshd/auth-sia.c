/*
 * Copyright (c) 2002 Chris Adams.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#ifdef HAVE_OSF_SIA
#include "ssh.h"
#include "auth.h"
#include "auth-sia.h"
#include "log.h"
#include "servconf.h"
#include "canohost.h"

#include <sia.h>
#include <siad.h>
#include <pwd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/resource.h>
#include <unistd.h>
#include <string.h>

extern ServerOptions options;
extern int saved_argc;
extern char **saved_argv;

extern int errno;

int
auth_sia_password(Authctxt *authctxt, char *pass)
{
	int ret;
	SIAENTITY *ent = NULL;
	const char *host;
	char *user = authctxt->user;

	host = get_canonical_hostname(options.verify_reverse_mapping);

	if (!user || !pass || pass[0] == '\0')
		return(0);

	if (sia_ses_init(&ent, saved_argc, saved_argv, host, user, NULL, 0,
	    NULL) != SIASUCCESS)
		return(0);

	if ((ret = sia_ses_authent(NULL, pass, ent)) != SIASUCCESS) {
		error("Couldn't authenticate %s from %s", user, host);
		if (ret & SIASTOP)
			sia_ses_release(&ent);
		return(0);
	}

	sia_ses_release(&ent);

	return(1);
}

void
session_setup_sia(char *user, char *tty)
{
	struct passwd *pw;
	SIAENTITY *ent = NULL;
	const char *host;

	host = get_canonical_hostname (options.verify_reverse_mapping);

	if (sia_ses_init(&ent, saved_argc, saved_argv, host, user, tty, 0,
	    NULL) != SIASUCCESS) {
		fatal("sia_ses_init failed");
	}

	if ((pw = getpwnam(user)) == NULL) {
		sia_ses_release(&ent);
		fatal("getpwnam: no user: %s", user);
	}
	if (sia_make_entity_pwd(pw, ent) != SIASUCCESS) {
		sia_ses_release(&ent);
		fatal("sia_make_entity_pwd failed");
	}

	ent->authtype = SIA_A_NONE;
	if (sia_ses_estab(sia_collect_trm, ent) != SIASUCCESS) {
		fatal("Couldn't establish session for %s from %s", user,
		    host);
	}

	if (setpriority(PRIO_PROCESS, 0, 0) == -1) {
		sia_ses_release(&ent);
		fatal("setpriority: %s", strerror (errno));
	}

	if (sia_ses_launch(sia_collect_trm, ent) != SIASUCCESS) {
		fatal("Couldn't launch session for %s from %s", user, host);
	}
	
	sia_ses_release(&ent);

	if (setreuid(geteuid(), geteuid()) < 0) {
		fatal("setreuid: %s", strerror(errno));
	}
}

#endif /* HAVE_OSF_SIA */

#pragma ident	"@(#)auth-sia.c	1.1	03/09/04 SMI"
