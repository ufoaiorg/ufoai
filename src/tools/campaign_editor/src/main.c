/**
 * @file main.c
 * @brief Main file for UFO:AI campaign editor
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

#include "interface.h"
#include "support.h"

int main (int argc, char *argv[])
{
#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	gtk_set_locale ();
	gtk_init (&argc, &argv);

	/* add ufo base dir to pixmaps searchpath */
	add_pixmap_directory ("base/pics/menu");
	/* system pixmaps dir */
	add_pixmap_directory (PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");

	/* first visible window */
	campaign_editor = create_campaign_editor ();
	gtk_widget_show (campaign_editor);

	/* create two more dialogs, but don't show them */
	mis_txt = create_mis_txt();
	mission_dialog = create_mission_dialog ();

	/* enter main loop */
	gtk_main ();
	return 0;
}

