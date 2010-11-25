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

#include "menu.h"

#include <gtk/gtk.h>

GtkMenuItem* new_sub_menu_item_with_mnemonic (const std::string& mnemonic)
{
	GtkMenuItem* item = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(mnemonic.c_str()));
	gtk_widget_show(GTK_WIDGET(item));

	GtkWidget* sub_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(item, sub_menu);

	return item;
}
