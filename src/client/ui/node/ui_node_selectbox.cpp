/**
 * @file
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

#include "../../cl_language.h"

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
void uiSelectBoxNode::onCapturedMouseMove (uiNode_t* node, int x, int y)
{
	UI_NodeAbsoluteToRelativePos(node, &x, &y);

	/* test bounded box */
	if (x < 0 || y < 0 || x > node->box.size[0] || y > node->box.size[1] * (EXTRADATA(node).count + 1)) {
		return;
	}

	int posy = node->box.size[1];
	for (uiNode_t* option = UI_AbstractOptionGetFirstOption(node); option; option = option->next) {
		if (option->invis)
			continue;
		OPTIONEXTRADATA(option).hovered = (posy <= y && y < posy + node->box.size[1]);
		posy += node->box.size[1];
	}
}

void uiSelectBoxNode::draw (uiNode_t* node)
{
	vec2_t nodepos;
	static vec4_t invisColor = {1.0, 1.0, 1.0, 0.7};

	const char* ref = UI_AbstractOptionGetCurrentValue(node);
	if (ref == nullptr)
		return;

	UI_GetNodeAbsPos(node, nodepos);
	const char* imageName = UI_GetReferenceString(node, node->image);
	if (!imageName)
		imageName = "ui/selectbox";

	const image_t* image = UI_LoadImage(imageName);

	const char* font = UI_GetFontFromNode(node);
	int selBoxX = nodepos[0] + SELECTBOX_SIDE_WIDTH;
	int selBoxY = nodepos[1] + SELECTBOX_SPACER;

	/* left border */
	UI_DrawNormImage(false, nodepos[0], nodepos[1], SELECTBOX_SIDE_WIDTH, node->box.size[1],
		SELECTBOX_SIDE_WIDTH, SELECTBOX_DEFAULT_HEIGHT, 0.0f, 0.0f, image);
	/* stretched middle bar */
	UI_DrawNormImage(false, nodepos[0] + SELECTBOX_SIDE_WIDTH, nodepos[1], node->box.size[0]-SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, node->box.size[1],
		12.0f, SELECTBOX_DEFAULT_HEIGHT, 7.0f, 0.0f, image);
	/* right border (arrow) */
	UI_DrawNormImage(false, nodepos[0] + node->box.size[0] - SELECTBOX_RIGHT_WIDTH, nodepos[1], SELECTBOX_DEFAULT_HEIGHT, node->box.size[1],
		12.0f + SELECTBOX_RIGHT_WIDTH, SELECTBOX_DEFAULT_HEIGHT, 12.0f, 0.0f, image);

	/* draw the label for the current selected option */
	for (uiNode_t* option = UI_AbstractOptionGetFirstOption(node); option; option = option->next) {
		if (!Q_streq(OPTIONEXTRADATA(option).value, ref))
			continue;

		if (option->invis)
			R_Color(invisColor);

		const char* label = CL_Translate(OPTIONEXTRADATA(option).label);

		UI_DrawString(font, ALIGN_UL, selBoxX, selBoxY,
			selBoxX, node->box.size[0] - 4,
			0, label, 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);

		R_Color(nullptr);
		break;
	}

	/* must we draw the drop-down list */
	if (UI_GetMouseCapture() == node) {
		UI_CaptureDrawOver(node);
	}
}

