/**
 * @file ui_node_tab.c
 * @todo auto tooltip for chopped options
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
#include "../ui_behaviour.h"
#include "../ui_parse.h"
#include "../ui_actions.h"
#include "../ui_font.h"
#include "../ui_input.h"
#include "../ui_sprite.h"
#include "../ui_render.h"
#include "../ui_tooltip.h"
#include "ui_node_tab.h"
#include "ui_node_abstractnode.h"
#include "ui_node_abstractoption.h"

#include "../../client.h" /* gettext _() */
#include "../../input/cl_input.h"

#define EXTRADATA_TYPE abstractOptionExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

typedef enum {
	UI_TAB_NOTHING = 0,
	UI_TAB_NORMAL = 1,
	UI_TAB_SELECTED = 2,
	UI_TAB_HIGHLIGHTED = 3,
	UI_TAB_DISABLED = 4
} ui_tabStatus_t;

static const int TILE_WIDTH = 33;
static const int TILE_HEIGHT = 36;
static const int TILE_SIZE = 40;

/**
 * @brief Return a tab located at a screen position
 * @param[in] node A tab node
 * @param[in] x The x position of the screen to test
 * @param[in] y The x position of the screen to test
 * @return A uiNode_t, or NULL if nothing.
 * @todo improve test when we are on a junction
 * @todo improve test when we are on a chopped tab
 */
static uiNode_t* UI_TabNodeTabAtPosition (const uiNode_t *node, int x, int y)
{
	const char *font;
	uiNode_t* option;
	uiNode_t* prev = NULL;
	int allowedWidth;

	UI_NodeAbsoluteToRelativePos(node, &x, &y);

	/** @todo this dont work when an option is hidden */
	allowedWidth = node->size[0] - TILE_WIDTH * (EXTRADATACONST(node).count + 1);

	/* Bounded box test (shound not need, but there are problem) */
	if (x < 0 || y < 0 || x >= node->size[0] || y >= node->size[1])
		return NULL;

	font = UI_GetFontFromNode(node);

	/* Text box test */
	for (option = node->firstChild; option; option = option->next) {
		int tabWidth;
		const char *label;
		assert(option->behaviour == ui_optionBehaviour);

		/* skip hidden options */
		if (option->invis)
			continue;

		if (x < TILE_WIDTH / 2)
			return prev;

		label = OPTIONEXTRADATA(option).label;
		if (label[0] == '_')
			label = _(label + 1);

		R_FontTextSize(font, label, 0, LONGLINES_PRETTYCHOP, &tabWidth, NULL, NULL, NULL);
		if (OPTIONEXTRADATA(option).icon && OPTIONEXTRADATA(option).icon->size[0] < allowedWidth) {
			tabWidth += OPTIONEXTRADATA(option).icon->size[0];
		}
		if (tabWidth > allowedWidth) {
			if (allowedWidth > 0)
				tabWidth = allowedWidth;
			else
				tabWidth = 0;
		}

		if (x < tabWidth + TILE_WIDTH)
			return option;

		allowedWidth -= tabWidth;
		x -= tabWidth + TILE_WIDTH;
		prev = option;
	}
	if (x < TILE_WIDTH / 2)
		return prev;
	return NULL;
}

/**
 * @brief Handles tab clicks
 */
static void UI_TabNodeClick (uiNode_t * node, int x, int y)
{
	uiNode_t* option;

	if (UI_AbstractOptionGetCurrentValue(node) == NULL)
		return;

	option = UI_TabNodeTabAtPosition(node, x, y);
	if (option == NULL)
		return;

	if (option->disabled)
		return;

	/* only execute the click stuff if the selectbox is active */
	if (node->state)
		UI_AbstractOptionSetCurrentValue(node, OPTIONEXTRADATA(option).value);
}

/**
 * @brief Normalized access to the texture structure of tab to display the plain part of a tab
 * @param[in] image The normalized tab texture to use
 * @param[in] x The upper-left position of the screen to draw the texture
 * @param[in] y The upper-left position of the screen to draw the texture
 * @param[in] width The width size of the screen to use (stretch)
 * @param[in] type The status of the tab we display
 */
