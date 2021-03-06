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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SPAWN_H
#define	_SPAWN_H

#pragma ident	"@(#)spawn.h	1.2	05/06/08 SMI"

#include <sys/feature_tests.h>
#include <sys/types.h>
#include <signal.h>
#include <sched.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	POSIX_SPAWN_RESETIDS		0x01
#define	POSIX_SPAWN_SETPGROUP		0x02
#define	POSIX_SPAWN_SETSIGDEF		0x04
#define	POSIX_SPAWN_SETSIGMASK		0x08
#define	POSIX_SPAWN_SETSCHEDPARAM	0x10
#define	POSIX_SPAWN_SETSCHEDULER	0x20

typedef struct {
	void *__spawn_attrp;	/* implementation-private */
} posix_spawnattr_t;

typedef struct {
	void *__file_attrp;	/* implementation-private */
} posix_spawn_file_actions_t;

#if defined(__STDC__)

extern int posix_spawn(
	pid_t *_RESTRICT_KYWD pid,
	const char *_RESTRICT_KYWD path,
	const posix_spawn_file_actions_t *file_actions,
	const posix_spawnattr_t *_RESTRICT_KYWD attrp,
	char *const argv[_RESTRICT_KYWD],
	char *const envp[_RESTRICT_KYWD]);

extern int posix_spawnp(
	pid_t *_RESTRICT_KYWD pid,
	const char *_RESTRICT_KYWD file,
	const posix_spawn_file_actions_t *file_actions,
	const posix_spawnattr_t *_RESTRICT_KYWD attrp,
	char *const argv[_RESTRICT_KYWD],
	char *const envp[_RESTRICT_KYWD]);

extern int posix_spawn_file_actions_init(
	posix_spawn_file_actions_t *file_actions);

extern int posix_spawn_file_actions_destroy(
	posix_spawn_file_actions_t *file_actions);

extern int posix_spawn_file_actions_addopen(
	posix_spawn_file_actions_t *_RESTRICT_KYWD file_actions,
	int filedes,
	const char *_RESTRICT_KYWD path,
	int oflag,
	mode_t mode);

extern int posix_spawn_file_actions_addclose(
	posix_spawn_file_actions_t *file_actions,
	int filedes);

extern int posix_spawn_file_actions_adddup2(
	posix_spawn_file_actions_t *file_actions,
	int filedes,
	int newfiledes);

extern int posix_spawnattr_init(
	posix_spawnattr_t *attr);

extern int posix_spawnattr_destroy(
	posix_spawnattr_t *attr);

extern int posix_spawnattr_setflags(
	posix_spawnattr_t *attr,
	short flags);

extern int posix_spawnattr_getflags(
	const posix_spawnattr_t *_RESTRICT_KYWD attr,
	short *_RESTRICT_KYWD flags);

extern int posix_spawnattr_setpgroup(
	posix_spawnattr_t *attr,
	pid_t pgroup);

extern int posix_spawnattr_getpgroup(
	const posix_spawnattr_t *_RESTRICT_KYWD attr,
	pid_t *_RESTRICT_KYWD pgroup);

extern int posix_spawnattr_setschedparam(
	posix_spawnattr_t *_RESTRICT_KYWD attr,
	const struct sched_param *_RESTRICT_KYWD schedparam);

extern int posix_spawnattr_getschedparam(
	const posix_spawnattr_t *_RESTRICT_KYWD attr,
	struct sched_param *_RESTRICT_KYWD schedparam);

extern int posix_spawnattr_setschedpolicy(
	posix_spawnattr_t *attr,
	int schedpolicy);

extern int posix_spawnattr_getschedpolicy(
	const posix_spawnattr_t *_RESTRICT_KYWD attr,
	int *_RESTRICT_KYWD schedpolicy);

extern int posix_spawnattr_setsigdefault(
	posix_spawnattr_t *_RESTRICT_KYWD attr,
	const sigset_t *_RESTRICT_KYWD sigdefault);

extern int posix_spawnattr_getsigdefault(
	const posix_spawnattr_t *_RESTRICT_KYWD attr,
	sigset_t *_RESTRICT_KYWD sigdefault);

extern int posix_spawnattr_setsigmask(
	posix_spawnattr_t *_RESTRICT_KYWD attr,
	const sigset_t *_RESTRICT_KYWD sigmask);

extern int posix_spawnattr_getsigmask(
	const posix_spawnattr_t *_RESTRICT_KYWD attr,
	sigset_t *_RESTRICT_KYWD sigmask);

#else	/* __STDC__ */

extern int posix_spawn();
extern int posix_spawnp();
extern int posix_spawn_file_actions_init();
extern int posix_spawn_file_actions_destroy();
extern int posix_spawn_file_actions_addopen();
extern int posix_spawn_file_actions_addclose();
extern int posix_spawn_file_actions_adddup2();
extern int posix_spawnattr_init();
extern int posix_spawnattr_destroy();
extern int posix_spawnattr_setflags();
extern int posix_spawnattr_getflags();
extern int posix_spawnattr_setpgroup();
extern int posix_spawnattr_getpgroup();
extern int posix_spawnattr_setschedparam();
extern int posix_spawnattr_getschedparam();
extern int posix_spawnattr_setschedpolicy();
extern int posix_spawnattr_getschedpolicy();
extern int posix_spawnattr_setsigdefault();
extern int posix_spawnattr_getsigdefault();
extern int posix_spawnattr_setsigmask();
extern int posix_spawnattr_getsigmask();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SPAWN_H */
