/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "cursor.h"

#include "../../../../shared/shared.h"
#include "stream/textstream.h"

#include <gdk/gdk.h>

#ifdef _WIN32

#include <gdk/gdkwin32.h>

void Sys_GetCursorPos(GtkWindow* window, int* x, int* y) {
	POINT pos;
	GetCursorPos(&pos);
	ScreenToClient((HWND)GDK_WINDOW_HWND(gtk_widget_get_window(GTK_WIDGET(window))), &pos);
	*x = pos.x;
	*y = pos.y;
}

void Sys_SetCursorPos(GtkWindow* window, int x, int y) {
	POINT pos;
	pos.x = x;
	pos.y = y;
	ClientToScreen((HWND)GDK_WINDOW_HWND(gtk_widget_get_window(GTK_WIDGET(window))), &pos);
	SetCursorPos(pos.x, pos.y);
}

#else

#include <gdk/gdkx.h>

void Sys_GetCursorPos (GtkWindow* window, int* x, int* y)
{
	gdk_display_get_pointer(gdk_display_get_default(), 0, x, y, 0);
}

void Sys_SetCursorPos (GtkWindow* window, int x, int y)
{
	XWarpPointer(gdk_x11_display_get_xdisplay(gdk_display_get_default()), None, GDK_ROOT_WINDOW(), 0, 0, 0, 0, x, y);
}

#endif
