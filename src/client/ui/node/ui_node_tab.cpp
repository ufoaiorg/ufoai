/**
 * @file
 * @todo auto tooltip for chopped options
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../ui_sound.h"
#include "../ui_sprite.h"
#include "../ui_render.h"
#include "../ui_tooltip.h"
#include "ui_node_tab.h"
#include "ui_node_abstractnode.h"
#include "ui_node_abstractoption.h"

#include "../../cl_language.h"
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

/**
 * @brief Return a tab located at a screen position
 * @param[in] node A tab node
 * @param[in] x,y The position of the screen to test
 * @return A uiNode_t, or nullptr if nothing.
 * @todo improve test when we are on a junction
 * @todo improve test when we are on a chopped tab
 */
static uiNode_t* UI_TabNodeTabAtPosition (const uiNode_t* node, int x, int y)
{
	UI_NodeAbsoluteToRelativePos(node, &x, &y);

	/** @todo this doesn't work when an option is hidden */
	int allowedWidth = node->box.size[0] - TILE_WIDTH * (EXTRADATACONST(node).count + 1);

	/* Bounded box test (should not be need, but there are problems without it) */
	if (x < 0 || y < 0 || x >= node->box.size[0] || y >= node->box.size[1])
		return nullptr;

	const char* font = UI_GetFontFromNode(node);

	/* Text box test */
	uiNode_t* prev = nullptr;
	for (uiNode_t* option = node->firstChild; option; option = option->next) {
		int tabWidth;
		assert(option->behaviour == ui_optionBehaviour);

		/* skip hidden options */
		if (option->invis)
			continue;

		if (x < TILE_WIDTH / 2)
			return prev;

		const char* label = CL_Translate(OPTIONEXTRADATA(option).label);

		R_FontTextSize(font, label, 0, LONGLINES_PRETTYCHOP, &tabWidth, nullptr, nullptr, nullptr);
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
	return nullptr;
}

/**
 * @brief Handles tab clicks
 */
void uiTabNode::onLeftClick (uiNode_t* node, int x, int y)
{
	uiNode_t* option;

	if (UI_AbstractOptionGetCurrentValue(node) == nullptr)
		return;

	option = UI_TabNodeTabAtPosition(node, x, y);
	if (option == nullptr)
		return;

	if (option->disabled)
		return;

	/* only execute the click stuff if the selectbox is active */
	if (node->state)
		UI_AbstractOptionSetCurrentValue(node, OPTIONEXTRADATA(option).value);

	UI_PlaySound("click1");
}

void uiTabNode::draw (uiNode_t* node)
{
	const char* ref = UI_AbstractOptionGetCurrentValue(node);
	if (ref == nullptr)
		return;

	const char* font = UI_GetFontFromNode(node);

	uiNode_t* overMouseOption = nullptr;
	if (node->state) {
		overMouseOption = UI_TabNodeTabAtPosition(node, mousePosX, mousePosY);
	}

	vec2_t pos;
	UI_GetNodeAbsPos(node, pos);
	int currentX = pos[0];
	uiNode_t* option = node->firstChild;
	assert(option->behaviour == ui_optionBehaviour);
	/** @todo this doesn't work when an option is hidden */
	int allowedWidth = node->box.size[0] - TILE_WIDTH * (EXTRADATA(node).count + 1);

	while (option) {
		int fontHeight;
		int fontWidth;
		int tabWidth;
		int textPos;
		bool drawIcon = false;
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

		currentX += TILE_WIDTH;

		const char* label = CL_Translate(OPTIONEXTRADATA(option).label);

		R_FontTextSize(font, label, 0, LONGLINES_PRETTYCHOP, &fontWidth, &fontHeight, nullptr, nullptr);
		tabWidth = fontWidth;
		if (OPTIONEXTRADATA(option).icon && OPTIONEXTRADATA(option).icon->size[0] < allowedWidth) {
			tabWidth += OPTIONEXTRADATA(option).icon->size[0];
			drawIcon = true;
		}
		if (tabWidth > allowedWidth) {
			if (allowedWidth > 0)
				tabWidth = allowedWidth;
			else
				tabWidth = 0;
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
		UI_DrawString(font, ALIGN_UL, textPos, pos[1] + ((node->box.size[1] - fontHeight) / 2),
			textPos, tabWidth + 1, 0, label, 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
		currentX += tabWidth;
		allowedWidth -= tabWidth;

		/* Next */
		option = option->next;
	}
}

/**
 * @brief Custom tooltip of tab node
 * @param[in] node Node we request to draw tooltip
 * @param[in] x,y Position of the mouse
 */
void uiTabNode::drawTooltip (const uiNode_t* node, int x, int y) const
{
	uiNode_t* option;
	const int tooltipWidth = 250;

	option = UI_TabNodeTabAtPosition(node, x, y);
	if (option == nullptr)
		return;

	if (!OPTIONEXTRADATA(option).truncated)
		return;

	const char* label = CL_Translate(OPTIONEXTRADATA(option).label);
	UI_DrawTooltip(label, x, y, tooltipWidth);
}

/** called when the window is pushed
 * check cvar then, reduce runtime check
 * @todo move cvar check to AbstractOption
 */
void uiTabNode::onWindowOpened (uiNode_t* node, linkedList_t* params)
{
	/* no cvar given? */
	if (!(EXTRADATA(node).cvar))
		return;

	/* not a cvar? */
	char const* const cvarName = Q_strstart(EXTRADATA(node).cvar, "*cvar:");
	if (!cvarName) {
		/* normalize */
		Com_Printf("UI_TabNodeInit: node '%s' doesn't have a valid cvar assigned (\"%s\" read)\n", UI_GetPath(node), EXTRADATA(node).cvar);
		EXTRADATA(node).cvar = nullptr;
		return;
	}

	/* cvar does not exist? */
	if (Cvar_FindVar(cvarName) == nullptr) {
		/* search default value, if possible */
		uiNode_t* option = node->firstChild;
		assert(option->behaviour == ui_optionBehaviour);
		Cvar_ForceSet(cvarName, OPTIONEXTRADATA(option).value);
	}
}

void UI_RegisterTabNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "tab";
	behaviour->extends = "abstractoption";
	behaviour->manager = UINodePtr(new uiTabNode());
	behaviour->drawItselfChild = true;
}
