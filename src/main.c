 /*
  * Pidgin SendScreenshot third-party plugin.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any laterr version.
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
  * Comments are very welcomed !
  *
  * --  Raoul Berger <contact@raoulito.info>
  *
  */

#include "main.h"

#include "prefs.h"
#include "menus.h"
#include "screencap.h"

#ifdef ENABLE_UPLOAD
#include "http_upload.h"	/* CLEAR_HOST_PARAM_DATA_FULL() */
#endif

GtkWidget *
get_receiver_window (PurplePlugin * plugin)
{
  if (PLUGIN (pconv))
    return pidgin_conv_get_window (PLUGIN (pconv))->window;
  else
    return NULL;
}

GtkIMHtml *
get_receiver_imhtml (PidginConversation * conv)
{
  if (conv)
    {
      return GTK_IMHTML (((GtkIMHtmlToolbar *) (conv->toolbar))->imhtml);
    }
  else
    return NULL;
}

void
plugin_stop (PurplePlugin * plugin)
{
  GList *convs;

  g_assert (PLUGIN (running) == TRUE);

  CLEAR_SEND_INFO_TO_NULL (plugin);
  PLUGIN (running) = FALSE;

  /* reactivate menuitems */
  convs = purple_get_conversations ();
  while (convs != NULL)
    {
      PurpleConversation *conv = (PurpleConversation *) convs->data;
      if (PIDGIN_IS_PIDGIN_CONVERSATION (conv))
	{
	  PidginConversation *gtkconv;
	  PidginWindow *win;
	  GtkWidget *screenshot_insert_menuitem;
	  GtkWidget *screenshot_menuitem, *conversation_menu;

	  gtkconv = PIDGIN_CONVERSATION (conv);

	  win = pidgin_conv_get_window (gtkconv);

	  conversation_menu =
	    gtk_item_factory_get_widget (win->menu.item_factory,
					 N_("/Conversation"));

	  if ((screenshot_insert_menuitem =
	       g_object_get_data (G_OBJECT (gtkconv->toolbar),
				  "screenshot_insert_menuitem")) != NULL)
	    gtk_widget_set_sensitive (screenshot_insert_menuitem, TRUE);

	  if ((screenshot_menuitem =
	       g_object_get_data (G_OBJECT (conversation_menu),
				  "screenshot_menuitem")) != NULL)
	    gtk_widget_set_sensitive (screenshot_menuitem, TRUE);

	}
      convs = g_list_next (convs);
    }
}

static gboolean
plugin_load (PurplePlugin * plugin)
{
  plugin->extra = NULL;

  if ((plugin->extra = g_try_malloc0 (sizeof (PluginExtraVars))) == NULL
#ifdef ENABLE_UPLOAD
      || (((PluginExtraVars *) plugin->extra)->host_data =
	  g_try_malloc0 (sizeof (struct host_param_data))) == NULL
#endif
    )
    {
      NotifyError (PLUGIN_LOAD_DATA_ERROR, (gulong) sizeof (PluginExtraVars));

      if (plugin->extra != NULL)
	g_free (plugin->extra);
      return FALSE;
    }
  else
    {
#ifdef ENABLE_UPLOAD
      PLUGIN (xml_hosts_filename) =
	g_build_filename (PLUGIN_DATADIR, "pidgin_screenshot_data",
			  "img_hosting_providers.xml", NULL);
      curl_global_init (CURL_GLOBAL_ALL);
#endif
      prepare_root_window (plugin);

      /* add menuitems each time a conversation is opened */
      purple_signal_connect (pidgin_conversations_get_handle (),
			     "conversation-switched",
			     plugin,
			     PURPLE_CALLBACK (create_plugin_menuitems), NULL);
      /* to add us to the buddy list context menu (and "plus" menu) */
      purple_signal_connect (purple_blist_get_handle (),
			     "blist-node-extended-menu", plugin,
			     PURPLE_CALLBACK (buddy_context_menu_add_item),
			     plugin);

      /* add menuitems to existing conversations */
      purple_conversation_foreach (create_plugin_menuitems);
      return TRUE;
    }
}

