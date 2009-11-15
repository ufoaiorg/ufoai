/**
 * @file ump.h
 * @brief UMP API headers
 */

/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

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

#ifndef UMP_H
#define UMP_H

#include "iump.h"

typedef struct _GtkMenu GtkMenu;
typedef struct _GtkMenuItem GtkMenuItem;

void UMP_constructMenu (GtkMenuItem* menuItem, GtkMenu* menu);

void UMP_Construct ();
void UMP_Destroy ();

namespace map
{
	namespace ump
	{

		/**
		 * class is responsible for updating rma sub menu according to actual opened file
		 */
		class UMPMenuCreator
		{
			private:
				GtkMenuItem *_menuItem;
				GtkMenu *_menu;

			public:
				void updateMenu (void);

				void setMenuEntry (GtkMenuItem* menuItem, GtkMenu *menu)
				{
					_menuItem = menuItem;
					_menu = menu;
				}
				/**
				 * Return the singleton instance
				 * @return the instance
				 */
				static UMPMenuCreator* getInstance (void)
				{
					static UMPMenuCreator _instance;
					return &_instance;
				}
		};
	}
}

#endif
