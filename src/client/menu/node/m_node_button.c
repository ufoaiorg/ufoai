/**
 * @file m_node_button.c
 * @brief <code>button</code> is a node to define a button with a random size. It is skinned
 * with a special image template (see the <code>image</code> property).
 * @todo add an icon if possible.
 * @todo implement clicked button when its possible.
 * @todo allow auto size (use the size need looking string, problem when we change language)
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../m_actions.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_render.h"
#include "../m_icon.h"
#include "m_node_button.h"
#include "m_node_custombutton.h"
#include "m_node_abstractnode.h"
#include "m_node_panel.h"

#define EXTRADATA_TYPE buttonExtraData_t
#define EXTRADATA(node) MN_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) MN_EXTRADATACONST(node, EXTRADATA_TYPE)

#include "../../client.h"

#define TILE_SIZE 64
#define CORNER_SIZE 17
#define MID_SIZE 1
#define MARGE 3

/**
 * @brief Handles Button clicks
 */
static void MN_ButtonNodeClick (uiNode_t * node, int x, int y)
{
	if (node->onClick) {
		MN_ExecuteEventActions(node, node->onClick);
	}
}

/**
 * @brief Handles Button draw
 */
static void MN_ButtonNodeDraw (uiNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	const char *text;
	int texX, texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};
	int iconPadding = 0;
	uiIconStatus_t iconStatus = ICON_STATUS_NORMAL;
	const char *font = MN_GetFontFromNode(node);

	if (!node->onClick || node->disabled) {
		/** @todo need custom color when button is disabled */
		textColor = disabledColor;
		texX = TILE_SIZE;
		texY = TILE_SIZE;
		iconStatus = ICON_STATUS_DISABLED;
	} else if (node->state) {
		textColor = node->selectedColor;
		texX = TILE_SIZE;
		texY = 0;
		iconStatus = ICON_STATUS_HOVER;
	} else {
		textColor = node->color;
		texX = 0;
		texY = 0;
	}

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node, node->image);
	if (image)
		MN_DrawPanel(pos, node->size, image, texX, texY, panelTemplate);

	/* display the icon at the left */
	/** @todo should we move it according to the text align? */
	if (EXTRADATA(node).icon) {
		/* use at least a box size equals to button height */
		int size = node->size[1] - node->padding - node->padding;
		if (size < EXTRADATA(node).icon->size[0])
			size = EXTRADATA(node).icon->size[0];
		MN_DrawIconInBox(EXTRADATA(node).icon, iconStatus, pos[0] + node->padding, pos[1] + node->padding, size, node->size[1] - node->padding - node->padding);
		iconPadding = size + node->padding;
	}

	text = MN_GetReferenceString(node, node->text);
	if (text != NULL && *text != '\0') {
		R_Color(textColor);
		text = _(text);
		MN_DrawStringInBox(font, node->textalign,
			pos[0] + node->padding + iconPadding, pos[1] + node->padding,
			node->size[0] - node->padding - node->padding - iconPadding, node->size[1] - node->padding - node->padding,
			text, LONGLINES_PRETTYCHOP);
		R_Color(NULL);
	}
}

/**
 * @brief Handles Button before loading. Used to set default attribute values
 */
static void MN_ButtonNodeLoading (uiNode_t *node)
{
	node->padding = 8;
	node->textalign = ALIGN_CC;
	Vector4Set(node->selectedColor, 1, 1, 1, 1);
	Vector4Set(node->color, 1, 1, 1, 1);
}

/**
 * @brief Handled after the end of the load of the node from script (all data and/or child are set)
 */
static void MN_ButtonNodeLoaded (uiNode_t *node)
{
	/* auto calc the size if none was given via script files */
	if (node->size[1] == 0) {
		const char *font = MN_GetFontFromNode(node);
		node->size[1] = (MN_FontGetHeight(font) / 2) + (node->padding * 2);
	}
#ifdef DEBUG
	if (node->size[0] < CORNER_SIZE + MID_SIZE + CORNER_SIZE || node->size[1] < CORNER_SIZE + MID_SIZE + CORNER_SIZE)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' too small. It can create graphical glitches\n", MN_GetPath(node));
#endif
}

static const value_t properties[] = {
	/* @override image
	 * Texture used by the button. It's a normalized texture of 128x128.
	 * Normal button start at 0x0, mouse over start at 64x0, mouse click
	 * start at 0x64 (but not yet implemented), and disabled start at 64x64.
	 * See the image to have a usable template for this node.
	 * @image html http://ufoai.ninex.info/wiki/images/Button_blue.png
	 */

	/* Icon used to display the node
	 */
	{"icon", V_UI_ICONREF, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, icon), MEMBER_SIZEOF(EXTRADATA_TYPE, icon)},

	{NULL, V_NULL, 0, 0}
};


void MN_RegisterButtonNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "button";
	behaviour->draw = MN_ButtonNodeDraw;
	behaviour->loaded = MN_ButtonNodeLoaded;
	behaviour->leftClick = MN_ButtonNodeClick;
	behaviour->loading = MN_ButtonNodeLoading;
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
}
