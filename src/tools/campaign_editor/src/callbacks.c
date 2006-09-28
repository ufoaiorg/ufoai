/**
 * @file callbacks.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <string.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

static int x, y;

/**
 * @brief
 */
void on_quit_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_exit(0);
}

/**
 * @brief
 */
void on_info_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	create_about_box();
}

/**
 * @brief
 * @param[in] button
 * @param[in] user_data
 */
void mission_save (GtkButton *button, gpointer user_data)
{
	GtkTextBuffer *txtbuf;
	char buffer[2048];

	snprintf(buffer, sizeof(buffer),
		"mission GIVE_ME_A_NAME\n"
		"{\n"
		"\tlocation\t\"_%s\"\n"
		"\ttype\t\"_%s\"\n"
		"\ttext\t\"_%s\"\n"
		"\tmap\t%s\n"
		"\tparam\t\"%s\"\n"
		"\tnation\t\"%s\"\n"
		"\tstoryrelated\t%s\n"
		"\tonwin\t\"%s\"\n"
		"\tonlose\t\"%s\"\n"
		"\tcommands\t\"%s\"\n"
		"\tmusic\t%s\n"
		"\tpos\t\"%i %i\"\n"
		"\taliens\t%s\n"
		"\tcivilians\t%s\n"
		"\trecruits\t%s\n"
		"\talienteam\t%s\n"
		"\tcivteam\t%s\n"
		"\talienequip\t%s\n"
		"\t$win\t%s\n"
		"\t$alien\t%s\n"
		"\t$civilian\t%s\n"
		"}\n",
		get_entry_txt("location_mission"),
		get_entry_txt("type_mission"),
		get_textfield_txt("text_mission"),
		get_selectbox_txt("combo_map"),
		get_entry_txt("map_assembly_param_entry"),
		get_entry_txt("nation_entry"),
		bool_translate(get_checkbutton_txt("storyrelated_checkbutton")),
		get_entry_txt("onwin_entry"),
		get_entry_txt("onlose_entry"),
		get_entry_txt("commands_entry"),
		get_selectbox_txt("combo_music"),
		x, y,
		get_selectbox_txt("combo_aliens"),
		get_selectbox_txt("combo_civilians"),
		get_selectbox_txt("combo_recruits"),
		get_entry_txt("alienteam_entry"),
		get_entry_txt("civteam_entry"),
		get_entry_txt("alien_equip_entry"),
		get_entry_txt("credits_win_entry"),
		get_entry_txt("credits_alien_entry"),
		get_entry_txt("credits_civ_entry")
	);
	gtk_widget_hide( mission_dialog );
	gtk_widget_show( mis_txt );
	txtbuf = gtk_text_buffer_new(NULL);
	gtk_text_buffer_insert_at_cursor(txtbuf, buffer, -1);
	gtk_text_view_set_buffer(mission_txt, txtbuf);
}

/**
 * @brief
 * @param[in] widget
 * @param[in] event
 */
void button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	float xf, yf;

	xf = (0.0f-(float)event->x / 2048 + 0.5) * 360.0;
	yf = (0.0f-(float)event->x / 1024 + 0.5) * 180.0;

	while (xf > 180.0)
		xf -= 360.0;
	while (yf < -180.0)
		yf += 360.0;

	x = xf;
	y = yf;

	gtk_widget_show (mission_dialog);

/*	g_print("click: long: %i - lat: %i, %i\n", x, y, event->state);*/
}

/**
 * @brief
 * @param[in] widget
 * @param[in] event
 */
void motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
}

/**
 * @brief
 * @param[in] widget
 * @param[in] event
 */
void button_mission_dialog_cancel (GtkWidget *widget, GdkEventButton *event)
{
	gtk_widget_hide( mission_dialog );
}

/**
 * @brief
 * @param[in] widget
 * @param[in] event
 */
void button_mis_txt_cancel (GtkWidget *widget, GdkEventButton *event)
{
	gtk_widget_hide( mis_txt );
}
