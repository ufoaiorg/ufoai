/**
 * @file
 * @todo manage disabled option
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
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_actions.h"
#include "../ui_font.h"
#include "../ui_sprite.h"
#include "../ui_render.h"
#include "../ui_input.h"
#include "ui_node_abstractoption.h"
#include "ui_node_abstractnode.h"
#include "ui_node_optionlist.h"
#include "ui_node_panel.h"
#include "ui_node_option.h"

#include "../../cl_language.h"
#include "../../input/cl_keys.h"

#define EXTRADATA_TYPE abstractOptionExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)

#define CORNER_SIZE 25
#define MID_SIZE 1
#define MARGE 3

/* Used for drag&drop-like scrolling */
static int mouseScrollX;
static int mouseScrollY;

/**
 * @brief Update the scroll according to the number
 * of items and the size of the node
 */
static void UI_OptionListNodeUpdateScroll (uiNode_t* node)
{
	int lineHeight;
	bool updated;
	int elements;

	lineHeight =  EXTRADATA(node).lineHeight;
	if (lineHeight == 0) {
		const char* font = UI_GetFontFromNode(node);
		lineHeight = UI_FontGetHeight(font);
	}

	elements = (node->box.size[1] - node->padding - node->padding) / lineHeight;
	updated = EXTRADATA(node).scrollY.set(-1, elements, EXTRADATA(node).count);
	if (updated && EXTRADATA(node).onViewChange)
		UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);
}

void uiOptionListNode::draw (uiNode_t* node)
{
	uiNode_t* option;
	const char* ref;
	const char* font;
	int lineHeight;
	vec2_t pos;
	int currentY;
	const float* textColor;
	int count = 0;

	ref = UI_AbstractOptionGetCurrentValue(node);
	if (ref == nullptr)
		return;

	UI_GetNodeAbsPos(node, pos);

	if (EXTRADATA(node).background) {
		UI_DrawSpriteInBox(false, EXTRADATA(node).background, SPRITE_STATUS_NORMAL, pos[0], pos[1], node->box.size[0], node->box.size[1]);
	}

	font = UI_GetFontFromNode(node);

	lineHeight =  EXTRADATA(node).lineHeight;
	if (lineHeight == 0)
		lineHeight = UI_FontGetHeight(font);
	currentY = pos[1] + node->padding;

	/* skip option over current position */
	option = UI_AbstractOptionGetFirstOption(node);
	while (option && count < EXTRADATA(node).scrollY.viewPos) {
		option = option->next;
		count++;
	}

	/* draw all available options for this selectbox */
	for (; option; option = option->next) {
		int decX = pos[0] + node->padding;
		/* outside the node */
		if (currentY + lineHeight > pos[1] + node->box.size[1] - node->padding) {
			count++;
			break;
		}

		/* draw the hover effect */
		if (OPTIONEXTRADATA(option).hovered)
			UI_DrawFill(pos[0] + node->padding, currentY, node->box.size[0] - node->padding - node->padding, lineHeight, node->color);

		/* text color */
		if (Q_streq(OPTIONEXTRADATA(option).value, ref)) {
			textColor = node->selectedColor;
		} else if (node->disabled || option->disabled) {
			textColor = node->disabledColor;
		} else if (option->color[3] == 0.0f) {
			textColor = node->color;
		} else {
			textColor = option->color;
		}

		if (OPTIONEXTRADATA(option).icon) {
			uiSpriteStatus_t iconStatus = SPRITE_STATUS_NORMAL;
			if (option->disabled)
				iconStatus = SPRITE_STATUS_DISABLED;
			R_Color(nullptr);
			UI_DrawSpriteInBox(OPTIONEXTRADATA(option).flipIcon, OPTIONEXTRADATA(option).icon, iconStatus, decX, currentY, OPTIONEXTRADATA(option).icon->size[0], lineHeight);
			decX += OPTIONEXTRADATA(option).icon->size[0] + lineHeight / 4;
		}

		/* print the option label */
		const char* label = CL_Translate(OPTIONEXTRADATA(option).label);

		R_Color(textColor);
		UI_DrawString(font, ALIGN_UL, decX, currentY,
			pos[0], node->box.size[0] - node->padding - node->padding,
			0, label, 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);

		/* next entries' position */
		currentY += lineHeight;
		count++;
	}
	R_Color(nullptr);

	/* count number of options (current architecture doesn't allow to know if the data change) */
	for (; option; option = option->next) {
		count++;
	}

	if (EXTRADATA(node).count != count) {
		EXTRADATA(node).count = count;
	}

	UI_OptionListNodeUpdateScroll(node);
}

