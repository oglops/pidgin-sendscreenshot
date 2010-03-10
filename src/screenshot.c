 /*
  *
  * Pidgin SendScreenshot third-party plugin.
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

#include "screenshot.h"
#include "prefs.h"

#ifdef ENABLE_UPLOAD
#include "upload_utils.h"
#include "http_upload.h"
#include "ftp_upload.h"
#endif

GtkWidget *
get_receiver_window (PurplePlugin * plugin)
{
  if (PLUGIN (pconv))
    return pidgin_conv_get_window (PLUGIN(pconv))->window;
  else
    return NULL;
}

GtkIMHtml *
get_receiver_imhtml (PidginConversation * conv)
{
  if (conv)
    {
      return
	GTK_IMHTML (((GtkIMHtmlToolbar *)(conv)->
		     toolbar)->imhtml);
    }
  else
    return NULL;
}


/*
 * Get, copy and show current desktop image.
 * TODO : use mmx ?
 */
guint
timeout_freeze_screen (PurplePlugin * plugin)
{
  GdkWindow *gdkroot;
  gint width, height;

  gdkroot = gdk_get_default_root_window ();
  gdk_drawable_get_size (gdkroot, &width, &height);

  PLUGIN (root_image) = gdk_drawable_get_image (gdkroot, 0, 0, width, height);

  if (purple_prefs_get_int (PREF_HIGHLIGHT_MODE) < 3
      || purple_prefs_get_int (PREF_HIGHLIGHT_MODE) == 5)
    {
      guchar *pixels;
      gint rowstride, n_channels;

      if ((PLUGIN (root_pixbuf) =
	   gdk_pixbuf_get_from_image (PLUGIN (root_pixbuf),
				      PLUGIN (root_image),
				      NULL,
				      0, 0, 0, 0, width, height)) != NULL)
	{
	  pixels = gdk_pixbuf_get_pixels (PLUGIN (root_pixbuf));
	  n_channels = gdk_pixbuf_get_n_channels (PLUGIN (root_pixbuf));
	  rowstride = gdk_pixbuf_get_rowstride (PLUGIN (root_pixbuf));

	  if (purple_prefs_get_int (PREF_HIGHLIGHT_MODE) == 1)
	    {			/* lighten */
	      gint x, y;

	      for (x = 0; x < width; x++)
		for (y = 0; y < height; y++)
		  {
		    guchar *p;

		    p = pixels + y * rowstride + x * n_channels;

		    p[0] = (guchar) MIN (p[0] + PIXEL_VAL, 255);
		    p[1] = (guchar) MIN (p[1] + PIXEL_VAL, 255);
		    p[2] = (guchar) MIN (p[2] + PIXEL_VAL, 255);
		  }
	    }
	  else if (purple_prefs_get_int (PREF_HIGHLIGHT_MODE) == 2)
	    {			/* darken */
	      gint x, y;

	      for (x = 0; x < width; x++)
		for (y = 0; y < height; y++)
		  {
		    guchar *p;

		    p = pixels + y * rowstride + x * n_channels;
		    p[0] = (guchar) MAX (p[0] - PIXEL_VAL, 0);
		    p[1] = (guchar) MAX (p[1] - PIXEL_VAL, 0);
		    p[2] = (guchar) MAX (p[2] - PIXEL_VAL, 0);
		  }
	    }
	  else
	    {
	      gint x, y;

	      for (x = 0; x < width; x++)
		for (y = 0; y < height; y++)
		  {
		    guchar *p, val;

		    p = pixels + y * rowstride + x * n_channels;
		    val = p[0] * 0.299 + p[1] * 0.587 + p[2] * 0.114;
		    p[0] = val;
		    p[1] = val;
		    p[2] = val;
		  }
	    }
	}
      else
	{
	  purple_notify_error (plugin, PLUGIN_NAME, PLUGIN_ERROR,
			       PLUGIN_MEMORY_ERROR);
	  if (PLUGIN (root_image))
	    {
	      g_object_unref (PLUGIN (root_image));
	      PLUGIN (root_image) = NULL;
	    }
	  return 0;
	}
    }
  gtk_widget_show (PLUGIN (root_window));
  return 0;
}

