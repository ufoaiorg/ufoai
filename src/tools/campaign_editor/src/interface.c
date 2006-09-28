/**
 * @file interface.c
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <dirent.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "scripts.h"

#define GLADE_HOOKUP_OBJECT(component,widget,name) \
  g_object_set_data_full (G_OBJECT (component), name, \
    gtk_widget_ref (widget), (GDestroyNotify) gtk_widget_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name) \
  g_object_set_data (G_OBJECT (component), name, widget)

GtkWidget *mission_dialog;

/**
 * @brief
 */
GtkWidget* create_campaign_editor (void)
{
	GtkWidget *editor_vbox;
	GtkWidget *menubar;
	AtkObject *atko;
	GtkWidget *file;
	GtkWidget *file_menu;
	GtkWidget *menu_item_quit;
	GtkWidget *help;
	GtkWidget *help_menu;
	GtkWidget *menu_item_info;
	GtkWidget *notebook;
	GtkWidget *geoscape_scrolledwindow;
	GtkWidget *geoscape_viewport;
	GtkWidget *geoscape_image;
	GtkWidget *geoscape_notebook_label;
	GtkWidget *campaign_notebook_label;
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new ();

	campaign_editor = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (campaign_editor), Q_("Campaign editor"));
	gtk_window_set_default_size (GTK_WINDOW (campaign_editor), 800, 600);

	gtk_signal_connect (GTK_OBJECT (campaign_editor), "destroy", GTK_SIGNAL_FUNC (gtk_exit), NULL);

	editor_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (editor_vbox);
	gtk_container_add (GTK_CONTAINER (campaign_editor), editor_vbox);

	menubar = gtk_menu_bar_new ();
	gtk_widget_show (menubar);
	gtk_box_pack_start (GTK_BOX (editor_vbox), menubar, FALSE, FALSE, 0);

	file = gtk_menu_item_new_with_mnemonic (_("_File"));
	gtk_widget_show (file);
	gtk_container_add (GTK_CONTAINER (menubar), file);

	file_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (file), file_menu);

	menu_item_quit = gtk_image_menu_item_new_from_stock ("gtk-quit", accel_group);
	gtk_widget_show (menu_item_quit);
	gtk_container_add (GTK_CONTAINER (file_menu), menu_item_quit);

	help = gtk_menu_item_new_with_mnemonic (_("_Help"));
	gtk_widget_show (help);
	gtk_container_add (GTK_CONTAINER (menubar), help);

	help_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (help), help_menu);

	menu_item_info = gtk_menu_item_new_with_mnemonic (_("_Info"));
	gtk_widget_show (menu_item_info);
	gtk_container_add (GTK_CONTAINER (help_menu), menu_item_info);

	atko = gtk_widget_get_accessible (menubar);
	atk_object_set_name (atko, _("About"));

	atko = gtk_widget_get_accessible (file);
	atk_object_set_name (atko, _("File"));

	atko = gtk_widget_get_accessible (menu_item_quit);
	atk_object_set_name (atko, _("Quit"));

	atko = gtk_widget_get_accessible (menu_item_info);
	atk_object_set_name (atko, _("About"));

	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_box_pack_start (GTK_BOX (editor_vbox), notebook, TRUE, TRUE, 0);

	geoscape_scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (geoscape_scrolledwindow);
	gtk_container_add (GTK_CONTAINER (notebook), geoscape_scrolledwindow);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (geoscape_scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	geoscape_viewport = gtk_viewport_new (NULL, NULL);
	gtk_widget_show (geoscape_viewport);
	gtk_container_add (GTK_CONTAINER (geoscape_scrolledwindow), geoscape_viewport);

	/* FIXME: First show a campaign selection dialog */
	/* then load the given campaign map */
	geoscape_image = create_pixmap (campaign_editor, "map_earth_day.jpg");
	gtk_widget_show (geoscape_image);
	gtk_container_add (GTK_CONTAINER (geoscape_viewport), geoscape_image);

	gtk_signal_connect (GTK_OBJECT (geoscape_viewport), "motion_notify_event",
						(GtkSignalFunc) motion_notify_event, NULL);
	gtk_signal_connect (GTK_OBJECT (geoscape_viewport), "button_press_event",
						(GtkSignalFunc) button_press_event, NULL);

	gtk_widget_set_events (geoscape_viewport, GDK_EXPOSURE_MASK
						| GDK_LEAVE_NOTIFY_MASK
						| GDK_BUTTON_PRESS_MASK
						| GDK_POINTER_MOTION_MASK
						| GDK_POINTER_MOTION_HINT_MASK);

	geoscape_notebook_label = gtk_label_new (_("Geoscape"));
	gtk_widget_show (geoscape_notebook_label);
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0), geoscape_notebook_label);

