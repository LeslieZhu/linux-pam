#
# Copyright (c) 2008, 2009 Thorsten Kukuk <kukuk@suse.de>
# Copyright (c) 2013 Red Hat, Inc.
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS) README

EXTRA_DIST = $(XMLS)

if HAVE_DOC
dist_man_MANS = pam_pwhistory.8 pwhistory_helper.8 pwhistory.conf.5
endif
XMLS = README.xml pam_pwhistory.8.xml pwhistory_helper.8.xml \
  pwhistory.conf.5.xml
dist_check_SCRIPTS = tst-pam_pwhistory
TESTS = $(dist_check_SCRIPTS)

securelibdir = $(SECUREDIR)
secureconfdir = $(SCONFIGDIR)

AM_CFLAGS = -I$(top_srcdir)/libpam/include -I$(top_srcdir)/libpamc/include \
	$(WARN_CFLAGS) -DPWHISTORY_HELPER=\"$(sbindir)/pwhistory_helper\"

pam_pwhistory_la_LDFLAGS = -no-undefined -avoid-version -module
if HAVE_VERSIONING
  pam_pwhistory_la_LDFLAGS += -Wl,--version-script=$(srcdir)/../modules.map
endif

noinst_HEADERS = opasswd.h pwhistory_config.h

dist_secureconf_DATA = pwhistory.conf

securelib_LTLIBRARIES = pam_pwhistory.la
pam_pwhistory_la_CFLAGS = $(AM_CFLAGS)
pam_pwhistory_la_LIBADD = $(top_builddir)/libpam/libpam.la @LIBCRYPT@ @LIBSELINUX@
pam_pwhistory_la_SOURCES = pam_pwhistory.c opasswd.c pwhistory_config.c

sbin_PROGRAMS = pwhistory_helper
pwhistory_helper_CFLAGS = $(AM_CFLAGS) -DHELPER_COMPILE=\"pwhistory_helper\" @EXE_CFLAGS@
pwhistory_helper_SOURCES = pwhistory_helper.c opasswd.c
pwhistory_helper_LDFLAGS = @EXE_LDFLAGS@
pwhistory_helper_LDADD = $(top_builddir)/libpam/libpam.la @LIBCRYPT@

if ENABLE_REGENERATE_MAN
dist_noinst_DATA = README
-include $(top_srcdir)/Make.xml.rules
endif
