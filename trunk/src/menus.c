 /*
  * Pidgin SendScreenshot third-party plugin - menus and menuitems-
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
  */

#include "menus.h"
#include "prefs.h"
#include "screencap.h"

#ifdef ENABLE_UPLOAD
#include "upload_utils.h"
#include "http_upload.h"
#include "ftp_upload.h"

static void
on_screenshot_insert_as_link_aux (PidginWindow * win,
				  PidginConversation * _gtkconv)
{

  PurplePlugin *plugin;
  PidginConversation *gtkconv;

  plugin = purple_plugins_find_with_id (PLUGIN_ID);

  TRY_SET_RUNNING_ON (plugin);

  PLUGIN (send_as) = SEND_AS_HTTP_LINK;

  if (win != NULL)
    gtkconv =
      PIDGIN_CONVERSATION (pidgin_conv_window_get_active_conversation (win));
  else
    gtkconv = _gtkconv;

  if (!strcmp (purple_prefs_get_string (PREF_UPLOAD_TO), HOST_DISABLED))
    {
      purple_notify_error (plugin, PLUGIN_NAME, PLUGIN_ERROR,
			   PLUGIN_HOST_DISABLED_ERROR);
      PLUGIN (running) = FALSE;
      return;
    }
  REMEMBER_ACCOUNT (gtkconv);

  PLUGIN (conv_features) = gtkconv->active_conv->features;
  FREEZE_DESKTOP ();
}

static void
on_screenshot_insert_as_link_fromwin_activate_cb (PidginWindow * win)
{
  on_screenshot_insert_as_link_aux (win, NULL);
}


static void
on_screenshot_insert_as_link_activate_cb (PidginConversation * gtkconv)
{
  on_screenshot_insert_as_link_aux (NULL, gtkconv);
}


static void
on_screenshot_insert_as_link_show_cb (GtkWidget * as_link,
				      PidginConversation * gtkconv)
{
  PurpleConversation *conv = gtkconv->active_conv;

  /*
   * Depending on which protocol the conv is associated with,
   * html is supported or not...
   */
#if GTK_CHECK_VERSION(2,16,0)
  if (purple_conversation_get_features (conv) & PURPLE_CONNECTION_NO_IMAGES)
    gtk_menu_item_set_label (GTK_MENU_ITEM (as_link), SEND_AS_HTML_LINK_TXT);
  else
    gtk_menu_item_set_label (GTK_MENU_ITEM (as_link), SEND_AS_HTML_URL_TXT);
#else
  if (purple_conversation_get_features (conv) & PURPLE_CONNECTION_NO_IMAGES)
    gtk_label_set_label (GTK_LABEL (GTK_BIN (as_link)->child),
			 SEND_AS_HTML_LINK_TXT);
  else
    gtk_label_set_label (GTK_LABEL (GTK_BIN (as_link)->child),
			 SEND_AS_HTML_URL_TXT);
#endif
}


static void
on_screenshot_insert_as_ftp_link_aux (PidginWindow * win,
				      PidginConversation * _gtkconv)
{
  PurplePlugin *plugin;
  PidginConversation *gtkconv;


  plugin = purple_plugins_find_with_id (PLUGIN_ID);

  TRY_SET_RUNNING_ON (plugin);

  if (win != NULL)
    gtkconv =
      PIDGIN_CONVERSATION (pidgin_conv_window_get_active_conversation (win));
  else
    gtkconv = _gtkconv;

  REMEMBER_ACCOUNT (gtkconv);

  PLUGIN (send_as) = SEND_AS_FTP_LINK;

  PLUGIN (conv_features) = gtkconv->active_conv->features;
  FREEZE_DESKTOP ();
}

static void
on_screenshot_insert_as_ftp_link_fromwin_activate_cb (PidginWindow * win)
{
  on_screenshot_insert_as_ftp_link_aux (win, NULL);
}