/*	campaign_notebook_label = gtk_label_new (_("Campaign"));
	gtk_widget_show (campaign_notebook_label);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0), campaign_notebook_label);*/

	g_signal_connect ((gpointer) menu_item_quit, "activate",
						G_CALLBACK (on_quit_activate),
						NULL);
	g_signal_connect ((gpointer) menu_item_info, "activate",
						G_CALLBACK (on_info_activate),
						NULL);

	/* Store pointers to all widgets, for use by lookup_widget(). */
	GLADE_HOOKUP_OBJECT_NO_REF (campaign_editor, campaign_editor, "campaign_editor");
	GLADE_HOOKUP_OBJECT (campaign_editor, editor_vbox, "editor_vbox");
	GLADE_HOOKUP_OBJECT (campaign_editor, menubar, "menubar");
	GLADE_HOOKUP_OBJECT (campaign_editor, file, "file");
	GLADE_HOOKUP_OBJECT (campaign_editor, file_menu, "file_menu");
	GLADE_HOOKUP_OBJECT (campaign_editor, menu_item_quit, "menu_item_quit");
	GLADE_HOOKUP_OBJECT (campaign_editor, help, "help");
	GLADE_HOOKUP_OBJECT (campaign_editor, help_menu, "help_menu");
	GLADE_HOOKUP_OBJECT (campaign_editor, menu_item_info, "menu_item_info");
	GLADE_HOOKUP_OBJECT (campaign_editor, notebook, "notebook");
	GLADE_HOOKUP_OBJECT (campaign_editor, geoscape_scrolledwindow, "geoscape_scrolledwindow");
	GLADE_HOOKUP_OBJECT (campaign_editor, geoscape_viewport, "geoscape_viewport");
	GLADE_HOOKUP_OBJECT (campaign_editor, geoscape_image, "geoscape_image");
	GLADE_HOOKUP_OBJECT (campaign_editor, geoscape_notebook_label, "geoscape_notebook_label");
	GLADE_HOOKUP_OBJECT (campaign_editor, campaign_notebook_label, "geoscape_campaign_label");

	gtk_window_add_accel_group (GTK_WINDOW (campaign_editor), accel_group);

	return campaign_editor;
}

/**
 * @brief
 */
