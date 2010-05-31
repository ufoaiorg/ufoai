/**
 * @file m_node_selectbox.c
 * @todo manage disabled option
 * @code
 * selectbox texres_box
 * {
 * 	{
 *	 	image	"ui/selectbox_green"
 *	 	pos	"774 232"
 * 		size	"100 20"
 * 		color	"0.6 0.6 0.6 0.3"
 * 		cvar	"*cvar:gl_maxtexres"
 *  }
 * 	option low_value {
 * 		label	"_Low"
 * 		value	"256"
 * 	}
 * 	option medium_value {
 * 		label	"_Medium"
 * 		value	"512"
 * 	}
 * }
 * @endcode
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
#include "../m_internal.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_input.h"
#include "../m_draw.h"
#include "../m_render.h"
#include "m_node_selectbox.h"
#include "m_node_abstractoption.h"
#include "m_node_abstractnode.h"
#include "m_node_option.h"

#include "../../client.h" /* gettext _() */

#define EXTRADATA(node) MN_EXTRADATA(node, abstractOptionExtraData_t)

#define SELECTBOX_DEFAULT_HEIGHT 20.0f

#define SELECTBOX_SIDE_WIDTH 7.0f
#define SELECTBOX_RIGHT_WIDTH 20.0f

#define SELECTBOX_SPACER 2.0f
#define SELECTBOX_BOTTOM_HEIGHT 4.0f

/**
 * @brief call when the mouse move is the node is captured
 * @todo we can remove the loop if we save the current element in the node
 */
static void MN_SelectBoxNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	menuNode_t* option;
	int posy;

	MN_NodeAbsoluteToRelativePos(node, &x, &y);

	/* test bounded box */
	if (x < 0 || y < 0 || x > node->size[0] || y > node->size[1] * (EXTRADATA(node).count + 1)) {
		return;
	}

	posy = node->size[1];
	for (option = MN_AbstractOptionGetFirstOption(node); option; option = option->next) {
		if (option->invis)
			continue;
		OPTIONEXTRADATA(option).hovered = (posy <= y && y < posy + node->size[1]);
		posy += node->size[1];
	}
}

static void MN_SelectBoxNodeDraw (menuNode_t *node)
{
	menuNode_t* option;
	int selBoxX, selBoxY;
	const char *ref;
	const char *font;
	vec2_t nodepos;
	const char* imageName;
	const image_t *image;
	static vec4_t invisColor = {1.0, 1.0, 1.0, 0.7};

	if (!node->cvar)
		return;

	MN_GetNodeAbsPos(node, nodepos);
	imageName = MN_GetReferenceString(node, node->image);
	if (!imageName)
		imageName = "ui/selectbox";

	image = MN_LoadImage(imageName);

	ref = MN_GetReferenceString(node, node->cvar);
	font = MN_GetFontFromNode(node);
	selBoxX = nodepos[0] + SELECTBOX_SIDE_WIDTH;
	selBoxY = nodepos[1] + SELECTBOX_SPACER;

	/* left border */
	MN_DrawNormImage(nodepos[0], nodepos[1], SELECTBOX_SIDE_WIDTH, node->size[1],
		SELECTBOX_SIDE_WIDTH, SELECTBOX_DEFAULT_HEIGHT, 0.0f, 0.0f, image);
	/* stretched middle bar */
	MN_DrawNormImage(nodepos[0] + SELECTBOX_SIDE_WIDTH, nodepos[1], node->size[0]-SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, node->size[1],
		12.0f, SELECTBOX_DEFAULT_HEIGHT, 7.0f, 0.0f, image);
	/* right border (arrow) */
	MN_DrawNormImage(nodepos[0] + node->size[0] - SELECTBOX_RIGHT_WIDTH, nodepos[1], SELECTBOX_DEFAULT_HEIGHT, node->size[1],
		12.0f + SELECTBOX_RIGHT_WIDTH, SELECTBOX_DEFAULT_HEIGHT, 12.0f, 0.0f, image);

	/* draw the label for the current selected option */
	for (option = MN_AbstractOptionGetFirstOption(node); option; option = option->next) {

		if (strcmp(OPTIONEXTRADATA(option).value, ref) != 0)
			continue;

		if (option->invis)
			R_Color(invisColor);

		MN_DrawString(font, ALIGN_UL, selBoxX, selBoxY,
			selBoxX, selBoxY, node->size[0] - 4, 0,
			0, _(OPTIONEXTRADATA(option).label), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);

		R_Color(NULL);
		break;
	}

	/* must we draw the drop-down list */
	if (MN_GetMouseCapture() == node) {
		MN_CaptureDrawOver(node);
	}
}