static inline void UI_TabNodeDrawPlain (const char *image, int x, int y, int width, ui_tabStatus_t type)
{
	/* Hack sl=1 to not use the pixel on the left border on the texture (create graphic bug) */
	UI_DrawNormImageByName(qfalse, x, y, width, TILE_HEIGHT, TILE_WIDTH + TILE_SIZE * 0, TILE_HEIGHT + TILE_SIZE * type,
		1 + TILE_SIZE * 0, 0 + TILE_SIZE * type, image);
}

/**
 * @brief Normalized access to the texture structure of tab to display a junction between each tabs
 * @param[in] image The normalized tab texture to use
 * @param[in] x The upper-left position of the screen to draw the texture
 * @param[in] y The upper-left position of the screen to draw the texture
 * @param[in] leftType The status of the left tab of the junction we display
 * @param[in] rightType The status of the right tab of the junction we display
 */
static inline void UI_TabNodeDrawJunction (const char *image, int x, int y, ui_tabStatus_t leftType, ui_tabStatus_t rightType)
{
	UI_DrawNormImageByName(qfalse, x, y, TILE_WIDTH, TILE_HEIGHT, TILE_WIDTH + TILE_SIZE * (1 + rightType), TILE_HEIGHT + TILE_SIZE * leftType,
		0 + TILE_SIZE * (1 + rightType), 0 + TILE_SIZE * leftType, image);
}