void create_music_widget(char* string, GtkWidget* table, int col1, int col2, int row1, int row2)
{
	struct dirent *dir_info;
	DIR *dir;
	char dirname[128];
	char buffer[128];
	GtkWidget* w;
	w = gtk_combo_box_new_text();
	gtk_widget_show (w);
	gtk_table_attach (GTK_TABLE (table), w, col1, col2, row1, row2,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

	if ( (dir = opendir("base/music")) == NULL )
		fprintf(stderr, "Could not read base/music\n");
	while ( dir && (dir_info = readdir(dir)) != NULL ) {
		if ( !file_ext(dir_info->d_name, ".ogg") )
			continue;
		file_strip_ext(dir_info->d_name, buffer);
		gtk_combo_box_append_text (GTK_COMBO_BOX (w), buffer);
	}
	if (dir)
		closedir(dir);
	gtk_combo_box_set_active( GTK_COMBO_BOX (w), 0 );
	GLADE_HOOKUP_OBJECT (mission_dialog, w, string);
}

/**
 * @brief
 */
void create_map_widget(char* string, GtkWidget* table, int col1, int col2, int row1, int row2, int active)
{
	struct dirent *dir_info;
	DIR *dir;
	char dirname[128];
	char buffer[128];
	GtkWidget* w;
	w = gtk_combo_box_new_text();
	gtk_widget_show (w);
	gtk_table_attach (GTK_TABLE (table), w, col1, col2, row1, row2,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	if ( (dir = opendir("base/maps")) == NULL )
		fprintf(stderr, "Could not read base/maps\n");
	if ( dir ) {
		while ( (dir_info = readdir(dir)) != NULL ) {
			/* only the day versions, otherwise we get the maps twice */
			if ( !file_ext(dir_info->d_name, "d.bsp") )
				continue;
			file_strip_ext(dir_info->d_name, buffer);
			buffer[strlen(buffer)-1] = 0;
			/* check whether night version exists, too */
			if ( !file_exists(va("base/maps/%sn.bsp", buffer)))
				continue;
			gtk_combo_box_append_text (GTK_COMBO_BOX (w), buffer);
		}
		rewinddir(dir);
		while ( (dir_info = readdir(dir)) != NULL ) {
			/* only the day versions, otherwise we get the maps twice */
			if ( !file_ext(dir_info->d_name, "d.ump") )
				continue;
			file_strip_ext(dir_info->d_name, buffer);
			buffer[strlen(buffer)-1] = 0;
			/* check whether night version exists, too */
			if ( !file_exists(va("base/maps/%sn.ump", buffer)))
				continue;
			gtk_combo_box_append_text (GTK_COMBO_BOX (w), va("+%s", buffer) );
		}
		closedir(dir);
	}

	gtk_combo_box_set_active( GTK_COMBO_BOX (w), active );
	GLADE_HOOKUP_OBJECT (mission_dialog, w, string);
}

/**
 * @brief
 */
void create_entry(char* string, char* defaultValue, GtkWidget* table, int col1, int col2, int row1, int row2)
{
	GtkWidget* w;
	w = gtk_entry_new_with_max_length (MAX_VAR);
	gtk_widget_show (w);
	gtk_table_attach (GTK_TABLE (table), w, col1, col2, row1, row2,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_entry_set_invisible_char (GTK_ENTRY (w), 9679);
	if (defaultValue)
		gtk_entry_set_text (GTK_ENTRY(w), defaultValue);
	GLADE_HOOKUP_OBJECT (mission_dialog, w, string);
}

/**
 * @brief
 */
void create_label(char* string, GtkWidget* table, int col1, int col2, int row1, int row2)
{
	GtkWidget* w;
	w = gtk_label_new(Q_(string));
	gtk_widget_show(w);
	gtk_table_attach(GTK_TABLE (table), w, col1, col2, row1, row2,
						(GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL (w), TRUE);
	gtk_misc_set_alignment(GTK_MISC (w), 0, 0.5);
}

/**
 * @brief
 */
void create_select_box_from_script_data(char* string, char* script_type, char* active_string, GtkWidget* table, int col1, int col2, int row1, int row2)
{
	struct dirent *dir_info;
	DIR *dir;
	char filename[128];
	char *token;
	char *buffer = NULL;
	char *name = NULL;
	int i = 0, len = 0;
	int active = -1;
	GtkWidget* w;

	w = gtk_combo_box_new_text();
	gtk_widget_show (w);
	gtk_table_attach (GTK_TABLE (table), w, col1, col2, row1, row2,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

	if ( (dir = opendir("base/ufos")) == NULL )
		fprintf(stderr, "Could not read base/ufos\n");
	if (dir) {
		while ( (dir_info = readdir(dir)) != NULL ) {
			/* only the *.ufo files */
			if ( !file_ext(dir_info->d_name, ".ufo") )
				continue;
			snprintf(filename, sizeof(filename), "base/ufos/%s", dir_info->d_name);
			len = file_load(filename);
			buffer = (char*)globalFileBuffer;
			if (!globalFileBuffer) {
				printf("failed to load %s\n", dir_info->d_name);
				continue;
#if 0
			} else {
				printf("loaded %s with size %i\n", dir_info->d_name, len);
#endif
			}
			token = loadscript(&name, &buffer);
			while (token && buffer) {
				if (!strcmp(token, script_type)) {
					printf("(%s-)name: %s\n", script_type, name);
					if (!strcmp(name, active_string)) {
						if (active > -1)
							fatal_error(va("found two %s with same name (%s)\n", script_type, name));
						active = i;
					}
					i++;
					// FIXME: This crashes the program - don't know why
					gtk_combo_box_append_text (GTK_COMBO_BOX (w), name);
				}
				if (buffer)
					token = loadscript(&name, &buffer);
			}
			file_close();
		}
		closedir(dir);
	}
	printf("found: %i\n", i);
	if (active < 0)
		active = 0;
	gtk_combo_box_set_active( GTK_COMBO_BOX (w), active);
	GLADE_HOOKUP_OBJECT (mission_dialog, w, string);
}

/**
 * @brief
 */
GtkWidget* create_mission_dialog (void)
{
	GtkWidget *entry, *checkbox, *select, *frame, *label, *vbox, *innervbox, *table, *text;
	GtkWidget *mission_action_area;
	GtkWidget *cancel_button, *ok_button;
	int i;

	/* just init this extern var */
	globalFileBuffer = NULL;

	mission_dialog = gtk_dialog_new ();
	gtk_window_set_title (GTK_WINDOW (mission_dialog), Q_("Mission"));
	gtk_window_set_modal (GTK_WINDOW (mission_dialog), TRUE);
	gtk_window_set_default_size (GTK_WINDOW (mission_dialog), 320, 260);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (mission_dialog), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (mission_dialog), TRUE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (mission_dialog), TRUE);
	gtk_window_set_type_hint (GTK_WINDOW (mission_dialog), GDK_WINDOW_TYPE_HINT_DIALOG);

	g_signal_connect (GTK_OBJECT (mission_dialog), "delete_event", G_CALLBACK (button_mission_dialog_cancel), NULL);

	vbox = GTK_DIALOG (mission_dialog)->vbox;
	gtk_widget_show (vbox);

	innervbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (innervbox);
	gtk_box_pack_start (GTK_BOX (vbox), innervbox, TRUE, TRUE, 0);

	frame = gtk_frame_new (NULL);
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (innervbox), frame, TRUE, TRUE, 0);

	table = gtk_table_new (3, 2, FALSE);
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (frame), table);

	create_label("Recruits", table, 0, 1, 0, 1);
	create_label("Civilians", table, 0, 1, 1, 2);
	create_label("Aliens", table, 0, 1, 2, 3);

	select = gtk_combo_box_new_text ();
	gtk_widget_show (select);
	gtk_table_attach (GTK_TABLE (table), select, 1, 2, 0, 1,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	for (i=1; i<=8; i++)
		gtk_combo_box_append_text (GTK_COMBO_BOX (select), va("%i", i));
	gtk_combo_box_set_active( GTK_COMBO_BOX (select), 5 );
	GLADE_HOOKUP_OBJECT (mission_dialog, select, "combo_recruits");

	select = gtk_combo_box_new_text ();
	gtk_widget_show (select);
	gtk_table_attach (GTK_TABLE (table), select, 1, 2, 1, 2,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	for (i=1; i<=8; i++)
		gtk_combo_box_append_text (GTK_COMBO_BOX (select), va("%i", i));
	gtk_combo_box_set_active( GTK_COMBO_BOX (select), 2 );
	GLADE_HOOKUP_OBJECT (mission_dialog, select, "combo_civilians");

	select = gtk_combo_box_new_text ();
	gtk_widget_show (select);
	gtk_table_attach (GTK_TABLE (table), select, 1, 2, 2, 3,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	for (i=1; i<=8; i++)
		gtk_combo_box_append_text (GTK_COMBO_BOX (select), va("%i", i));
	gtk_combo_box_set_active( GTK_COMBO_BOX (select), 5 );
	GLADE_HOOKUP_OBJECT (mission_dialog, select, "combo_aliens");

	label = gtk_label_new (_("Actors"));
	gtk_widget_show (label);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);

	frame = gtk_frame_new (NULL);
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (innervbox), frame, TRUE, TRUE, 0);

	table = gtk_table_new (9, 2, FALSE);
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (frame), table);

	create_label("Map", table, 0, 1, 0, 1);
	create_label("Map Assembly Param", table, 0, 1, 1, 2);
	create_label("Music", table, 0, 1, 2, 3);
	create_label("Text", table, 0, 1, 3, 4);
	create_label("Location", table, 0, 1, 4, 5);
	create_label("Type", table, 0, 1, 5, 6);
	create_label("Story related mission", table, 0, 1, 6, 7);
	create_label("Activate Trigger", table, 0, 1, 7, 8);
	create_label("OnWin Trigger", table, 0, 1, 8, 9);
	create_label("OnLose Trigger", table, 0, 1, 9, 10);

	create_map_widget("combo_map", table, 1, 2, 0, 1, 0);
	create_entry("map_assembly_param_entry", NULL, table, 1, 2, 1, 2);
	create_music_widget("combo_music", table, 1, 2, 2, 3);
	create_entry("text_mission", "Protect inhabitants", table, 1, 2, 3, 4);
	create_entry("map_assembly_param_entry", "TYPE_LOCATION", table, 1, 2, 4, 5);
	create_entry("type_mission", "Terror Attack", table, 1, 2, 5, 6);

	checkbox = gtk_check_button_new();
	gtk_widget_show (checkbox);
	gtk_table_attach (GTK_TABLE (table), checkbox, 1, 2, 6, 7,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	GLADE_HOOKUP_OBJECT (mission_dialog, checkbox, "storyrelated_checkbutton");

	create_entry("commands_entry", NULL, table, 1, 2, 7, 8);
	create_entry("onwin_entry", NULL, table, 1, 2, 8, 9);
	create_entry("onlose_entry", NULL, table, 1, 2, 9, 10);

	label = gtk_label_new (_("Mission"));
	gtk_widget_show (label);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);

	/* new frame */
	frame = gtk_frame_new (NULL);
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (innervbox), frame, TRUE, TRUE, 0);

	/* new table */
	table = gtk_table_new (7, 2, FALSE);
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (frame), table);

	create_label("Alienteam", table, 0, 1, 0, 1);
	create_label("Civilianteam", table, 0, 1, 1, 2);
	create_label("Alienequipment", table, 0, 1, 2, 3);
	create_label("Nation", table, 0, 1, 3, 4);
	create_label("Credits on win", table, 0, 1, 4, 5);
	create_label("Credits for rescued civilians", table, 0, 1, 5, 6);
	create_label("Credits for killed aliens", table, 0, 1, 6, 7);

	create_select_box_from_script_data("alienteam_entry", "team", "ortnok", table, 1, 2, 0, 1);
	create_select_box_from_script_data("civteam_entry", "team", "european", table, 1, 2, 1, 2);
	create_select_box_from_script_data("alien_equip_entry", "equipment", "campaign_alien", table, 1, 2, 2, 3);
	create_select_box_from_script_data("nation_entry", "nation", "europa", table, 1, 2, 3, 4);
	create_entry("credits_win_entry", "1000", table, 1, 2, 4, 5);
	create_entry("credits_civ_entry", "500", table, 1, 2, 5, 6);
	create_entry("credits_alien_entry", "100", table, 1, 2, 6, 7);

	label = gtk_label_new (_("Other"));
	gtk_widget_show (label);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);

	mission_action_area = GTK_DIALOG (mission_dialog)->action_area;
	gtk_widget_show (mission_action_area);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (mission_action_area), GTK_BUTTONBOX_END);

	cancel_button = gtk_button_new_from_stock ("gtk-cancel");
	gtk_widget_show (cancel_button);
	gtk_dialog_add_action_widget (GTK_DIALOG (mission_dialog), cancel_button, 0);

	ok_button = gtk_button_new_from_stock ("gtk-apply");
	gtk_widget_show (ok_button);
	gtk_dialog_add_action_widget (GTK_DIALOG (mission_dialog), ok_button, 0);

	g_signal_connect ((gpointer) cancel_button, "clicked",
						G_CALLBACK (button_mission_dialog_cancel),
						NULL);
	g_signal_connect ((gpointer) cancel_button, "pressed",
						G_CALLBACK (button_mission_dialog_cancel),
						NULL);
	g_signal_connect ((gpointer) ok_button, "clicked",
						G_CALLBACK (mission_save),
						NULL);
	g_signal_connect ((gpointer) ok_button, "pressed",
						G_CALLBACK (mission_save),
						NULL);

	return mission_dialog;
}