static void MN_SelectBoxNodeDrawOverMenu (menuNode_t *node)
{
	menuNode_t* option;
	int selBoxX, selBoxY;
	const char *ref;
	const char *font;
	vec2_t nodepos;
	const char* imageName;
	const image_t *image;

	if (!node->cvar)
		return;

	MN_GetNodeAbsPos(node, nodepos);
	imageName = MN_GetReferenceString(node, node->image);
	if (!imageName)
		imageName = "ui/selectbox";

	image = MN_LoadImage(imageName);

	ref = MN_GetReferenceString(node, node->cvar);
	font = MN_GetFontFromNode(node);
	selBoxX = nodepos[0] + SELECTBOX_SIDE_WIDTH;
	selBoxY = nodepos[1] + SELECTBOX_SPACER;

	selBoxY += node->size[1];

	/* drop down menu */
	/* left side */
	MN_DrawNormImage(nodepos[0], nodepos[1] + node->size[1], SELECTBOX_SIDE_WIDTH, node->size[1] * EXTRADATA(node).count,
		7.0f, 28.0f, 0.0f, 21.0f, image);

	/* stretched middle bar */
	MN_DrawNormImage(nodepos[0] + SELECTBOX_SIDE_WIDTH, nodepos[1] + node->size[1], node->size[0] -SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, node->size[1] * EXTRADATA(node).count,
		16.0f, 28.0f, 7.0f, 21.0f, image);

	/* right side */
	MN_DrawNormImage(nodepos[0] + node->size[0] -SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, nodepos[1] + node->size[1], SELECTBOX_SIDE_WIDTH, node->size[1] * EXTRADATA(node).count,
		23.0f, 28.0f, 16.0f, 21.0f, image);

	/* now draw all available options for this selectbox */
	for (option = MN_AbstractOptionGetFirstOption(node); option; option = option->next) {
		if (option->invis)
			continue;
		/* draw the hover effect */
		if (OPTIONEXTRADATA(option).hovered)
			MN_DrawFill(selBoxX, selBoxY, node->size[0] -SELECTBOX_SIDE_WIDTH - SELECTBOX_SIDE_WIDTH - SELECTBOX_RIGHT_WIDTH,
					SELECTBOX_DEFAULT_HEIGHT, node->color);
		/* print the option label */
		MN_DrawString(font, ALIGN_UL, selBoxX, selBoxY,
			selBoxX, nodepos[1] + node->size[1], node->size[0] - 4, 0,
			0, _(OPTIONEXTRADATA(option).label), 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		/* next entries' position */
		selBoxY += node->size[1];
	}
	/* left side */
	MN_DrawNormImage(nodepos[0], selBoxY - SELECTBOX_SPACER, SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
		7.0f, 32.0f, 0.0f, 28.0f, image);

	/* stretched middle bar */
	MN_DrawNormImage(nodepos[0] + SELECTBOX_SIDE_WIDTH, selBoxY - SELECTBOX_SPACER, node->size[0] - SELECTBOX_SIDE_WIDTH - SELECTBOX_RIGHT_WIDTH,
			SELECTBOX_BOTTOM_HEIGHT,
		16.0f, 32.0f, 7.0f, 28.0f, image);

	/* right bottom side */
	MN_DrawNormImage(nodepos[0] + node->size[0] - SELECTBOX_SIDE_WIDTH - SELECTBOX_RIGHT_WIDTH, selBoxY - SELECTBOX_SPACER,
		SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
		23.0f, 32.0f, 16.0f, 28.0f, image);
}

/**
 * @brief Handles selectboxes clicks
 */
static void MN_SelectBoxNodeClick (menuNode_t *node, int x, int y)
{
	menuNode_t* option;
	int clickedAtOption;
	vec2_t pos;

	/* dropdown the node */
	if (MN_GetMouseCapture() == NULL) {
		MN_SetMouseCapture(node);
		return;
	}

	MN_GetNodeAbsPos(node, pos);
	clickedAtOption = (y - pos[1]);

	/* we click outside */
	if (x < pos[0] || y < pos[1] || x >= pos[0] + node->size[0] || y >= pos[1] + node->size[1] * (EXTRADATA(node).count + 1)) {
		MN_MouseRelease();
		return;
	}

	/* we click on the head */
	if (clickedAtOption < node->size[1]) {
		MN_MouseRelease();
		return;
	}

	clickedAtOption = (clickedAtOption - node->size[1]) / node->size[1];
	if (clickedAtOption < 0 || clickedAtOption >= EXTRADATA(node).count)
		return;

	/* the cvar string is stored in dataModelSkinOrCVar */
	/* no cvar given? */
	if (!node->cvar || !*(char*)node->cvar) {
		Com_Printf("MN_SelectboxClick: node '%s' doesn't have a valid cvar assigned\n", MN_GetPath(node));
		return;
	}

	/* no cvar? */
	if (strncmp((const char *)node->cvar, "*cvar", 5))
		return;

	/* select the right option */
	option = MN_AbstractOptionGetFirstOption(node);
	for (; option; option = option->next) {
		if (option->invis)
			continue;
		if (clickedAtOption == 0)
			break;
		clickedAtOption--;
	}

	/* update the status */
	if (option) {
		const char *cvarName = &((const char *)node->cvar)[6];
		MN_SetCvar(cvarName, OPTIONEXTRADATA(option).value, 0);
		if (node->onChange)
			MN_ExecuteEventActions(node, node->onChange);
	}

	/* close the dropdown */
	MN_MouseRelease();
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
	behaviour->loading = MN_SelectBoxNodeLoading;
	behaviour->loaded = MN_SelectBoxNodeLoaded;
	behaviour->capturedMouseMove = MN_SelectBoxNodeCapturedMouseMove;
	behaviour->drawItselfChild = qtrue;
}
