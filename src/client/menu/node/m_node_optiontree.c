/**
 * @file m_node_optiontree.c
 * @todo manage disabled option
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "m_node_panel.h"

#include "../../client.h" /* gettext _() */

#define EXTRADATA(node) (node->u.option)

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
static menuOption_t* MN_OptionTreeNodeGetFirstOption(menuNode_t * node);

static void MN_OptionTreeNodeUpdateCache (menuNode_t * node)
{
	menuOption_t* option = MN_OptionTreeNodeGetFirstOption(node);
	if (option)
		EXTRADATA(node).count = MN_OptionUpdateCache(option);
}

/**
 * @brief Return the first option of the node
 * @todo check versionId and update cached data, and fire events
 */
static menuOption_t* MN_OptionTreeNodeGetFirstOption (menuNode_t * node)
{
	if (EXTRADATA(node).first) {
		return EXTRADATA(node).first;
	} else {
		const int v = MN_GetDataVersion(EXTRADATA(node).dataId);
		menuOption_t *option = MN_GetOption(EXTRADATA(node).dataId);
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
	menuOption_t* option;
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

	if (!node->cvar)
		return;

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node, node->image);
	if (image)
		MN_DrawPanel(pos, node->size, image, 0, 0, panelTemplate);

	ref = MN_GetReferenceString(node, node->cvar);
	font = MN_GetFontFromNode(node);
	fontHeight = EXTRADATA(node).lineHeight;
	currentY = pos[1] + node->padding;
	if (fontHeight == 0)
		fontHeight = MN_FontGetHeight(font);
	else {
		const int height = MN_FontGetHeight(font);
		currentDecY = (fontHeight - height) / 2;
		currentY += currentDecY;
	}

	/* skip option over current position */
	option = MN_OptionTreeNodeGetFirstOption(node);
	MN_OptionTreeNodeUpdateScroll(node);
	option = MN_InitOptionIteratorAtIndex(EXTRADATA(node).scrollY.viewPos, option, &iterator);

	/* draw all available options for this selectbox */
	for (; option; option = MN_OptionIteratorNextOption(&iterator)) {
		int decX;

		/* outside the node */
		if (currentY + fontHeight > pos[1] + node->size[1] - node->padding) {
			count++;
			break;
		}

		/* draw the hover effect */
		if (option->hovered)
			MN_DrawFill(pos[0] + node->padding, currentY, node->size[0] - node->padding - node->padding, fontHeight, node->color);

		/* text color */
		if (!strcmp(option->value, ref)) {
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
			menuIcon_t *icon = option->collapsed ? systemExpand : systemCollapse;
			MN_DrawIconInBox(icon, 0, decX, currentY - currentDecY, icon->size[0], fontHeight);
		}

		decX += COLLAPSEBUTTON_WIDTH;

		if (option->icon) {
			int iconStatus = 0;
			if (option->disabled)
				iconStatus = 2;
			MN_DrawIconInBox(option->icon, iconStatus, decX, currentY, option->icon->size[0], fontHeight);
			decX += option->icon->size[0] + fontHeight / 4;
		}

		R_Color(textColor);
		MN_DrawString(font, ALIGN_UL, decX, currentY,
			pos[0], currentY, node->size[0] - node->padding - node->padding, node->size[1],
			0, _(option->label), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);

		/* next entries' position */
		currentY += fontHeight;
		count++;
	}
	R_Color(NULL);
}

static menuOption_t* MN_OptionTreeNodeGetOptionAtPosition (menuNode_t * node, int x, int y, int *depth)
{
	menuOption_t* option;
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
	menuOption_t* option;
	int depth;

	/* the cvar string is stored in dataModelSkinOrCVar */
	/* no cvar given? */
	if (!node->cvar || !*(char*)node->cvar) {
		Com_Printf("MN_OptionTreeNodeClick: node '%s' doesn't have a valid cvar assigned\n", MN_GetPath(node));
		return;
	}

	/* no cvar? */
	if (strncmp((const char *)node->cvar, "*cvar:", 6))
		return;

	/* select the right option */
	option = MN_OptionTreeNodeGetOptionAtPosition(node, x, y, &depth);

	MN_NodeAbsoluteToRelativePos(node, &x, &y);

	/* extend/collapse*/
	x -= depth * DEPTH_WIDTH;
	if (x >= 0 && x < COLLAPSEBUTTON_WIDTH) {
		if (option && option->firstChild) {
			option->collapsed = !option->collapsed;
			MN_OptionTreeNodeUpdateCache(node);
		}
		return;
	}

	/* update the status */
	if (option) {
		const char *cvarName = &((const char *)node->cvar)[6];
		MN_SetCvar(cvarName, option->value, 0);
		if (node->onChange)
			MN_ExecuteEventActions(node, node->onChange);
	}
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

void MN_RegisterOptionTreeNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "optiontree";
	behaviour->extends = "abstractoption";
	behaviour->draw = MN_OptionTreeNodeDraw;
	behaviour->leftClick = MN_OptionTreeNodeClick;
	behaviour->mouseWheel = MN_OptionTreeNodeMouseWheel;
	behaviour->loading = MN_OptionTreeNodeLoading;
	behaviour->loaded = MN_OptionTreeNodeLoaded;
}