/**
 * @brief
 */
GtkWidget* create_mis_txt (void)
{
	GtkWidget *mis_txt_vbox;
	GtkWidget *mis_txt_action_area;
	GtkWidget *mis_txt_close;
	GtkWidget *mis_txt;

	mis_txt = gtk_dialog_new ();
	gtk_window_set_default_size (GTK_WINDOW (mis_txt), 480, 300);
	gtk_window_set_type_hint (GTK_WINDOW (mis_txt), GDK_WINDOW_TYPE_HINT_DIALOG);

	g_signal_connect (GTK_OBJECT (mis_txt), "delete_event", G_CALLBACK (button_mis_txt_cancel), NULL);

	mis_txt_vbox = GTK_DIALOG (mis_txt)->vbox;
	gtk_widget_show (mis_txt_vbox);

	mission_txt = gtk_text_view_new ();
	gtk_widget_show (mission_txt);
	gtk_box_pack_start (GTK_BOX (mis_txt_vbox), mission_txt, TRUE, TRUE, 0);

	mis_txt_action_area = GTK_DIALOG (mis_txt)->action_area;
	gtk_widget_show (mis_txt_action_area);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (mis_txt_action_area), GTK_BUTTONBOX_END);

	mis_txt_close = gtk_button_new_with_mnemonic (Q_("Close"));
	gtk_dialog_add_action_widget (GTK_DIALOG (mis_txt), mis_txt_close, 0);
	gtk_widget_show (mis_txt_close);

	g_signal_connect_swapped ((gpointer) mis_txt_close, "clicked",
						G_CALLBACK (button_mis_txt_cancel),
						NULL);
	g_signal_connect_swapped ((gpointer) mis_txt_close, "pressed",
						G_CALLBACK (button_mis_txt_cancel),
						NULL);

	/* Store pointers to all widgets, for use by lookup_widget(). */
	GLADE_HOOKUP_OBJECT_NO_REF (mis_txt, mis_txt, "mis_txt");
	GLADE_HOOKUP_OBJECT (mis_txt, mission_txt, "mission_txt");
