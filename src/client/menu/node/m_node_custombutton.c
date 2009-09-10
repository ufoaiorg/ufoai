/**
 * @file m_node_custombutton.c
 * @brief Custombutton is a button using a skin with a fixed size.
 * We can define every pixels into the texture. It can't be bigger than
 * 256x64, because each row of 64 pixels are used for the button status (normal,
 * hovered, disabled, and the last is not used).
 * @code
 * // the button use all the size allowed
 * custombutton button_load
 * {
 * 	pos	"780 130"
 * 	size	"128 64"
 * 	click	{ cmd "echo click;" }
 * 	image	ui/multi_buttons2
 * 	font	f_menubig
 * 	color	"0 0.5 0 1"
 * 	selectcolor "1 1 1 1"
 * 	string	"_Load"
 * }
 * @endcode
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "../m_main.h"
#include "../m_icon.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_input.h"
#include "../m_render.h"
#include "m_node_custombutton.h"
#include "m_node_button.h"
#include "m_node_abstractnode.h"

#define MN_CUSTOMBUTTON_TEX_HEIGHT 64
#define MN_CUSTOMBUTTON_TEX_WIDTH 256

/**
 * @brief Handles CustomButton draw
 */
static void MN_CustomButtonNodeDraw (menuNode_t *node)
{
	const char *text;
	int texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};
	int status = 0;
	const char *font = MN_GetFontFromNode(node);

	if (!node->onClick || node->disabled) {
		/** @todo need custom color when button is disabled */
		textColor = disabledColor;
		texY = MN_CUSTOMBUTTON_TEX_HEIGHT * 2;
		status = 2;
	} else if (node->state) {
		textColor = node->selectedColor;
		texY = MN_CUSTOMBUTTON_TEX_HEIGHT;
		status = 1;
	} else {
		textColor = node->color;
		texY = 0;
	}

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node, node->image);
	if (image) {
		const int texX = rint(node->texl[0]);
		texY += node->texl[1];
		MN_DrawNormImageByName(pos[0], pos[1], node->size[0], node->size[1],
			texX + node->size[0], texY + node->size[1], texX, texY, image);
	}

	if (node->icon) {
		MN_DrawIconInBox(node->icon, status, pos[0], pos[1], node->size[0], node->size[1]);
	}

	text = MN_GetReferenceString(node, node->text);
	if (text != NULL && *text != '\0') {
		R_Color(textColor);
		MN_DrawStringInBox(font, node->textalign,
			pos[0] + node->padding, pos[1] + node->padding,
			node->size[0] - node->padding - node->padding, node->size[1] - node->padding - node->padding,
			text, LONGLINES_PRETTYCHOP);
		R_Color(NULL);
	}
}

static const value_t properties[] = {
	/* Skin position. Define the top-left position of the skin we will used from the image. Y should not be bigger than 64. To compute the high corner we use the node size. */
	{"texl", V_POS, offsetof(menuNode_t, texl), MEMBER_SIZEOF(menuNode_t, texl)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterCustomButtonNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "custombutton";
	behaviour->extends = "button";
	behaviour->draw = MN_CustomButtonNodeDraw;
	behaviour->properties = properties;
}
