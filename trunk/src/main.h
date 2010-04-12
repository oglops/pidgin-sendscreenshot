/*
 * Pidgin SendScreenshot plugin - header -
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 *
 * --  Raoul Berger <contact@raoulito.info>
 *
 *
 */

#ifndef __SCREENSHOT_H__
#define __SCREENSHOT_H__

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif

#ifdef DISABLE_LIBCURL
#undef HAVE_LIBCURL
#endif

#ifndef PURPLE_PLUGINS
#define PURPLE_PLUGINS
#endif
#include <gtkmenutray.h>
#include <glib/gi18n-lib.h>
#include <plugin.h>
#include <debug.h>
#include <pidginstock.h>
#include <version.h>
#include <gtkplugin.h>
#include <gtkconv.h>
#include <gtkimhtmltoolbar.h>
#include <string.h>		/* strrchr() */
#include <gdk/gdkkeysyms.h>	/* GDK_Escape, GDK_Down */
#include <gtkutils.h>
#include <gtkblist.h>
#include <glib/gstdio.h>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#endif

#ifdef G_OS_WIN32
#include <win32dep.h>
#define PLUGIN_DATADIR wpurple_install_dir()
#endif

#ifndef G_MARKUP_ERROR_MISSING_ATTRIBUTE
#define G_MARKUP_ERROR_MISSING_ATTRIBUTE G_MARKUP_ERROR_PARSE
#endif

#define gettext_noop(String) String

/* various infos about this plugin */
#define PLUGIN_ID "gtk-rberger-screenshot"

#define PLUGIN_NAME	_("Send Screenshot")
#define PLUGIN_SUMMARY	_("Capture and send a screenshot.")
#define PLUGIN_DESCRIPTION _("This plugin will capture a screenshot given "\
			     "a rectangular area.")

#define PLUGIN_AUTHOR	"Raoul Berger <"PACKAGE_BUGREPORT">"
#define PLUGIN_WEBSITE 	"http://code.google.com/p/pidgin-sendscreenshot"

G_LOCK_EXTERN (unload);

/* error reporting strings */
#define PLUGIN_ERROR _("Error")

#define PLUGIN_UPLOAD_DISCLOC_ERROR _("Location of your screenshot on disk:")

#define PLUGIN_LOAD_DATA_ERROR _("Cannot allocate enough memory (%lu bytes) to load plugin data !")
#define PLUGIN_MEMORY_ERROR _("Failed to allocate enough memory to create the screenshot."\
			      " You will need to quit some applications and retry.")
#define PLUGIN_SAVE_TO_FILE_ERROR _("Failed to save your screenshot to '%s'.")

#define PLUGIN_GET_DATA_ERROR _("Failed to get '%s' data.")
#define PLUGIN_STORE_FAILED_ERROR _("Failed to insert your screenshot in the text area.")

#define PLUGIN_SIGNATURE_TOOBIG_ERROR _("The image used to sign the screenshot is too big.\n"\
					"%dx%d is the maximum allowed.")

/* see on_screenshot_insert_menuitem_activate_cb () */
#define MSEC_TIMEOUT_VAL 500

/* darken / lighten */
#define PIXEL_VAL 85

#define HOST_DISABLED _("No selection")
#define PIDGIN_HOST_TOS _("Terms Of Service")

#define PIXBUF_HOSTS_SIZE 32

#define JPG 0
#define PNG 1

#define SIGN_MAXWIDTH 128
#define SIGN_MAXHEIGHT 32

#define SCREENSHOT_INSERT_MENUITEM_LABEL _("_Screenshot")
#define SCREENSHOT_MENUITEM_LABEL _("Insert _Screenshot...")
#define SCREENSHOT_SEND_MENUITEM_LABEL _("Send a _Screenshot...")

#define CAPTURE _("capture")


#define SEND_AS_IMAGE_TXT _("as an _Image")

