/**
 * @file ui_node_spinner.c
 * @brief The spinner2 node is a vertical widget used to change a numerical value.
 *
 * It use 3 sprites to display the node: A sprite for the background, a sprite
 * for the top button, and a sprite for a bottom button. All are displayed in the
 * center of the node. And according to the node status and the mouse position, and
 * displayed nornal, hovered, or disabled.
 *
 * This node extends spinner node and only override the draw method. Then the behaviour
 * is the same as the spinner.
 *
 * @image html http://ufoai.org/wiki/images/Spinner2.svg
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
#include "../ui_main.h"
#include "../ui_sprite.h"
#include "../ui_render.h"
#include "ui_node_spinner2.h"
#include "ui_node_abstractnode.h"

#include "../../input/cl_input.h"
#include "../../input/cl_keys.h"

#define EXTRADATA_TYPE spinner2ExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static void UI_Spinner2NodeDraw (uiNode_t *node)
{
	vec2_t pos;
	const float delta = UI_GetReferenceFloat(node, EXTRADATA(node).super.super.delta);
	/* TODO quite stupid? */
	const qboolean disabled = node->disabled || node->parent->disabled;

	uiSpriteStatus_t status;
	uiSpriteStatus_t topStatus;
	uiSpriteStatus_t bottomStatus;

	UI_GetNodeAbsPos(node, pos);

	if (disabled || delta == 0) {
		status = SPRITE_STATUS_DISABLED;
		topStatus = SPRITE_STATUS_DISABLED;
		bottomStatus = SPRITE_STATUS_DISABLED;
	} else {
		const float value = UI_GetReferenceFloat(node, EXTRADATA(node).super.super.value);
		const float min = UI_GetReferenceFloat(node, EXTRADATA(node).super.super.min);
		const float max = UI_GetReferenceFloat(node, EXTRADATA(node).super.super.max);

		status = SPRITE_STATUS_NORMAL;

		/* top button status */
		if (value >= max) {
			topStatus = SPRITE_STATUS_DISABLED;
		} else if (node->state && mousePosY < pos[1] + node->size[1] * 0.5) {
			topStatus = SPRITE_STATUS_HOVER;
		} else {
			topStatus = SPRITE_STATUS_NORMAL;
		}
		/* bottom button status */
		if (value <= min) {
			bottomStatus = SPRITE_STATUS_DISABLED;
		} else if (node->state && mousePosY > pos[1] + node->size[1] * 0.5) {
			bottomStatus = SPRITE_STATUS_HOVER;
		} else {
			bottomStatus = SPRITE_STATUS_NORMAL;
		}
	}

	if (EXTRADATA(node).background)
		UI_DrawSpriteInBox(qfalse, EXTRADATA(node).background, status, pos[0], pos[1], node->size[0], node->size[1]);
	if (EXTRADATA(node).topIcon)
		UI_DrawSpriteInBox(qfalse, EXTRADATA(node).topIcon, topStatus, pos[0], pos[1], node->size[0], node->size[1]);
	if (EXTRADATA(node).bottomIcon)
		UI_DrawSpriteInBox(qfalse, EXTRADATA(node).bottomIcon, bottomStatus, pos[0], pos[1], node->size[0], node->size[1]);
}

void UI_RegisterSpinner2Node (uiBehaviour_t *behaviour)
{
	behaviour->name = "spinner2";
	behaviour->extends = "spinner";
	behaviour->draw = UI_Spinner2NodeDraw;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/**
	 * @brief Backround used to display the spinner. It is displayed in the center of the node.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "background", V_UI_SPRITEREF, EXTRADATA_TYPE, background);

	/**
	 * @brief Top icon used to decorate the top button of the spinner. It is displayed in the center of the node.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "topIcon", V_UI_SPRITEREF, EXTRADATA_TYPE, topIcon);

	/**
	 * @brief Sprite used to decorate the bottom button of the spinner. It is displayed in the center of the node.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "bottomIcon", V_UI_SPRITEREF, EXTRADATA_TYPE, bottomIcon);
}
