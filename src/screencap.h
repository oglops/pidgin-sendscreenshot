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

#ifndef __SCREENCAP_H__
#define __SCREENCAP_H__ 1

#include "main.h"
#include "debug.h"

/* selection border width :
   should we add an option to modify ? */
#define BORDER_WIDTH 1

typedef enum {
  LighenUp=1,
  Darken=2,
  InvertOnly=3,
  BordersOnly=4,
  Grayscale=5
} HighlightMode;

/*
 * If we immediately freeze the screen, then the menuitem we just
 * click on may remain. That's we wait for a small timeout.
 */
#define FREEZE_DESKTOP()\
  purple_timeout_add\
  (MAX(MSEC_TIMEOUT_VAL, purple_prefs_get_int(PREF_WAIT_BEFORE_SCREENSHOT) * 1000), \
   (GSourceFunc) timeout_freeze_screen, plugin)

/* Give back focus to Pidgin. */
#define THAW_DESKTOP()					\
  gtk_widget_hide (PLUGIN (root_events));		\
  gtk_widget_hide (PLUGIN (root_window));		\
  if (PLUGIN (root_pixbuf_x) != NULL)			\
    {							\
      g_object_unref (PLUGIN (root_pixbuf_x));		\
      PLUGIN (root_pixbuf_x) = NULL;			\
    }							\
  if (G_LIKELY (PLUGIN (root_pixbuf_orig) != NULL))	\
    {							\
      g_object_unref (PLUGIN (root_pixbuf_orig));	\
      PLUGIN (root_pixbuf_orig) = NULL;			\
    }							\
  PLUGIN (resize_mode) = ResizeAny;			\
  PLUGIN (resize_allow) = FALSE

#define MIN_X(plugin)\
  MIN(PLUGIN (x1), PLUGIN (x2))
#define MAX_X(plugin)\
  MAX(PLUGIN (x1), PLUGIN (x2))
#define MIN_Y(plugin)\
  MIN(PLUGIN (y1), PLUGIN (y2))
#define MAX_Y(plugin)\
  MAX(PLUGIN (y1), PLUGIN (y2))
#define CAPTURE_WIDTH(plugin)\
  ABS(PLUGIN (x2) - PLUGIN (x1)) + 1
#define CAPTURE_HEIGHT(plugin)\
  ABS(PLUGIN (y2) - PLUGIN (y1)) + 1

/* background = not (roi) */
#define BACKGROUND_PIXBUF\
  PLUGIN (root_pixbuf_x) != NULL ?  PLUGIN (root_pixbuf_x) : PLUGIN (root_pixbuf_orig)

#define CLEAR_CAPTURE_AREA(plugin)\
  PLUGIN (x1) = -1;\
  PLUGIN (y1) = -1;\
  PLUGIN (x2) = -1;\
  PLUGIN (y2) = -1;\
  PLUGIN (_x) = -1;\
  PLUGIN (_y) = -1

/* prototypes */
void prepare_root_window (PurplePlugin * plugin);
guint timeout_freeze_screen (PurplePlugin * plugin);

#endif /* __SCREENCAP_H__ */

/* end of screencap.h */
