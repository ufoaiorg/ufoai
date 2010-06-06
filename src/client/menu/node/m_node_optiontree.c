/**
 * @file m_node_optiontree.c
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

#include "../m_main.h"
#include "../m_parse.h"
#include "../m_actions.h"
#include "../m_font.h"
#include "../m_data.h"
#include "../m_icon.h"
#include "../m_render.h"
#include "m_node_abstractoption.h"
#include "m_node_abstractnode.h"
#include "m_node_optiontree.h"
#include "m_node_option.h"
#include "m_node_panel.h"

#include "../../client.h" /* gettext _() */

#define EXTRADATA(node) MN_EXTRADATA(node, abstractOptionExtraData_t)

#define CORNER_SIZE 25
#define MID_SIZE 1
#define MARGE 3

static const int COLLAPSEBUTTON_WIDTH = 20;		/**< Size used for the collapse button */
static const int DEPTH_WIDTH = 25;				/**< Width between each depth level */

static menuIcon_t *systemCollapse;
static menuIcon_t *systemExpand;

/**
 * @brief Update the scroll according to the number
 * of items and the size of the node
 */
static void MN_OptionTreeNodeUpdateScroll (menuNode_t *node)
{
	const char *font;
	int fontHeight;
	qboolean updated;
	int elements;

	font = MN_GetFontFromNode(node);
	fontHeight = EXTRADATA(node).lineHeight;
	if (fontHeight == 0)
		fontHeight = MN_FontGetHeight(font);

	elements = (node->size[1] - node->padding - node->padding) / fontHeight;
	updated = MN_SetScroll(&EXTRADATA(node).scrollY, -1, elements, EXTRADATA(node).count);
	if (updated && EXTRADATA(node).onViewChange)
		MN_ExecuteEventActions(node, EXTRADATA(node).onViewChange);
}

/** @todo we should remove this call loop */
static menuNode_t* MN_OptionTreeNodeGetFirstOption(menuNode_t * node);

static void MN_OptionTreeNodeUpdateCache (menuNode_t * node)
{
	menuNode_t* option = MN_OptionTreeNodeGetFirstOption(node);
	if (option)
		EXTRADATA(node).count = MN_OptionUpdateCache(option);
}

/**
 * @brief Return the first option of the node
 * @todo check versionId and update cached data, and fire events
 */
static menuNode_t* MN_OptionTreeNodeGetFirstOption (menuNode_t * node)
{
	if (node->firstChild) {
		/** FIXME it MUST be an option behaviour */
		assert(node->firstChild->behaviour == optionBehaviour);
		return node->firstChild;
	} else {
		const int v = MN_GetDataVersion(EXTRADATA(node).dataId);
		menuNode_t *option = MN_GetOption(EXTRADATA(node).dataId);
		if (v != EXTRADATA(node).versionId) {
			EXTRADATA(node).versionId = v;
			MN_OptionTreeNodeUpdateCache(node);
		}
		return option;
	}
}

