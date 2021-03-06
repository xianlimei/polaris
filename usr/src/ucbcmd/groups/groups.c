/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#pragma ident	"@(#)groups.c	1.4	05/08/04 SMI"

/*
 * groups
 */

#include <sys/types.h>
#include <sys/param.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static void showgroups(char *user);

int
main(int argc, char *argv[])
{
	int ngroups, i;
	char *sep = "";
	struct group *gr;
	gid_t groups[NGROUPS_UMAX];

	if (argc > 1) {
		for (i = 1; i < argc; i++)
			showgroups(argv[i]);
		exit(0);
	}

	ngroups = getgroups(NGROUPS_UMAX, groups);
	if (getpwuid(getuid()) == NULL) {
		(void) fprintf(stderr, "groups: could not find passwd entry\n");
		exit(1);
	}

	for (i = 0; i < ngroups; i++) {
		gr = getgrgid(groups[i]);
		if (gr == NULL) {
			(void) printf("%s%ld", sep, groups[i]);
			sep = " ";
			continue;
		}
		(void) printf("%s%s", sep, gr->gr_name);
		sep = " ";
	}

	(void) printf("\n");
	return (0);
}

static void
showgroups(char *user)
{
	struct group *gr;
	struct passwd *pw;
	char **cp;
	char *sep = "";
	int pwgid_printed = 0;

	if ((pw = getpwnam(user)) == NULL) {
		(void) fprintf(stderr, "groups: %s : No such user\n", user);
		return;
	}
	setgrent();
	(void) printf("%s : ", user);
	while (gr = getgrent()) {
		if (pw->pw_gid == gr->gr_gid) {
			/*
			 * To avoid duplicate group entries
			 */
			if (pwgid_printed == 0) {
			    (void) printf("%s%s", sep, gr->gr_name);
			    sep = " ";
			    pwgid_printed = 1;
			}
			continue;
		}
		for (cp = gr->gr_mem; cp && *cp; cp++)
			if (strcmp(*cp, user) == 0) {
				(void) printf("%s%s", sep, gr->gr_name);
				sep = " ";
				break;
			}
	}
	(void) printf("\n");
	endgrent();
}
