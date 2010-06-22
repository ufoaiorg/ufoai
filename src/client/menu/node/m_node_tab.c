/**
 * @file m_node_tab.c
 * @todo auto tooltip for chopped options
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
#include "../m_parse.h"
#include "../m_actions.h"
#include "../m_font.h"
#include "../m_input.h"
#include "../m_icon.h"
#include "../m_render.h"
#include "../m_tooltip.h"
#include "m_node_tab.h"
#include "m_node_abstractnode.h"
#include "m_node_abstractoption.h"

#include "../../client.h" /* gettext _() */
#include "../../input/cl_input.h"

#define EXTRADATA_TYPE abstractOptionExtraData_t
#define EXTRADATA(node) MN_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) MN_EXTRADATACONST(node, EXTRADATA_TYPE)

typedef enum {
	MN_TAB_NOTHING = 0,
	MN_TAB_NORMAL = 1,
	MN_TAB_SELECTED = 2,
	MN_TAB_HIGHLIGHTED = 3,
	MN_TAB_DISABLED = 4
} mn_tab_type_t;

static const int TILE_WIDTH = 33;
static const int TILE_HEIGHT = 36;
static const int TILE_SIZE = 40;

/**
 * @brief Return a tab located at a screen position
 * @param[in] node A tab node
 * @param[in] x The x position of the screen to test
 * @param[in] y The x position of the screen to test
 * @return A menuNode_t, or NULL if nothing.
 * @todo improve test when we are on a junction
 * @todo improve test when we are on a chopped tab
 */
