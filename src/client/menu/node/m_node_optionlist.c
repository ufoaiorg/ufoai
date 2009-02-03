/**
 * @file m_node_optionlist.c
 * @todo change to mouse behaviour. It better to click to dropdown the list
 * @todo manage disabled option
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../m_main.h"
#include "../m_internal.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_input.h"
#include "m_node_selectbox.h"
#include "m_node_abstractnode.h"
#include "m_node_optionlist.h"

#define CORNER_SIZE 17
#define MID_SIZE 1
#define MARGE 3

#define ELEMENT_HEIGHT 20	/**< todo remove it, whould not need */

static void MN_OptionListNodeDraw (menuNode_t *node)
{
	static const int panelTemplate[] = {
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		CORNER_SIZE, MID_SIZE, CORNER_SIZE,
		MARGE
	};
	selectBoxOptions_t* option;
	const char *ref;
	const char *font;
	vec2_t pos;
	const char* image;
	int elementHeight = ELEMENT_HEIGHT;	/**< fix it with text font */
	int currentY;
	const float *textColor;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};
	int count = 0;

	if (!node->cvar)
		return;

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node->menu, node->image);
	if (image) {
		R_DrawPanel(pos, node->size, image, node->blend, 0, 0, panelTemplate);
	}

	ref = MN_GetReferenceString(node->menu, node->cvar);
	font = MN_GetFont(node->menu, node);
	currentY = pos[1] + node->padding;

	/* skip option over current position */
	option = node->u.option.first;
	while (option && count < node->u.option.pos) {
		option = option->next;
		count++;
	}

	/* draw all available options for this selectbox */
	for (; option; option = option->next) {
		/* outside the node */
		if (currentY + elementHeight > pos[1] + node->size[1] - node->padding) {
			count++;
			break;
		}

		/* draw the hover effect */
		if (option->hovered)
			R_DrawFill(pos[0] + node->padding, currentY, node->size[0] - node->padding - node->padding, elementHeight, ALIGN_UL, node->color);

		/* text color */
		if (!Q_strcmp(option->value, ref)) {
			textColor = node->selectedColor;
		} else if (node->disabled) {
			textColor = disabledColor;
		} else {
			textColor = node->color;
		}

		/* print the option label */
		R_ColorBlend(textColor);
		R_FontDrawString(font, ALIGN_UL, pos[0] + node->padding, currentY,
			pos[0], currentY, node->size[0] - node->padding - node->padding, node->size[1],
			0, _(option->label), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);

		/* next entries' position */
		currentY += elementHeight;
		count++;
	}
	R_ColorBlend(NULL);

	/* count number of options (current architecture dont allow to know if the data change) */
	for (; option; option = option->next) {
		count++;
	}

	if (node->u.option.count != count) {
		node->u.option.count = count;
		if (node->u.option.onViewChange) {
			MN_ExecuteEventActions(node, node->u.option.onViewChange);
		}
	}
}

static selectBoxOptions_t* MN_OptionListNodeGetOptionAtPosition (menuNode_t * node, int x, int y)
{
	selectBoxOptions_t* option;
	vec2_t pos;
	int elementHeight = ELEMENT_HEIGHT;	/**< fix it with text font */
	int currentY;
	int count = 0;

	MN_GetNodeAbsPos(node, pos);
	currentY = pos[1] + node->padding;

	option = node->u.option.first;
	while (option && count < node->u.option.pos) {
		option = option->next;
		count++;
	}

	/* now draw all available options for this selectbox */
	for (; option; option = option->next) {
		if (y < currentY + elementHeight)
			return option;
		if (currentY + elementHeight > pos[1] + node->size[1] - node->padding)
			break;
		currentY += elementHeight;
	}
	return NULL;
}

/**
 * @brief Handles selectboxes clicks
 */
static void MN_OptionListNodeClick (menuNode_t * node, int x, int y)
{
	selectBoxOptions_t* option;

	/* the cvar string is stored in dataModelSkinOrCVar */
	/* no cvar given? */
	if (!node->cvar || !*(char*)node->cvar) {
		Com_Printf("MN_OptionListNodeClick: node '%s' doesn't have a valid cvar assigned (menu %s)\n", node->name, node->menu->name);
		return;
	}

	/* no cvar? */
	if (Q_strncmp((const char *)node->cvar, "*cvar", 5))
		return;

	/* select the right option */
	option = MN_OptionListNodeGetOptionAtPosition(node, x, y);

	/* update the status */
	if (option) {
		const char *cvarName = &((const char *)node->cvar)[6];
		MN_SetCvar(cvarName, option->value, 0);
		if (option->action[0] != '\0') {
#ifdef DEBUG
			if (option->action[strlen(option->action) - 1] != ';')
				Com_Printf("MN_OptionListNodeClick: Option with none terminated action command (%s.%s)\n", node->menu->name, node->name);
#endif
			Cbuf_AddText(option->action);
		}
	}
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void MN_OptionListNodeLoading (menuNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
}

static void MN_OptionListNodeLoaded (menuNode_t *node)
{
}

void MN_RegisterOptionListNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "optionlist";
	behaviour->extends = "abstractoption";
	behaviour->draw = MN_OptionListNodeDraw;
	behaviour->leftClick = MN_OptionListNodeClick;
	behaviour->loading = MN_OptionListNodeLoading;
	behaviour->loaded = MN_OptionListNodeLoaded;
}
