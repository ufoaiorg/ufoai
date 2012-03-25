/**
 * @file ui_node_optiontree.c
 * @todo manage disabled option
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
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_actions.h"
#include "../ui_font.h"
#include "../ui_data.h"
#include "../ui_sprite.h"
#include "../ui_render.h"
#include "../ui_input.h"
#include "ui_node_abstractoption.h"
#include "ui_node_abstractnode.h"
#include "ui_node_optiontree.h"
#include "ui_node_option.h"
#include "ui_node_panel.h"

#include "../../client.h" /* gettext _() */

#define EXTRADATA(node) UI_EXTRADATA(node, abstractOptionExtraData_t)

#define CORNER_SIZE 25
#define MID_SIZE 1
#define MARGE 3

static const int COLLAPSEBUTTON_WIDTH = 20;		/**< Size used for the collapse button */
static const int DEPTH_WIDTH = 25;				/**< Width between each depth level */

static uiSprite_t *systemCollapse;
static uiSprite_t *systemExpand;

/* Used for drag&drop-like scrolling */
static int mouseScrollX;
static int mouseScrollY;

/**
 * @brief Update the scroll according to the number
 * of items and the size of the node
 */
static void UI_OptionTreeNodeUpdateScroll (uiNode_t *node)
{
	const char *font;
	int fontHeight;
	qboolean updated;
	int elements;

	font = UI_GetFontFromNode(node);
	fontHeight = EXTRADATA(node).lineHeight;
	if (fontHeight == 0)
		fontHeight = UI_FontGetHeight(font);

	elements = (node->size[1] - node->padding - node->padding) / fontHeight;
	updated = UI_SetScroll(&EXTRADATA(node).scrollY, -1, elements, EXTRADATA(node).count);
	if (updated && EXTRADATA(node).onViewChange)
		UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);
}

/** @todo we should remove this call loop */
static uiNode_t* UI_OptionTreeNodeGetFirstOption(uiNode_t * node);

static void UI_OptionTreeNodeUpdateCache (uiNode_t * node)
{
	uiNode_t* option = UI_OptionTreeNodeGetFirstOption(node);
	if (option)
		EXTRADATA(node).count = UI_OptionUpdateCache(option);
}

/**
 * @brief Return the first option of the node
 * @todo check versionId and update cached data, and fire events
 */
static uiNode_t* UI_OptionTreeNodeGetFirstOption (uiNode_t * node)
{
	if (node->firstChild) {
		/** @todo FIXME it MUST be an option behaviour */
		assert(node->firstChild->behaviour == ui_optionBehaviour);
		return node->firstChild;
	} else {
		const int v = UI_GetDataVersion(EXTRADATA(node).dataId);
		uiNode_t *option = UI_GetOption(EXTRADATA(node).dataId);
		if (v != EXTRADATA(node).versionId) {
			EXTRADATA(node).versionId = v;
			UI_OptionTreeNodeUpdateCache(node);
		}
		return option;
	}
}

static void UI_OptionTreeNodeDraw (uiNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	uiNode_t* option;
	const char *ref;
	const char *font;
	vec2_t pos;
	const char* image;
	int fontHeight;
	int currentY;
	int currentDecY = 0;
	const float *textColor;
	vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};
	int count = 0;
	uiOptionIterator_t iterator;

	if (!systemExpand)
		systemExpand = UI_GetSpriteByName("icons/system_expand");
	if (!systemCollapse)
		systemCollapse = UI_GetSpriteByName("icons/system_collapse");

	ref = UI_AbstractOptionGetCurrentValue(node);
	if (ref == NULL)
		return;

	UI_GetNodeAbsPos(node, pos);

	image = UI_GetReferenceString(node, node->image);
	if (image)
		UI_DrawPanel(pos, node->size, image, 0, 0, panelTemplate);

	font = UI_GetFontFromNode(node);
	fontHeight = EXTRADATA(node).lineHeight;
	currentY = pos[1] + node->padding;
	if (fontHeight == 0)
		fontHeight = UI_FontGetHeight(font);
	else {
		const int height = UI_FontGetHeight(font);
		currentDecY = (fontHeight - height) / 2;
	}

	/* skip option over current position */
	option = UI_OptionTreeNodeGetFirstOption(node);
	UI_OptionTreeNodeUpdateScroll(node);
	option = UI_InitOptionIteratorAtIndex(EXTRADATA(node).scrollY.viewPos, option, &iterator);

	/* draw all available options for this selectbox */
	for (; option; option = UI_OptionIteratorNextOption(&iterator)) {
		int decX;
		const char *label;

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

		/* print the option label */
		decX = pos[0] + node->padding + iterator.depthPos * DEPTH_WIDTH;

		R_Color(NULL);
		if (option->firstChild) {
			uiSprite_t *icon = OPTIONEXTRADATA(option).collapsed ? systemExpand : systemCollapse;
			UI_DrawSpriteInBox(OPTIONEXTRADATA(option).flipIcon, icon, SPRITE_STATUS_NORMAL, decX, currentY, icon->size[0], fontHeight);
		}

		decX += COLLAPSEBUTTON_WIDTH;

		if (OPTIONEXTRADATA(option).icon) {
			uiSpriteStatus_t iconStatus = SPRITE_STATUS_NORMAL;
			if (option->disabled)
				iconStatus = SPRITE_STATUS_DISABLED;
			UI_DrawSpriteInBox(OPTIONEXTRADATA(option).flipIcon, OPTIONEXTRADATA(option).icon, iconStatus, decX, currentY,
					OPTIONEXTRADATA(option).icon->size[0], fontHeight);
			decX += OPTIONEXTRADATA(option).icon->size[0] + fontHeight / 4;
		}

		label = OPTIONEXTRADATA(option).label;
		if (label[0] == '_')
			label = _(label + 1);

		R_Color(textColor);
		UI_DrawString(font, ALIGN_UL, decX, currentY + currentDecY,
			pos[0], node->size[0] - node->padding - node->padding,
			0, label, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);

		/* next entries' position */
		currentY += fontHeight;
		count++;
	}
	R_Color(NULL);
}

