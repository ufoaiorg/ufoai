/**
 * @file m_node_string.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "m_nodes.h"
#include "m_font.h"
#include "m_parse.h"
#include "../client.h"

void MN_DrawStringNode (menuNode_t *node)
{
	vec2_t nodepos;
	menu_t *menu = node->menu;
	const char *font = MN_GetFont(menu, node);
	const char* ref = MN_GetSaifeReferenceString(node->menu, node->text);
	if (!ref)
		return;
	MN_GetNodeAbsPos(node, nodepos);
	ref += node->horizontalScroll;
	/* blinking */
	/** @todo should this wrap or chop long lines? */
	if (!node->mousefx || cl.time % 1000 < 500)
		R_FontDrawString(font, node->align, nodepos[0], nodepos[1], nodepos[0], nodepos[1], node->size[0], 0, node->texh[0], ref, 0, 0, NULL, qfalse, 0);
	else
		R_FontDrawString(font, node->align, nodepos[0], nodepos[1], nodepos[0], nodepos[1], node->size[0], node->size[1], node->texh[0], va("%s*\n", ref), 0, 0, NULL, qfalse, 0);
}

void MN_RegisterNodeString (nodeBehaviour_t *behaviour)
{
	behaviour->name = "string";
	behaviour->draw = MN_DrawStringNode;
}
