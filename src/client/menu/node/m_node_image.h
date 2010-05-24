/**
 * @file m_node_image.h
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

#ifndef CLIENT_MENU_M_NODE_IMAGE_H
#define CLIENT_MENU_M_NODE_IMAGE_H

struct nodeBehaviour_s;
struct menuNode_s;

void MN_RegisterImageNode(struct nodeBehaviour_s* behaviour);
void MN_ImageNodeDraw(struct menuNode_s *node);

typedef struct imageExtraData_s {
	vec2_t texh;				/**< lower right texture coordinates, for text nodes texh[0] is the line height and texh[1] tabs width */
	vec2_t texl;				/**< upper left texture coordinates */
	qboolean preventRatio;
	int mousefx;
} imageExtraData_t;

#endif