void uiSelectBoxNode::drawOverWindow (uiNode_t* node)
{
	const char* ref = UI_AbstractOptionGetCurrentValue(node);
	if (ref == nullptr)
		return;

	vec2_t nodepos;
	UI_GetNodeAbsPos(node, nodepos);

	const char* imageName = UI_GetReferenceString(node, node->image);
	if (!imageName)
		imageName = "ui/selectbox";

	const image_t* image = UI_LoadImage(imageName);

	const char* font = UI_GetFontFromNode(node);
	int selBoxX = nodepos[0] + SELECTBOX_SIDE_WIDTH;
	int selBoxY = nodepos[1] + SELECTBOX_SPACER;

	selBoxY += node->box.size[1];

	/* drop down list */
	/* left side */
	UI_DrawNormImage(false, nodepos[0], nodepos[1] + node->box.size[1], SELECTBOX_SIDE_WIDTH, node->box.size[1] * EXTRADATA(node).count,
		7.0f, 28.0f, 0.0f, 21.0f, image);

	/* stretched middle bar */
	UI_DrawNormImage(false, nodepos[0] + SELECTBOX_SIDE_WIDTH, nodepos[1] + node->box.size[1], node->box.size[0] -SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, node->box.size[1] * EXTRADATA(node).count,
		16.0f, 28.0f, 7.0f, 21.0f, image);

	/* right side */
	UI_DrawNormImage(false, nodepos[0] + node->box.size[0] -SELECTBOX_SIDE_WIDTH-SELECTBOX_RIGHT_WIDTH, nodepos[1] + node->box.size[1], SELECTBOX_SIDE_WIDTH, node->box.size[1] * EXTRADATA(node).count,
		23.0f, 28.0f, 16.0f, 21.0f, image);

	/* now draw all available options for this selectbox */
	int check = 0;
	for (uiNode_t* option = UI_AbstractOptionGetFirstOption(node); option; option = option->next) {
		if (option->invis)
			continue;
		/* draw the hover effect */
		if (OPTIONEXTRADATA(option).hovered)
			UI_DrawFill(selBoxX, selBoxY, node->box.size[0] -SELECTBOX_SIDE_WIDTH - SELECTBOX_SIDE_WIDTH - SELECTBOX_RIGHT_WIDTH,
					SELECTBOX_DEFAULT_HEIGHT, node->color);
		/* print the option label */
		const char* label = CL_Translate(OPTIONEXTRADATA(option).label);
		UI_DrawString(font, ALIGN_UL, selBoxX, selBoxY,
			selBoxX, node->box.size[0] - 4,
			0, label, 0, 0, nullptr, false, LONGLINES_PRETTYCHOP);
		/* next entries' position */
		selBoxY += node->box.size[1];
		check++;
	}

	/** detect inconsistency */
	if (check != EXTRADATA(node).count) {
		/** force clean up cache */
		Com_Printf("uiSelectBoxNode::drawOverWindow: Node '%s' contains unsynchronized option list. Fixed.\n", UI_GetPath(node));
		EXTRADATA(node).versionId = 0;
	}

	/* left side */
	UI_DrawNormImage(false, nodepos[0], selBoxY - SELECTBOX_SPACER, SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
		7.0f, 32.0f, 0.0f, 28.0f, image);

	/* stretched middle bar */
	UI_DrawNormImage(false, nodepos[0] + SELECTBOX_SIDE_WIDTH, selBoxY - SELECTBOX_SPACER, node->box.size[0] - SELECTBOX_SIDE_WIDTH - SELECTBOX_RIGHT_WIDTH,
			SELECTBOX_BOTTOM_HEIGHT,
		16.0f, 32.0f, 7.0f, 28.0f, image);

	/* right bottom side */
	UI_DrawNormImage(false, nodepos[0] + node->box.size[0] - SELECTBOX_SIDE_WIDTH - SELECTBOX_RIGHT_WIDTH, selBoxY - SELECTBOX_SPACER,
		SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
		23.0f, 32.0f, 16.0f, 28.0f, image);
}

/**
 * @brief Handles selectboxes clicks
 */
void uiSelectBoxNode::onLeftClick (uiNode_t* node, int x, int y)
{
	/* dropdown the node */
	if (UI_GetMouseCapture() == nullptr) {
		UI_SetMouseCapture(node);
		return;
	}

	vec2_t pos;
	UI_GetNodeAbsPos(node, pos);
	int clickedAtOption = (y - pos[1]);

	/* we click outside */
	if (x < pos[0] || y < pos[1] || x >= pos[0] + node->box.size[0] || y >= pos[1] + node->box.size[1] * (EXTRADATA(node).count + 1)) {
		UI_MouseRelease();
		return;
	}

	/* we click on the head */
	if (clickedAtOption < node->box.size[1]) {
		UI_MouseRelease();
		return;
	}

	clickedAtOption = (clickedAtOption - node->box.size[1]) / node->box.size[1];
	if (clickedAtOption < 0 || clickedAtOption >= EXTRADATA(node).count)
		return;

	if (UI_AbstractOptionGetCurrentValue(node) == nullptr)
		return;

	/* select the right option */
	uiNode_t* option = UI_AbstractOptionGetFirstOption(node);
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
void uiSelectBoxNode::onLoading (uiNode_t* node)
{
	Vector4Set(node->color, 0.6, 0.6, 0.6, 0.3);
}

void uiSelectBoxNode::onLoaded (uiNode_t* node)
{
	/* force a size (according to the texture) */
	node->box.size[1] = SELECTBOX_DEFAULT_HEIGHT;
}

void UI_RegisterSelectBoxNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "selectbox";
	behaviour->extends = "abstractoption";
	behaviour->manager = UINodePtr(new uiSelectBoxNode());
	behaviour->drawItselfChild = true;
}
