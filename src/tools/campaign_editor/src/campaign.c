/**
 * @file campaign.c
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
#include "campaign.h"

static GtkWidget* campaign_vbox = NULL;

/**
 * @brief
 */
void load_campaign(char* name)
{
	//tab_geoscape()
	gtk_widget_hide(campaign_vbox);
}

/**
 * @brief Event function for campaign select box
 * @param[in] widget
 * @param[in] event
 */
void open_campaign (GtkWidget *widget, GdkEventMotion *event)
{
	char* campaign = gtk_combo_box_get_active_text (GTK_COMBO_BOX(widget));
	printf("current selected campaign: %s\n", campaign);
	load_campaign(campaign);
}

/**
 * @brief
 */
void tab_campaigns(GtkWidget *notebook)
{
	GtkWidget *label, *table, *w;

	campaign_vbox = gtk_vbox_new(TRUE, 2);
	gtk_widget_show(campaign_vbox);
	label = gtk_label_new (_("Campaign"));

	/* new table */
	table = gtk_table_new (1, 2, FALSE);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (campaign_vbox), table, FALSE, FALSE, 0);
	//gtk_container_add (GTK_BOX (campaign_vbox), table);
	create_label("Campaign", table, 0, 1, 0, 1);
	w = create_select_box_from_script_data("campaign_select_entry", "campaign", "main", table, 1, 2, 0, 1);
	gtk_signal_connect (GTK_COMBO_BOX (w), "changed",
						(GtkSignalFunc) open_campaign, NULL);

	gtk_widget_show (label);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), campaign_vbox, label);
	GLADE_HOOKUP_OBJECT (ufoai_editor, label, "campaign_tab");
}

/**
 * @brief
 */
void tab_geoscape(GtkWidget *notebook, char* campaign_map)
{
	GtkWidget *label, *scrolledwindow, *viewport, *image;

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow);
	gtk_container_add (GTK_CONTAINER (notebook), scrolledwindow);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_widget_show (viewport);
	gtk_container_add (GTK_CONTAINER (scrolledwindow), viewport);

	/* FIXME: First show a campaign selection dialog */
	/* then load the given campaign map */
	image = create_pixmap (ufoai_editor, va("map_%s_day.jpg", campaign_map));
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (viewport), image);

	gtk_signal_connect (GTK_OBJECT (viewport), "motion_notify_event",
						(GtkSignalFunc) motion_notify_event, NULL);
	gtk_signal_connect (GTK_OBJECT (viewport), "button_press_event",
						(GtkSignalFunc) button_press_event, NULL);

	gtk_widget_set_events (viewport, GDK_EXPOSURE_MASK
						| GDK_LEAVE_NOTIFY_MASK
						| GDK_BUTTON_PRESS_MASK
						| GDK_POINTER_MOTION_MASK
						| GDK_POINTER_MOTION_HINT_MASK);

	label = gtk_label_new (_("Mission"));
	gtk_widget_show (label);
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0), label);
	GLADE_HOOKUP_OBJECT (ufoai_editor, label, "new_mission_tab");
}