static void
on_root_window_realize_cb (GtkWidget * root_window)
{
  GdkWindow *gdkwin;
  GdkCursor *cursor = gdk_cursor_new (GDK_CROSSHAIR);

#if GTK_CHECK_VERSION(2,14,0)
  gdkwin = gtk_widget_get_window (root_window);
#else
  gdkwin = root_window->window;
#endif
  /* be sensitive to user interaction  */
  gdk_window_set_events (gdkwin, GDK_EXPOSURE_MASK |
			 GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  gdk_window_set_cursor (gdkwin, cursor);
  gdk_window_set_back_pixmap (gdkwin, NULL, FALSE);
  gdk_cursor_unref (cursor);
}

static gboolean
on_root_window_button_press_cb (GtkWidget * root_window,
				GdkEventButton * event, PurplePlugin * plugin)
{
  if (event->button == 1)
    {				/* start defining the capture area */
      PLUGIN (x1) = (gint) event->x;
      PLUGIN (y1) = (gint) event->y;
      PLUGIN (x2) = (gint) event->x;
      PLUGIN (y2) = (gint) event->y;

      PLUGIN (_x) = PLUGIN (x2);
      PLUGIN (_y) = PLUGIN (y2);

      /* delete visual cues */
      if (purple_prefs_get_bool (PREF_SHOW_VISUAL_CUES))
	draw_hv_lines (root_window, NULL, plugin);

    }
  else if (event->button == 2 &&	/* hide the current conversation window  */
	   get_receiver_window (plugin) && !PLUGIN (iconified))
    {
      gtk_widget_hide (root_window);

      if (PLUGIN (root_image))
	{
	  g_object_unref (PLUGIN (root_image));
	  PLUGIN (root_image) = NULL;
	}
      if (PLUGIN (root_pixbuf))
	{
	  g_object_unref (PLUGIN (root_pixbuf));
	  PLUGIN (root_pixbuf) = NULL;
	}

      gtk_window_iconify (GTK_WINDOW (get_receiver_window (plugin)));

      PLUGIN (iconified) = TRUE;
      FREEZE_DESKTOP ();
    }
  else if (event->button == 3)
    {
      if (event->type == GDK_2BUTTON_PRESS)
	{
	  /* cancel whole screenshot process and come back to Pidgin */
	  gtk_widget_hide (root_window);
	  CLEAR_CAPTURE_AREA (plugin);
	  if (PLUGIN (iconified) == TRUE)
	    {
	      gtk_window_deiconify (GTK_WINDOW
				    (get_receiver_window (plugin)));
	      PLUGIN (iconified) = FALSE;
	    }
	  CLEAR_SEND_INFO_TO_NULL (plugin);
	  PLUGIN (running) = FALSE;
	}
      else if (PLUGIN (x1) != -1)
	{
	  GdkRectangle area;

	  /* cancel current selection to try a new one */
	  area.x = MIN_X (plugin);
	  area.y = MIN_Y (plugin);
	  area.width = CAPTURE_WIDTH (plugin);
	  area.height = CAPTURE_HEIGHT (plugin);

	  paint_background (root_window, area, plugin);

	  CLEAR_CAPTURE_AREA (plugin);
	}
    }
  return TRUE;

}

/* stolen from gtkfilechooserentry.c */
gboolean
entry_select_filename_in_focus_cb (GtkWidget * entry, GdkEventFocus * event)
{
  const gchar *str, *ext;
  glong len = -1;

  str = gtk_entry_get_text (GTK_ENTRY (entry));
  ext = g_strrstr (str, ".");

  if (ext)
    len = g_utf8_pointer_to_offset (str, ext);

  gtk_editable_select_region (GTK_EDITABLE (entry), 0, (gint) len);

  (void) event;
  return TRUE;
}

/* part stolen from gtkutils.c */
static void
set_sensitive_if_input_and_noexist (GtkWidget * entry, PurplePlugin * plugin)
{
  GtkWidget *dlgbox_rename;
  GtkWidget *warn_label;
  gchar *capture_path_filename = NULL;
  const gchar *text;

  dlgbox_rename =
    g_object_get_data (G_OBJECT (PLUGIN (blist_window)), "dlgbox-rename");
  warn_label = g_object_get_data (G_OBJECT (dlgbox_rename), "warn-label");

  text = gtk_entry_get_text (GTK_ENTRY (entry));

  capture_path_filename =
    g_build_filename (purple_prefs_get_string (PREF_STORE_FOLDER),
		      text, NULL);

  gtk_dialog_set_response_sensitive (GTK_DIALOG (dlgbox_rename),
				     GTK_RESPONSE_OK, (*text != '\0'));
  /* warn user if file already exists */
  if (g_file_test (capture_path_filename, G_FILE_TEST_EXISTS)
      && (*text != '\0'))
    gtk_widget_show (warn_label);
  else
    gtk_widget_hide (warn_label);

  if (capture_path_filename != NULL)
    g_free (capture_path_filename);
}

/* part stolen from pidgin/gtkconv.c */
static void
capture_rename (PurplePlugin * plugin, const gchar * capture_name)
{
  GdkColor red = { 0, 65535, 0, 0 };
  GtkWidget *dlgbox_rename;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *warn_label;
  GtkWidget *gtkconv_window;
  GtkWidget *img;

  img =
    gtk_image_new_from_stock (PIDGIN_STOCK_DIALOG_QUESTION,
			      gtk_icon_size_from_name
			      (PIDGIN_ICON_SIZE_TANGO_HUGE));
  gtkconv_window = get_receiver_window (plugin);

  dlgbox_rename = gtk_dialog_new_with_buttons (DLGBOX_CAPNAME_TITLE,
					       GTK_WINDOW ((gtkconv_window) ?
							   gtkconv_window :
							   PLUGIN
							   (blist_window)),
					       GTK_DIALOG_MODAL |
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_STOCK_OK, GTK_RESPONSE_OK,
					       NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dlgbox_rename),
				   GTK_RESPONSE_OK);

  gtk_container_set_border_width (GTK_CONTAINER
				  (dlgbox_rename), PIDGIN_HIG_BOX_SPACE);
  gtk_window_set_resizable (GTK_WINDOW (dlgbox_rename), FALSE);
  gtk_dialog_set_has_separator (GTK_DIALOG (dlgbox_rename), FALSE);
  gtk_box_set_spacing (GTK_BOX
		       (GTK_DIALOG (dlgbox_rename)->vbox), PIDGIN_HIG_BORDER);
  gtk_container_set_border_width (GTK_CONTAINER
				  (GTK_DIALOG
				   (dlgbox_rename)->vbox),
				  PIDGIN_HIG_BOX_SPACE);

  hbox = gtk_hbox_new (FALSE, PIDGIN_HIG_BORDER);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlgbox_rename)->vbox), hbox);
  gtk_box_pack_start (GTK_BOX (hbox), img, FALSE, FALSE, 0);

  gtk_misc_set_alignment (GTK_MISC (img), 0, 0);
  label = gtk_label_new (DLGBOX_CAPNAME_LABEL);

  warn_label = gtk_label_new (NULL);

  gtk_widget_modify_fg (warn_label, GTK_STATE_NORMAL, &red);
  gtk_label_set_text (GTK_LABEL (warn_label), DLGBOX_CAPNAME_WARNEXISTS);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX
		      (GTK_DIALOG (dlgbox_rename)->vbox), warn_label, FALSE,
		      FALSE, 0);
  entry = gtk_entry_new ();

  gtk_entry_set_text (GTK_ENTRY (entry), capture_name);

  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

  g_object_set (gtk_widget_get_settings (entry),
		"gtk-entry-select-on-focus", FALSE, NULL);
  g_signal_connect (G_OBJECT (entry), "changed",
		    G_CALLBACK (set_sensitive_if_input_and_noexist), plugin);
  g_signal_connect (G_OBJECT (entry), "focus-out-event",
		    G_CALLBACK (entry_select_filename_in_focus_cb), plugin);

  g_signal_connect (G_OBJECT (entry), "focus-in-event",
		    G_CALLBACK (entry_select_filename_in_focus_cb), plugin);
  gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);

  gtk_widget_show (label);
  gtk_widget_show (entry);
  gtk_widget_show (img);
  gtk_widget_show (hbox);
  gtk_widget_show (GTK_DIALOG (dlgbox_rename)->vbox);

  g_object_set_data (G_OBJECT (dlgbox_rename), "entry", entry);
  g_object_set_data (G_OBJECT (dlgbox_rename), "warn-label", warn_label);
  g_object_set_data (G_OBJECT (PLUGIN (blist_window)),
		     "dlgbox-rename", dlgbox_rename);
}

