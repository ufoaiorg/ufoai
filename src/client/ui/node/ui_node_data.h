/**
 * @file ui_node_data.h
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

#ifndef CLIENT_UI_UI_NODE_DATA_H
#define CLIENT_UI_UI_NODE_DATA_H

struct uiBehaviour_s;

void UI_RegisterDataNode(struct uiBehaviour_s* behaviour);

typedef struct dataExtraData_s {
	/** @todo Add it again when "string" from  uiNode_t is removed / or property "string" from abstract node
	 * char* string;
	 */
	float number;
	int integer;
} dataExtraData_t;

#endif
