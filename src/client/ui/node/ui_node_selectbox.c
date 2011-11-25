/**
 * @file ui_node_selectbox.c
 * @todo manage disabled option
 * @code
 * selectbox texres_box
 * {
 * 	{
 *	 	image	"ui/selectbox"
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
#include "../ui_internal.h"
#include "../ui_parse.h"
#include "../ui_font.h"
#include "../ui_input.h"
#include "../ui_draw.h"
#include "../ui_render.h"
#include "ui_node_selectbox.h"
#include "ui_node_abstractoption.h"
#include "ui_node_abstractnode.h"
#include "ui_node_option.h"

#include "../../client.h" /* gettext _() */

#define EXTRADATA(node) UI_EXTRADATA(node, abstractOptionExtraData_t)

#define SELECTBOX_DEFAULT_HEIGHT 20.0f

#define SELECTBOX_SIDE_WIDTH 7.0f
#define SELECTBOX_RIGHT_WIDTH 20.0f

#define SELECTBOX_SPACER 2.0f
#define SELECTBOX_BOTTOM_HEIGHT 4.0f

/**
 * @brief call when the mouse move is the node is captured
 * @todo we can remove the loop if we save the current element in the node
 */
static void UI_SelectBoxNodeCapturedMouseMove (uiNode_t *node, int x, int y)
{
	uiNode_t* option;
	int posy;

	UI_NodeAbsoluteToRelativePos(node, &x, &y);

	/* test bounded box */
	if (x < 0 || y < 0 || x > node->size[0] || y > node->size[1] * (EXTRADATA(node).count + 1)) {
		return;
	}

	posy = node->size[1];
	for (option = UI_AbstractOptionGetFirstOption(node); option; option = option->next) {
		if (option->invis)
			continue;
		OPTIONEXTRADATA(option).hovered = (posy <= y && y < posy + node->size[1]);
		posy += node->size[1];
	}
}

static void UI_SelectBoxNodeDraw (uiNode_t *node)
{
	uiNode_t* option;
	int selBoxX, selBoxY;
	const char *ref;
	const char *font;
	vec2_t nodepos;
	const char* imageName;
	const image_t *image;
	static vec4_t invisColor = {1.0, 1.0, 1.0, 0.7};

	ref = UI_AbstractOptionGetCurrentValue(node);
	if (ref == NULL)
		return;

	UI_GetNodeAbsPos(node, nodepos);
	imageName = UI_GetReferenceString(node, node->image);
	if (!imageName)
		imageName = "ui/selectbox";

	image = UI_LoadImage(imageName);

	font = UI_GetFontFromNode(node);
	selBoxX = nodepos[0] + SELECTBOX_SIDE_WIDTH;
	selBoxY = nodepos[1] + SELECTBOX_SPACER;

	/* left border */
	UI_DrawNormImage(qfalse, nodepos[0], nodepos[1], SELECTBOX_SIDE_WIDTH, node->size[1],
		SELECTBOX_SIDE_WIDTH, SELECTBOX_DEFAULT_HEIGHT, 0.0f, 0.0f, image);
	/* stretched middle bar */
	UI_DrawNormImage(qfalse, nodepos[0] + SELECTBOX_SIDE_WIDTH, nodepos[1], node->size[0]-SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, node->size[1],
		12.0f, SELECTBOX_DEFAULT_HEIGHT, 7.0f, 0.0f, image);
	/* right border (arrow) */
	UI_DrawNormImage(qfalse, nodepos[0] + node->size[0] - SELECTBOX_RIGHT_WIDTH, nodepos[1], SELECTBOX_DEFAULT_HEIGHT, node->size[1],
		12.0f + SELECTBOX_RIGHT_WIDTH, SELECTBOX_DEFAULT_HEIGHT, 12.0f, 0.0f, image);

	/* draw the label for the current selected option */
	for (option = UI_AbstractOptionGetFirstOption(node); option; option = option->next) {
		const char *label;

		if (!Q_streq(OPTIONEXTRADATA(option).value, ref))
			continue;

		if (option->invis)
			R_Color(invisColor);

		label = OPTIONEXTRADATA(option).label;
		if (label[0] == '_')
			label = _(label + 1);

		UI_DrawString(font, ALIGN_UL, selBoxX, selBoxY,
			selBoxX, node->size[0] - 4,
			0, label, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);

		R_Color(NULL);
		break;
	}

	/* must we draw the drop-down list */
	if (UI_GetMouseCapture() == node) {
		UI_CaptureDrawOver(node);
	}
}