static gboolean
on_root_window_button_release_cb (GtkWidget * root_window,
				  GdkEventButton * event,
				  PurplePlugin * plugin)
{
  GdkPixbuf *capture;

  if (event->button != 1 || PLUGIN (x2) == -1)
    return TRUE;

  gtk_widget_hide (root_window);
  capture = NULL;

  /* create our new pixbuf */
  if ((capture = gdk_pixbuf_get_from_image (NULL,
					    PLUGIN (root_image),
					    NULL,
					    MIN_X (plugin),
					    MIN_Y (plugin),
					    0, 0,
					    CAPTURE_WIDTH (plugin),
					    CAPTURE_HEIGHT (plugin))) == NULL)
    {
      purple_notify_error (plugin, PLUGIN_NAME, PLUGIN_ERROR,
			   PLUGIN_MEMORY_ERROR);
    }
  else
    {
      gchar *capture_path_filename = NULL;	/* /the/path/capture.extension */
      gchar *capture_name = NULL;	/* capture */
      const gchar *const extension =
	purple_prefs_get_string (PREF_IMAGE_TYPE);
      gchar *param_name = NULL;
      gchar *param_value = NULL;
      GError *error = NULL;
      GTimeVal g_tv;

      if (!strcmp (extension, "png"))
	{
	  param_name = g_strdup_printf ("%s", "compression");
	  param_value =
	    g_strdup_printf ("%d",
			     purple_prefs_get_int (PREF_PNG_COMPRESSION));
	}
      else
	{
	  param_name = g_strdup ("quality");
	  param_value =
	    g_strdup_printf ("%d", purple_prefs_get_int (PREF_JPEG_QUALITY));
	}

      g_get_current_time (&g_tv);
      capture_name =
	g_strdup_printf ("%s_%ld.%s", CAPTURE, g_tv.tv_sec, extension);

      /** Eventually ask for a custom capture name when sending :
	    - as file 
	    - to a remote FTP server
      */
      if (purple_prefs_get_bool (PREF_ASK_FILENAME) &&
	  (PLUGIN (send_as) == SEND_AS_FILE
#ifdef ENABLE_UPLOAD
	   || PLUGIN (send_as) == SEND_AS_FTP_LINK)
#else
	  )
#endif
	)
	{
	  GtkWidget *dlgbox_rename = NULL;
	  gint result = 0;

	  capture_rename (plugin, capture_name);

	  dlgbox_rename =
	    g_object_get_data (G_OBJECT
			       (PLUGIN (blist_window)), "dlgbox-rename");

	  result = gtk_dialog_run (GTK_DIALOG (dlgbox_rename));

	  if (result == GTK_RESPONSE_OK)
	    {
	      GtkWidget *entry = NULL;

	      entry = g_object_get_data (G_OBJECT (dlgbox_rename), "entry");

	      g_free (capture_name);
	      capture_name =
		g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	    }

	  gtk_widget_destroy (dlgbox_rename);
	}
      capture_path_filename =
	g_build_filename (purple_prefs_get_string (PREF_STORE_FOLDER),
			  capture_name, NULL);

      /* store capture in a file */
      gdk_pixbuf_save (capture, capture_path_filename, extension,
		       &error, param_name, param_value, NULL);

      if (error != NULL)
	{
	  gchar *errmsg_saveto = g_strdup_printf (PLUGIN_SAVE_TO_FILE_ERROR,
						  capture_path_filename);

	  NotifyError ("%s\n\n\%s", errmsg_saveto, error->message);

	  g_free (errmsg_saveto);
	  CLEAR_SEND_INFO_TO_NULL (plugin);
	  PLUGIN (running) = FALSE;
	}
      else			/* capture was successfully stored in file */
	{
	  switch (PLUGIN (send_as))
	    {
	    case SEND_AS_FILE:	/* send capture as file */
	      {
		serv_send_file
		  (purple_account_get_connection
		   (PLUGIN (account)), PLUGIN (name), capture_path_filename);
		break;
	      }
	    case SEND_AS_IMAGE:
	      {
		gchar *filedata = NULL;
		size_t size;

		if (g_file_get_contents
		    (capture_path_filename, &filedata, &size,
		     &error) == FALSE)
		  {
		    gchar *errmsg_getdata;
		    if (filedata != NULL)
		      g_free (filedata);

		    errmsg_getdata = g_strdup_printf (PLUGIN_GET_DATA_ERROR,
						      capture_path_filename);


		    NotifyError ("%s\n\n\%s", errmsg_getdata, error->message);
		    g_free (errmsg_getdata);
		  }
		else
		  {
		    gchar *basename = NULL;
		    GtkTextIter iter;
		    GtkTextMark *ins = NULL;
		    gint purple_tmp_id;
		    GtkIMHtml *imhtml =
		      get_receiver_imhtml (PLUGIN(pconv));

		    basename = g_path_get_basename (capture_path_filename);

		    ins =
		      gtk_text_buffer_get_insert
		      (gtk_text_view_get_buffer (GTK_TEXT_VIEW (imhtml)));

		    gtk_text_buffer_get_iter_at_mark
		      (gtk_text_view_get_buffer (GTK_TEXT_VIEW (imhtml)),
		       &iter, ins);

		    purple_tmp_id =
		      purple_imgstore_add_with_id (filedata, size, basename);
		    g_free (basename);

		    if (purple_tmp_id == 0)
		      {

			NotifyError ("%s\n\n\%s",
				     PLUGIN_STORE_FAILED_ERROR,
				     capture_path_filename);
			g_free (filedata);
		      }
		    else
		      {
			gtk_imhtml_insert_image_at_iter
			  (GTK_IMHTML (imhtml), purple_tmp_id, &iter);
			/* filedata is freed here */
			purple_imgstore_unref_by_id (purple_tmp_id);
			gtk_widget_grab_focus (GTK_WIDGET (imhtml));
		      }
		  }
		break;
	      }
#ifdef ENABLE_UPLOAD
	    case SEND_AS_HTTP_LINK:
	      {
		http_upload_prepare (capture_path_filename, capture_name,
				     plugin);
		break;
	      }
	    case SEND_AS_FTP_LINK:
	      {
		ftp_upload_prepare (plugin,
				    capture_path_filename, capture_name);
		break;
	      }
#endif
	    }
	}

      /*
       * back to normal
       */
      if (PLUGIN (iconified) && get_receiver_window (plugin))
	{
	  gtk_window_deiconify (GTK_WINDOW (get_receiver_window (plugin)));

	  PLUGIN (iconified) = FALSE;
	}
      if (error != NULL)
	g_error_free (error);
      if (capture_path_filename != NULL)
	g_free (capture_path_filename);
      if (capture_name != NULL)
	g_free (capture_name);
      if (param_name != NULL)
	g_free (param_name);
      if (param_value != NULL)
	g_free (param_value);
      if (capture != NULL)
	g_object_unref (capture);

      if (PLUGIN (root_image))
	{
	  g_object_unref (PLUGIN (root_image));
	  PLUGIN (root_image) = NULL;
	}
      if (PLUGIN (root_pixbuf))
	{
	  g_object_unref (PLUGIN (root_pixbuf));
	  PLUGIN (root_pixbuf) = NULL;
	}

      CLEAR_CAPTURE_AREA (plugin);

      /* finish immediately */
      if (PLUGIN (send_as) == SEND_AS_IMAGE ||
	  PLUGIN (send_as) == SEND_AS_FILE)
	{
	  PLUGIN (running) = FALSE;
	  CLEAR_SEND_INFO_TO_NULL (plugin);
	}
    }
  return TRUE;
}

