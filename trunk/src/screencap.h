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

#define CLEAR_CAPTURE_AREA(plugin)\
  PLUGIN (x1) = -1;\
  PLUGIN (y1) = -1;\
  PLUGIN (x2) = -1;\
  PLUGIN (y2) = -1;\
  PLUGIN (_x) = -1;\
  PLUGIN (_y) = -1

void
prepare_root_window (PurplePlugin * plugin);

guint
timeout_freeze_screen (PurplePlugin * plugin);

#endif

/* end of screencap.h */