static void MN_OptionTreeNodeDraw (menuNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	menuNode_t* option;
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
	menuOptionIterator_t iterator;

	if (!systemExpand)
		systemExpand = MN_GetIconByName("system_expand");
	if (!systemCollapse)
		systemCollapse = MN_GetIconByName("system_collapse");

	ref = MN_AbstractOptionGetCurrentValue(node);
	if (ref == NULL)
		return;

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node, node->image);
	if (image)
		MN_DrawPanel(pos, node->size, image, 0, 0, panelTemplate);

	font = MN_GetFontFromNode(node);
	fontHeight = EXTRADATA(node).lineHeight;
	currentY = pos[1] + node->padding;
	if (fontHeight == 0)
		fontHeight = MN_FontGetHeight(font);
	else {
		const int height = MN_FontGetHeight(font);
		currentDecY = (fontHeight - height) / 2;
	}

	/* skip option over current position */
	option = MN_OptionTreeNodeGetFirstOption(node);
	MN_OptionTreeNodeUpdateScroll(node);
	option = MN_InitOptionIteratorAtIndex(EXTRADATA(node).scrollY.viewPos, option, &iterator);

	/* draw all available options for this selectbox */
	for (; option; option = MN_OptionIteratorNextOption(&iterator)) {
		int decX;
		const char *label;

		/* outside the node */
		if (currentY + fontHeight > pos[1] + node->size[1] - node->padding) {
			count++;
			break;
		}

		/* draw the hover effect */
		if (OPTIONEXTRADATA(option).hovered)
			MN_DrawFill(pos[0] + node->padding, currentY, node->size[0] - node->padding - node->padding, fontHeight, node->color);

		/* text color */
		if (!strcmp(OPTIONEXTRADATA(option).value, ref)) {
			textColor = node->selectedColor;
		} else if (node->disabled) {
			textColor = disabledColor;
		} else {
			textColor = node->color;
		}

		/* print the option label */
		decX = pos[0] + node->padding + iterator.depthPos * DEPTH_WIDTH;

		R_Color(NULL);
		if (option->firstChild) {
			menuIcon_t *icon = OPTIONEXTRADATA(option).collapsed ? systemExpand : systemCollapse;
			MN_DrawIconInBox(icon, ICON_STATUS_NORMAL, decX, currentY, icon->size[0], fontHeight);
		}

		decX += COLLAPSEBUTTON_WIDTH;

		if (OPTIONEXTRADATA(option).icon) {
			iconStatus_t iconStatus = ICON_STATUS_NORMAL;
			if (option->disabled)
				iconStatus = ICON_STATUS_DISABLED;
			MN_DrawIconInBox(OPTIONEXTRADATA(option).icon, iconStatus, decX, currentY, OPTIONEXTRADATA(option).icon->size[0], fontHeight);
			decX += OPTIONEXTRADATA(option).icon->size[0] + fontHeight / 4;
		}

		label = OPTIONEXTRADATA(option).label;
		if (label[0] == '_')
			label = _(label + 1);

		R_Color(textColor);
		MN_DrawString(font, ALIGN_UL, decX, currentY + currentDecY,
			pos[0], currentY + currentDecY, node->size[0] - node->padding - node->padding, node->size[1],
			0, label, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);

		/* next entries' position */
		currentY += fontHeight;
		count++;
	}
	R_Color(NULL);
}

static menuNode_t* MN_OptionTreeNodeGetOptionAtPosition (menuNode_t * node, int x, int y, int *depth)
{
	menuNode_t* option;
	const char *font;
	int fontHeight;
	int count;
	menuOptionIterator_t iterator;

	MN_NodeAbsoluteToRelativePos(node, &x, &y);

	font = MN_GetFontFromNode(node);
	fontHeight = EXTRADATA(node).lineHeight;
	if (fontHeight == 0)
		fontHeight = MN_FontGetHeight(font);

	option = MN_OptionTreeNodeGetFirstOption(node);
	count = EXTRADATA(node).scrollY.viewPos + (y - node->padding) / fontHeight;
	option = MN_InitOptionIteratorAtIndex(count, option, &iterator);
	*depth = iterator.depthPos;
	return option;
}

/**
 * @brief Handles selectboxes clicks
 */
static void MN_OptionTreeNodeClick (menuNode_t * node, int x, int y)
{
	menuNode_t* option;
	int depth;

	if (MN_AbstractOptionGetCurrentValue(node) == NULL)
		return;

	/* select the right option */
	option = MN_OptionTreeNodeGetOptionAtPosition(node, x, y, &depth);

	MN_NodeAbsoluteToRelativePos(node, &x, &y);

	/* extend/collapse*/
	x -= depth * DEPTH_WIDTH;
	if (x >= 0 && x < COLLAPSEBUTTON_WIDTH) {
		if (option && option->firstChild) {
			OPTIONEXTRADATA(option).collapsed = !OPTIONEXTRADATA(option).collapsed;
			MN_OptionTreeNodeUpdateCache(node);
		}
		return;
	}

	/* update the status */
	if (option)
		MN_AbstractOptionSetCurrentValue(node, OPTIONEXTRADATA(option).value);
}