static void
on_screenshot_insert_as_ftp_link_activate_cb (PidginConversation * gtkconv)
{
  on_screenshot_insert_as_ftp_link_aux (NULL, gtkconv);
}

static void
on_screenshot_insert_as_ftp_link_show_cb (GtkWidget *
					  as_link,
					  PidginConversation * gtkconv)
{
  PurpleConversation *conv = gtkconv->active_conv;

  /*
   * Depending on which protocol the conv is associated with,
   * html is supported or not...
   */
#if GTK_CHECK_VERSION(2,16,0)
  if (purple_conversation_get_features (conv) & PURPLE_CONNECTION_NO_IMAGES)
    gtk_menu_item_set_label (GTK_MENU_ITEM (as_link), SEND_AS_FTP_LINK_TXT);
  else
    gtk_menu_item_set_label (GTK_MENU_ITEM (as_link), SEND_AS_FTP_URL_TXT);
#else
  if (purple_conversation_get_features (conv) & PURPLE_CONNECTION_NO_IMAGES)
    gtk_label_set_label (GTK_LABEL (GTK_BIN (as_link)->child),
			 SEND_AS_FTP_LINK_TXT);
  else
    gtk_label_set_label (GTK_LABEL (GTK_BIN (as_link)->child),
			 SEND_AS_FTP_URL_TXT);
#endif
}
#endif /* ENABLE_UPLOAD */

static void
on_screenshot_insert_as_image_aux (PidginWindow * win,
				   PidginConversation * _gtkconv)
{
  PurplePlugin *plugin;
  PidginConversation *gtkconv;

  plugin = purple_plugins_find_with_id (PLUGIN_ID);

  TRY_SET_RUNNING_ON (plugin);

  PLUGIN (send_as) = SEND_AS_IMAGE;

  if (win != NULL)
    gtkconv =
      PIDGIN_CONVERSATION (pidgin_conv_window_get_active_conversation (win));
  else
    gtkconv = _gtkconv;


  REMEMBER_ACCOUNT (gtkconv);

  PLUGIN (conv_features) = gtkconv->active_conv->features;
  FREEZE_DESKTOP ();
}

static void
on_screenshot_insert_as_image_fromwin_activate_cb (PidginWindow * win)
{
  on_screenshot_insert_as_image_aux (win, NULL);
}

static void
on_screenshot_insert_as_image_activate_cb (PidginConversation * gtkconv)
{
  on_screenshot_insert_as_image_aux (NULL, gtkconv);
}

static void
on_screenshot_insert_as_image_show_cb (GtkWidget *
				       as_image, PidginConversation * gtkconv)
{
  PurpleConversation *conv = gtkconv->active_conv;

  /*
   * Depending on which protocol the conv is associated with, direct
   * image send is allowed or not. So we gray out our menu or not.
   */
  gtk_widget_set_sensitive (as_image,
			    !(purple_conversation_get_features (conv) &
			      PURPLE_CONNECTION_NO_IMAGES));
}

/**
 * Handle hiding and showing stuff based on what type of conv this is...
 */
static void
on_conversation_menu_show_cb (PidginWindow * win)
{
  PurpleConversation *conv;
  GtkWidget *conversation_menu, *img_menuitem;
#ifdef ENABLE_UPLOAD
  GtkWidget *link_menuitem, *ftp_link_menuitem;
#endif


  conv = pidgin_conv_window_get_active_conversation (win);
  conversation_menu =
    gtk_item_factory_get_widget (win->menu.item_factory, N_("/Conversation"));
#ifdef ENABLE_UPLOAD
  link_menuitem =
    g_object_get_data (G_OBJECT (conversation_menu), "link_menuitem");
  ftp_link_menuitem =
    g_object_get_data (G_OBJECT (conversation_menu), "ftp_link_menuitem");
  on_screenshot_insert_as_link_show_cb (link_menuitem,
					PIDGIN_CONVERSATION (conv));
  on_screenshot_insert_as_ftp_link_show_cb (ftp_link_menuitem,
					    PIDGIN_CONVERSATION (conv));
#endif

  img_menuitem =
    g_object_get_data (G_OBJECT (conversation_menu), "img_menuitem");
  on_screenshot_insert_as_image_show_cb (img_menuitem,
					 PIDGIN_CONVERSATION (conv));
}