static gboolean
plugin_unload (PurplePlugin * plugin)
{
#ifdef ENABLE_UPLOAD
  struct host_param_data *host_data;
  guint timeout_handle;

  /* 
   * Make sure that the upload thread won't read nor write the structs we are
   * going to free...
   *
   * see upload ()
   */
  G_LOCK (unload);

  host_data = PLUGIN (host_data);
  timeout_handle = PLUGIN (timeout_cb_handle);
  if (timeout_handle)
    purple_timeout_remove (timeout_handle);
  if (PLUGIN (uploading_dialog) != NULL)
    {
      gtk_widget_destroy (PLUGIN (uploading_dialog));
      PLUGIN (uploading_dialog) = NULL;
    }
#endif

  /* remove menuitems */
  purple_conversation_foreach (remove_pidgin_menuitems);

  if (PLUGIN (root_window) != NULL)
    {
      gtk_widget_destroy (PLUGIN (root_window));
      PLUGIN (root_window) = NULL;
    }
  if (PLUGIN (root_events) != NULL)
    {
      gtk_widget_destroy (PLUGIN (root_events));
      PLUGIN (root_events) = NULL;
    }
  if (PLUGIN (gc) != NULL)
    {
      g_object_unref (PLUGIN (gc));
      PLUGIN (gc) = NULL;
    }
  if (PLUGIN (capture_path_filename) != NULL)
    {
      g_free (PLUGIN (capture_path_filename));
      PLUGIN (capture_path_filename = NULL);
    }
  
#ifdef ENABLE_UPLOAD
  CLEAR_HOST_PARAM_DATA_FULL (host_data);
  g_free (PLUGIN (xml_hosts_filename));
  g_free (PLUGIN (host_data));
#endif

  g_free (plugin->extra);
  plugin->extra = NULL;

#if defined ENABLE_UPLOAD
  curl_global_cleanup ();
  G_UNLOCK (unload);
#endif
  return TRUE;
}

static PidginPluginUiInfo ui_info = {
  get_plugin_pref_frame,
  0,				/* reserved */
  NULL,
  NULL,
  NULL,
  NULL
};

static PurplePluginInfo info = {
  PURPLE_PLUGIN_MAGIC,
  PURPLE_MAJOR_VERSION,
  PURPLE_MINOR_VERSION,
  PURPLE_PLUGIN_STANDARD,
  PIDGIN_PLUGIN_TYPE,
  0,
  NULL,
  PURPLE_PRIORITY_DEFAULT,

  /* some elements need translation,
     initialize them in init_plugin() */
  PLUGIN_ID,
  NULL,				/* PLUGIN_NAME */
  PACKAGE_VERSION,
  NULL,				/* PLUGIN_SUMMARY */
  NULL,				/* PLUGIN_DESCRIPTION */
  PLUGIN_AUTHOR,
  PLUGIN_WEBSITE,

  plugin_load,
  plugin_unload,
  NULL,

  &ui_info,
  NULL,
  NULL,

  NULL,

  /* reserved */
  NULL,
  NULL,
  NULL,
  NULL
};

static void
init_plugin (PurplePlugin * plugin)
{
#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
  purple_prefs_add_none (PREF_PREFIX);

  /*
   * default values, if node already created, nothing is done 
   */
  purple_prefs_add_int (PREF_JPEG_QUALITY, PREF_JPEG_QUALITY_DEFAULT);
  purple_prefs_add_int (PREF_PNG_COMPRESSION, PREF_PNG_COMPRESSION_DEFAULT);
  purple_prefs_add_string (PREF_IMAGE_TYPE, "jpeg");
  purple_prefs_add_int (PREF_HIGHLIGHT_MODE, 2);

#ifdef ENABLE_UPLOAD
  purple_prefs_add_string (PREF_UPLOAD_TO, HOST_DISABLED);
  purple_prefs_add_int (PREF_WAIT_BEFORE_SCREENSHOT, 0);
  purple_prefs_add_string (PREF_FTP_REMOTE_URL, "ftp://");
  purple_prefs_add_string (PREF_FTP_WEB_ADDR, "");
  purple_prefs_add_int (PREF_UPLOAD_TIMEOUT, 30);
  purple_prefs_add_int (PREF_UPLOAD_CONNECTTIMEOUT, 10);
#endif

  purple_prefs_add_bool (PREF_ASK_FILENAME, FALSE);
  purple_prefs_add_bool (PREF_SHOW_VISUAL_CUES, TRUE);

  purple_prefs_add_string (PREF_STORE_FOLDER, g_get_tmp_dir ());

  purple_prefs_add_bool (PREF_ADD_SIGNATURE, FALSE);
  purple_prefs_add_string (PREF_SIGNATURE_FILENAME, "");

  /* clean up old options... */
  purple_prefs_remove (PREF_PREFIX "/highlight-all");
  purple_prefs_remove (PREF_PREFIX "/upload-activity_timeout");

  info.name = PLUGIN_NAME;
  info.summary = PLUGIN_SUMMARY;
  info.description = PLUGIN_DESCRIPTION;

  (void) plugin;
}

PURPLE_INIT_PLUGIN (screenshot, init_plugin, info)
/* end of main.c */
