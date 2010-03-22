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
#include "http_upload.h" /* CLEAR_HOST_PARAM_DATA_FULL() */
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

static void
create_screenshot_insert_menuitem_pidgin (PidginConversation * gtkconv)
{
  PidginWindow *win;
  GtkWidget *conversation_menu,
    *screenshot_insert_menuitem, *screenshot_menuitem;

  PurplePlugin *plugin;

  plugin = purple_plugins_find_with_id (PLUGIN_ID);

  win = pidgin_conv_get_window (gtkconv);

  conversation_menu =
    gtk_item_factory_get_widget (win->menu.item_factory, N_("/Conversation"));
  screenshot_insert_menuitem =
    g_object_get_data (G_OBJECT (gtkconv->toolbar),
		       "screenshot_insert_menuitem");
  screenshot_menuitem =
    g_object_get_data (G_OBJECT (conversation_menu), "screenshot_menuitem");


  /* Add us to the conv "Insert" menu */
  if (screenshot_insert_menuitem == NULL)
    {
      GtkWidget *insert_menu, *submenu;

      if ((insert_menu =
	   g_object_get_data (G_OBJECT (gtkconv->toolbar),
			      "insert_menu")) != NULL)
	{
	  /* add us to the "insert" list */
	  screenshot_insert_menuitem =
	    gtk_menu_item_new_with_mnemonic
	    (SCREENSHOT_INSERT_MENUITEM_LABEL);

	  submenu = create_plugin_submenu (gtkconv, FALSE);

	  gtk_menu_item_set_submenu (GTK_MENU_ITEM
				     (screenshot_insert_menuitem), submenu);

	  gtk_menu_shell_insert (GTK_MENU_SHELL (insert_menu),
				 screenshot_insert_menuitem, 1);

	  /* register new widget */
	  g_object_set_data (G_OBJECT (gtkconv->toolbar),
			     "screenshot_insert_menuitem",
			     screenshot_insert_menuitem);
	}
    }

  /* Add us to the conv "Conversation" menu. */
  if (screenshot_menuitem == NULL)
    {
      GList *children = NULL, *head_chld = NULL;	/* don't g_list_free() it */
      guint i = 0;
      GtkWidget *submenu = create_plugin_submenu (gtkconv, TRUE);

      screenshot_menuitem =
	gtk_menu_item_new_with_mnemonic (SCREENSHOT_MENUITEM_LABEL);

      gtk_menu_item_set_submenu (GTK_MENU_ITEM
				 (screenshot_menuitem), submenu);

      gtk_widget_show_all (submenu);

      children =
	gtk_container_get_children (GTK_CONTAINER (conversation_menu));
      head_chld = children;	/* keep first element addr */

      /* pack our menuitem at correct place */
      while (children != NULL && children->data !=
	     (gpointer) win->menu.insert_image)
	{
	  children = g_list_next (children);
	  i++;
	}
      g_list_free (head_chld);

      gtk_menu_shell_insert (GTK_MENU_SHELL (conversation_menu),
			     screenshot_menuitem, i + 1);
      gtk_widget_show (screenshot_menuitem);

      g_object_set_data (G_OBJECT (conversation_menu),
			 "screenshot_menuitem", screenshot_menuitem);
    }
}

static void
remove_screenshot_insert_menuitem_pidgin (PidginConversation * gtkconv)
{
  GtkWidget *screenshot_insert_menuitem, *screenshot_menuitem,
    *conversation_menu;
  PidginWindow *win;

  PurplePlugin *plugin;

  plugin = purple_plugins_find_with_id (PLUGIN_ID);


  win = pidgin_conv_get_window (gtkconv);
  if (win != NULL)
    {
      if ((conversation_menu =
	   gtk_item_factory_get_widget (win->menu.item_factory,
					N_("/Conversation"))) != NULL)
	{
	  if ((screenshot_menuitem =
	       g_object_get_data (G_OBJECT (conversation_menu),
				  "screenshot_menuitem")) != NULL)
	    {
	      gtk_widget_destroy (screenshot_menuitem);
	      g_object_steal_data (G_OBJECT (conversation_menu),
				   "screenshot_menuitem");
	      g_object_steal_data (G_OBJECT (conversation_menu),
				   "img_menuitem");
#ifdef ENABLE_UPLOAD
	      g_object_steal_data (G_OBJECT (conversation_menu),
				   "link_menuitem");
	      g_object_steal_data (G_OBJECT (conversation_menu),
				   "ftp_link_menuitem");
#endif
	    }
	}
    }
  screenshot_insert_menuitem =
    g_object_get_data (G_OBJECT (gtkconv->toolbar),
		       "screenshot_insert_menuitem");
  if (screenshot_insert_menuitem != NULL)
    {
      gtk_widget_destroy (screenshot_insert_menuitem);
      g_object_steal_data (G_OBJECT (gtkconv->toolbar),
			   "screenshot_insert_menuitem");
    }
}

static void
conversation_switched_cb (PurpleConversation * conv)
{
  PidginConversation *gtkconv;

  gtkconv = PIDGIN_CONVERSATION (conv);

  if (gtkconv != NULL)
    {
      create_screenshot_insert_menuitem_pidgin (gtkconv);
    }
}

