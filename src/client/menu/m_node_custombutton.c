/**
 * @file m_node_custombutton.c
 * @todo add an icon if possible.
 * @todo implement click button when its possible.
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
#include "m_node_custombutton.h"
#include "m_main.h"
#include "m_node_button.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_input.h"

#define MN_CUSTOMBUTTON_TEX_HEIGHT 64
#define MN_CUSTOMBUTTON_TEX_WIDTH 256

/**
 * @brief Handles CustomButton draw
 * @sa MN_CUSTOMBUTTON
 */
static void MN_CustomButtonNodeDraw (menuNode_t *node)
{
	const char *text;
	const char *font;
	int baseY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};

	if (!node->click || node->disabled) {
		/** @todo need custom color when button is disabled */
		textColor = disabledColor;
		baseY = MN_CUSTOMBUTTON_TEX_HEIGHT * 2;
	} else if (node->state) {
		textColor = node->selectedColor;
		baseY = MN_CUSTOMBUTTON_TEX_HEIGHT;
	} else {
		textColor = node->color;
		baseY = 0;
	}

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	if (image) {
		R_DrawNormPic(pos[0], pos[1], node->size[0], node->size[1],
			node->size[0], baseY + node->size[1], 0, baseY, ALIGN_UL, node->blend, image);
	}

	text = MN_GetReferenceString(node->menu, node->text);
	if (text != NULL && *text != '\0') {
		font = MN_GetFont(node->menu, node);
		R_ColorBlend(textColor);
		R_FontDrawStringInBox(font, node->textalign,
			pos[0] + node->padding, pos[1] + node->padding,
			node->size[0] - node->padding - node->padding, node->size[1] - node->padding - node->padding,
			text, LONGLINES_PRETTYCHOP);
		R_ColorBlend(NULL);
	}
}

void MN_RegisterCustomButtonNode (nodeBehaviour_t *behaviour)
{
	/* inheritance */
	MN_RegisterButtonNode(behaviour);
	/* overwrite */
	behaviour->name = "custombutton";
	behaviour->id = MN_CUSTOMBUTTON;
	behaviour->draw = MN_CustomButtonNodeDraw;
}