/* clear area */
void
paint_background (GtkWidget * root_window, GdkRectangle area,
		  PurplePlugin * plugin)
{
  GdkWindow *gdkwin;

#if GTK_CHECK_VERSION(2,14,0)
  gdkwin = gtk_widget_get_window (root_window);
#else
  gdkwin = root_window->window;
#endif

  if (purple_prefs_get_int (PREF_HIGHLIGHT_MODE) < 3
      || purple_prefs_get_int (PREF_HIGHLIGHT_MODE) == 5)
    {
      gdk_draw_pixbuf (gdkwin,
		       root_window->style->fg_gc[0],
		       PLUGIN (root_pixbuf),
		       area.x, area.y, area.x, area.y,
		       area.width, area.height, GDK_RGB_DITHER_NONE, 0, 0);
    }
  else
    {
      gdk_draw_image (gdkwin,
		      root_window->style->fg_gc[0],
		      PLUGIN (root_image),
		      area.x, area.y, area.x, area.y, area.width,
		      area.height);
    }
}

/* clear screen */
static void
paint_entire_background (GtkWidget * root_window, PurplePlugin * plugin)
{
  GdkWindow *gdkwin;
  GdkRectangle area;
  gint width, height;

#if GTK_CHECK_VERSION(2,14,0)
  gdkwin = gtk_widget_get_window (root_window);
#else
  gdkwin = root_window->window;
#endif
  gdk_drawable_get_size (gdkwin, &width, &height);

  area.x = 0;
  area.y = 0;
  area.width = width;
  area.height = height;
  paint_background (root_window, area, plugin);
}

