/**
 * @file ui_node_bar.h
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

#ifndef CLIENT_UI_UI_NODE_BAR_H
#define CLIENT_UI_UI_NODE_BAR_H

#include "ui_node_abstractvalue.h"

/* prototype */
struct uiBehaviour_s;

/**
 * @brief extradata for the panel node
 */
typedef struct {
	abstractValueExtraData_t super;
	align_t orientation;	/**< Orientation of the bar (left, right, top, down) */
	qboolean readOnly;	/**< True if the user can't edit the content */
	qboolean noHover;	/**< True to show the bar at 100% opacity - even if not hovered */
} barExtraData_t;

void UI_RegisterBarNode(struct uiBehaviour_s *behaviour);

#endif
