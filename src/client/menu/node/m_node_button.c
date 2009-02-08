/**
 * @file m_node_button.c
 * @todo add an icon if possible.
 * @todo implement clicked button when its possible.
 * @todo allow autosize (use the size need looking string, problem when we change langage)
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

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../m_main.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_input.h"
#include "m_node_button.h"
#include "m_node_custombutton.h"
#include "m_node_abstractnode.h"

#define TILE_SIZE 64
#define CORNER_SIZE 17
#define MID_SIZE 1
#define MARGE 3

/**
 * @brief Handles Button clicks
 * @todo node->disabled is not need, a disabled node must not receive
 * any input event; but it dont work like that for the moment
 */
static void MN_ButtonNodeClick (menuNode_t * node, int x, int y)
{
	if (node->onClick && !node->disabled) {
		MN_ExecuteEventActions(node, node->onClick);
	}
}

/**
 * @brief Handles Button draw
 */
static void MN_ButtonNodeDraw (menuNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	const char *text;
	const char *font;
	int texX, texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};

	if (!node->onClick || node->disabled) {
		/** @todo need custom color when button is disabled */
		textColor = disabledColor;
		texX = TILE_SIZE;
		texY = TILE_SIZE;
	} else if (node->state) {
		textColor = node->selectedColor;
		texX = TILE_SIZE;
		texY = 0;
	} else {
		textColor = node->color;
		texX = 0;
		texY = 0;
	}

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node, node->image);
	if (image) {
		R_DrawPanel(pos, node->size, image, node->blend, texX, texY, panelTemplate);
	}

	text = MN_GetReferenceString(node, node->text);
	if (text != NULL && *text != '\0') {
		font = MN_GetFont(node);
		R_ColorBlend(textColor);
		text = _(text);
		R_FontDrawStringInBox(font, node->textalign,
			pos[0] + node->padding, pos[1] + node->padding,
			node->size[0] - node->padding - node->padding, node->size[1] - node->padding - node->padding,
			text, LONGLINES_PRETTYCHOP);
		R_ColorBlend(NULL);
	}
}

/**
 * @brief Handles Button before loading. Used to set default attribute values
 */
static void MN_ButtonNodeLoading (menuNode_t *node)
{
	node->padding = 8;
	node->textalign = ALIGN_CC;
	Vector4Set(node->selectedColor, 1, 1, 1, 1);
	Vector4Set(node->color, 1, 1, 1, 1);
}

/**
 * @brief Handled alfer the end of the load of the node from script (all data and/or child are set)
 */
static void MN_ButtonNodeLoaded (menuNode_t *node)
{
	/* auto calc the size if none was given via script files */
	if (node->size[1] == 0) {
		/** @todo clean this up once font_t is known in the client */
		const char *font = MN_GetFont(node);
		node->size[1] = (R_FontGetHeight(font) / 2) + (node->padding * 2);
	}
#ifdef DEBUG
	if (node->size[0] < CORNER_SIZE + MID_SIZE + CORNER_SIZE || node->size[1] < CORNER_SIZE + MID_SIZE + CORNER_SIZE)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s.%s' too small. It can create graphical glitches\n", node->menu->name, node->name);
#endif
}

void MN_RegisterButtonNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "button";
	behaviour->draw = MN_ButtonNodeDraw;
	behaviour->loaded = MN_ButtonNodeLoaded;
	behaviour->leftClick = MN_ButtonNodeClick;
	behaviour->loading = MN_ButtonNodeLoading;
}