static gboolean
on_root_window_expose_cb (GtkWidget * root_window,
			  GdkEventExpose * event, PurplePlugin * plugin)
{
  if (PLUGIN (x1) == -1)
    paint_entire_background (root_window, plugin);
  else
    {
      GdkWindow *gdkwin;
      GdkRegion *selection_region;
      GdkRegion *background_region;
      GdkRectangle selection_rectangle;
      GdkRectangle *background_rectangles = NULL;
      gint n_rectangles, idx;

#if GTK_CHECK_VERSION(2,14,0)
      gdkwin = gtk_widget_get_window (root_window);
#else
      gdkwin = root_window->window;
#endif

      selection_rectangle.width = CAPTURE_WIDTH (plugin);
      selection_rectangle.height = CAPTURE_HEIGHT (plugin);
      selection_rectangle.x = MIN_X (plugin);
      selection_rectangle.y = MIN_Y (plugin);

      selection_region = gdk_region_rectangle (&selection_rectangle);
      background_region = gdk_region_rectangle (&event->area);

      gdk_region_subtract (background_region, selection_region);
      gdk_region_destroy (selection_region);
      gdk_region_get_rectangles (background_region,
				 &background_rectangles, &n_rectangles);
      gdk_region_destroy (background_region);

      /* redraw background only where it needs to be */
      for (idx = 0; idx < n_rectangles; idx++)
	{
	  GdkRectangle rect = background_rectangles[idx];

	  if (purple_prefs_get_int (PREF_HIGHLIGHT_MODE) < 3
	      || purple_prefs_get_int (PREF_HIGHLIGHT_MODE) == 5)
	    gdk_draw_pixbuf (gdkwin, root_window->style->fg_gc[0],
			     PLUGIN (root_pixbuf), rect.x,
			     rect.y, rect.x, rect.y, rect.width,
			     rect.height, GDK_RGB_DITHER_NONE, 0, 0);
	  else
	    gdk_draw_image (gdkwin,
			    root_window->style->fg_gc[0],
			    PLUGIN (root_image),
			    rect.x, rect.y, rect.x, rect.y,
			    rect.width, rect.height);
	}
      if (background_rectangles != NULL)
	g_free (background_rectangles);

      if (purple_prefs_get_int (PREF_HIGHLIGHT_MODE) == 3)
	gdk_gc_set_function (root_window->style->fg_gc[0], GDK_COPY_INVERT);
      /* draw selection */
      gdk_draw_image (gdkwin, root_window->style->fg_gc[0], PLUGIN (root_image), selection_rectangle.x + 1,	/* +1 => borders */
		      selection_rectangle.y + 1,
		      selection_rectangle.x + 1,
		      selection_rectangle.y + 1,
		      selection_rectangle.width - 1,
		      selection_rectangle.height - 1);

      /* highlight borders */
      gdk_gc_set_function (root_window->style->fg_gc[0], GDK_COPY_INVERT);

      /* north */
      gdk_draw_image (gdkwin,
		      root_window->style->fg_gc[0],
		      PLUGIN (root_image),
		      selection_rectangle.x, selection_rectangle.y,
		      selection_rectangle.x, selection_rectangle.y,
		      selection_rectangle.width, 1);
      /* south */
      gdk_draw_image (gdkwin,
		      root_window->style->fg_gc[0],
		      PLUGIN (root_image),
		      selection_rectangle.x,
		      selection_rectangle.y + selection_rectangle.height -
		      1, selection_rectangle.x,
		      selection_rectangle.y + selection_rectangle.height -
		      1, selection_rectangle.width, 1);
      /* west */
      gdk_draw_image (gdkwin,
		      root_window->style->fg_gc[0],
		      PLUGIN (root_image),
		      selection_rectangle.x, selection_rectangle.y,
		      selection_rectangle.x, selection_rectangle.y,
		      1, selection_rectangle.height);
      /* est */
      gdk_draw_image (gdkwin,
		      root_window->style->fg_gc[0],
		      PLUGIN (root_image),
		      selection_rectangle.x + selection_rectangle.width -
		      1, selection_rectangle.y,
		      selection_rectangle.x + selection_rectangle.width -
		      1, selection_rectangle.y, 1,
		      selection_rectangle.height);

      gdk_gc_set_function (root_window->style->fg_gc[0], GDK_COPY);

      /* remember old coords to clear after */
      PLUGIN (_x) = PLUGIN (x2);
      PLUGIN (_y) = PLUGIN (y2);
    }
  return TRUE;
}