#define DLGBOX_CAPNAME_TITLE _("Set capture name")
#define DLGBOX_CAPNAME_LABEL _("Capture name:")
#define DLGBOX_CAPNAME_WARNEXISTS _("File already exists!")

/* convenient? macros */
#define PLUGIN_EXTRA(plugin)\
  ((PluginExtraVars*)(plugin->extra))

#define PLUGIN(what)\
  ((PluginExtraVars*)(plugin->extra))->what


#define NotifyError(strval,arg...)				\
  {								\
  gchar *strmsg;						\
  strmsg = g_strdup_printf(strval, arg);			\
    purple_notify_error(plugin, PLUGIN_NAME, PLUGIN_ERROR, strmsg);	\
    g_free(strmsg);							\
  }

#define CLEAR_SEND_INFO_TO_NULL(plugin)\
  PLUGIN (account) = NULL;\
  PLUGIN (pconv) = NULL;\
  if (PLUGIN(name)) {\
    g_free (PLUGIN(name));\
    PLUGIN (name) = NULL;\
  }

/*
 * If we immediately freeze the screen, then the menuitem we just
 * click on may remain. That's we wait for a small timeout.
 */
#define FREEZE_DESKTOP()\
  purple_timeout_add\
  (MAX(MSEC_TIMEOUT_VAL, purple_prefs_get_int(PREF_WAIT_BEFORE_SCREENSHOT) * 1000), \
   (GSourceFunc) timeout_freeze_screen, plugin)

#define REMEMBER_ACCOUNT(conv)\
  PLUGIN (account) = (conv->active_conv)->account;\
  PLUGIN (name) = g_strdup_printf ("%s", (conv->active_conv)->name);\
  PLUGIN (pconv) = conv


#ifdef ENABLE_UPLOAD
typedef enum
{ SEND_AS_FILE, SEND_AS_IMAGE, SEND_AS_HTTP_LINK, SEND_AS_FTP_LINK } SendType;
#else
typedef enum
{ SEND_AS_FILE, SEND_AS_IMAGE } SendType;
#endif

typedef enum
{ SELECT_REGULAR, SELECT_CENTER_HOLD} SelectionMode;

/* functions */

GtkWidget *get_receiver_window (PurplePlugin * plugin);
GtkIMHtml *get_receiver_imhtml (PidginConversation * gtkconv);
void plugin_stop (PurplePlugin * plugin);

/* main struct holding data */
typedef struct
{
  /* prevent two instances of SndScreenshot to run
     simultenaously */
  gboolean running;

  SendType send_as;

  /* to display frozen desktop state */
  GdkGC *gc;
  GtkWidget *root_window;
  GtkWidget *root_events;

  /* original image */
  GdkPixbuf *root_pixbuf_orig;
  /* modified image (highlight mode) */
  GdkPixbuf *root_pixbuf_x;
  GdkRegion *border_new, *border_old, *new, *old;
 

  /* where to send capture ? */
  PurpleConnectionFlags conv_features;
  PurpleAccount *account;
  gchar *name;
  PidginConversation *pconv;

  /* conv window is minimized */
  gboolean iconified;

  /* capture area */
  gint x1, y1, x2, y2, _x, _y;
  SelectionMode select_mode;

  /* screenshot's location */
  gchar *capture_path_filename;

#ifdef ENABLE_UPLOAD
  GtkWidget *uploading_dialog;
  GThread *libcurl_thread;
  gchar *xml_hosts_filename;
  guint timeout_cb_handle;

  off_t read_size;
  off_t total_size;

  /* host data from xml */
  struct host_param_data
  {
    gchar *xml_hosts_version;
    gchar *selected_hostname;
    gchar *form_action;
    gchar *file_input_name;
    gchar *regexp;

    gchar *htmlcode;

    GArray *host_names;
    GArray *extra_names;
    GArray *extra_values;

    gboolean is_inside;
    gboolean quit_handlers;

    /* needed by conf dialog */
  } *host_data;
#endif

} PluginExtraVars;
#endif

/* end of main.h */
