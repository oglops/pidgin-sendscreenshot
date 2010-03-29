 /*
  * Pidgin SendScreenshot plugin - common upload utilities -
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

#include "upload_utils.h"


/*
 * Insert the html link we just fetched from server.
 */
void
real_insert_link (PurplePlugin * plugin, const gchar * url)
{
  GtkIMHtml *imhtml = get_receiver_imhtml (PLUGIN (pconv));

  if (imhtml == NULL)
    {
      NotifyError (PLUGIN_UPLOAD_CLOSED_CONV_ERROR, url);
    }
  else
    {
      GtkTextMark *mark =
	gtk_text_buffer_get_insert
	(gtk_text_view_get_buffer (GTK_TEXT_VIEW (imhtml)));
      
      g_assert (mark != NULL);
      
      if (PLUGIN (conv_features) & PURPLE_CONNECTION_HTML)
	{
	  gtk_imhtml_insert_link (imhtml, mark, url, 
				  g_path_get_basename (PLUGIN (capture_path_filename)));
	}
      else
	{
	  GtkTextIter iter;

	  gtk_text_buffer_get_iter_at_mark
	    (gtk_text_view_get_buffer (GTK_TEXT_VIEW (imhtml)), &iter, mark);

	  gtk_text_buffer_insert (gtk_text_view_get_buffer
				  (GTK_TEXT_VIEW (imhtml)), &iter,
				  url, (gint) strlen (url));
	}
    }
}


/*
 * Let the user know that we are uploading...
 */
GtkWidget *
show_uploading_dialog (PurplePlugin * plugin, const gchar * str)
{
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *img;
  GtkWidget *hbox;
  GtkWidget *progress_bar;
  GtkWidget *gtkconv_window;
  GtkWidget *blist_window;
  gchar *send_msg = NULL;

  progress_bar = gtk_progress_bar_new ();	/* FIXME */
  img =
    gtk_image_new_from_stock (PIDGIN_STOCK_UPLOAD,
			      gtk_icon_size_from_name
			      (PIDGIN_ICON_SIZE_TANGO_MEDIUM));
  hbox = gtk_hbox_new (FALSE, PIDGIN_HIG_BOX_SPACE);

  gtkconv_window = get_receiver_window (plugin);
  blist_window = pidgin_blist_get_default_gtk_blist()->window;

  dialog = gtk_dialog_new_with_buttons (PLUGIN_NAME,
					GTK_WINDOW ((gtkconv_window) ?
						    gtkconv_window : blist_window),
					GTK_DIALOG_NO_SEPARATOR, NULL);

  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (progress_bar), 0.05);

  g_object_set_data (G_OBJECT (dialog), "progress-bar", progress_bar);

  send_msg = g_strdup_printf (PLUGIN_SENDING_INFO, str);


  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress_bar), send_msg);

  g_free (send_msg);
  send_msg = NULL;

#if GTK_CHECK_VERSION (2,14,0)
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
#else
  content_area = (GTK_DIALOG (dialog))->vbox;
#endif

  gtk_window_set_decorated (GTK_WINDOW (dialog), FALSE);
  gtk_box_pack_start (GTK_BOX (content_area), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), img, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), progress_bar, FALSE, FALSE, 0);

  gtk_widget_show_all (dialog);
  return dialog;
}

/* end of upload_utils.c */