/* draw visual cues */
void
draw_hv_lines (GtkWidget * root_window, GdkEventMotion * event,
	       PurplePlugin * plugin)
{
  GdkRegion *union_r = NULL;
  gint width, height;
  GdkRectangle v, h;

#if GTK_CHECK_VERSION(2,14,0)
  GdkWindow *gdkwin = gtk_widget_get_window (root_window);
#else
  GdkWindow *gdkwin = root_window->window;
#endif
  gdk_drawable_get_size (gdkwin, &width, &height);

  /* clear previous lines */
  h.x = 0;
  h.y = PLUGIN (_y);
  h.width = width;
  h.height = 1;

  v.x = PLUGIN (_x);
  v.y = 0;
  v.width = 1;
  v.height = height;

  if (event != NULL)
    {
      GdkRegion *_rv, *rv, *_rh, *rh;
      GdkRectangle _v, _h;

      _v.x = (gint) event->x;
      _v.y = 0;
      _v.width = 1;
      _v.height = height;

      _h.x = 0;
      _h.y = (gint) event->y;
      _h.width = width;
      _h.height = 1;

      _rv = gdk_region_rectangle (&_v);
      _rh = gdk_region_rectangle (&_h);
      rv = gdk_region_rectangle (&v);
      rh = gdk_region_rectangle (&h);

      union_r = gdk_region_copy (_rv);
      gdk_region_union (union_r, rv);
      gdk_region_union (union_r, rh);
      gdk_region_union (union_r, _rh);

      /* double buffering on */
      gdk_window_begin_paint_region (gdkwin, union_r);

      gdk_region_destroy (rv);
      gdk_region_destroy (_rv);
      gdk_region_destroy (rh);
      gdk_region_destroy (_rh);
    }

  if (PLUGIN (_y) >= 0 && PLUGIN (_x) >= 0)
    {
      paint_background (root_window, h, plugin);
      paint_background (root_window, v, plugin);

    }

  if (event != NULL)
    {				/* otherwise, clear only */
      /* draw vertical and horizontal lines */
      gdk_gc_set_function (root_window->style->fg_gc[0], GDK_COPY_INVERT);

      gdk_draw_image (gdkwin,
		      root_window->style->fg_gc[0],
		      PLUGIN (root_image),
		      0, (gint) event->y, 0, (gint) event->y, width, 1);
      gdk_draw_image (gdkwin,
		      root_window->style->fg_gc[0],
		      PLUGIN (root_image),
		      (gint) event->x, 0, (gint) event->x, 0, 1, height);

      PLUGIN (_x) = (gint) event->x;	/* remember old coords to clean up after */
      PLUGIN (_y) = (gint) event->y;
      gdk_gc_set_function (root_window->style->fg_gc[0], GDK_COPY);
      /* double buffering off */
      gdk_window_end_paint (gdkwin);
      gdk_region_destroy (union_r);
    }
}

static gboolean
on_root_window_motion_notify_cb (GtkWidget * root_window,
				 GdkEventMotion * event,
				 PurplePlugin * plugin)
{
  /* mouse button pressed */
  if ((event->state & GDK_BUTTON1_MASK) == GDK_BUTTON1_MASK &&
      PLUGIN (x1) != -1)
    {
      gint xmin, ymin, xmax, ymax;

      PLUGIN (x2) = (gint) event->x;
      PLUGIN (y2) = (gint) event->y;

      xmin = MIN (MIN_X (plugin), PLUGIN (_x));
      ymin = MIN (MIN_Y (plugin), PLUGIN (_y));
      xmax = MAX (MAX_X (plugin), PLUGIN (_x));
      ymax = MAX (MAX_Y (plugin), PLUGIN (_y));
      gtk_widget_queue_draw_area (root_window,
				  xmin, ymin, xmax - xmin + 1,
				  ymax - ymin + 1);
    }
  /* make sure our frozen root window is visible
     before drawing visual cues */
  else if (purple_prefs_get_bool (PREF_SHOW_VISUAL_CUES)
	   && GTK_WIDGET_MAPPED (root_window))
    {
      draw_hv_lines (root_window, event, plugin);
    }
  return TRUE;

}

