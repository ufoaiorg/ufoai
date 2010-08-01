/**
 * @file m_node_map.h
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef CLIENT_MENU_M_NODE_MAP_H
#define CLIENT_MENU_M_NODE_MAP_H

#include "../m_nodes.h"

#define MN_MAPEXTRADATA_TYPE mapExtraData_t
#define MN_MAPEXTRADATA(node) MN_EXTRADATA(node, MN_MAPEXTRADATA_TYPE)
#define MN_MAPEXTRADATACONST(node) MN_EXTRADATACONST(node, MN_MAPEXTRADATA_TYPE)

typedef struct mapExtraData_s {
	float paddingRight;
} mapExtraData_t;

void MN_RegisterMapNode(uiBehaviour_t *behaviour);

#endif