static void
on_blist_context_menu_send_capture (PurpleBlistNode * node,
				    PurplePlugin * plugin)
{
  if (PURPLE_BLIST_NODE_IS_BUDDY (node))
    {

      PurpleBuddy *buddy = (PurpleBuddy *) node;
      /*  PurpleConversation *conv; */

      TRY_SET_RUNNING_ON (plugin);

      PLUGIN (send_as) = SEND_AS_FILE;

      PLUGIN (account) = purple_buddy_get_account (buddy);
      PLUGIN (name) = g_strdup_printf ("%s", purple_buddy_get_name (buddy));

      /* see on_screenshot_insert_as_image_activate_cb () */
      FREEZE_DESKTOP ();
    }
}

/* 
 *  90% stolen from autoaccept.c and gtkblist.c files.
 */
static void
add_to_blist_context_menu (PurpleBlistNode * node, GList ** menu,
			   gpointer plugin)
{
  PurplePluginProtocolInfo *prpl_info;

  if (PURPLE_BLIST_NODE_IS_BUDDY (node))
    {
      prpl_info =
	PURPLE_PLUGIN_PROTOCOL_INFO (((PurpleBuddy *) node)->account->gc->
				     prpl);

      if (prpl_info && prpl_info->send_file)
	{
	  if (!prpl_info->can_receive_file ||
	      prpl_info->can_receive_file (((PurpleBuddy *) node)->account->
					   gc, ((PurpleBuddy *) node)->name))
	    {
	      PurpleMenuAction *action;

	      if (!PURPLE_BLIST_NODE_IS_BUDDY (node)
		  && !PURPLE_BLIST_NODE_IS_CONTACT (node)
		  && !(purple_blist_node_get_flags (node) &
		       PURPLE_BLIST_NODE_FLAG_NO_SAVE))
		return;

	      action =
		purple_menu_action_new (SCREENSHOT_SEND_MENUITEM_LABEL,
					PURPLE_CALLBACK
					(on_blist_context_menu_send_capture),
					plugin, NULL);
	      (*menu) = g_list_prepend (*menu, action);	/* add */
	    }
	}
    }
}

static void
get_blist_window_cb (PurpleBuddyList * blist, PurplePlugin * plugin)
{
  PidginBuddyList *gtk_blist = PIDGIN_BLIST (blist);

  PLUGIN (blist_window) = gtk_blist->window;
  g_assert (PLUGIN (blist_window));
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
      GList *convs;

#ifdef ENABLE_UPLOAD
      PLUGIN (xml_hosts_filename) =
	g_build_filename (PLUGIN_DATADIR, "pidgin_screenshot_data",
			  "img_hosting_providers.xml", NULL);
      /* only once */
      curl_global_init (CURL_GLOBAL_ALL);
#endif
      convs = purple_get_conversations ();
      prepare_root_window (plugin);

      /* load us each time a conversation is opened */
      purple_signal_connect (pidgin_conversations_get_handle (),
			     "conversation-switched",
			     plugin,
			     PURPLE_CALLBACK (conversation_switched_cb),
			     NULL);
      /* to add us to the buddy list context menu (and "plus" menu) */
      purple_signal_connect (purple_blist_get_handle (),
			     "blist-node-extended-menu", plugin,
			     PURPLE_CALLBACK (add_to_blist_context_menu),
			     plugin);
      /* save pointer to the budddy list window when created */
      if (!
	  (PLUGIN (blist_window) =
	   PIDGIN_BLIST (purple_get_blist ())->window))
	{
	  purple_signal_connect (pidgin_blist_get_handle (),
				 "gtkblist-created", plugin,
				 PURPLE_CALLBACK (get_blist_window_cb),
				 plugin);
	}

      while (convs)
	{
	  PurpleConversation *conv = (PurpleConversation *) convs->data;

	  /* Setup Screenshot menuitem */
	  if (PIDGIN_IS_PIDGIN_CONVERSATION (conv))
	    {
	      create_screenshot_insert_menuitem_pidgin (PIDGIN_CONVERSATION
							(conv));
	    }
	  convs = g_list_next (convs);
	}
      return TRUE;
    }
}

static gboolean
plugin_unload (PurplePlugin * plugin)
{
  GList *convs;

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

  convs = purple_get_conversations ();
  while (convs)
    {
      PurpleConversation *conv = (PurpleConversation *) convs->data;

      /* Remove Screenshot menuitem */
      if (PIDGIN_IS_PIDGIN_CONVERSATION (conv))
	{
	  remove_screenshot_insert_menuitem_pidgin (PIDGIN_CONVERSATION
						    (conv));
	}
      convs = g_list_next (convs);
    }

  if (PLUGIN (root_window) != NULL)
    {
      gtk_widget_destroy (PLUGIN (root_window));
      PLUGIN (root_window) = NULL;
    }

  CLEAR_PLUGIN_EXTRA_GCHARS (plugin);

#ifdef ENABLE_UPLOAD

  CLEAR_HOST_PARAM_DATA_FULL (host_data);
  g_free (PLUGIN (xml_hosts_filename));
  g_free (PLUGIN (host_data));
  curl_global_cleanup ();

#endif

  g_free (plugin->extra);
  plugin->extra = NULL;

#if defined ENABLE_UPLOAD
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
  NULL, /* PLUGIN_NAME */
  PACKAGE_VERSION,
  NULL, /* PLUGIN_SUMMARY */
  NULL, /* PLUGIN_DESCRIPTION */
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
#ifdef HAVE_LIBCURL
  purple_prefs_add_int (PREF_UPLOAD_TIMEOUT, 30);
  purple_prefs_add_int (PREF_UPLOAD_CONNECTTIMEOUT, 10);
#else
  purple_prefs_add_int (PREF_UPLOAD_ACTIVITY_TIMEOUT, 60);
#endif
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