static menuNode_t* MN_TabNodeTabAtPosition (const menuNode_t *node, int x, int y)
{
	const char *font;
	menuNode_t* option;
	menuNode_t* prev = NULL;
	int allowedWidth;

	MN_NodeAbsoluteToRelativePos(node, &x, &y);

	/** @todo this dont work when an option is hidden */
	allowedWidth = node->size[0] - TILE_WIDTH * (EXTRADATACONST(node).count + 1);

	/* Bounded box test (shound not need, but there are problem) */
	if (x < 0 || y < 0 || x >= node->size[0] || y >= node->size[1])
		return NULL;

	font = MN_GetFontFromNode(node);

	/* Text box test */
	for (option = node->firstChild; option; option = option->next) {
		int tabWidth;
		const char *label;
		assert(option->behaviour == optionBehaviour);

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
static void MN_TabNodeClick (menuNode_t * node, int x, int y)
{
	menuNode_t* option;

	if (MN_AbstractOptionGetCurrentValue(node) == NULL)
		return;

	option = MN_TabNodeTabAtPosition(node, x, y);
	if (option == NULL)
		return;

	if (option->disabled)
		return;

	/* only execute the click stuff if the selectbox is active */
	if (node->state)
		MN_AbstractOptionSetCurrentValue(node, OPTIONEXTRADATA(option).value);
}

/**
 * @brief Normalized access to the texture structure of tab to display the plain part of a tab
 * @param[in] image The normalized tab texture to use
 * @param[in] x The upper-left position of the screen to draw the texture
 * @param[in] y The upper-left position of the screen to draw the texture
 * @param[in] width The width size of the screen to use (stretch)
 * @param[in] type The status of the tab we display
 */
static inline void MN_TabNodeDrawPlain (const char *image, int x, int y, int width, mn_tab_type_t type)
{
	/* Hack sl=1 to not use the pixel on the left border on the texture (create graphic bug) */
	MN_DrawNormImageByName(x, y, width, TILE_HEIGHT, TILE_WIDTH + TILE_SIZE * 0, TILE_HEIGHT + TILE_SIZE * type,
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
static inline void MN_TabNodeDrawJunction (const char *image, int x, int y, mn_tab_type_t leftType, mn_tab_type_t rightType)
{
	MN_DrawNormImageByName(x, y, TILE_WIDTH, TILE_HEIGHT, TILE_WIDTH + TILE_SIZE * (1 + rightType), TILE_HEIGHT + TILE_SIZE * leftType,
		0 + TILE_SIZE * (1 + rightType), 0 + TILE_SIZE * leftType, image);
}

static void MN_TabNodeDraw (menuNode_t *node)
{
	mn_tab_type_t lastStatus = MN_TAB_NOTHING;
	menuNode_t* option;
	menuNode_t* overMouseOption = NULL;
	const char *ref;
	const char *font;
	int currentX;
	int allowedWidth;
	vec2_t pos;

	const char* image = MN_GetReferenceString(node, node->image);
	if (!image)
		image = "ui/tab";

	ref = MN_AbstractOptionGetCurrentValue(node);
	if (ref == NULL)
		return;

	font = MN_GetFontFromNode(node);

	if (node->state) {
		overMouseOption = MN_TabNodeTabAtPosition(node, mousePosX, mousePosY);
	}

	MN_GetNodeAbsPos(node, pos);
	currentX = pos[0];
	option = node->firstChild;
	assert(option->behaviour == optionBehaviour);
	/** @todo this dont work when an option is hidden */
	allowedWidth = node->size[0] - TILE_WIDTH * (EXTRADATA(node).count + 1);

	while (option) {
		int fontHeight;
		int fontWidth;
		int tabWidth;
		int textPos;
		const char *label;
		qboolean drawIcon = qfalse;
		mn_tab_type_t status = MN_TAB_NORMAL;
		assert(option->behaviour == optionBehaviour);

		/* skip hidden options */
		if (option->invis) {
			option = option->next;
			continue;
		}

		/* Check the status of the current tab */
		if (!strcmp(OPTIONEXTRADATA(option).value, ref)) {
			status = MN_TAB_SELECTED;
		} else if (option->disabled || node->disabled) {
			status = MN_TAB_DISABLED;
		} else if (option == overMouseOption) {
			status = MN_TAB_HIGHLIGHTED;
		}

		/* Display */
		MN_TabNodeDrawJunction(image, currentX, pos[1], lastStatus, status);
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
			MN_TabNodeDrawPlain(image, currentX, pos[1], tabWidth, status);
		}

		textPos = currentX;
		if (drawIcon) {
			iconStatus_t iconStatus = ICON_STATUS_NORMAL;
			if (status == MN_TAB_DISABLED) {
				iconStatus = ICON_STATUS_DISABLED;
			}
			MN_DrawIconInBox(OPTIONEXTRADATA(option).icon, iconStatus, currentX, pos[1], OPTIONEXTRADATA(option).icon->size[0], TILE_HEIGHT);
			textPos += OPTIONEXTRADATA(option).icon->size[0];
		}

		/** @todo fontWidth can be =0, maybe a bug from the font cache */
		OPTIONEXTRADATA(option).truncated = tabWidth < fontWidth || tabWidth == 0;
		MN_DrawString(font, ALIGN_UL, textPos, pos[1] + ((node->size[1] - fontHeight) / 2),
			textPos, pos[1], tabWidth + 1, TILE_HEIGHT,
			0, label, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		currentX += tabWidth;
		allowedWidth -= tabWidth;

		/* Next */
		lastStatus = status;
		option = option->next;
	}

	/* Display last junction and end of header */
	MN_TabNodeDrawJunction(image, currentX, pos[1], lastStatus, MN_TAB_NOTHING);
	currentX += TILE_WIDTH;
	if (currentX < pos[0] + node->size[0])
		MN_TabNodeDrawPlain(image, currentX, pos[1], pos[0] + node->size[0] - currentX, MN_TAB_NOTHING);
}

/**
 * @brief Custom tooltip of tab node
 * @param[in] node Node we request to draw tooltip
 * @param[in] x Position x of the mouse
 * @param[in] y Position y of the mouse
 */
static void MN_TabNodeDrawTooltip (menuNode_t *node, int x, int y)
{
	menuNode_t *option;
	const int tooltipWidth = 250;
	const char *label;

	option = MN_TabNodeTabAtPosition(node, x, y);
	if (option == NULL)
		return;

	if (!OPTIONEXTRADATA(option).truncated)
		return;

	label = OPTIONEXTRADATA(option).label;
	if (label[0] == '_')
		label = _(label + 1);

	MN_DrawTooltip(label, x, y, tooltipWidth, 0);
}

/** called when the window is pushed
 * check cvar then, reduce runtime check
 * @todo move cvar check to AbstractOption
 */
static void MN_TabNodeInit (menuNode_t *node)
{
	const char *cvarName;

	/* no cvar given? */
	if (!(EXTRADATA(node).cvar))
		return;

	/* not a cvar? */
	if (strncmp((char *)(EXTRADATA(node).cvar), "*cvar:", 6) != 0) {
		/* normalize */
		Com_Printf("MN_TabNodeInit: node '%s' doesn't have a valid cvar assigned (\"%s\" read)\n", MN_GetPath(node), (char*) (EXTRADATA(node).cvar));
		EXTRADATA(node).cvar = NULL;
		return;
	}

	/* cvar do not exists? */
	cvarName = &((const char *)(EXTRADATA(node).cvar))[6];
	if (Cvar_FindVar(cvarName) == NULL) {
		/* search default value, if possible */
		menuNode_t* option = node->firstChild;
		assert(option->behaviour == optionBehaviour);
		if (option)
			Cvar_ForceSet(cvarName, OPTIONEXTRADATA(option).value);
	}
}

void MN_RegisterTabNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "tab";
	behaviour->extends = "abstractoption";
	behaviour->draw = MN_TabNodeDraw;
	behaviour->drawTooltip = MN_TabNodeDrawTooltip;
	behaviour->leftClick = MN_TabNodeClick;
	behaviour->init = MN_TabNodeInit;
	behaviour->drawItselfChild = qtrue;
}
