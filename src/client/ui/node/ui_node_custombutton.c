/**
 * @file ui_node_custombutton.c
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

#include "../ui_main.h"
#include "../ui_sprite.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_font.h"
#include "../ui_input.h"
#include "../ui_render.h"
#include "ui_node_custombutton.h"
#include "ui_node_button.h"
#include "ui_node_abstractnode.h"

#define UI_CUSTOMBUTTON_TEX_HEIGHT 64
#define UI_CUSTOMBUTTON_TEX_WIDTH 256

#define EXTRADATA_TYPE customButtonExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

/**
 * @brief Handles CustomButton draw
 */
static void UI_CustomButtonNodeDraw (uiNode_t *node)
{
	const char *text;
	int texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};
	uiSpriteStatus_t iconStatus = SPRITE_STATUS_NORMAL;
	const char *font = UI_GetFontFromNode(node);

	if (!node->onClick || node->disabled) {
		/** @todo need custom color when button is disabled */
		textColor = disabledColor;
		texY = UI_CUSTOMBUTTON_TEX_HEIGHT * 2;
		iconStatus = SPRITE_STATUS_DISABLED;
	} else if (node->state) {
		textColor = node->selectedColor;
		texY = UI_CUSTOMBUTTON_TEX_HEIGHT;
		iconStatus = SPRITE_STATUS_HOVER;
	} else {
		textColor = node->color;
		texY = 0;
	}

	UI_GetNodeAbsPos(node, pos);

	image = UI_GetReferenceString(node, node->image);
	if (image) {
		const int texX = rint(EXTRADATA(node).texl[0]);
		texY += EXTRADATA(node).texl[1];
		UI_DrawNormImageByName(qfalse, pos[0], pos[1], node->size[0], node->size[1],
			texX + node->size[0], texY + node->size[1], texX, texY, image);
	}

	if (EXTRADATA(node).background) {
		UI_DrawSpriteInBox(qfalse, EXTRADATA(node).background, iconStatus, pos[0], pos[1], node->size[0], node->size[1]);
	}

	if (EXTRADATA(node).super.icon) {
		UI_DrawSpriteInBox(EXTRADATA(node).super.flipIcon, EXTRADATA(node).super.icon, iconStatus, pos[0], pos[1], node->size[0], node->size[1]);
	}

	text = UI_GetReferenceString(node, node->text);
	if (text != NULL && *text != '\0') {
		R_Color(textColor);
		UI_DrawStringInBox(font, node->contentAlign,
			pos[0] + node->padding, pos[1] + node->padding,
			node->size[0] - node->padding - node->padding, node->size[1] - node->padding - node->padding,
			text, LONGLINES_PRETTYCHOP);
		R_Color(NULL);
	}
}

void UI_RegisterCustomButtonNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "custombutton";
	behaviour->extends = "button";
	behaviour->draw = UI_CustomButtonNodeDraw;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Skin position. Define the top-left position of the skin we will used from the image.
	 * Y should not be bigger than 64. To compute the high corner we use the node size. */
	UI_RegisterExtradataNodeProperty(behaviour, "texl", V_POS, EXTRADATA_TYPE, texl);
	/* Sprite used to display the background */
	UI_RegisterExtradataNodeProperty(behaviour, "background", V_UI_SPRITEREF, EXTRADATA_TYPE, background);
}
