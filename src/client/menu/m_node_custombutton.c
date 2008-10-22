/**
 * @file m_node_custombutton.c
 * @todo add an icon if possible.
 * @todo implement click button when its possible.
 * @todo allow to disable the node when it own a click action (generic for every nodes)
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
#include "m_main.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_input.h"

/**
 * @brief Handles CustomButton clicks
 * @sa MN_CUSTOMBUTTON
 */
static void MN_CustomButtonNodeClick (menuNode_t * node, int x, int y)
{
	int mouseOver = 1;
	char cmd[MAX_VAR];
	Com_sprintf(cmd, sizeof(cmd), "%s_click", node->name);
	if (Cmd_Exists(cmd))
		Cbuf_AddText(va("%s %i\n", cmd, mouseOver - 1));
	else if (node->click && node->click->type == EA_CMD) {
		assert(node->click->data);
		Cbuf_AddText(va("%s %i\n", (const char *)node->click->data, mouseOver - 1));
	}
}

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
	vec2_t nodepos;

	if (!node->click) {
		/** @todo need custom color when button is disabled */
		textColor = node->color;
		baseY = MN_CUSTOMBUTTON_TEX_HEIGHT * 2;
	} else if (node->state) {
		textColor = node->selectedColor;
		baseY = MN_CUSTOMBUTTON_TEX_HEIGHT;
	} else {
		textColor = node->color;
		baseY = 0;
	}

	MN_GetNodeAbsPos(node, nodepos);

	image = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	if (image) {
		R_ColorBlend(NULL);
		R_DrawNormPic(nodepos[0], nodepos[1], node->size[0], node->size[1],
			node->size[0], baseY + node->size[1], 0, baseY, ALIGN_UL, node->blend, image);
	}

	text = MN_GetReferenceString(node->menu, node->text);
	if (text != NULL && *text != '\0') {
		font = MN_GetFont(node->menu, node);
		R_ColorBlend(textColor);
		/** @todo remove the *1.5, only here because R_FontDrawString ius buggy */
		R_FontDrawString(font, ALIGN_CC, nodepos[0] + (node->size[0] / 2), nodepos[1] + (node->size[1] / 2),
			nodepos[0], nodepos[1], node->size[0] * 1.5, node->size[1],
			0, _(text), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	}
}

void MN_RegisterNodeCustomButton (nodeBehaviour_t *behaviour)
{
	behaviour->name = "custombutton";
	behaviour->draw = MN_CustomButtonNodeDraw;
	behaviour->leftClick = MN_CustomButtonNodeClick;
}
