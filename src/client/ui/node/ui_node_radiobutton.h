/**
 * @file ui_node_radiobutton.h
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CLIENT_UI_UI_NODE_RADIOBUTTON_H
#define CLIENT_UI_UI_NODE_RADIOBUTTON_H

#include "../ui_nodes.h"

typedef struct radioButtonExtraData_s {
	void* cvar;
	float value;
	char *string;
	struct uiSprite_s *background;
	struct uiSprite_s *icon;	/**< Link to an icon */
	qboolean flipIcon;			/**< Flip the icon rendering (horizontal) */
} radioButtonExtraData_t;

void UI_RegisterRadioButtonNode(struct uiBehaviour_s *behaviour);

#endif
