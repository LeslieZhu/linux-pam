/* pam_keyinit.c: Initialise the session keyring on login through a PAM module
 *
 * Copyright (C) 2006 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include "config.h"
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include <sys/syscall.h>

#define KEY_SPEC_SESSION_KEYRING	-3 /* ID for session keyring */
#define KEY_SPEC_USER_KEYRING		-4 /* ID for UID-specific keyring */
#define KEY_SPEC_USER_SESSION_KEYRING	-5 /* - key ID for UID-session keyring */

#define KEYCTL_GET_KEYRING_ID		0 /* ask for a keyring's ID */
#define KEYCTL_JOIN_SESSION_KEYRING	1 /* start named session keyring */
#define KEYCTL_REVOKE			3 /* revoke a key */
#define KEYCTL_LINK			8 /* link a key into a keyring */

static int my_session_keyring;
static int do_revoke;
static int xdebug = 1;

static void debug(pam_handle_t *pamh, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

static void debug(pam_handle_t *pamh, const char *fmt, ...)
{
	va_list va;

	if (xdebug) {
		va_start(va, fmt);
		pam_vsyslog(pamh, LOG_DEBUG, fmt, va);
		va_end(va);
	}
}

static int error(pam_handle_t *pamh, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

static int error(pam_handle_t *pamh, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	pam_vsyslog(pamh, LOG_ERR, fmt, va);
	va_end(va);

	return PAM_SESSION_ERR;
}

/*
 * initialise the session keyring for this process
 */
static int init_keyrings(pam_handle_t *pamh, int force)
{
	int session, usession, ret;

	if (!force) {
		/* get the IDs of the session keyring and the user session
		 * keyring */
		session = syscall(__NR_keyctl,
				  KEYCTL_GET_KEYRING_ID,
				  KEY_SPEC_SESSION_KEYRING,
				  0);
		debug(pamh, "GET SESSION = %d", session);
		if (session < 0) {
			/* don't worry about keyrings if facility not
			 * installed */
			if (errno == ENOSYS)
				return PAM_SUCCESS;
			return PAM_SESSION_ERR;
		}

		usession = syscall(__NR_keyctl,
				   KEYCTL_GET_KEYRING_ID,
				   KEY_SPEC_USER_SESSION_KEYRING,
				   0);
		debug(pamh, "GET SESSION = %d", usession);
		if (usession < 0)
			return PAM_SESSION_ERR;

		/* if the user session keyring is our keyring, then we don't
		 * need to do anything if we're not forcing */
		if (session != usession) {
			do_revoke = 0;
			return PAM_SUCCESS;
		}
	}

	/* create a session keyring, discarding the old one */
	ret = syscall(__NR_keyctl,
		      KEYCTL_JOIN_SESSION_KEYRING,
		      NULL);
	debug(pamh, "JOIN = %d", ret);
	if (ret < 0)
		return PAM_SESSION_ERR;

	my_session_keyring = ret;

	/* make a link from the session keyring to the user keyring */
	ret = syscall(__NR_keyctl,
		      KEYCTL_LINK,
		      KEY_SPEC_USER_KEYRING,
		      KEY_SPEC_SESSION_KEYRING);

	return ret < 0 ? PAM_SESSION_ERR : PAM_SUCCESS;
}

/*
 * revoke the session keyring for this process
 */
static void kill_keyrings(pam_handle_t *pamh)
{
	/* revoke the session keyring we created earlier */
	if (my_session_keyring > 0) {
		debug(pamh, "REVOKE %d", my_session_keyring);

		syscall(__NR_keyctl,
			KEYCTL_REVOKE,
			my_session_keyring);
	}
}

/*
 * open a PAM session by making sure there's a session keyring
 */
PAM_EXTERN
int pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED,
			int argc, const char **argv)
{
	struct passwd *pw;
	const char *username;
	int ret, old_uid, uid, old_gid, gid, loop, force = 0;

	for (loop = 0; loop < argc; loop++) {
		if (strcmp(argv[loop], "force") == 0)
			force = 1;
		else if (strcmp(argv[loop], "debug") == 0)
			xdebug = 1;
		else if (strcmp(argv[loop], "revoke") == 0)
			do_revoke = 1;
	}

	/* look up the target UID */
	ret = pam_get_user(pamh, &username, "key user");
	if (ret != PAM_SUCCESS)
		return ret;

	pw = pam_modutil_getpwnam(pamh, username);
	if (!pw) {
		error(pamh, "Unable to look up user \"%s\"\n", username);
		return PAM_USER_UNKNOWN;
	}

	uid = pw->pw_uid;
	old_uid = getuid();
	gid = pw->pw_gid;
	old_gid = getgid();
	debug(pamh, "UID:%d [%d]  GID:%d [%d]", uid, old_uid, gid, old_gid);

	/* switch to the real UID and GID so that the keyring ends up owned by
	 * the right user */
	if (uid != old_uid && setreuid(uid, -1) < 0)
		return error(pamh, "Unable to change UID to %d temporarily\n", uid);

	if (gid != old_gid && setregid(gid, -1) < 0) {
		error(pamh, "Unable to change GID to %d temporarily\n", gid);
		setreuid(old_uid, -1);
		return PAM_SESSION_ERR;
	}

	ret = init_keyrings(pamh, force);

	/* return to the orignal UID and GID (probably root) */
	if (uid != old_uid && setreuid(old_uid, -1) < 0)
		ret = error(pamh, "Unable to change UID back to %d\n", old_uid);

	if (gid != old_gid && setregid(old_gid, -1) < 0)
		ret = error(pamh, "Unable to change GID back to %d\n", old_gid);

	return ret;
}

/*
 * close a PAM session by revoking the session keyring if requested
 */
PAM_EXTERN
int pam_sm_close_session(pam_handle_t *pamh, int flags UNUSED,
			 int argc UNUSED, const char **argv UNUSED)
{
	if (do_revoke)
		kill_keyrings(pamh);

	return PAM_SUCCESS;
}