static void
prepare_root_window (PurplePlugin * plugin)
{
  GtkWidget *root_window;
  GdkScreen *screen;

  /* from gdk API :
   *   "a screen may consist of multiple monitors which are merged to form a large screen area" 
   */
  screen = gdk_screen_get_default ();

  /* no toplevel otherwise some desktops (say Xfce4),
   * won't allow us to cover the entire screen. */
  root_window = gtk_window_new (GTK_WINDOW_POPUP);
  PLUGIN (root_window) = root_window;

  g_assert (PLUGIN (root_window));
  /* here, gtk_window_fullscreen() won't work 'cos most (every)
     WMs do this on the current monitor only, while we want our
     invisible window to cover the entire GdkScreen. */
  gtk_widget_set_size_request (root_window,
			       gdk_screen_get_width (screen),
			       gdk_screen_get_height (screen));

  gtk_window_move (GTK_WINDOW (root_window), 0, 0);

  /* not sure this is actually needed */
#if GTK_CHECK_VERSION(2,4,0)
  gtk_window_set_keep_above (GTK_WINDOW (root_window), TRUE);
#endif

  /* install callbacks */
  g_signal_connect (GTK_OBJECT (root_window), "realize",
		    G_CALLBACK (on_root_window_realize_cb), NULL);
  g_signal_connect (GTK_OBJECT (root_window),
		    "button-press-event",
		    G_CALLBACK (on_root_window_button_press_cb), plugin);
  g_signal_connect (GTK_OBJECT (root_window),
		    "button-release-event",
		    G_CALLBACK (on_root_window_button_release_cb), plugin);

  g_signal_connect (GTK_OBJECT (root_window),
		    "expose-event",
		    G_CALLBACK (on_root_window_expose_cb), plugin);
  g_signal_connect (GTK_OBJECT (root_window),
		    "motion-notify-event",
		    G_CALLBACK (on_root_window_motion_notify_cb), plugin);
  CLEAR_CAPTURE_AREA (plugin);
}

static void
on_screenshot_insert_as_image_activate_cb (PidginConversation* gtkconv)
{
  PurplePlugin *plugin;

  plugin = purple_plugins_find_with_id (PLUGIN_ID);

  TRY_SET_RUNNING_ON (plugin);

  PLUGIN (send_as) = SEND_AS_IMAGE;

  REMEMBER_ACCOUNT (gtkconv);

  PLUGIN (conv_features) = gtkconv->active_conv->features;
  FREEZE_DESKTOP ();
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

/*
 * Handle hiding and showing stuff based on what type of conv this is...
 */
static void
on_conversation_menu_show_cb (PidginWindow * win)
{
  GtkWidget *conversation_menu, *img_menuitem
#ifdef ENABLE_UPLOAD
    , *link_menuitem, *ftp_link_menuitem;
#endif
   ;

  PurpleConversation *conv;

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

static void
create_screenshot_insert_menuitem_pidgin (PidginConversation * gtkconv)
{
  PidginWindow *win;
  GtkWidget *conversation_menu,
    *screenshot_insert_menuitem, *screenshot_menuitem;

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
      GtkWidget *insert_menu, *submenu, *as_image
#ifdef ENABLE_UPLOAD
       , *as_link, *as_ftp_link;
#endif
      ;
      if ((insert_menu =
	   g_object_get_data (G_OBJECT (gtkconv->toolbar),
			      "insert_menu")) != NULL)
	{
	  /* add us to the "insert" list */
	  screenshot_insert_menuitem =
	    gtk_menu_item_new_with_mnemonic
	    (SCREENSHOT_INSERT_MENUITEM_LABEL);

	  submenu = gtk_menu_new ();
	  as_image = gtk_menu_item_new_with_mnemonic (SEND_AS_IMAGE_TXT);

#ifdef ENABLE_UPLOAD
	  as_link = gtk_menu_item_new_with_mnemonic (SEND_AS_HTML_LINK_TXT);
	  as_ftp_link =
	    gtk_menu_item_new_with_mnemonic (SEND_AS_FTP_LINK_TXT);
	  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_ftp_link, 0);
	  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_link, 1);
	  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_image, 2);
#else
	  gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_image, 0);
#endif

	  gtk_menu_item_set_submenu (GTK_MENU_ITEM
				     (screenshot_insert_menuitem), submenu);

	  gtk_menu_shell_insert (GTK_MENU_SHELL (insert_menu),
				 screenshot_insert_menuitem, 1);

	  g_signal_connect_swapped (G_OBJECT (as_image),
				    "activate",
				    G_CALLBACK
				    (on_screenshot_insert_as_image_activate_cb),
				    gtkconv);

	  g_signal_connect (G_OBJECT (as_image), "show",
			    G_CALLBACK
			    (on_screenshot_insert_as_image_show_cb), gtkconv);

#ifdef ENABLE_UPLOAD
	  g_signal_connect_swapped (G_OBJECT (as_link), "activate",
				    G_CALLBACK
				    (on_screenshot_insert_as_link_activate_cb),
				    gtkconv);
	  g_signal_connect (G_OBJECT (as_link), "show",
			    G_CALLBACK
			    (on_screenshot_insert_as_link_show_cb), gtkconv);
	  g_signal_connect (G_OBJECT (as_ftp_link), "show",
			    G_CALLBACK
			    (on_screenshot_insert_as_ftp_link_show_cb),
			    gtkconv);
	  g_signal_connect_swapped (G_OBJECT (as_ftp_link), "activate",
				    G_CALLBACK
				    (on_screenshot_insert_as_ftp_link_activate_cb),
				    gtkconv);