static uiNode_t* UI_OptionTreeNodeGetOptionAtPosition (uiNode_t * node, int x, int y, int *depth)
{
	uiNode_t* option;
	const char *font;
	int fontHeight;
	int count;
	uiOptionIterator_t iterator;

	UI_NodeAbsoluteToRelativePos(node, &x, &y);

	font = UI_GetFontFromNode(node);
	fontHeight = EXTRADATA(node).lineHeight;
	if (fontHeight == 0)
		fontHeight = UI_FontGetHeight(font);

	option = UI_OptionTreeNodeGetFirstOption(node);
	count = EXTRADATA(node).scrollY.viewPos + (y - node->padding) / fontHeight;
	option = UI_InitOptionIteratorAtIndex(count, option, &iterator);
	*depth = iterator.depthPos;
	return option;
}

/**
 * @brief Handles selectboxes clicks
 */
static void UI_OptionTreeNodeClick (uiNode_t * node, int x, int y)
{
	uiNode_t* option;
	int depth;

	if (UI_AbstractOptionGetCurrentValue(node) == NULL)
		return;

	/* select the right option */
	option = UI_OptionTreeNodeGetOptionAtPosition(node, x, y, &depth);

	UI_NodeAbsoluteToRelativePos(node, &x, &y);

	/* extend/collapse*/
	x -= depth * DEPTH_WIDTH;
	if (x >= 0 && x < COLLAPSEBUTTON_WIDTH) {
		if (option && option->firstChild) {
			OPTIONEXTRADATA(option).collapsed = !OPTIONEXTRADATA(option).collapsed;
			UI_OptionTreeNodeUpdateCache(node);
		}
		return;
	}

	/* update the status */
	if (option)
		UI_AbstractOptionSetCurrentValue(node, OPTIONEXTRADATA(option).value);
}

/**
 * @brief Auto scroll the list
 */
static qboolean UI_OptionTreeNodeMouseWheel (uiNode_t *node, int deltaX, int deltaY)
{
	qboolean down = deltaY > 0;
	qboolean updated;
	if (deltaY == 0)
		return qfalse;
	updated = UI_SetScroll(&EXTRADATA(node).scrollY, EXTRADATA(node).scrollY.viewPos + (down ? 1 : -1), -1, -1);
	if (EXTRADATA(node).onViewChange && updated)
		UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);

	/* @todo use super behaviour */
	if (node->onWheelUp && !down) {
		UI_ExecuteEventActions(node, node->onWheelUp);
		updated = qtrue;
	}
	if (node->onWheelDown && down) {
		UI_ExecuteEventActions(node, node->onWheelDown);
		updated = qtrue;
	}
	if (node->onWheel) {
		UI_ExecuteEventActions(node, node->onWheel);
		updated = qtrue;
	}
	return updated;
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void UI_OptionTreeNodeLoading (uiNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
	EXTRADATA(node).versionId = -1;
	node->padding = 3;
}

static void UI_OptionTreeNodeLoaded (uiNode_t *node)
{
}

