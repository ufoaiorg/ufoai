/*
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

#include "ufoai_gtk.h"

#include <gtk/gtk.h>

/**
 * GTK callback functions
 */

class UFOAIGtk
{
	GtkWindow* m_gtk_window;
public:
	UFOAIGtk(void* gtk_window) : m_gtk_window((GtkWindow*)gtk_window)
	{
	}
};

/**
 * @brief If you return FALSE in the "delete_event" signal handler,
 * GTK will emit the "destroy" signal. Returning TRUE means
 * you don't want the window to be destroyed.
 * This is useful for popping up 'are you sure you want to quit?'
 * type dialogs.
 */
static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	return FALSE;
}

/**
 * @brief destroy widget if destroy signal is passed to widget
 */
static void destroy(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(widget);
}

/**
 * @brief function for close button to destroy the toplevel widget
 */
static void close_window(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(gtk_widget_get_toplevel(widget));
}
