#
# Makefile.am
#
# Some parts stolen from Guification plugin.
#

EXTRA_DIST = \
	prefs.h \
	screenshot.h \
	http_upload.h \
	ftp_upload.h \
	upload_utils.h

ardir = $(PIDGIN_LIBDIR)/pidgin

screenshot_la_LDFLAGS = -module -avoid-version


ar_LTLIBRARIES = screenshot.la



screenshot_la_SOURCES = \
	prefs.c \
	screenshot.c

if ENABLE_UPLOAD
screenshot_la_SOURCES += \
	upload_utils.c \
	http_upload.c \
	ftp_upload.c
endif

screenshot_la_LIBADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(PANGOFT2_LIBS) \
	$(PIDGIN_LIBS) \
	$(PURPLE_LIBS) \
	$(LIBCURL)

AM_CPPFLAGS = \
	-DPLUGIN_VERSION=\"$(VERSION)\" \
	-DPLUGIN_DATADIR=\"$(PIDGIN_DATADIR)\" \
	-DLOCALEDIR=\"$(PIDGIN_DATADIR)/locale\" \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(PIDGIN_CFLAGS) \
	$(PURPLE_CFLAGS) \
	$(LIBCURL_CPPFLAGS)

# end