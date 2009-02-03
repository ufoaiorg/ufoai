/**
 * @file m_node_selectbox.c
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
#include "../m_draw.h"
#include "m_node_selectbox.h"
#include "m_node_abstractnode.h"

#define SELECTBOX_SIDE_WIDTH 7.0f
#define SELECTBOX_RIGHT_WIDTH 20.0f

#define SELECTBOX_SPACER 2.0f
#define SELECTBOX_BOTTOM_HEIGHT 4.0f

/**
 * @brief Sort options by alphabet
 */
void MN_OptionNodeSortOptions (menuNode_t *node)
{
	selectBoxOptions_t *option;
	selectBoxOptions_t *sorted = NULL;
	assert(MN_NodeInstanceOf(node, "abstractoption"));

	option = node->u.option.first;
	if (option == NULL)
		return;

	/* invariant: list [...sorted] is sorted; list [option...] need to be sort */

	while (option->next) {
		selectBoxOptions_t *find = NULL;
		selectBoxOptions_t *prev = option;
		selectBoxOptions_t *search = option->next;
		char *label = option->label;

		/* search the smaller element */
		while (search) {
			if (0 < Q_strcmp(label, search->label)) {
				find = prev;
				label = search->label;
			}
			prev = search;
			search = search->next;
		}

		/* link the element the the result list*/
		if (find == NULL) {
			/* find element is already at the right position */
			find = option;
		} else {
			selectBoxOptions_t *tmp = find->next;
			find->next = tmp->next;
			find = tmp;
			find->next = option;
			if (sorted == NULL) {
				node->u.option.first = find;
			} else {
				sorted->next = find;
			}

		}

		/* next */
		sorted = find;
		option = find->next;
	}
}

/**
 * @brief Adds a new selectbox option to a selectbox node
 * @return NULL if menuSelectBoxes is 'full' - otherwise pointer to the selectBoxOption
 * @param[in] node The abstractoption where you want to append the option
 * @note You have to add the values manually to the option pointer
 */
selectBoxOptions_t* MN_NodeAddOption (menuNode_t *node)
{
	selectBoxOptions_t *selectBoxOption;

	assert(MN_NodeInstanceOf(node, "abstractoption"));

	if (mn.numSelectBoxes >= MAX_SELECT_BOX_OPTIONS) {
		Com_Printf("MN_AddSelectboxOption: numSelectBoxes exceeded - increase MAX_SELECT_BOX_OPTIONS\n");
		return NULL;
	}
	/* initial options entry */
	if (!node->u.option.first)
		node->u.option.first = &mn.menuSelectBoxes[mn.numSelectBoxes];
	else {
		/* link it in */
		for (selectBoxOption = node->u.option.first; selectBoxOption->next; selectBoxOption = selectBoxOption->next) {}
		selectBoxOption->next = &mn.menuSelectBoxes[mn.numSelectBoxes];
		selectBoxOption->next->next = NULL;
	}
	selectBoxOption = &mn.menuSelectBoxes[mn.numSelectBoxes];
	node->u.option.count++;
	mn.numSelectBoxes++;
	return selectBoxOption;
}

/**
 * @brief call when the mouse move over the node
 */
static void MN_SelectBoxNodeMouseMove (menuNode_t *node, int x, int y)
{
	MN_SetMouseCapture(node);
}

/**
 * @brief call when the mouse move is the node is captured
 * @todo we can remove the loop if we save the current element in the node
 */
static void MN_SelectBoxNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	selectBoxOptions_t* selectBoxOption;
	int posy;

	MN_NodeAbsoluteToRelativePos(node, &x, &y);

	/* test bounded box */
	if (x < 0 || y < 0 || x > node->size[0] || y > node->size[1] * (node->u.option.count + 1)) {
		MN_MouseRelease();
		return;
	}

	posy = node->size[1];
	for (selectBoxOption = node->u.option.first; selectBoxOption; selectBoxOption = selectBoxOption->next) {
		selectBoxOption->hovered = (posy <= y && y < posy + node->size[1]) ? qtrue : qfalse;
		posy += node->size[1];
	}
}

