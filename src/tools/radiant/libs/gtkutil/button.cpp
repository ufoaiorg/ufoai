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

#include "button.h"

#include <gtk/gtkradiobutton.h>

#include "stream/textstream.h"
#include "stream/stringstream.h"
#include "generic/callback.h"

#include "image.h"
#include "pointer.h"

void button_connect_callback (GtkButton* button, const Callback& callback)
{
	g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK(callback.getThunk()), callback.getEnvironment());
}

void button_set_icon (GtkButton* button, const std::string& icon)
{
	GtkWidget* image = gtkutil::getImage(icon);
	gtk_widget_show(image);
	gtk_container_add(GTK_CONTAINER(button), image);
}

void toggle_button_set_active_no_signal (GtkToggleButton* button, gboolean active)
{
	guint handler_id = gpointer_to_int(g_object_get_data(G_OBJECT(button), "handler"));
	g_signal_handler_block(G_OBJECT(button), handler_id);
	gtk_toggle_button_set_active(button, active);
	g_signal_handler_unblock(G_OBJECT(button), handler_id);
}

inline GtkToggleButton* radio_button_get_nth (GtkRadioButton* radio, int index)
{
	GSList *group = gtk_radio_button_group(radio);
	return GTK_TOGGLE_BUTTON(g_slist_nth_data(group, g_slist_length(group) - index - 1));
}

void radio_button_set_active (GtkRadioButton* radio, int index)
{
	gtk_toggle_button_set_active(radio_button_get_nth(radio, index), TRUE);
}

void radio_button_set_active_no_signal (GtkRadioButton* radio, int index)
{
	{
		for (GSList* l = gtk_radio_button_get_group(radio); l != 0; l = g_slist_next(l)) {
			g_signal_handler_block(G_OBJECT(l->data), gpointer_to_int(g_object_get_data(G_OBJECT(l->data), "handler")));
		}
	}
	radio_button_set_active(radio, index);
	{
		for (GSList* l = gtk_radio_button_get_group(radio); l != 0; l = g_slist_next(l)) {
			g_signal_handler_unblock(G_OBJECT(l->data),
					gpointer_to_int(g_object_get_data(G_OBJECT(l->data), "handler")));
		}
	}
}

int radio_button_get_active (GtkRadioButton* radio)
{
	GSList *group = gtk_radio_button_group(radio);
	int index = g_slist_length(group) - 1;
	for (; group != 0; group = g_slist_next(group)) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(group->data))) {
			break;
		} else {
			index--;
		}
	}
	return index;
}
