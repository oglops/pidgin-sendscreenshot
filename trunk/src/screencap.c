 /*
  * Pidgin SendScreenshot third-party plugin - root window capture, callbacks.
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
  * --  Raoul Berger <contact@raoulito.info>
  *
  */

#include "screencap.h"

#include "prefs.h"
#include "pixbuf_utils.h"
#include "dialogs.h"

#ifdef ENABLE_UPLOAD
#include "upload_utils.h"
#include "http_upload.h"
#include "ftp_upload.h"
#endif

#ifdef G_OS_WIN32
#include "mswin-freeze.h"
#endif

/**
 * Get, copy and show current desktop image.
 */
guint
timeout_freeze_screen (PurplePlugin * plugin)
{
  GdkWindow *gdkroot;
  gint x_orig, y_orig, width, height;

#ifdef G_OS_WIN32
  GdkScreen *screen;
  GdkRectangle rect;
  gint i;

  /* Under Windows, the primary monitor origin is always (0;0),
   * so other monitor might have negative coordinates.
   * If so, calling gdk_drawable_get_image() from gdk_get_default_root_window ()
   * will fail to retrieve all the pixels at negative coordinates.
   * We fallback using Window API...
   */
  gint x_offset, y_offset;

  screen = gdk_screen_get_default ();
  gdk_screen_get_monitor_geometry (screen, 0, &rect);

  x_offset = -rect.x;
  y_offset = -rect.y;

  rect.x += x_offset;
  rect.y += y_offset;

  for (i = 1; i < gdk_screen_get_n_monitors (screen); i++)
    {
      GdkRectangle monitor_rect;
      gdk_screen_get_monitor_geometry (screen, i, &monitor_rect);
      monitor_rect.x += x_offset;
      monitor_rect.y += y_offset;
      gdk_rectangle_union (&rect, &monitor_rect, &rect);
    }

  x_orig = rect.x;
  y_orig = rect.y;
  width = rect.width;
  height = rect.height;

  /* primary monitor not at the top-left corner, use Win32 API. */
  if (rect.x != 0 || rect.y != 0)
    {
      if (mswin_freeze_screen (plugin, rect) == FALSE)
	{
	  purple_notify_error (plugin, PLUGIN_NAME, PLUGIN_ERROR,
			       PLUGIN_MEMORY_ERROR);
	  plugin_stop (plugin);
	  return 0;
	}
    }
  else
    {
#endif

      /* vvvvvv
         Part stolen from gnome-screenshot code, part
         of GnomeUtils ( See http://live.gnome.org/GnomeUtils)
       */
      gint x_real_orig, y_real_orig;
      gint real_width, real_height;

      gdkroot = gdk_get_default_root_window ();

      gdk_drawable_get_size (gdkroot, &real_width, &real_height);
      gdk_window_get_origin (gdkroot, &x_real_orig, &y_real_orig);

      x_orig = x_real_orig;
      y_orig = y_real_orig;
      width = real_width;
      height = real_height;

      if (x_orig < 0)
	{
	  width = width + x_orig;
	  x_orig = 0;
	}

      if (y_orig < 0)
	{
	  height = height + y_orig;
	  y_orig = 0;
	}

      if (x_orig + width > gdk_screen_width ())
	width = gdk_screen_width () - x_orig;

      if (y_orig + height > gdk_screen_height ())
	height = gdk_screen_height () - y_orig;
      /* ^^^^ */

      g_assert (PLUGIN (root_pixbuf_orig) == NULL);
      if ((PLUGIN (root_pixbuf_orig) =
	   gdk_pixbuf_get_from_drawable (NULL, gdkroot, NULL,
					 x_orig, y_orig, 0, 0, width, height)) == NULL)
	{
	  purple_notify_error (plugin, PLUGIN_NAME, PLUGIN_ERROR,
			       PLUGIN_MEMORY_ERROR);
	  plugin_stop (plugin);
	  return 0;
	}
#ifdef G_OS_WIN32
    }
#endif

  if (purple_prefs_get_int (PREF_HIGHLIGHT_MODE) < 3 ||
      purple_prefs_get_int (PREF_HIGHLIGHT_MODE) == 5)
    {
      g_assert (PLUGIN (root_pixbuf_x) == NULL);
      g_assert (PLUGIN (root_pixbuf_orig) != NULL);
      if ((PLUGIN (root_pixbuf_x) =
	   gdk_pixbuf_copy (PLUGIN (root_pixbuf_orig))) == NULL)
	{
	  purple_notify_error (plugin, PLUGIN_NAME, PLUGIN_ERROR,
			       PLUGIN_MEMORY_ERROR);
	  g_object_unref (PLUGIN (root_pixbuf_orig));
	  PLUGIN (root_pixbuf_orig) = NULL;
	  plugin_stop (plugin);
	  return 0;
	}
    }
  /* apply effects */
  if (PLUGIN (root_pixbuf_x) != NULL)
    {
      if (purple_prefs_get_int (PREF_HIGHLIGHT_MODE) == 1)
	mygdk_pixbuf_lighten (PLUGIN (root_pixbuf_x), PIXEL_VAL);
      else if (purple_prefs_get_int (PREF_HIGHLIGHT_MODE) == 2)
	mygdk_pixbuf_darken (PLUGIN (root_pixbuf_x), PIXEL_VAL);
      else if (purple_prefs_get_int (PREF_HIGHLIGHT_MODE) == 5)
	mygdk_pixbuf_grey (PLUGIN (root_pixbuf_x));
    }
  /* let the user capture an area... */
  gtk_widget_show (PLUGIN (root_window));
  return 0;
}