static uiNode_t* UI_OptionListNodeGetOptionAtPosition (uiNode_t* node, int x, int y)
{
	uiNode_t* option;
	vec2_t pos;
	int lineHeight;
	int currentY;
	int count = 0;

	UI_GetNodeAbsPos(node, pos);
	currentY = pos[1] + node->padding;

	lineHeight =  EXTRADATA(node).lineHeight;
	if (lineHeight == 0) {
		const char* font = UI_GetFontFromNode(node);
		lineHeight = UI_FontGetHeight(font);
	}

	option = UI_AbstractOptionGetFirstOption(node);
	while (option && count < EXTRADATA(node).scrollY.viewPos) {
		option = option->next;
		count++;
	}

	/* now draw all available options for this selectbox */
	for (; option; option = option->next) {
		if (y < currentY + lineHeight)
			return option;
		if (currentY + lineHeight > pos[1] + node->box.size[1] - node->padding)
			break;
		currentY += lineHeight;
	}
	return nullptr;
}

/**
 * @brief Handles selectboxes clicks
 */
void uiOptionListNode::onLeftClick (uiNode_t* node, int x, int y)
{
	uiNode_t* option;

	if (UI_AbstractOptionGetCurrentValue(node) == nullptr)
		return;

	/* select the right option */
	option = UI_OptionListNodeGetOptionAtPosition(node, x, y);

	/* update the status */
	if (option)
		UI_AbstractOptionSetCurrentValue(node, OPTIONEXTRADATA(option).value);
}

/**
 * @brief Auto scroll the list
 */
bool uiOptionListNode::onScroll (uiNode_t* node, int deltaX, int deltaY)
{
	bool down = deltaY > 0;
	bool updated;
	if (deltaY == 0)
		return false;
	updated = EXTRADATA(node).scrollY.moveDelta(down ? 1 : -1);
	if (EXTRADATA(node).onViewChange && updated)
		UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);

	/* @todo use super behaviour */
	if (node->onWheelUp && !down) {
		UI_ExecuteEventActions(node, node->onWheelUp);
		updated = true;
	}
	if (node->onWheelDown && down) {
		UI_ExecuteEventActions(node, node->onWheelDown);
		updated = true;
	}
	if (node->onWheel) {
		UI_ExecuteEventActions(node, node->onWheel);
		updated = true;
	}
	return updated;
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
void uiOptionListNode::onLoading (uiNode_t* node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
	Vector4Set(node->disabledColor, 0.5, 0.5, 0.5, 1.0);
	EXTRADATA(node).versionId = -1;
	node->padding = 3;
}

void uiOptionListNode::onLoaded (uiNode_t* node)
{
}

/**
 * @brief Track mouse down/up events to implement drag&drop-like scrolling, for touchscreen devices
 * @sa UI_OptionListNodeMouseUp, UI_OptionListNodeCapturedMouseMove
*/
void uiOptionListNode::onMouseDown (uiNode_t* node, int x, int y, int button)
{
	if (!UI_GetMouseCapture() && button == K_MOUSE1 &&
		EXTRADATA(node).scrollY.fullSize > EXTRADATA(node).scrollY.viewSize) {
		UI_SetMouseCapture(node);
		mouseScrollX = x;
		mouseScrollY = y;
	}
}

void uiOptionListNode::onMouseUp (uiNode_t* node, int x, int y, int button)
{
	if (UI_GetMouseCapture() == node)  /* More checks can never hurt */
		UI_MouseRelease();
}

void uiOptionListNode::onCapturedMouseMove (uiNode_t* node, int x, int y)
{
	const int lineHeight = getCellHeight(node);
	const int deltaY = (mouseScrollY - y) / lineHeight;
	/* We're doing only vertical scroll, that's enough for the most instances */
	if (deltaY != 0) {
		bool updated;
		updated = EXTRADATA(node).scrollY.moveDelta(deltaY);
		if (EXTRADATA(node).onViewChange && updated)
			UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);
		/* @todo not accurate */
		mouseScrollX = x;
		mouseScrollY = y;
	}
	onMouseMove(node, x, y);
}

/**
 * @brief Return size of the cell, which is the size (in virtual "pixel") which represent 1 in the scroll values.
 * Here we guess the widget can scroll pixel per pixel.
 * @return Size in pixel.
 */
int uiOptionListNode::getCellHeight (uiNode_t* node)
{
	int lineHeight = EXTRADATA(node).lineHeight;
	if (lineHeight == 0)
		lineHeight = UI_FontGetHeight(UI_GetFontFromNode(node));
	return lineHeight;
}

void UI_RegisterOptionListNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "optionlist";
	behaviour->extends = "abstractoption";
	behaviour->manager = UINodePtr(new uiOptionListNode());
	behaviour->drawItselfChild = true;

	/* Sprite used to display the background */
	UI_RegisterExtradataNodeProperty(behaviour, "background", V_UI_SPRITEREF, EXTRADATA_TYPE, background);
}