/*	GLADE_HOOKUP_OBJECT (mis_txt, mis_txt_close, "mis_txt_close");*/

	return mis_txt;
}

/**
 * @brief
 */
void create_about_box (void)
{
	const gchar *authors[] = {
		"Martin Gerhardy (mattn/tlh2000)",
		NULL
	};

	const gchar *license =
	"This library is free software; you can redistribute it and/or\n"
	"modify it under the terms of the GNU Library General Public License as\n"
	"published by the Free Software Foundation; either version 2 of the\n"
	"License, or (at your option) any later version.\n"
	"\n"
	"This library is distributed in the hope that it will be useful,\n"
	"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
	"Library General Public License for more details.\n"
	"\n"
	"You should have received a copy of the GNU Library General Public\n"
	"License along with the Gnome Library; see the file COPYING.LIB.  If not,\n"
	"write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,\n"
	"Boston, MA 02111-1307, USA.\n";

	gtk_about_dialog_new();
	/* gtk_about_dialog_set_authors ((GtkAboutDialog*)aboutdialog, authors); */

	gtk_show_about_dialog ( NULL, // or the parent window
						"name", NAME,
						"version", VERSION,
						"website", WEBSITE,
						"license", license,
						"copyright", "(C) 2002-2006 The UFO:AI Team",
						"comments", "Program to generate new campaigns",
						"authors", authors,
						NULL); // end on NULL, for end of list
}
