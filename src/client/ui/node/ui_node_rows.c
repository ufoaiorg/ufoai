/**
 * @file ui_node_panel.c
 * @todo clean up all properties
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

#include "../ui_render.h"
#include "ui_node_rows.h"
#include "ui_node_abstractnode.h"

#define EXTRADATA_TYPE rowsExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

/**
 * @brief Handles Button draw
 */
static void UI_RowsNodeDraw (uiNode_t *node)
{
	int current = 0;
	int i = EXTRADATA(node).current;
	vec2_t pos;
	UI_GetNodeAbsPos(node, pos);

	while (current < node->size[1]) {
		const float *color;
		const int height = min(EXTRADATA(node).lineHeight, node->size[1] - current);

		if (i % 2)
			color = node->color;
		else
			color = node->selectedColor;
		UI_DrawFill(pos[0], pos[1] + current, node->size[0], height, color);
		current += height;
		i++;
	}
}

static void UI_RowsNodeLoaded (uiNode_t *node)
{
	/* prevent infinite loop into the draw */
	if (EXTRADATA(node).lineHeight == 0) {
		EXTRADATA(node).lineHeight = 10;
	}
}

static const value_t properties[] = {
	/* Background color for odd elements */
	{"color1", V_COLOR, offsetof(uiNode_t, color), MEMBER_SIZEOF(uiNode_t, color)},
	/* Background color for even elements */
	{"color2", V_COLOR, offsetof(uiNode_t, selectedColor), MEMBER_SIZEOF(uiNode_t, selectedColor)},
	/* Element height */
	{"lineheight", V_INT, UI_EXTRADATA_OFFSETOF(rowsExtraData_t, lineHeight), MEMBER_SIZEOF(rowsExtraData_t, lineHeight)},
	/* Element number on the top of the list. It is used to scroll the node content. */
	{"current", V_INT, UI_EXTRADATA_OFFSETOF(rowsExtraData_t, current), MEMBER_SIZEOF(rowsExtraData_t, current)},
	{NULL, V_NULL, 0, 0}
};

void UI_RegisterRowsNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "rows";
	behaviour->draw = UI_RowsNodeDraw;
	behaviour->loaded = UI_RowsNodeLoaded;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
