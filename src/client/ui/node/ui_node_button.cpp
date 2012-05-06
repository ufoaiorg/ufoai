/**
 * @file
 * @brief <code>button</code> is a node to define a button with a random size. It is skinned
 * with a special image template (see the <code>image</code> property).
 * @todo implement clicked button when its possible.
 * @todo allow auto size (use the size need looking string, problem when we change language)
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
#include "../ui_actions.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_font.h"
#include "../ui_render.h"
#include "../ui_sound.h"
#include "../ui_sprite.h"
#include "ui_node_button.h"
#include "ui_node_custombutton.h"
#include "ui_node_abstractnode.h"
#include "ui_node_panel.h"

#define EXTRADATA_TYPE buttonExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

#include "../../client.h"

#define TILE_SIZE 64
#define CORNER_SIZE 17
#define MID_SIZE 1
#define MARGE 3

/**
 * @brief Handles Button clicks
 * @todo it is useless !
 */
void uiButtonNode::onLeftClick (uiNode_t * node, int x, int y)
{
	if (node->onClick) {
		UI_ExecuteEventActions(node, node->onClick);
		UI_PlaySound("click1");
	}
}

/**
 * @brief Handles Button draw
 */
void uiButtonNode::draw (uiNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	int texX, texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	static const vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};
	uiSpriteStatus_t iconStatus = SPRITE_STATUS_NORMAL;
	const char *font = UI_GetFontFromNode(node);

	if (!node->onClick || node->disabled) {
		/** @todo need custom color when button is disabled */
		textColor = disabledColor;
		texX = TILE_SIZE;
		texY = TILE_SIZE;
		iconStatus = SPRITE_STATUS_DISABLED;
	} else if (node->state) {
		textColor = node->selectedColor;
		texX = TILE_SIZE;
		texY = 0;
		iconStatus = SPRITE_STATUS_HOVER;
	} else {
		textColor = node->color;
		texX = 0;
		texY = 0;
	}

	UI_GetNodeAbsPos(node, pos);

	image = UI_GetReferenceString(node, node->image);
	if (image)
		UI_DrawPanel(pos, node->size, image, texX, texY, panelTemplate);

	if (EXTRADATA(node).background) {
		UI_DrawSpriteInBox(qfalse, EXTRADATA(node).background, iconStatus, pos[0], pos[1], node->size[0], node->size[1]);
	}

	/* compute node box with padding */
	uiBox_t inside;
	inside.set(pos, node->size);
	inside.expand(-node->padding);
	uiBox_t content;
	content.clear();

	const bool hasIcon = EXTRADATA(node).icon != NULL;
	const char* text = UI_GetReferenceString(node, node->text);
	const bool hasText = text != NULL && *text != '\0';
	if (hasText) {
		text = _(text);
	}

	vec2_t iconPos;
	Vector2Clear(iconPos);
	if (hasIcon) {
		content.size[0] += EXTRADATA(node).icon->size[0];
		content.size[1] += EXTRADATA(node).icon->size[1];
	}

	vec2_t textPos;
	Vector2Clear(textPos);
	int textWidth;
	if (hasText) {
		textPos[0] = content.size[0];
		const int availableTextWidth = inside.size[0] - content.size[0];
		int height;
		R_FontTextSize(font, text, availableTextWidth, LONGLINES_PRETTYCHOP, &textWidth, &height, NULL, NULL);
		content.size[0] += textWidth;
		if (height > content.size[1]) {
			content.size[1] = height;
			iconPos[1] = (height - content.size[1]) * 0.5;
		} else {
			textPos[1] = (content.size[1] - height) * 0.5;
		}
	}

	inside.alignBox(content, (align_t)node->contentAlign);

	/* display the icon at the left */
	/** @todo should we move it according to the text align? */
	if (hasIcon) {
		iconPos[0] += content.pos[0];
		iconPos[1] += content.pos[1];
		UI_DrawSpriteInBox(EXTRADATA(node).flipIcon, EXTRADATA(node).icon, iconStatus,
				iconPos[0], iconPos[1], EXTRADATA(node).icon->size[0], EXTRADATA(node).icon->size[1]);
	}

	if (hasText) {
		textPos[0] += content.pos[0];
		textPos[1] += content.pos[1];
		R_Color(textColor);
		/* @todo here IMO we should not use contentalign. need to check other toolkits to check layout */
		UI_DrawStringInBox(font, (align_t) node->contentAlign,
			textPos[0], content.pos[1], textWidth, content.size[1],
			text, LONGLINES_PRETTYCHOP);
		R_Color(NULL);
	}
}

/**
 * @brief Handles Button before loading. Used to set default attribute values
 */
void uiButtonNode::onLoading (uiNode_t *node)
{
	node->padding = 8;
	node->contentAlign = ALIGN_CC;
	Vector4Set(node->selectedColor, 1, 1, 1, 1);
	Vector4Set(node->color, 1, 1, 1, 1);
}

/**
 * @brief Handled after the end of the load of the node from script (all data and/or child are set)
 */
void uiButtonNode::onLoaded (uiNode_t *node)
{
	/* auto calc the size if none was given via script files */
	if (node->size[1] == 0) {
		const char *font = UI_GetFontFromNode(node);
		node->size[1] = (UI_FontGetHeight(font) / 2) + (node->padding * 2);
	}
#ifdef DEBUG
	if (node->size[0] < CORNER_SIZE + MID_SIZE + CORNER_SIZE || node->size[1] < CORNER_SIZE + MID_SIZE + CORNER_SIZE)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' too small. It can create graphical glitches\n", UI_GetPath(node));
#endif
}

void UI_RegisterButtonNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "button";
	behaviour->manager = new uiButtonNode();
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Texture used by the button. It's a normalized texture of 128x128.
	 * Normal button start at 0x0, mouse over start at 64x0, mouse click
	 * start at 0x64 (but not yet implemented), and disabled start at 64x64.
	 * See the image to have a usable template for this node.
	 * @image html http://ufoai.org/wiki/images/Button_blue.png
	 */
	UI_RegisterOveridedNodeProperty(behaviour, "image");

	/* Icon used to display the node
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "icon", V_UI_SPRITEREF, EXTRADATA_TYPE, icon);
	UI_RegisterExtradataNodeProperty(behaviour, "flipicon", V_CPPBOOL, EXTRADATA_TYPE, flipIcon);

	/* Sprite used to display the background */
	UI_RegisterExtradataNodeProperty(behaviour, "background", V_UI_SPRITEREF, EXTRADATA_TYPE, background);
}
