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

#define GLADE_HOOKUP_OBJECT(component,widget,name) \
  g_object_set_data_full (G_OBJECT (component), name, \
    gtk_widget_ref (widget), (GDestroyNotify) gtk_widget_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name) \
  g_object_set_data (G_OBJECT (component), name, widget)

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
	GtkWidget *geoscape_notebook;
	GtkWidget *geoscape_scrolledwindow;
	GtkWidget *geoscape_viewport;
	GtkWidget *geoscape_image;
	GtkWidget *geoscape_notebook_label;
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

	geoscape_notebook = gtk_notebook_new ();
	gtk_widget_show (geoscape_notebook);
	gtk_box_pack_start (GTK_BOX (editor_vbox), geoscape_notebook, TRUE, TRUE, 0);

	geoscape_scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (geoscape_scrolledwindow);
	gtk_container_add (GTK_CONTAINER (geoscape_notebook), geoscape_scrolledwindow);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (geoscape_scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	geoscape_viewport = gtk_viewport_new (NULL, NULL);
	gtk_widget_show (geoscape_viewport);
	gtk_container_add (GTK_CONTAINER (geoscape_scrolledwindow), geoscape_viewport);

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

	geoscape_notebook_label = gtk_label_new ("Geoscape");
	gtk_widget_show (geoscape_notebook_label);
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (geoscape_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (geoscape_notebook), 0), geoscape_notebook_label);

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
	GLADE_HOOKUP_OBJECT (campaign_editor, geoscape_notebook, "geoscape_notebook");
	GLADE_HOOKUP_OBJECT (campaign_editor, geoscape_scrolledwindow, "geoscape_scrolledwindow");
	GLADE_HOOKUP_OBJECT (campaign_editor, geoscape_viewport, "geoscape_viewport");
	GLADE_HOOKUP_OBJECT (campaign_editor, geoscape_image, "geoscape_image");
	GLADE_HOOKUP_OBJECT (campaign_editor, geoscape_notebook_label, "geoscape_notebook_label");

	gtk_window_add_accel_group (GTK_WINDOW (campaign_editor), accel_group);

	return campaign_editor;
}