/**
   Create a submenu with send as Image, Link (Http) and Link (Ftp) menuitems.
 */
GtkWidget *
create_plugin_submenu (PidginConversation * gtkconv, gboolean multiconv)
{
  GtkWidget *submenu;
  GtkWidget *as_image, *as_link, *as_ftp_link;

  submenu = gtk_menu_new ();

  as_image = gtk_menu_item_new_with_mnemonic (SEND_AS_IMAGE_TXT);	/* FIXME */

#ifdef ENABLE_UPLOAD
  as_link = gtk_menu_item_new_with_mnemonic (SEND_AS_HTML_LINK_TXT);	/* FIXME */
  as_ftp_link = gtk_menu_item_new_with_mnemonic (SEND_AS_FTP_LINK_TXT);	/* FIXME */
  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_ftp_link, 0);	/* FIXME */
  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_link, 1);	/* FIXME */
  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_image, 2);	/* FIXME */
#else
  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_image, 0);
#endif

  if (multiconv)
    {

      PidginWindow *win = pidgin_conv_get_window (gtkconv);
      GtkWidget *conversation_menu =
	gtk_item_factory_get_widget (win->menu.item_factory,
				     N_("/Conversation"));

      g_signal_connect_swapped (G_OBJECT (conversation_menu), "show",
				G_CALLBACK (on_conversation_menu_show_cb),
				win);


      g_signal_connect_swapped (G_OBJECT (as_image),
				"activate",
				G_CALLBACK
				(on_screenshot_insert_as_image_fromwin_activate_cb),
				win);

      g_signal_connect_swapped (G_OBJECT (as_link), "activate",
				G_CALLBACK
				(on_screenshot_insert_as_link_fromwin_activate_cb),
				win);

      g_signal_connect_swapped (G_OBJECT (as_ftp_link), "activate",
				G_CALLBACK
				(on_screenshot_insert_as_ftp_link_fromwin_activate_cb),
				win);


      g_object_set_data (G_OBJECT (conversation_menu),
			 "img_menuitem", as_image);
#ifdef ENABLE_UPLOAD
      g_object_set_data (G_OBJECT (conversation_menu),
			 "link_menuitem", as_link);
      g_object_set_data (G_OBJECT (conversation_menu),
			 "ftp_link_menuitem", as_ftp_link);
#endif
    }
  else
    {
      g_signal_connect (G_OBJECT (as_image), "show",
			G_CALLBACK
			(on_screenshot_insert_as_image_show_cb), gtkconv);

#ifdef ENABLE_UPLOAD
      g_signal_connect (G_OBJECT (as_link), "show",
			G_CALLBACK
			(on_screenshot_insert_as_link_show_cb), gtkconv);

      g_signal_connect_swapped (G_OBJECT (as_image),
				"activate",
				G_CALLBACK
				(on_screenshot_insert_as_image_activate_cb),
				gtkconv);

      g_signal_connect_swapped (G_OBJECT (as_link), "activate",
				G_CALLBACK
				(on_screenshot_insert_as_link_activate_cb),
				gtkconv);
      g_signal_connect_swapped (G_OBJECT (as_ftp_link), "activate",
				G_CALLBACK
				(on_screenshot_insert_as_ftp_link_activate_cb),
				gtkconv);

      g_signal_connect (G_OBJECT (as_ftp_link), "show",
			G_CALLBACK
			(on_screenshot_insert_as_ftp_link_show_cb), gtkconv);
#endif
    }
  return submenu;
}

/* end of menus.c */