/**
 * @brief Auto scroll the list
 */
static void MN_OptionTreeNodeMouseWheel (menuNode_t *node, qboolean down, int x, int y)
{
	qboolean updated;
	updated = MN_SetScroll(&EXTRADATA(node).scrollY, EXTRADATA(node).scrollY.viewPos + (down ? 1 : -1), -1, -1);
	if (EXTRADATA(node).onViewChange && updated)
		MN_ExecuteEventActions(node, EXTRADATA(node).onViewChange);

	if (node->onWheelUp && !down)
		MN_ExecuteEventActions(node, node->onWheelUp);
	if (node->onWheelDown && down)
		MN_ExecuteEventActions(node, node->onWheelDown);
	if (node->onWheel)
		MN_ExecuteEventActions(node, node->onWheel);
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void MN_OptionTreeNodeLoading (menuNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
	EXTRADATA(node).versionId = -1;
	node->padding = 3;
}

static void MN_OptionTreeNodeLoaded (menuNode_t *node)
{
}

static void MN_OptionTreeSetSelectedValue (menuNode_t *node, const menuCallContext_t *context)
{
	menuOptionIterator_t iterator;
	menuNode_t *option;
	menuNode_t *firstOption;
	const char* value;
	int pos, i;

	if (MN_GetParamNumber(context) != 1) {
		Com_Printf("MN_OptionTreeSetSelectedValue: Invalide number of param\n");
		return;
	}

	value = MN_GetParam(context, 1);

	/* is the option exists */
	firstOption = MN_OptionTreeNodeGetFirstOption(node);
	MN_InitOptionIteratorAtIndex(0, firstOption, &iterator);
	/** @todo merge that into the Init iterator function */
	iterator.skipCollapsed = qfalse;
	option = MN_FindOptionByValue(&iterator, value);

	/* update the selection */
	if (option) {
		MN_AbstractOptionSetCurrentValue(node, OPTIONEXTRADATA(option).value);
	} else {
		Com_Printf("MN_OptionTreeSetSelectedValue: Option value \"%s\" not found\n", value);
		return;
	}

	/* expend parents */
	for (i = 0; i < iterator.depthPos; i++)
		OPTIONEXTRADATA(iterator.depthCache[i]).collapsed = qfalse;
	MN_OptionTreeNodeUpdateCache(node);
	MN_OptionTreeNodeUpdateScroll(node);

	/* fix scroll bar */
	firstOption = MN_OptionTreeNodeGetFirstOption(node);
	MN_InitOptionIteratorAtIndex(0, firstOption, &iterator);
	pos = MN_FindOptionPosition(&iterator, option);
	if (pos == -1)
		return;
	if (pos < EXTRADATA(node).scrollY.viewPos || pos >= EXTRADATA(node).scrollY.viewPos + EXTRADATA(node).scrollY.viewSize) {
		qboolean updated;
		updated = MN_SetScroll(&EXTRADATA(node).scrollY, pos, -1, -1);
		if (updated && EXTRADATA(node).onViewChange)
			MN_ExecuteEventActions(node, EXTRADATA(node).onViewChange);
	}
}

static const value_t properties[] = {
	/* Call it to toggle the node status. */
	{"setselectedvalue", V_UI_NODEMETHOD, ((size_t) MN_OptionTreeSetSelectedValue), 0},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterOptionTreeNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "optiontree";
	behaviour->extends = "abstractoption";
	behaviour->draw = MN_OptionTreeNodeDraw;
	behaviour->leftClick = MN_OptionTreeNodeClick;
	behaviour->mouseWheel = MN_OptionTreeNodeMouseWheel;
	behaviour->loading = MN_OptionTreeNodeLoading;
	behaviour->loaded = MN_OptionTreeNodeLoaded;
	behaviour->properties = properties;
	behaviour->drawItselfChild = qtrue;
}