/* clear area */
static void
paint_background (GtkWidget * root_window, GdkRectangle area,
		  PurplePlugin * plugin)
{
  GdkWindow *gdkwin;
  GdkPixbuf * pixbuf;

#if GTK_CHECK_VERSION(2,14,0)
  gdkwin = gtk_widget_get_window (root_window);
#else
  gdkwin = root_window->window;
#endif
  pixbuf = PLUGIN (root_pixbuf_x) != NULL ?  
    PLUGIN (root_pixbuf_x) : PLUGIN (root_pixbuf_orig);
  
  gdk_draw_pixbuf (gdkwin,
		   PLUGIN(gc),
		   pixbuf,
		   area.x, area.y, area.x, area.y,
		   area.width, area.height, GDK_RGB_DITHER_NONE, 0, 0);
}

/* draw visual cues */
static void
draw_cues (GtkWidget * root_window,
	   gint x, gint y, gboolean erase_only, PurplePlugin * plugin)
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

  if (!erase_only)
    {
      GdkRegion *_rv, *rv, *_rh, *rh;
      GdkRectangle _v, _h;

      _v.x = x;
      _v.y = 0;
      _v.width = 1;
      _v.height = height;

      _h.x = 0;
      _h.y = y;
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

  /* erase old cues */
  if (PLUGIN (_y) >= 0 && PLUGIN (_x) >= 0)
    {
      paint_background (root_window, h, plugin);
      paint_background (root_window, v, plugin);
    }

  /* draw cues */
  if (!erase_only)
    {
      gdk_gc_set_function (PLUGIN(gc), GDK_COPY_INVERT);
      gdk_draw_pixbuf (gdkwin, PLUGIN(gc),
		       PLUGIN (root_pixbuf_orig),
		       0, y, 0, y, width, 1, GDK_RGB_DITHER_NONE, 0, 0);
      gdk_draw_pixbuf (gdkwin, PLUGIN(gc),
		       PLUGIN (root_pixbuf_orig),
		       x, 0, x, 0, 1, height, GDK_RGB_DITHER_NONE, 0, 0);
      /* remember old coords to clean up after */
      PLUGIN (_x) = x;
      PLUGIN (_y) = y;
      gdk_gc_set_function (PLUGIN(gc), GDK_COPY);
      /* double buffering off */
      gdk_window_end_paint (gdkwin);
      gdk_region_destroy (union_r);
    }
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

      if (purple_prefs_get_bool (PREF_SHOW_VISUAL_CUES))
	draw_cues (root_window, event->x, event->y, TRUE, plugin);
    }
  else if (event->button == 2 &&	/* hide the current conversation window  */
	   get_receiver_window (plugin) && !PLUGIN (iconified))
    {
      gtk_widget_hide (root_window);

      if (PLUGIN (root_pixbuf_x) != NULL)
	{
	  g_object_unref (PLUGIN (root_pixbuf_x));
	  PLUGIN (root_pixbuf_x) = NULL;
	}
      if (PLUGIN (root_pixbuf_orig) != NULL)
	{
	  g_object_unref (PLUGIN (root_pixbuf_orig));
	  PLUGIN (root_pixbuf_orig) = NULL;
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

	  if (PLUGIN (root_pixbuf_orig) != NULL)
	    {
	      g_object_unref (PLUGIN (root_pixbuf_orig));
	      PLUGIN (root_pixbuf_orig) = NULL;
	    }
	  if (PLUGIN (root_pixbuf_x) != NULL)
	    {
	      g_object_unref (PLUGIN (root_pixbuf_x));
	      PLUGIN (root_pixbuf_x) = NULL;
	    }
	  plugin_stop (plugin);
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

static GdkPixbuf*
extract_capture (PurplePlugin * plugin) {
  GdkPixbuf *capture = NULL;
  
  /* 1/ create our new pixbuf */
  g_assert (PLUGIN (root_pixbuf_orig) != NULL);
  capture =
    gdk_pixbuf_new_subpixbuf (PLUGIN (root_pixbuf_orig),
			      MIN_X (plugin),
			      MIN_Y (plugin),
			      CAPTURE_WIDTH (plugin),
			      CAPTURE_HEIGHT (plugin));
  
  if (capture != NULL) {

    /* 2/ make invisible areas black */
    mask_monitors (capture, 
		   gdk_get_default_root_window (),
		   MIN_X (plugin), MIN_Y (plugin));
    
    /* 3/ add a signature to the bottom-right corner */
    if (purple_prefs_get_bool (PREF_ADD_SIGNATURE))
      {
	GdkPixbuf *sign_pixbuf =
	  gdk_pixbuf_new_from_file (purple_prefs_get_string
				    (PREF_SIGNATURE_FILENAME),
				    NULL);
	if (sign_pixbuf != NULL) {
	  if (!mygdk_pixbuf_check_maxsize
	      (sign_pixbuf, SIGN_MAXWIDTH, SIGN_MAXHEIGHT))
	    {
	      NotifyError (PLUGIN_SIGNATURE_TOOBIG_ERROR, SIGN_MAXWIDTH,
			   SIGN_MAXHEIGHT);
	      purple_prefs_set_string (PREF_SIGNATURE_FILENAME, "");
	    }
	  else
	    mygdk_pixbuf_compose (capture, sign_pixbuf);
	  
	  g_object_unref (sign_pixbuf);
	  sign_pixbuf = NULL;
	}
      }
  }

  if (G_LIKELY(PLUGIN (root_pixbuf_orig) != NULL))
    {
      g_object_unref (PLUGIN (root_pixbuf_orig));
      PLUGIN (root_pixbuf_orig) = NULL;
    }

  return capture;
}

static gboolean
save_capture (PurplePlugin *plugin,
		GdkPixbuf *capture) {
  gchar *basename = NULL;
  gchar *param_name = NULL;
  gchar *param_value = NULL;
  GError *error = NULL;
  GTimeVal g_tv;
  const gchar *const extension =
    purple_prefs_get_string (PREF_IMAGE_TYPE);
  
  if (PLUGIN (capture_path_filename) != NULL)
    g_free (PLUGIN (capture_path_filename));

  if (!strcmp (extension, "png"))
    {
      param_name = g_strdup ("compression");
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
  
  /* create default name */
  g_get_current_time (&g_tv);
  basename =
    g_strdup_printf ("%s_%ld.%s", CAPTURE, g_tv.tv_sec, extension);
  
  /* eventually ask the user for a new name */
  screenshot_maybe_rename (plugin, &basename);
  
  PLUGIN (capture_path_filename) =
    g_build_filename (purple_prefs_get_string (PREF_STORE_FOLDER),
		      basename, NULL);
  g_free (basename);
  basename = NULL;
  
  /* store capture in a file */
  gdk_pixbuf_save (capture, PLUGIN(capture_path_filename), extension,
		   &error, param_name, param_value, NULL);
  
  if (param_name != NULL)
    g_free (param_name);
  if (param_value != NULL)
    g_free (param_value);
  
  if (error != NULL)
    {
      gchar *errmsg_saveto = g_strdup_printf (PLUGIN_SAVE_TO_FILE_ERROR,
					      PLUGIN(capture_path_filename));
      
      NotifyError ("%s\n\n\%s", errmsg_saveto, error->message);
      
      g_free (errmsg_saveto);
      plugin_stop (plugin);
      g_error_free (error);
      return FALSE;
    }
  return TRUE;
}

static gboolean
on_root_window_button_release_cb (GtkWidget * root_window,
				  GdkEventButton * event,
				  PurplePlugin * plugin)
{
  GdkPixbuf *capture = NULL;
 
  /* capture not yet defined */
  if (event->button != 1 || PLUGIN (x2) == -1)
    return TRUE;

  /* come back to the real world */
  gtk_widget_hide (root_window);
  if (PLUGIN (root_pixbuf_x) != NULL)
    {
      g_object_unref (PLUGIN (root_pixbuf_x));
      PLUGIN (root_pixbuf_x) = NULL;
    }
  if (PLUGIN (iconified) && get_receiver_window (plugin))
    {
      gtk_window_deiconify (GTK_WINDOW (get_receiver_window (plugin)));
      PLUGIN (iconified) = FALSE;
    }

  /* process screenshot */
  if ((capture = extract_capture (plugin)) == NULL)
    {
      purple_notify_error (plugin, PLUGIN_NAME, PLUGIN_ERROR,
			   PLUGIN_MEMORY_ERROR);
      plugin_stop (plugin);
    }
  else
    {
      /* capture was successfully stored in file */
      if (save_capture (plugin, capture))
	{
	  CLEAR_CAPTURE_AREA (plugin);
	  g_object_unref (capture);
	  capture = NULL;
	  
	  switch (PLUGIN (send_as))
	    {
	    case SEND_AS_FILE:
	      {
		serv_send_file
		  (purple_account_get_connection
		   (PLUGIN (account)), PLUGIN (name), PLUGIN(capture_path_filename));
		plugin_stop (plugin);
		break;
	      }
	    case SEND_AS_IMAGE:
	      {
		gchar *filedata = NULL;
		size_t size;
		GError *error = NULL;

		if (g_file_get_contents
		    (PLUGIN(capture_path_filename), &filedata, &size,
		     &error) == FALSE)
		  {
		    gchar *errmsg_getdata;
		    if (filedata != NULL)
		      g_free (filedata);

		    errmsg_getdata = g_strdup_printf (PLUGIN_GET_DATA_ERROR,
						      PLUGIN(capture_path_filename));
		    NotifyError ("%s\n\n\%s", errmsg_getdata, error->message);
		    g_free (errmsg_getdata);
		    g_error_free (error);
		  }
		else
		  {
		    gchar *basename = NULL;
		    GtkTextIter iter;
		    GtkTextMark *ins = NULL;
		    gint purple_tmp_id;
		    GtkIMHtml *imhtml = get_receiver_imhtml (PLUGIN (pconv));

		    basename = g_path_get_basename (PLUGIN(capture_path_filename));

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
				     PLUGIN(capture_path_filename));
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
		plugin_stop (plugin);
		break;
	      }
#ifdef ENABLE_UPLOAD
	    case SEND_AS_HTTP_LINK:
	      {
		http_upload_prepare (plugin);
		break;
	      }
	    case SEND_AS_FTP_LINK:
	      {
		ftp_upload_prepare (plugin);
		break;
	      }
#endif
	    }
	}
    }
  return TRUE;
}

static void
on_root_window_map_event_cb (GtkWidget * root_window, GdkEvent * event, PurplePlugin *plugin)
{
 GdkWindow *gdkwin;

 gtk_window_move (GTK_WINDOW (root_window), 0, 0);

#if GTK_CHECK_VERSION(2,14,0)
  gdkwin = gtk_widget_get_window (root_window);
#else
  gdkwin = root_window->window;
#endif

  if (PLUGIN(gc) == NULL)
    PLUGIN(gc) = gdk_gc_new (gdkwin);

  (void) event;
}

static gboolean
on_root_window_expose_cb (GtkWidget * root_window,
			  GdkEventExpose * event, PurplePlugin * plugin)
{
  /* no area is selected */
  if (PLUGIN (x1) == - 1) 
    {
      paint_background (root_window, event->area, plugin);
    } 
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
	  paint_background (root_window, rect, plugin);
	}
      if (background_rectangles != NULL)
	g_free (background_rectangles);

      if (purple_prefs_get_int (PREF_HIGHLIGHT_MODE) == 3)
	{
	  gdk_gc_set_function (PLUGIN(gc), GDK_COPY_INVERT);

	  /* draw selection */
	  gdk_draw_pixbuf (gdkwin, PLUGIN(gc),
			   PLUGIN (root_pixbuf_orig),
			   selection_rectangle.x,
			   selection_rectangle.y,
			   selection_rectangle.x,
			   selection_rectangle.y,
			   selection_rectangle.width,
			   selection_rectangle.height, GDK_RGB_DITHER_NONE, 0,
			   0);
	  gdk_gc_set_function (PLUGIN(gc), GDK_COPY);
	}
      else
	{
	  /* draw selection */
	  gdk_draw_pixbuf (gdkwin, PLUGIN(gc), 
			   PLUGIN (root_pixbuf_orig), 
			   selection_rectangle.x + 1,	/* +1 => avoid borders */
			   selection_rectangle.y + 1,
			   selection_rectangle.x + 1,
			   selection_rectangle.y + 1,
			   selection_rectangle.width - 1,
			   selection_rectangle.height - 1,
			   GDK_RGB_DITHER_NONE, 0, 0);

	  /* highlight borders */
	  gdk_gc_set_function (PLUGIN(gc), GDK_COPY_INVERT);

	  /* north */
	  gdk_draw_pixbuf (gdkwin, PLUGIN(gc),
			   PLUGIN (root_pixbuf_orig),
			   selection_rectangle.x, selection_rectangle.y,
			   selection_rectangle.x, selection_rectangle.y,
			   selection_rectangle.width, 1,
			   GDK_RGB_DITHER_NONE, 0, 0);

	  /* south */
	  gdk_draw_pixbuf (gdkwin, PLUGIN(gc),
			   PLUGIN (root_pixbuf_orig),
			   selection_rectangle.x,
			   selection_rectangle.y +
			   selection_rectangle.height - 1,
			   selection_rectangle.x,
			   selection_rectangle.y +
			   selection_rectangle.height - 1,
			   selection_rectangle.width, 1, GDK_RGB_DITHER_NONE,
			   0, 0);

	  /* west */
	  gdk_draw_pixbuf (gdkwin, PLUGIN(gc),
			   PLUGIN (root_pixbuf_orig),
			   selection_rectangle.x, selection_rectangle.y,
			   selection_rectangle.x, selection_rectangle.y,
			   1, selection_rectangle.height,
			   GDK_RGB_DITHER_NONE, 0, 0);

	  /* est */
	  gdk_draw_pixbuf (gdkwin, PLUGIN(gc),
			   PLUGIN (root_pixbuf_orig),
			   selection_rectangle.x + selection_rectangle.width -
			   1, selection_rectangle.y,
			   selection_rectangle.x + selection_rectangle.width -
			   1, selection_rectangle.y, 1,
			   selection_rectangle.height,
			   GDK_RGB_DITHER_NONE, 0, 0);

	  gdk_gc_set_function (PLUGIN(gc), GDK_COPY);
	}
      /* remember old coords to clear after */
      PLUGIN (_x) = PLUGIN (x2);
      PLUGIN (_y) = PLUGIN (y2);
  }
   return TRUE;
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
      draw_cues (root_window, event->x, event->y, FALSE, plugin);
    }
    return TRUE;
}