GtkWidget* create_mission_dialog (void)
{
	GtkWidget *mission_vbox;
	GtkWidget *mission_variables_vbox;
	GtkWidget *actors_frame;
	GtkWidget *actors_table;
	GtkWidget *recruits;
	GtkWidget *civilians;
	GtkWidget *aliens;
	GtkWidget *combo_recruits;
	GtkWidget *combo_civilians;
	GtkWidget *combo_aliens;
	GtkWidget *label1;
	GtkWidget *mission_frame;
	GtkWidget *mission_table;
	GtkWidget *map_label;
	GtkWidget *music_label;
	GtkWidget *text_label;
	GtkWidget *combo_map;
	GtkWidget *combo_music;
	GtkWidget *text_mission;
	GtkWidget *label2;
	GtkWidget *other_frame;
	GtkWidget *other_table;
	GtkWidget *alienteam_label;
	GtkWidget *civteam_label;
	GtkWidget *alienequip_label;
	GtkWidget *win_label;
	GtkWidget *credits_civ_label;
	GtkWidget *credits_aliens_label;
	GtkWidget *alienteam_entry;
	GtkWidget *civ_team_entry;
	GtkWidget *alien_equip_entry;
	GtkWidget *credits_win_entry;
	GtkWidget *credits_civ_entry;
	GtkWidget *credits_alien_entry;
	GtkWidget *other_label;
	GtkWidget *mission_action_area;
	GtkWidget *cancel_button;
	GtkWidget *ok_button;
	GtkWidget *map_assembly_param_entry;
	GtkWidget *map_assembly_param_label;
	GtkWidget *location_mission;
	GtkWidget *type_mission;
	GtkWidget *type_label;
	GtkWidget *location_label;
	struct dirent *dir_info;
	DIR *dir;
	char dirname[128];
	char buffer[128];

	mission_dialog = gtk_dialog_new ();
	gtk_window_set_title (GTK_WINDOW (mission_dialog), Q_("Mission"));
	gtk_window_set_modal (GTK_WINDOW (mission_dialog), TRUE);
	gtk_window_set_default_size (GTK_WINDOW (mission_dialog), 320, 260);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (mission_dialog), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (mission_dialog), TRUE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (mission_dialog), TRUE);
	gtk_window_set_type_hint (GTK_WINDOW (mission_dialog), GDK_WINDOW_TYPE_HINT_DIALOG);

	g_signal_connect (GTK_OBJECT (mission_dialog), "delete_event", G_CALLBACK (button_mission_dialog_cancel), NULL);

	mission_vbox = GTK_DIALOG (mission_dialog)->vbox;
	gtk_widget_show (mission_vbox);

	mission_variables_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (mission_variables_vbox);
	gtk_box_pack_start (GTK_BOX (mission_vbox), mission_variables_vbox, TRUE, TRUE, 0);

	actors_frame = gtk_frame_new (NULL);
	gtk_widget_show (actors_frame);
	gtk_box_pack_start (GTK_BOX (mission_variables_vbox), actors_frame, TRUE, TRUE, 0);

	actors_table = gtk_table_new (3, 2, FALSE);
	gtk_widget_show (actors_table);
	gtk_container_add (GTK_CONTAINER (actors_frame), actors_table);

	recruits = gtk_label_new (Q_("Recruits"));
	gtk_widget_show (recruits);
	gtk_table_attach (GTK_TABLE (actors_table), recruits, 0, 1, 0, 1,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_label_set_use_markup (GTK_LABEL (recruits), TRUE);
	gtk_misc_set_alignment (GTK_MISC (recruits), 0, 0.5);

	civilians = gtk_label_new (Q_("Civilians"));
	gtk_widget_show (civilians);
	gtk_table_attach (GTK_TABLE (actors_table), civilians, 0, 1, 1, 2,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (civilians), 0, 0.5);

	aliens = gtk_label_new (Q_("Aliens"));
	gtk_widget_show (aliens);
	gtk_table_attach (GTK_TABLE (actors_table), aliens, 0, 1, 2, 3,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (aliens), 0, 0.5);

	combo_recruits = gtk_combo_box_new_text ();
	gtk_widget_show (combo_recruits);
	gtk_table_attach (GTK_TABLE (actors_table), combo_recruits, 1, 2, 0, 1,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_recruits), _("1"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_recruits), _("2"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_recruits), _("3"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_recruits), _("4"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_recruits), _("5"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_recruits), _("6"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_recruits), _("7"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_recruits), _("8"));
	gtk_combo_box_set_active( GTK_COMBO_BOX (combo_recruits), 5 );

	combo_civilians = gtk_combo_box_new_text ();
	gtk_widget_show (combo_civilians);
	gtk_table_attach (GTK_TABLE (actors_table), combo_civilians, 1, 2, 1, 2,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_civilians), _("1"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_civilians), _("2"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_civilians), _("3"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_civilians), _("4"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_civilians), _("5"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_civilians), _("6"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_civilians), _("7"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_civilians), _("8"));
	gtk_combo_box_set_active( GTK_COMBO_BOX (combo_civilians), 2 );

	combo_aliens = gtk_combo_box_new_text ();
	gtk_widget_show (combo_aliens);
	gtk_table_attach (GTK_TABLE (actors_table), combo_aliens, 1, 2, 2, 3,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_aliens), _("1"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_aliens), _("2"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_aliens), _("3"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_aliens), _("4"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_aliens), _("5"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_aliens), _("6"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_aliens), _("7"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_aliens), _("8"));
	gtk_combo_box_set_active( GTK_COMBO_BOX (combo_aliens), 5 );

	label1 = gtk_label_new (_("Actors"));
	gtk_widget_show (label1);
	gtk_frame_set_label_widget (GTK_FRAME (actors_frame), label1);

	mission_frame = gtk_frame_new (NULL);
	gtk_widget_show (mission_frame);
	gtk_box_pack_start (GTK_BOX (mission_variables_vbox), mission_frame, TRUE, TRUE, 0);

	mission_table = gtk_table_new (6, 2, FALSE);
	gtk_widget_show (mission_table);
	gtk_container_add (GTK_CONTAINER (mission_frame), mission_table);

	map_label = gtk_label_new (Q_("Map"));
	gtk_widget_show (map_label);
	gtk_table_attach (GTK_TABLE (mission_table), map_label, 0, 1, 0, 1,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (map_label), 0, 0.5);

	map_assembly_param_label = gtk_label_new (Q_("Map Assembly Param"));
	gtk_widget_show (map_assembly_param_label);
	gtk_table_attach (GTK_TABLE (mission_table), map_assembly_param_label, 0, 1, 1, 2,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (map_assembly_param_label), 0, 0.5);

	music_label = gtk_label_new (Q_("Music"));
	gtk_widget_show (music_label);
	gtk_table_attach (GTK_TABLE (mission_table), music_label, 0, 1, 2, 3,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (music_label), 0, 0.5);

	text_label = gtk_label_new (Q_("Text"));
	gtk_widget_show (text_label);
	gtk_table_attach (GTK_TABLE (mission_table), text_label, 0, 1, 3, 4,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (text_label), 0, 0.5);

	location_label = gtk_label_new (Q_("Location"));
	gtk_widget_show (location_label);
	gtk_table_attach (GTK_TABLE (mission_table), location_label, 0, 1, 4, 5,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (location_label), 0, 0.5);

	type_label = gtk_label_new (Q_("Location"));
	gtk_widget_show (type_label);
	gtk_table_attach (GTK_TABLE (mission_table), type_label, 0, 1, 5, 6,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (type_label), 0, 0.5);

	combo_map = gtk_combo_box_new_text();
	gtk_widget_show (combo_map);
	gtk_table_attach (GTK_TABLE (mission_table), combo_map, 1, 2, 0, 1,
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
			gtk_combo_box_append_text (GTK_COMBO_BOX (combo_map), buffer);
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
			gtk_combo_box_append_text (GTK_COMBO_BOX (combo_map), va("+%s", buffer) );
		}
	}

	gtk_combo_box_set_active( GTK_COMBO_BOX (combo_map), 0 );

	map_assembly_param_entry = gtk_entry_new_with_max_length (MAX_VAR);
	gtk_widget_show (map_assembly_param_entry);
	gtk_table_attach (GTK_TABLE (mission_table), map_assembly_param_entry, 1, 2, 1, 2,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_entry_set_invisible_char (GTK_ENTRY (map_assembly_param_entry), 9679);

	combo_music = gtk_combo_box_new_text();
	gtk_widget_show (combo_music);
	gtk_table_attach (GTK_TABLE (mission_table), combo_music, 1, 2, 2, 3,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

	if ( (dir = opendir("base/music")) == NULL )
		fprintf(stderr, "Could not read base/music\n");
	while ( dir && (dir_info = readdir(dir)) != NULL ) {
		if ( !file_ext(dir_info->d_name, ".ogg") )
			continue;
		file_strip_ext(dir_info->d_name, buffer);
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo_music), buffer);
	}
	gtk_combo_box_set_active( GTK_COMBO_BOX (combo_music), 0 );

	text_mission = gtk_text_new (NULL, NULL);
	gtk_widget_show (text_mission);
	gtk_table_attach (GTK_TABLE (mission_table), text_mission, 1, 2, 3, 4,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_text_insert (GTK_EDITABLE (text_mission), NULL, NULL, NULL, "Protect inhabitants", -1);

	location_mission = gtk_entry_new_with_max_length (MAX_VAR);
	gtk_widget_show (location_mission);
	gtk_table_attach (GTK_TABLE (mission_table), location_mission, 1, 2, 4, 5,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_entry_set_text (location_mission, "TYPE_LOCATION");

	type_mission = gtk_entry_new_with_max_length (MAX_VAR);
	gtk_widget_show (type_mission);
	gtk_table_attach (GTK_TABLE (mission_table), type_mission, 1, 2, 5, 6,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_entry_set_text (type_mission, "Terror Attack");

	label2 = gtk_label_new (_("Mission"));
	gtk_widget_show (label2);
	gtk_frame_set_label_widget (GTK_FRAME (mission_frame), label2);

	other_frame = gtk_frame_new (NULL);
	gtk_widget_show (other_frame);
	gtk_box_pack_start (GTK_BOX (mission_variables_vbox), other_frame, TRUE, TRUE, 0);

	other_table = gtk_table_new (6, 2, FALSE);
	gtk_widget_show (other_table);
	gtk_container_add (GTK_CONTAINER (other_frame), other_table);

	alienteam_label = gtk_label_new (Q_("Alienteam"));
	gtk_widget_show (alienteam_label);
	gtk_table_attach (GTK_TABLE (other_table), alienteam_label, 0, 1, 0, 1,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (alienteam_label), 0, 0.5);

	civteam_label = gtk_label_new (Q_("Civilianteam"));
	gtk_widget_show (civteam_label);
	gtk_table_attach (GTK_TABLE (other_table), civteam_label, 0, 1, 1, 2,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (civteam_label), 0, 0.5);

	alienequip_label = gtk_label_new (Q_("Alienequipment"));
	gtk_widget_show (alienequip_label);
	gtk_table_attach (GTK_TABLE (other_table), alienequip_label, 0, 1, 2, 3,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (alienequip_label), 0, 0.5);

	win_label = gtk_label_new (Q_("Credits on win"));
	gtk_widget_show (win_label);
	gtk_table_attach (GTK_TABLE (other_table), win_label, 0, 1, 3, 4,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (win_label), 0, 0.5);

	credits_civ_label = gtk_label_new (Q_("Credits for rescued civilians"));
	gtk_widget_show (credits_civ_label);
	gtk_table_attach (GTK_TABLE (other_table), credits_civ_label, 0, 1, 4, 5,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (credits_civ_label), 0, 0.5);

	credits_aliens_label = gtk_label_new (Q_("Credits for killed aliens"));
	gtk_widget_show (credits_aliens_label);
	gtk_table_attach (GTK_TABLE (other_table), credits_aliens_label, 0, 1, 5, 6,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (credits_aliens_label), 0, 0.5);

	alienteam_entry = gtk_entry_new_with_max_length (MAX_VAR);
	gtk_widget_show (alienteam_entry);
	gtk_table_attach (GTK_TABLE (other_table), alienteam_entry, 1, 2, 0, 1,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_entry_set_invisible_char (GTK_ENTRY (alienteam_entry), 9679);

	civ_team_entry = gtk_entry_new_with_max_length (MAX_VAR);
	gtk_widget_show (civ_team_entry);
	gtk_table_attach (GTK_TABLE (other_table), civ_team_entry, 1, 2, 1, 2,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_entry_set_invisible_char (GTK_ENTRY (civ_team_entry), 9679);

	alien_equip_entry = gtk_entry_new_with_max_length (MAX_VAR);
	gtk_widget_show (alien_equip_entry);
	gtk_table_attach (GTK_TABLE (other_table), alien_equip_entry, 1, 2, 2, 3,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_entry_set_invisible_char (GTK_ENTRY (alien_equip_entry), 9679);

	credits_win_entry = gtk_entry_new_with_max_length (MAX_VAR);
	gtk_widget_show (credits_win_entry);
	gtk_table_attach (GTK_TABLE (other_table), credits_win_entry, 1, 2, 3, 4,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_entry_set_invisible_char (GTK_ENTRY (credits_win_entry), 9679);

	credits_civ_entry = gtk_entry_new_with_max_length (MAX_VAR);
	gtk_widget_show (credits_civ_entry);
	gtk_table_attach (GTK_TABLE (other_table), credits_civ_entry, 1, 2, 4, 5,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_entry_set_invisible_char (GTK_ENTRY (credits_civ_entry), 9679);

	credits_alien_entry = gtk_entry_new_with_max_length (MAX_VAR);
	gtk_widget_show (credits_alien_entry);
	gtk_table_attach (GTK_TABLE (other_table), credits_alien_entry, 1, 2, 5, 6,
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_entry_set_invisible_char (GTK_ENTRY (credits_alien_entry), 9679);

	other_label = gtk_label_new (_("Other"));
	gtk_widget_show (other_label);
	gtk_frame_set_label_widget (GTK_FRAME (other_frame), other_label);

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

	/* Store pointers to all widgets, for use by lookup_widget(). */
	GLADE_HOOKUP_OBJECT_NO_REF (mission_dialog, mission_dialog, "mission_dialog");
	GLADE_HOOKUP_OBJECT (mission_dialog, combo_recruits, "combo_recruits");
	GLADE_HOOKUP_OBJECT (mission_dialog, combo_civilians, "combo_civilians");
	GLADE_HOOKUP_OBJECT (mission_dialog, combo_aliens, "combo_aliens");
	GLADE_HOOKUP_OBJECT (mission_dialog, combo_map, "combo_map");
	GLADE_HOOKUP_OBJECT (mission_dialog, combo_music, "combo_music");
	GLADE_HOOKUP_OBJECT (mission_dialog, text_mission, "text_mission");
	GLADE_HOOKUP_OBJECT (mission_dialog, alienteam_entry, "alienteam_entry");
	GLADE_HOOKUP_OBJECT (mission_dialog, civ_team_entry, "civ_team_entry");
	GLADE_HOOKUP_OBJECT (mission_dialog, alien_equip_entry, "alien_equip_entry");
	GLADE_HOOKUP_OBJECT (mission_dialog, credits_win_entry, "credits_win_entry");
	GLADE_HOOKUP_OBJECT (mission_dialog, credits_civ_entry, "credits_civ_entry");
	GLADE_HOOKUP_OBJECT (mission_dialog, credits_alien_entry, "credits_alien_entry");
	GLADE_HOOKUP_OBJECT (mission_dialog, type_mission, "type_mission");
	GLADE_HOOKUP_OBJECT (mission_dialog, location_mission, "location_mission");
	GLADE_HOOKUP_OBJECT (mission_dialog, cancel_button, "cancel_button");
	GLADE_HOOKUP_OBJECT (mission_dialog, ok_button, "ok_button");
	GLADE_HOOKUP_OBJECT (mission_dialog, map_assembly_param_entry, "map_assembly_param_entry");

	return mission_dialog;
}

GtkWidget* create_mis_txt (void)
{
	GtkWidget *mis_txt_vbox;
	GtkWidget *mis_txt_action_area;
	GtkWidget *mis_txt_close;

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
	GLADE_HOOKUP_OBJECT (mis_txt, mis_txt_close, "mis_txt_close");

	return mis_txt;
}

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
