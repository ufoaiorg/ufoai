/**
 * @file ui_node_optionlist.c
 * @todo manage disabled option
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

#include "../ui_main.h"
#include "../ui_parse.h"
#include "../ui_actions.h"
#include "../ui_font.h"
#include "../ui_sprite.h"
#include "../ui_render.h"
#include "ui_node_abstractoption.h"
#include "ui_node_abstractnode.h"
#include "ui_node_optionlist.h"
#include "ui_node_panel.h"
#include "ui_node_option.h"

#include "../../client.h" /* gettext _() */

#define EXTRADATA(node) UI_EXTRADATA(node, abstractOptionExtraData_t)

#define CORNER_SIZE 25
#define MID_SIZE 1
#define MARGE 3

/**
 * @brief Update the scroll according to the number
 * of items and the size of the node
 */
static void UI_OptionListNodeUpdateScroll (uiNode_t *node)
{
	const char *font;
	int fontHeight;
	qboolean updated;
	int elements;

	font = UI_GetFontFromNode(node);
	fontHeight = UI_FontGetHeight(font);

	elements = (node->size[1] - node->padding - node->padding) / fontHeight;
	updated = UI_SetScroll(&EXTRADATA(node).scrollY, -1, elements, EXTRADATA(node).count);
	if (updated && EXTRADATA(node).onViewChange)
		UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);
}

static void UI_OptionListNodeDraw (uiNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	uiNode_t* option;
	const char *ref;
	const char *font;
	int fontHeight;
	vec2_t pos;
	const char* image;
	int currentY;
	const float *textColor;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};
	int count = 0;

	ref = UI_AbstractOptionGetCurrentValue(node);
	if (ref == NULL)
		return;

	UI_GetNodeAbsPos(node, pos);

	image = UI_GetReferenceString(node, node->image);
	if (image)
		UI_DrawPanel(pos, node->size, image, 0, 0, panelTemplate);

	font = UI_GetFontFromNode(node);
	fontHeight = UI_FontGetHeight(font);
	currentY = pos[1] + node->padding;

	/* skip option over current position */
	option = UI_AbstractOptionGetFirstOption(node);
	while (option && count < EXTRADATA(node).scrollY.viewPos) {
		option = option->next;
		count++;
	}

	/* draw all available options for this selectbox */
	for (; option; option = option->next) {
		const char *label;
		int decX = pos[0] + node->padding;
		/* outside the node */
		if (currentY + fontHeight > pos[1] + node->size[1] - node->padding) {
			count++;
			break;
		}

		/* draw the hover effect */
		if (OPTIONEXTRADATA(option).hovered)
			UI_DrawFill(pos[0] + node->padding, currentY, node->size[0] - node->padding - node->padding, fontHeight, node->color);

		/* text color */
		if (Q_streq(OPTIONEXTRADATA(option).value, ref)) {
			textColor = node->selectedColor;
		} else if (node->disabled || option->disabled) {
			textColor = disabledColor;
		} else if (option->color[3] == 0.0f) {
			textColor = node->color;
		} else {
			textColor = option->color;
		}

		if (OPTIONEXTRADATA(option).icon) {
			uiSpriteStatus_t iconStatus = SPRITE_STATUS_NORMAL;
			if (option->disabled)
				iconStatus = SPRITE_STATUS_DISABLED;
			R_Color(NULL);
			UI_DrawSpriteInBox(OPTIONEXTRADATA(option).icon, iconStatus, decX, currentY, OPTIONEXTRADATA(option).icon->size[0], fontHeight);
			decX += OPTIONEXTRADATA(option).icon->size[0] + fontHeight / 4;
		}

		/* print the option label */
		label = OPTIONEXTRADATA(option).label;
		if (label[0] == '_')
			label = _(label + 1);

		R_Color(textColor);
		UI_DrawString(font, ALIGN_UL, decX, currentY,
			pos[0], node->size[0] - node->padding - node->padding,
			0, label, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);

		/* next entries' position */
		currentY += fontHeight;
		count++;
	}
	R_Color(NULL);

	/* count number of options (current architecture doesn't allow to know if the data change) */
	for (; option; option = option->next) {
		count++;
	}

	if (EXTRADATA(node).count != count) {
		EXTRADATA(node).count = count;
	}

	UI_OptionListNodeUpdateScroll(node);
}

static uiNode_t* UI_OptionListNodeGetOptionAtPosition (uiNode_t * node, int x, int y)
{
	uiNode_t* option;
	vec2_t pos;
	int fontHeight;
	int currentY;
	int count = 0;
	const char *font;

	UI_GetNodeAbsPos(node, pos);
	currentY = pos[1] + node->padding;

	font = UI_GetFontFromNode(node);
	fontHeight = UI_FontGetHeight(font);

	option = UI_AbstractOptionGetFirstOption(node);
	while (option && count < EXTRADATA(node).scrollY.viewPos) {
		option = option->next;
		count++;
	}

	/* now draw all available options for this selectbox */
	for (; option; option = option->next) {
		if (y < currentY + fontHeight)
			return option;
		if (currentY + fontHeight > pos[1] + node->size[1] - node->padding)
			break;
		currentY += fontHeight;
	}
	return NULL;
}

/**
 * @brief Handles selectboxes clicks
 */
static void UI_OptionListNodeClick (uiNode_t * node, int x, int y)
{
	uiNode_t* option;

	if (UI_AbstractOptionGetCurrentValue(node) == NULL)
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
static void UI_OptionListNodeMouseWheel (uiNode_t *node, qboolean down, int x, int y)
{
	qboolean updated;
	updated = UI_SetScroll(&EXTRADATA(node).scrollY, EXTRADATA(node).scrollY.viewPos + (down ? 1 : -1), -1, -1);
	if (EXTRADATA(node).onViewChange && updated)
		UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);

	if (node->onWheelUp && !down)
		UI_ExecuteEventActions(node, node->onWheelUp);
	if (node->onWheelDown && down)
		UI_ExecuteEventActions(node, node->onWheelDown);
	if (node->onWheel)
		UI_ExecuteEventActions(node, node->onWheel);
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void UI_OptionListNodeLoading (uiNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
	EXTRADATA(node).versionId = -1;
	node->padding = 3;
}

static void UI_OptionListNodeLoaded (uiNode_t *node)
{
}

void UI_RegisterOptionListNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "optionlist";
	behaviour->extends = "abstractoption";
	behaviour->draw = UI_OptionListNodeDraw;
	behaviour->leftClick = UI_OptionListNodeClick;
	behaviour->mouseWheel = UI_OptionListNodeMouseWheel;
	behaviour->loading = UI_OptionListNodeLoading;
	behaviour->loaded = UI_OptionListNodeLoaded;
	behaviour->drawItselfChild = qtrue;
}