#endif
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
      GtkWidget *submenu, *as_image
#ifdef ENABLE_UPLOAD
       , *as_link, *as_ftp_link;
#endif
      ;

      screenshot_menuitem =
	gtk_menu_item_new_with_mnemonic (SCREENSHOT_MENUITEM_LABEL);

      submenu = gtk_menu_new ();
      as_image = gtk_menu_item_new_with_mnemonic (SEND_AS_IMAGE_TXT);

#ifdef ENABLE_UPLOAD
      as_link = gtk_menu_item_new_with_mnemonic (SEND_AS_HTML_LINK_TXT);
      as_ftp_link = gtk_menu_item_new_with_mnemonic (SEND_AS_FTP_LINK_TXT);
      gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_ftp_link, 0);
      gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_link, 1);
      gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_image, 2);
#else
      gtk_menu_shell_insert (GTK_MENU_SHELL (submenu), as_image, 0);
#endif

      gtk_menu_item_set_submenu (GTK_MENU_ITEM
				 (screenshot_menuitem), submenu);


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
      gtk_menu_shell_insert (GTK_MENU_SHELL (conversation_menu),
			     screenshot_menuitem, i + 1);
      gtk_widget_show (screenshot_menuitem);
      gtk_widget_show (as_image);
#ifdef ENABLE_UPLOAD
      gtk_widget_show (as_link);
      gtk_widget_show (as_ftp_link);
#endif
      g_signal_connect_swapped (G_OBJECT (as_image),
				"activate",
				G_CALLBACK
				(on_screenshot_insert_as_image_activate_cb),
				gtkconv);

#ifdef ENABLE_UPLOAD
      g_signal_connect_swapped (G_OBJECT (as_link), "activate",
				G_CALLBACK
				(on_screenshot_insert_as_link_activate_cb),
				gtkconv);
      g_signal_connect_swapped (G_OBJECT (as_ftp_link), "activate",
				G_CALLBACK
				(on_screenshot_insert_as_ftp_link_activate_cb),
				gtkconv);
#endif
      g_signal_connect_swapped (G_OBJECT (conversation_menu), "show",
				G_CALLBACK (on_conversation_menu_show_cb),
				win);
      g_object_set_data (G_OBJECT (conversation_menu),
			 "screenshot_menuitem", screenshot_menuitem);
      g_object_set_data (G_OBJECT (conversation_menu),
			 "img_menuitem", as_image);
#ifdef ENABLE_UPLOAD
      g_object_set_data (G_OBJECT (conversation_menu),
			 "link_menuitem", as_link);
      g_object_set_data (G_OBJECT (conversation_menu),
			 "ftp_link_menuitem", as_ftp_link);
#endif
    }
}

static void
remove_screenshot_insert_menuitem_pidgin (PidginConversation * gtkconv)
{
  GtkWidget *screenshot_insert_menuitem, *screenshot_menuitem,
    *conversation_menu;
  PidginWindow *win;

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
      ||
      (((PluginExtraVars *) plugin->extra)->host_data =
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

#ifdef HAVE_LIBCURL
  /* 
   * Make sure that the upload thread won't read nor write the structs we are
   * going to free...
   *
   * see upload ()
   */
  G_LOCK (unload);
#endif

  host_data = PLUGIN (host_data);
  timeout_handle = PLUGIN (timeout_cb_handle);
  if (timeout_handle)
    purple_timeout_remove (timeout_handle);
  if (PLUGIN (uploading_dialog))
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

  if (PLUGIN (root_window))
    gtk_widget_destroy (PLUGIN (root_window));

  CLEAR_PLUGIN_EXTRA_GCHARS (plugin);

#ifdef ENABLE_UPLOAD

  CLEAR_HOST_PARAM_DATA_FULL (host_data);
  g_free (PLUGIN (xml_hosts_filename));
  g_free (PLUGIN (host_data));

#endif
  g_free (plugin->extra);
  plugin->extra = NULL;

#if defined (ENABLE_UPLOAD) && (HAVE_LIBCURL)
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
  PLUGIN_ID,
  NULL,
  PACKAGE_VERSION,
  NULL,
  NULL,
  PLUGIN_AUTHOR,
  PLUGIN_WEBSITE,
  plugin_load,
  plugin_unload,
  NULL,
  &ui_info,
  NULL,
  NULL,
  NULL,
  /*
   * padding 
   */
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
  purple_prefs_add_string (PREF_IMAGE_TYPE, PREF_IMAGE_TYPE_DEFAULT);
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

  /* clean up old options... */
  purple_prefs_remove (PREF_PREFIX "/highlight-all");
  purple_prefs_remove (PREF_PREFIX "/upload-activity_timeout");

  info.name = PLUGIN_NAME;
  info.summary = PLUGIN_SUMMARY;
  info.description = PLUGIN_DESCRIPTION;

  (void) plugin;
}

PURPLE_INIT_PLUGIN (screenshot, init_plugin, info)

/* end of screenshot.c */
