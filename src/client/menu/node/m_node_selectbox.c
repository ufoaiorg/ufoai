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

#define SELECTBOX_DEFAULT_HEIGHT 20.0f

#define SELECTBOX_SIDE_WIDTH 7.0f
#define SELECTBOX_RIGHT_WIDTH 20.0f

#define SELECTBOX_SPACER 2.0f
#define SELECTBOX_BOTTOM_HEIGHT 4.0f

/**
 * @brief Return the first option of the node
 * @todo check versionId and update cached data, and fire events
 */
static menuOption_t*  MN_SelectBoxNodeGetFirstOption (menuNode_t * node)
{
	if (node->u.option.first) {
		return node->u.option.first;
	} else {
		const int v = MN_GetDataVersion(node->u.option.dataId);
		if (v != node->u.option.dataId) {
			int count = 0;
			menuOption_t *option = MN_GetOption(node->u.option.dataId);
			while (option) {
				count++;
				option = option->next;
			}
			node->u.option.count = count;

			node->u.option.versionId = v;
		}
		return MN_GetOption(node->u.option.dataId);
	}
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
	menuOption_t* option;
	int posy;

	MN_NodeAbsoluteToRelativePos(node, &x, &y);

	/* test bounded box */
	if (x < 0 || y < 0 || x > node->size[0] || y > node->size[1] * (node->u.option.count + 1)) {
		MN_MouseRelease();
		return;
	}

	posy = node->size[1];
	for (option = MN_SelectBoxNodeGetFirstOption(node); option; option = option->next) {
		option->hovered = (posy <= y && y < posy + node->size[1]) ? qtrue : qfalse;
		posy += node->size[1];
	}
}

static void MN_SelectBoxNodeDraw (menuNode_t *node)
{
	menuOption_t* option;
	int selBoxX, selBoxY;
	const char *ref;
	const char *font;
	vec2_t nodepos;
	const char* image;

	if (!node->cvar)
		return;

	MN_GetNodeAbsPos(node, nodepos);
	image = MN_GetReferenceString(node, node->image);
	if (!image)
		image = "menu/selectbox";

	ref = MN_GetReferenceString(node, node->cvar);
	font = MN_GetFont(node);
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
	for (option = MN_SelectBoxNodeGetFirstOption(node); option; option = option->next) {
		if (!Q_strcmp(option->value, ref)) {
			R_FontDrawString(font, ALIGN_UL, selBoxX, selBoxY,
				selBoxX, selBoxY, node->size[0] - 4, 0,
				0, _(option->label), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		}
	}

	/* must we draw the drop-down list */
	if (MN_GetMouseCapture() == node) {
		MN_CaptureDrawOver(node);
	}
}

static void MN_SelectBoxNodeDrawOverMenu (menuNode_t *node)
{
	menuOption_t* option;
	int selBoxX, selBoxY;
	const char *ref;
	const char *font;
	vec2_t nodepos;
	const char* image;

	if (!node->cvar)
		return;

	MN_GetNodeAbsPos(node, nodepos);
	image = MN_GetReferenceString(node, node->image);
	if (!image)
		image = "menu/selectbox";

	ref = MN_GetReferenceString(node, node->cvar);
	font = MN_GetFont(node);
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
	for (option = MN_SelectBoxNodeGetFirstOption(node); option; option = option->next) {
		/* draw the hover effect */
		if (option->hovered)
			R_DrawFill(selBoxX, selBoxY, node->size[0] -SELECTBOX_SIDE_WIDTH-SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, SELECTBOX_DEFAULT_HEIGHT, ALIGN_UL, node->color);
		/* print the option label */
		R_FontDrawString(font, ALIGN_UL, selBoxX, selBoxY,
			selBoxX, nodepos[1] + node->size[1], node->size[0] - 4, 0,
			0, _(option->label), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
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
	menuOption_t* option;
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
	option = MN_SelectBoxNodeGetFirstOption(node);
	for (; clickedAtOption > 0 && option; option = option->next) {
		clickedAtOption--;
	}

	/* update the status */
	if (option) {
		const char *cvarName = &((const char *)node->cvar)[6];
		MN_SetCvar(cvarName, option->value, 0);
		if (option->action[0] != '\0') {
#ifdef DEBUG
			if (option->action[strlen(option->action) - 1] != ';')
				Com_Printf("Selectbox option with none terminated action command (%s.%s)\n", node->menu->name, node->name);
#endif
			Cbuf_AddText(option->action);
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
