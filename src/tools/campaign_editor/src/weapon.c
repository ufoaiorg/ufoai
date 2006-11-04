/**
 * @file weapon.c
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

/**
 * @brief
 */
void load_weapon(char* name)
{

}


/**
 * @brief
 */
void tab_weapons(GtkWidget *notebook)
{
	GtkWidget *vbox, *label;

	vbox = gtk_vbox_new(FALSE, 2);
	gtk_widget_show(vbox);
	label = gtk_label_new (_("Weapons/Items"));
	gtk_widget_show (label);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
	GLADE_HOOKUP_OBJECT (ufoai_editor, label, "weapons_items_tab");
}