static void UI_OptionTreeSetSelectedValue (uiNode_t *node, const uiCallContext_t *context)
{
	uiOptionIterator_t iterator;
	uiNode_t *option;
	uiNode_t *firstOption;
	const char* value;
	int pos, i;

	if (UI_GetParamNumber(context) != 1) {
		Com_Printf("UI_OptionTreeSetSelectedValue: Invalide number of param\n");
		return;
	}

	value = UI_GetParam(context, 1);

	/* is the option exists */
	firstOption = UI_OptionTreeNodeGetFirstOption(node);
	UI_InitOptionIteratorAtIndex(0, firstOption, &iterator);
	/** @todo merge that into the Init iterator function */
	iterator.skipCollapsed = qfalse;
	option = UI_FindOptionByValue(&iterator, value);

	/* update the selection */
	if (option) {
		UI_AbstractOptionSetCurrentValue(node, OPTIONEXTRADATA(option).value);
	} else {
		Com_Printf("UI_OptionTreeSetSelectedValue: Option value \"%s\" not found\n", value);
		return;
	}

	/* expend parents */
	for (i = 0; i < iterator.depthPos; i++)
		OPTIONEXTRADATA(iterator.depthCache[i]).collapsed = qfalse;
	UI_OptionTreeNodeUpdateCache(node);
	UI_OptionTreeNodeUpdateScroll(node);

	/* fix scroll bar */
	firstOption = UI_OptionTreeNodeGetFirstOption(node);
	UI_InitOptionIteratorAtIndex(0, firstOption, &iterator);
	pos = UI_FindOptionPosition(&iterator, option);
	if (pos == -1)
		return;
	if (pos < EXTRADATA(node).scrollY.viewPos || pos >= EXTRADATA(node).scrollY.viewPos + EXTRADATA(node).scrollY.viewSize) {
		qboolean updated;
		updated = UI_SetScroll(&EXTRADATA(node).scrollY, pos, -1, -1);
		if (updated && EXTRADATA(node).onViewChange)
			UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);
	}
}

static void UI_OptionTreeNodeDoLayout (uiNode_t *node)
{
	UI_OptionTreeNodeUpdateCache(node);
	node->invalidated = qfalse;
}

/**
 * @brief Track mouse down/up events to implement drag&drop-like scrolling, for touchscreen devices
 * @sa UI_OptionTreeNodeMouseUp, UI_OptionTreeNodeCapturedMouseMove
*/
static void UI_OptionTreeNodeMouseDown (struct uiNode_s *node, int x, int y, int button)
{
	if (!UI_GetMouseCapture() && button == K_MOUSE1 &&
		EXTRADATA(node).scrollY.fullSize > EXTRADATA(node).scrollY.viewSize) {
		UI_SetMouseCapture(node);
		mouseScrollX = x;
		mouseScrollY = y;
	}
}

static void UI_OptionTreeNodeMouseUp (struct uiNode_s *node, int x, int y, int button)
{
	if (UI_GetMouseCapture() == node)  /* More checks can never hurt */
		UI_MouseRelease();
}

static void UI_OptionTreeNodeCapturedMouseMove (uiNode_t *node, int x, int y)
{
	const int lineHeight = node->behaviour->getCellHeight(node);
	const int deltaY = (mouseScrollY - y) / lineHeight;
	/* We're doing only vertical scroll, that's enough for the most instances */
	if (deltaY != 0) {
		qboolean updated;
		updated = UI_SetScroll(&EXTRADATA(node).scrollY, EXTRADATA(node).scrollY.viewPos + deltaY, -1, -1);
		if (EXTRADATA(node).onViewChange && updated)
			UI_ExecuteEventActions(node, EXTRADATA(node).onViewChange);
		/* @todo not accurate */
		mouseScrollX = x;
		mouseScrollY = y;
	}
	if (node->behaviour->mouseMove)
		node->behaviour->mouseMove(node, x, y);
}

/**
 * @brief Return size of the cell, which is the size (in virtual "pixel") which represent 1 in the scroll values.
 * Here we guess the widget can scroll pixel per pixel.
 * @return Size in pixel.
 */
static int UI_OptionTreeNodeGetCellHeight (uiNode_t *node)
{
	int lineHeight = EXTRADATA(node).lineHeight;
	if (lineHeight == 0)
		lineHeight = UI_FontGetHeight(UI_GetFontFromNode(node));
	return lineHeight;
}

void UI_RegisterOptionTreeNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "optiontree";
	behaviour->extends = "abstractoption";
	behaviour->draw = UI_OptionTreeNodeDraw;
	behaviour->leftClick = UI_OptionTreeNodeClick;
	behaviour->scroll = UI_OptionTreeNodeMouseWheel;
	behaviour->mouseDown = UI_OptionTreeNodeMouseDown;
	behaviour->mouseUp = UI_OptionTreeNodeMouseUp;
	behaviour->capturedMouseMove = UI_OptionTreeNodeCapturedMouseMove;
	behaviour->loading = UI_OptionTreeNodeLoading;
	behaviour->loaded = UI_OptionTreeNodeLoaded;
	behaviour->doLayout = UI_OptionTreeNodeDoLayout;
	behaviour->getCellHeight = UI_OptionTreeNodeGetCellHeight;
	behaviour->drawItselfChild = qtrue;

	/* Call it to toggle the node status. */
	UI_RegisterNodeMethod(behaviour, "setselectedvalue", UI_OptionTreeSetSelectedValue);
}
