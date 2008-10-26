/**
 * @file m_node_button.c
 * @todo add an icon if possible.
 * @todo implement clicked button when its possible.
 * @todo allow to disable the node (also when it own a click action) (generic for every nodes)
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

#include "../client.h"
#include "m_node_button.h"
#include "m_node_custombutton.h"
#include "m_main.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_input.h"

static const int TILE_SIZE = 64;

static const int CORNER_WIDTH = 17;
static const int CORNER_HEIGHT = 17;
static const int MID_WIDTH = 1;
static const int MID_HEIGHT = 1;
static const int MARGE = 3;

static const int LEFT_POSX = 0;
#define MID_POSX LEFT_POSX+CORNER_WIDTH+MARGE
#define RIGHT_POSX MID_POSX+MID_WIDTH+MARGE

static const int TOP_POSY = 0;
#define MID_POSY TOP_POSY+CORNER_HEIGHT+MARGE
#define BOTTOM_POSY MID_POSY+MID_HEIGHT+MARGE

/**
 * @brief Handled alfer the end and the initialization of the node (all data and/or child are set)
 */
static void MN_ButtonNodeInit (menuNode_t *node) {
#ifdef DEBUG
	if (node->size[0] < CORNER_WIDTH+MID_WIDTH+CORNER_WIDTH || node->size[1] < CORNER_HEIGHT+MID_HEIGHT+CORNER_HEIGHT)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' too small. It can create graphical bugs\n", node->name);
#endif
}

/**
 * @brief Handles Button clicks
 * @sa MN_BUTTON
 * @todo node->disabled is not need, a disabled node must not receive
 * any input event; but it dont work like that for the moment
 */
static void MN_ButtonNodeClick (menuNode_t * node, int x, int y)
{
	if (node->click && !node->disabled) {
		MN_ExecuteActions(node->menu, node->click);
	}
}

/**
 * @brief Handles CustomButton draw
 * @sa MN_CUSTOMBUTTON
 */
static void MN_ButtonNodeDraw (menuNode_t *node)
{
	const char *text;
	const char *font;
	int texX, texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	int y, h;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};

	if (!node->click || node->disabled) {
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

	image = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	if (image) {
		/* draw top (from left to right) */
		R_DrawNormPic(pos[0], pos[1], CORNER_WIDTH, CORNER_HEIGHT, texX+LEFT_POSX+CORNER_WIDTH, texY+TOP_POSY+CORNER_HEIGHT,
			texX+LEFT_POSX, texY+TOP_POSY, ALIGN_UL, node->blend, image);
		R_DrawNormPic(pos[0] + CORNER_WIDTH, pos[1], node->size[0] - CORNER_WIDTH - CORNER_WIDTH, CORNER_HEIGHT, texX+MID_POSX+MID_WIDTH, texY+TOP_POSY+CORNER_HEIGHT,
			texX+MID_POSX, texY+TOP_POSY, ALIGN_UL, node->blend, image);
		R_DrawNormPic(pos[0] + node->size[0] - CORNER_WIDTH, pos[1], CORNER_WIDTH, CORNER_HEIGHT, texX+RIGHT_POSX+CORNER_WIDTH, texY+TOP_POSY+CORNER_HEIGHT,
			texX+RIGHT_POSX, texY+TOP_POSY, ALIGN_UL, node->blend, image);

		/* draw middle (from left to right) */
		y = pos[1] + CORNER_HEIGHT;
		h = node->size[1] - CORNER_HEIGHT - CORNER_HEIGHT; /*< height of middle */
		R_DrawNormPic(pos[0], y, CORNER_WIDTH, h, texX+LEFT_POSX+CORNER_WIDTH, texY+MID_POSY+MID_HEIGHT,
			texX+LEFT_POSX, texY+MID_POSY, ALIGN_UL, node->blend, image);
		R_DrawNormPic(pos[0] + CORNER_WIDTH, y, node->size[0] - CORNER_WIDTH - CORNER_WIDTH, h, texX+MID_POSX+MID_WIDTH, texY+MID_POSY+MID_HEIGHT,
			texX+MID_POSX, texY+MID_POSY, ALIGN_UL, node->blend, image);
		R_DrawNormPic(pos[0] + node->size[0] - CORNER_WIDTH, y, CORNER_WIDTH, h, texX+RIGHT_POSX+CORNER_WIDTH, texY+MID_POSY+MID_HEIGHT,
			texX+RIGHT_POSX, texY+MID_POSY, ALIGN_UL, node->blend, image);

		/* draw bottom (from left to right) */
		y = pos[1] + node->size[1] - CORNER_HEIGHT;
		R_DrawNormPic(pos[0], y, CORNER_WIDTH, CORNER_HEIGHT, texX+LEFT_POSX+CORNER_WIDTH, texY+BOTTOM_POSY+CORNER_HEIGHT,
			texX+LEFT_POSX, texY+BOTTOM_POSY, ALIGN_UL, node->blend, image);
		R_DrawNormPic(pos[0] + CORNER_WIDTH, y, node->size[0] - CORNER_WIDTH - CORNER_WIDTH, CORNER_HEIGHT, texX+MID_POSX+MID_WIDTH, texY+BOTTOM_POSY+CORNER_HEIGHT,
			texX+MID_POSX, texY+BOTTOM_POSY, ALIGN_UL, node->blend, image);
		R_DrawNormPic(pos[0] + node->size[0] - CORNER_WIDTH, y, CORNER_WIDTH, CORNER_HEIGHT, texX+RIGHT_POSX+CORNER_WIDTH, texY+BOTTOM_POSY+CORNER_HEIGHT,
			texX+RIGHT_POSX, texY+BOTTOM_POSY, ALIGN_UL, node->blend, image);
	}

	text = MN_GetReferenceString(node->menu, node->text);
	if (text != NULL && *text != '\0') {
		font = MN_GetFont(node->menu, node);
		R_ColorBlend(textColor);
		/** @todo remove the *1.5, only here because R_FontDrawString ius buggy */
		R_FontDrawString(font, ALIGN_CC, pos[0] + (node->size[0] / 2), pos[1] + (node->size[1] / 2),
			pos[0], pos[1], node->size[0] * 1.5, node->size[1],
			0, _(text), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	}
}

void MN_RegisterNodeButton (nodeBehaviour_t *behaviour)
{
	behaviour->name = "button";
	behaviour->draw = MN_ButtonNodeDraw;
	behaviour->init = MN_ButtonNodeInit;
	behaviour->leftClick = MN_ButtonNodeClick;
}
