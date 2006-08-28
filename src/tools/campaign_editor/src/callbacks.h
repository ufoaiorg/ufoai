/**
 * @file callbacks.h
 * @brief
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <gtk/gtk.h>

void on_quit_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_info_activate (GtkMenuItem *menuitem, gpointer user_data);
void mission_save (GtkButton *button, gpointer user_data);
void motion_notify_event (GtkWidget *widget, GdkEventMotion *event);
void button_press_event (GtkWidget *widget, GdkEventButton *event);
void button_mis_txt_cancel (GtkWidget *widget, GdkEventButton *event);
void button_mission_dialog_cancel (GtkWidget *widget, GdkEventButton *event);