static void MN_SelectBoxNodeDraw (menuNode_t *node)
{
	selectBoxOptions_t* selectBoxOption;
	int selBoxX, selBoxY;
	const char *ref;
	const char *font;
	vec2_t nodepos;
	const char* image;

	if (!node->cvar)
		return;

	MN_GetNodeAbsPos(node, nodepos);
	image = MN_GetReferenceString(node->menu, node->image);
	if (!image)
		image = "menu/selectbox";

	ref = MN_GetReferenceString(node->menu, node->cvar);
	font = MN_GetFont(node->menu, node);
	selBoxX = nodepos[0] + SELECTBOX_SIDE_WIDTH;
	selBoxY = nodepos[1] + SELECTBOX_SPACER;

	/* left border */
	R_DrawNormPic(nodepos[0], nodepos[1], SELECTBOX_SIDE_WIDTH, node->size[1],
		SELECTBOX_SIDE_WIDTH, SELECTBOX_DEFAULT_HEIGHT, 0.0f, 0.0f, ALIGN_UL, node->blend, image);
	/* stretched middle bar */
	R_DrawNormPic(nodepos[0] + SELECTBOX_SIDE_WIDTH, nodepos[1], node->size[0]-SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, node->size[1],
		12.0f, SELECTBOX_DEFAULT_HEIGHT, 7.0f, 0.0f, ALIGN_UL, node->blend, image);
	/* right border (arrow) */
	R_DrawNormPic(nodepos[0] + node->size[0] - SELECTBOX_RIGHT_WIDTH, nodepos[1], SELECTBOX_DEFAULT_HEIGHT, node->size[1],
		12.0f + SELECTBOX_RIGHT_WIDTH, SELECTBOX_DEFAULT_HEIGHT, 12.0f, 0.0f, ALIGN_UL, node->blend, image);
	/* draw the label for the current selected option */
	for (selectBoxOption = node->u.option.first; selectBoxOption; selectBoxOption = selectBoxOption->next) {
		if (!Q_strcmp(selectBoxOption->value, ref)) {
			R_FontDrawString(font, ALIGN_UL, selBoxX, selBoxY,
				selBoxX, selBoxY, node->size[0] - 4, 0,
				0, _(selectBoxOption->label), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		}
	}

	/* must we draw the drop-down list */
	if (MN_GetMouseCapture() == node) {
		MN_CaptureDrawOver(node);
	}
}

static void MN_SelectBoxNodeDrawOverMenu (menuNode_t *node)
{
	selectBoxOptions_t* selectBoxOption;
	int selBoxX, selBoxY;
	const char *ref;
	const char *font;
	vec2_t nodepos;
	const char* image;

	if (!node->cvar)
		return;

	MN_GetNodeAbsPos(node, nodepos);
	image = MN_GetReferenceString(node->menu, node->image);
	if (!image)
		image = "menu/selectbox";

	ref = MN_GetReferenceString(node->menu, node->cvar);
	font = MN_GetFont(node->menu, node);
	selBoxX = nodepos[0] + SELECTBOX_SIDE_WIDTH;
	selBoxY = nodepos[1] + SELECTBOX_SPACER;

	selBoxY += node->size[1];

	/* drop down menu */
	/* left side */
	R_DrawNormPic(nodepos[0], nodepos[1] + node->size[1], SELECTBOX_SIDE_WIDTH, node->size[1] * node->u.option.count,
		7.0f, 28.0f, 0.0f, 21.0f, ALIGN_UL, node->blend, image);

	/* stretched middle bar */
	R_DrawNormPic(nodepos[0] + SELECTBOX_SIDE_WIDTH, nodepos[1] + node->size[1], node->size[0] -SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, node->size[1] * node->u.option.count,
		16.0f, 28.0f, 7.0f, 21.0f, ALIGN_UL, node->blend, image);

	/* right side */
	R_DrawNormPic(nodepos[0] + node->size[0] -SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, nodepos[1] + node->size[1], SELECTBOX_SIDE_WIDTH, node->size[1] * node->u.option.count,
		23.0f, 28.0f, 16.0f, 21.0f, ALIGN_UL, node->blend, image);

	/* now draw all available options for this selectbox */
	for (selectBoxOption = node->u.option.first; selectBoxOption; selectBoxOption = selectBoxOption->next) {
		/* draw the hover effect */
		if (selectBoxOption->hovered)
			R_DrawFill(selBoxX, selBoxY, node->size[0] -SELECTBOX_SIDE_WIDTH-SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, SELECTBOX_DEFAULT_HEIGHT, ALIGN_UL, node->color);
		/* print the option label */
		R_FontDrawString(font, ALIGN_UL, selBoxX, selBoxY,
			selBoxX, nodepos[1] + node->size[1], node->size[0] - 4, 0,
			0, _(selectBoxOption->label), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		/* next entries' position */
		selBoxY += node->size[1];
	}
	/* left side */
	R_DrawNormPic(nodepos[0], selBoxY - SELECTBOX_SPACER, SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
		7.0f, 32.0f, 0.0f, 28.0f, ALIGN_UL, node->blend, image);

	/* stretched middle bar */
	R_DrawNormPic(nodepos[0] + SELECTBOX_SIDE_WIDTH, selBoxY - SELECTBOX_SPACER, node->size[0]-SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
		16.0f, 32.0f, 7.0f, 28.0f, ALIGN_UL, node->blend, image);

	/* right bottom side */
	R_DrawNormPic(nodepos[0] + node->size[0] -SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, selBoxY - SELECTBOX_SPACER,
		SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
		23.0f, 32.0f, 16.0f, 28.0f, ALIGN_UL, node->blend, image);
}

/**
 * @brief Handles selectboxes clicks
 */
static void MN_SelectBoxNodeClick (menuNode_t *node, int x, int y)
{
	selectBoxOptions_t* selectBoxOption;
	int clickedAtOption;
	vec2_t pos;

	/* only execute the click stuff if the selectbox is active */
	if (MN_GetMouseCapture() != node)
		return;

	MN_GetNodeAbsPos(node, pos);
	clickedAtOption = (y - pos[1]);

	/* we click on the head */
	if (clickedAtOption < node->size[1])
		return;

	clickedAtOption = (clickedAtOption - node->size[1]) / node->size[1];
	if (clickedAtOption < 0 || clickedAtOption >= node->u.option.count)
		return;

	/* the cvar string is stored in dataModelSkinOrCVar */
	/* no cvar given? */
	if (!node->cvar || !*(char*)node->cvar) {
		Com_Printf("MN_SelectboxClick: node '%s' doesn't have a valid cvar assigned (menu %s)\n", node->name, node->menu->name);
		return;
	}

	/* no cvar? */
	if (Q_strncmp((const char *)node->cvar, "*cvar", 5))
		return;

	/* select the right option */
	selectBoxOption = node->u.option.first;
	for (; clickedAtOption > 0 && selectBoxOption; selectBoxOption = selectBoxOption->next) {
		clickedAtOption--;
	}

	/* update the status */
	if (selectBoxOption) {
		const char *cvarName = &((const char *)node->cvar)[6];
		MN_SetCvar(cvarName, selectBoxOption->value, 0);
		if (selectBoxOption->action[0] != '\0') {
#ifdef DEBUG
			if (selectBoxOption->action[strlen(selectBoxOption->action) - 1] != ';')
				Com_Printf("Selectbox option with none terminated action command (%s.%s)\n", node->menu->name, node->name);
#endif
			Cbuf_AddText(selectBoxOption->action);
		}
	}
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void MN_SelectBoxNodeLoading (menuNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
}

static void MN_SelectBoxNodeLoaded (menuNode_t *node)
{
	/* force a size (according to the texture) */
	node->size[1] = SELECTBOX_DEFAULT_HEIGHT;
}


static const value_t properties[] = {
	/* very special attribute */
	{"option", V_SPECIAL_OPTIONNODE, offsetof(menuNode_t, u.option.first), 0},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractOptionNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractoption";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
}

void MN_RegisterSelectBoxNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "selectbox";
	behaviour->extends = "abstractoption";
	behaviour->draw = MN_SelectBoxNodeDraw;
	behaviour->drawOverMenu = MN_SelectBoxNodeDrawOverMenu;
	behaviour->leftClick = MN_SelectBoxNodeClick;
	behaviour->mouseMove = MN_SelectBoxNodeMouseMove;
	behaviour->loading = MN_SelectBoxNodeLoading;
	behaviour->loaded = MN_SelectBoxNodeLoaded;
	behaviour->capturedMouseMove = MN_SelectBoxNodeCapturedMouseMove;
}
