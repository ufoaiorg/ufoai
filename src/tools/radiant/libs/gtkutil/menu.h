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

#if !defined(INCLUDED_GTKUTIL_MENU_H)
#define INCLUDED_GTKUTIL_MENU_H

#include "generic/callbackfwd.h"
#include <string>

typedef int gint;
typedef gint gboolean;
typedef struct _GSList GSList;
typedef struct _GtkMenu GtkMenu;
typedef struct _GtkMenuBar GtkMenuBar;
typedef struct _GtkMenuItem GtkMenuItem;
typedef struct _GtkCheckMenuItem GtkCheckMenuItem;
typedef struct _GtkRadioMenuItem GtkRadioMenuItem;
typedef struct _GtkTearoffMenuItem GtkTearoffMenuItem;

GtkMenuItem* new_sub_menu_item_with_mnemonic (const std::string& mnemonic);
GtkMenu* create_sub_menu_with_mnemonic (GtkMenuBar* bar, const std::string& mnemonic);
GtkMenu* create_sub_menu_with_mnemonic (GtkMenu* parent, const std::string& mnemonic);

#endif