static void UI_TabNodeDraw (uiNode_t *node)
{
	ui_tabStatus_t lastStatus = UI_TAB_NOTHING;
	uiNode_t* option;
	uiNode_t* overMouseOption = NULL;
	const char *ref;
	const char *font;
	int currentX;
	int allowedWidth;
	vec2_t pos;

	const char* image = UI_GetReferenceString(node, node->image);
	if (!image)
		image = "ui/tab";

	ref = UI_AbstractOptionGetCurrentValue(node);
	if (ref == NULL)
		return;

	font = UI_GetFontFromNode(node);

	if (node->state) {
		overMouseOption = UI_TabNodeTabAtPosition(node, mousePosX, mousePosY);
	}

	UI_GetNodeAbsPos(node, pos);
	currentX = pos[0];
	option = node->firstChild;
	assert(option->behaviour == ui_optionBehaviour);
	/** @todo this dont work when an option is hidden */
	allowedWidth = node->size[0] - TILE_WIDTH * (EXTRADATA(node).count + 1);

	while (option) {
		int fontHeight;
		int fontWidth;
		int tabWidth;
		int textPos;
		const char *label;
		qboolean drawIcon = qfalse;
		ui_tabStatus_t status = UI_TAB_NORMAL;
		assert(option->behaviour == ui_optionBehaviour);

		/* skip hidden options */
		if (option->invis) {
			option = option->next;
			continue;
		}

		/* Check the status of the current tab */
		if (Q_streq(OPTIONEXTRADATA(option).value, ref)) {
			status = UI_TAB_SELECTED;
		} else if (option->disabled || node->disabled) {
			status = UI_TAB_DISABLED;
		} else if (option == overMouseOption) {
			status = UI_TAB_HIGHLIGHTED;
		}

		/* Display */
		UI_TabNodeDrawJunction(image, currentX, pos[1], lastStatus, status);
		currentX += TILE_WIDTH;

		label = OPTIONEXTRADATA(option).label;
		if (label[0] == '_')
			label = _(label + 1);

		R_FontTextSize(font, label, 0, LONGLINES_PRETTYCHOP, &fontWidth, &fontHeight, NULL, NULL);
		tabWidth = fontWidth;
		if (OPTIONEXTRADATA(option).icon && OPTIONEXTRADATA(option).icon->size[0] < allowedWidth) {
			tabWidth += OPTIONEXTRADATA(option).icon->size[0];
			drawIcon = qtrue;
		}
		if (tabWidth > allowedWidth) {
			if (allowedWidth > 0)
				tabWidth = allowedWidth;
			else
				tabWidth = 0;
		}

		if (tabWidth > 0) {
			UI_TabNodeDrawPlain(image, currentX, pos[1], tabWidth, status);
		}

		textPos = currentX;
		if (drawIcon) {
			uiSpriteStatus_t iconStatus = SPRITE_STATUS_NORMAL;
			if (status == UI_TAB_DISABLED) {
				iconStatus = SPRITE_STATUS_DISABLED;
			}
			UI_DrawSpriteInBox(OPTIONEXTRADATA(option).flipIcon, OPTIONEXTRADATA(option).icon, iconStatus, currentX, pos[1], OPTIONEXTRADATA(option).icon->size[0], TILE_HEIGHT);
			textPos += OPTIONEXTRADATA(option).icon->size[0];
		}

		/** @todo fontWidth can be =0, maybe a bug from the font cache */
		OPTIONEXTRADATA(option).truncated = tabWidth < fontWidth || tabWidth == 0;
		UI_DrawString(font, ALIGN_UL, textPos, pos[1] + ((node->size[1] - fontHeight) / 2),
			textPos, tabWidth + 1, 0, label, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		currentX += tabWidth;
		allowedWidth -= tabWidth;

		/* Next */
		lastStatus = status;
		option = option->next;
	}

	/* Display last junction and end of header */
	UI_TabNodeDrawJunction(image, currentX, pos[1], lastStatus, UI_TAB_NOTHING);
	currentX += TILE_WIDTH;
	if (currentX < pos[0] + node->size[0])
		UI_TabNodeDrawPlain(image, currentX, pos[1], pos[0] + node->size[0] - currentX, UI_TAB_NOTHING);
}

/**
 * @brief Custom tooltip of tab node
 * @param[in] node Node we request to draw tooltip
 * @param[in] x Position x of the mouse
 * @param[in] y Position y of the mouse
 */
static void UI_TabNodeDrawTooltip (uiNode_t *node, int x, int y)
{
	uiNode_t *option;
	const int tooltipWidth = 250;
	const char *label;

	option = UI_TabNodeTabAtPosition(node, x, y);
	if (option == NULL)
		return;

	if (!OPTIONEXTRADATA(option).truncated)
		return;

	label = OPTIONEXTRADATA(option).label;
	if (label[0] == '_')
		label = _(label + 1);

	UI_DrawTooltip(label, x, y, tooltipWidth);
}

/** called when the window is pushed
 * check cvar then, reduce runtime check
 * @todo move cvar check to AbstractOption
 */
static void UI_TabNodeInit (uiNode_t *node, linkedList_t *params)
{
	const char *cvarName;

	/* no cvar given? */
	if (!(EXTRADATA(node).cvar))
		return;

	/* not a cvar? */
	if (!Q_strstart(EXTRADATA(node).cvar, "*cvar:")) {
		/* normalize */
		Com_Printf("UI_TabNodeInit: node '%s' doesn't have a valid cvar assigned (\"%s\" read)\n", UI_GetPath(node), EXTRADATA(node).cvar);
		EXTRADATA(node).cvar = NULL;
		return;
	}

	/* cvar do not exists? */
	cvarName = &EXTRADATA(node).cvar[6];
	if (Cvar_FindVar(cvarName) == NULL) {
		/* search default value, if possible */
		uiNode_t* option = node->firstChild;
		assert(option->behaviour == ui_optionBehaviour);
		Cvar_ForceSet(cvarName, OPTIONEXTRADATA(option).value);
	}
}

void UI_RegisterTabNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "tab";
	behaviour->extends = "abstractoption";
	behaviour->draw = UI_TabNodeDraw;
	behaviour->drawTooltip = UI_TabNodeDrawTooltip;
	behaviour->leftClick = UI_TabNodeClick;
	behaviour->windowOpened = UI_TabNodeInit;
	behaviour->drawItselfChild = qtrue;
}
