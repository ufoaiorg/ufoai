/**
 * @file ui_node_todo.c
 * @brief A node allowing to tag a GUI with comment (only visible on debug mode).
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

#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_draw.h"
#include "../ui_tooltip.h"
#include "../ui_render.h"
#include "ui_node_todo.h"
#include "ui_node_abstractnode.h"

#include "../../input/cl_input.h"

/**
 * @brief Custom tooltip of todo node
 * @param[in] node Node we request to draw tooltip
 * @param[in] x Position x of the mouse
 * @param[in] y Position y of the mouse
 */
static void UI_TodoNodeDrawTooltip (uiNode_t *node, int x, int y)
{
	const int tooltipWidth = 250;
	static char tooltiptext[1024];

	const char* text = UI_GetReferenceString(node, node->text);
	if (!text)
		return;

	tooltiptext[0] = '\0';
	Q_strcat(tooltiptext, text, sizeof(tooltiptext));
	UI_DrawTooltip(tooltiptext, x, y, tooltipWidth);
}

static void UI_TodoNodeDraw (uiNode_t *node)
{
	static vec4_t red = {1.0, 0.0, 0.0, 1.0};
	vec2_t pos;

	UI_GetNodeAbsPos(node, pos);
	UI_DrawFill(pos[0], pos[1], node->size[0], node->size[1], red);

	if (node->state)
		UI_CaptureDrawOver(node);
}

static void UI_TodoNodeDrawOverWindow (uiNode_t *node)
{
	UI_TodoNodeDrawTooltip(node, mousePosX, mousePosY);
}

static void UI_TodoNodeLoading (uiNode_t *node)
{
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
}

static void UI_TodoNodeLoaded (uiNode_t *node)
{
#ifndef DEBUG
	node->invis = qtrue;
#endif
	node->size[0] = 10;
	node->size[1] = 10;
}

void UI_RegisterTodoNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "todo";
	behaviour->extends = "string";
	behaviour->draw = UI_TodoNodeDraw;
	behaviour->drawOverWindow = UI_TodoNodeDrawOverWindow;
	behaviour->loading = UI_TodoNodeLoading;
	behaviour->loaded = UI_TodoNodeLoaded;
}