static void
on_screen_monitors_changed_cb (GdkScreen * screen, PurplePlugin * plugin)
{
  gtk_widget_set_size_request (PLUGIN (root_window),
			       gdk_screen_get_width (screen),
			       gdk_screen_get_height (screen));

  CLEAR_CAPTURE_AREA (plugin);
}

void
prepare_root_window (PurplePlugin * plugin)
{
  GdkScreen *screen;

  /* from gdk API :
   *   "a screen may consist of multiple monitors which a
   *   re merged to form a large screen area".
   */
  screen = gdk_screen_get_default ();

  /* no toplevel otherwise some desktops (say Xfce4),
   * won't allow us to cover the entire screen. */
  PLUGIN (root_window) = gtk_window_new (GTK_WINDOW_POPUP);

  /* here, gtk_window_fullscreen() won't work 'cos most (every)
     WMs do this on the current monitor only, while we want our
     invisible window to cover the entire GdkScreen. */
  gtk_widget_set_size_request (PLUGIN (root_window),
			       gdk_screen_get_width (screen),
			       gdk_screen_get_height (screen));

  /* not sure this is actually needed */
#if GTK_CHECK_VERSION(2,4,0)
  gtk_window_set_keep_above (GTK_WINDOW (PLUGIN (root_window)), TRUE);
#endif

  /* install callbacks */
  g_signal_connect (GTK_OBJECT (PLUGIN (root_window)), "realize",
		    G_CALLBACK (on_root_window_realize_cb), NULL);
  g_signal_connect (GTK_OBJECT (PLUGIN (root_window)),
		    "button-press-event",
		    G_CALLBACK (on_root_window_button_press_cb), plugin);
  g_signal_connect (GTK_OBJECT (PLUGIN (root_window)),
		    "button-release-event",
		    G_CALLBACK (on_root_window_button_release_cb), plugin);

  g_signal_connect (GTK_OBJECT (PLUGIN (root_window)),
		    "expose-event",
		    G_CALLBACK (on_root_window_expose_cb), plugin);
  g_signal_connect (GTK_OBJECT (PLUGIN (root_window)),
		    "motion-notify-event",
		    G_CALLBACK (on_root_window_motion_notify_cb), plugin);
  g_signal_connect (GTK_OBJECT (PLUGIN (root_window)),
		    "map-event",
		    G_CALLBACK (on_root_window_map_event_cb), plugin);
#ifdef G_OS_WIN32
  /* waiting for signal "monitors-changed" to be implemented
     under Win32 */
  g_signal_connect (G_OBJECT (screen), "size-changed",
		    G_CALLBACK (on_screen_monitors_changed_cb), plugin);
#else
  g_signal_connect (G_OBJECT (screen),
		    "monitors-changed",
		    G_CALLBACK (on_screen_monitors_changed_cb), plugin);
#endif

  CLEAR_CAPTURE_AREA (plugin);
}

/* end of screencap.c */