static void UI_SelectBoxNodeDrawOverWindow (uiNode_t *node)
{
	uiNode_t* option;
	int selBoxX, selBoxY;
	const char *ref;
	const char *font;
	vec2_t nodepos;
	const char* imageName;
	const image_t *image;

	ref = UI_AbstractOptionGetCurrentValue(node);
	if (ref == NULL)
		return;

	UI_GetNodeAbsPos(node, nodepos);
	imageName = UI_GetReferenceString(node, node->image);
	if (!imageName)
		imageName = "ui/selectbox";

	image = UI_LoadImage(imageName);

	font = UI_GetFontFromNode(node);
	selBoxX = nodepos[0] + SELECTBOX_SIDE_WIDTH;
	selBoxY = nodepos[1] + SELECTBOX_SPACER;

	selBoxY += node->size[1];

	/* drop down list */
	/* left side */
	UI_DrawNormImage(qfalse, nodepos[0], nodepos[1] + node->size[1], SELECTBOX_SIDE_WIDTH, node->size[1] * EXTRADATA(node).count,
		7.0f, 28.0f, 0.0f, 21.0f, image);

	/* stretched middle bar */
	UI_DrawNormImage(qfalse, nodepos[0] + SELECTBOX_SIDE_WIDTH, nodepos[1] + node->size[1], node->size[0] -SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, node->size[1] * EXTRADATA(node).count,
		16.0f, 28.0f, 7.0f, 21.0f, image);

	/* right side */
	UI_DrawNormImage(qfalse, nodepos[0] + node->size[0] -SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, nodepos[1] + node->size[1], SELECTBOX_SIDE_WIDTH, node->size[1] * EXTRADATA(node).count,
		23.0f, 28.0f, 16.0f, 21.0f, image);

	/* now draw all available options for this selectbox */
	for (option = UI_AbstractOptionGetFirstOption(node); option; option = option->next) {
		const char *label;
		if (option->invis)
			continue;
		/* draw the hover effect */
		if (OPTIONEXTRADATA(option).hovered)
			UI_DrawFill(selBoxX, selBoxY, node->size[0] -SELECTBOX_SIDE_WIDTH - SELECTBOX_SIDE_WIDTH - SELECTBOX_RIGHT_WIDTH,
					SELECTBOX_DEFAULT_HEIGHT, node->color);
		/* print the option label */
		label = OPTIONEXTRADATA(option).label;
		if (label[0] == '_')
			label = _(label + 1);
		UI_DrawString(font, ALIGN_UL, selBoxX, selBoxY,
			selBoxX, node->size[0] - 4,
			0, label, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		/* next entries' position */
		selBoxY += node->size[1];
	}
	/* left side */
	UI_DrawNormImage(qfalse, nodepos[0], selBoxY - SELECTBOX_SPACER, SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
		7.0f, 32.0f, 0.0f, 28.0f, image);

	/* stretched middle bar */
	UI_DrawNormImage(qfalse, nodepos[0] + SELECTBOX_SIDE_WIDTH, selBoxY - SELECTBOX_SPACER, node->size[0] - SELECTBOX_SIDE_WIDTH - SELECTBOX_RIGHT_WIDTH,
			SELECTBOX_BOTTOM_HEIGHT,
		16.0f, 32.0f, 7.0f, 28.0f, image);

	/* right bottom side */
	UI_DrawNormImage(qfalse, nodepos[0] + node->size[0] - SELECTBOX_SIDE_WIDTH - SELECTBOX_RIGHT_WIDTH, selBoxY - SELECTBOX_SPACER,
		SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
		23.0f, 32.0f, 16.0f, 28.0f, image);
}

/**
 * @brief Handles selectboxes clicks
 */
static void UI_SelectBoxNodeClick (uiNode_t *node, int x, int y)
{
	uiNode_t* option;
	int clickedAtOption;
	vec2_t pos;

	/* dropdown the node */
	if (UI_GetMouseCapture() == NULL) {
		UI_SetMouseCapture(node);
		return;
	}

	UI_GetNodeAbsPos(node, pos);
	clickedAtOption = (y - pos[1]);

	/* we click outside */
	if (x < pos[0] || y < pos[1] || x >= pos[0] + node->size[0] || y >= pos[1] + node->size[1] * (EXTRADATA(node).count + 1)) {
		UI_MouseRelease();
		return;
	}

	/* we click on the head */
	if (clickedAtOption < node->size[1]) {
		UI_MouseRelease();
		return;
	}

	clickedAtOption = (clickedAtOption - node->size[1]) / node->size[1];
	if (clickedAtOption < 0 || clickedAtOption >= EXTRADATA(node).count)
		return;

	if (UI_AbstractOptionGetCurrentValue(node) == NULL)
		return;

	/* select the right option */
	option = UI_AbstractOptionGetFirstOption(node);
	for (; option; option = option->next) {
		if (option->invis)
			continue;
		if (clickedAtOption == 0)
			break;
		clickedAtOption--;
	}

	/* update the status */
	if (option)
		UI_AbstractOptionSetCurrentValue(node, OPTIONEXTRADATA(option).value);

	/* close the dropdown */
	UI_MouseRelease();
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void UI_SelectBoxNodeLoading (uiNode_t *node)
{
	Vector4Set(node->color, 0.6, 0.6, 0.6, 0.3);
}

static void UI_SelectBoxNodeLoaded (uiNode_t *node)
{
	/* force a size (according to the texture) */
	node->size[1] = SELECTBOX_DEFAULT_HEIGHT;
}

void UI_RegisterSelectBoxNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "selectbox";
	behaviour->extends = "abstractoption";
	behaviour->draw = UI_SelectBoxNodeDraw;
	behaviour->drawOverWindow = UI_SelectBoxNodeDrawOverWindow;
	behaviour->leftClick = UI_SelectBoxNodeClick;
	behaviour->loading = UI_SelectBoxNodeLoading;
	behaviour->loaded = UI_SelectBoxNodeLoaded;
	behaviour->capturedMouseMove = UI_SelectBoxNodeCapturedMouseMove;
	behaviour->drawItselfChild = qtrue;
}
