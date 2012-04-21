/**
 * @file ui_node_checkbox.c
 * @brief The checkbox node is a three state widget. If the value is 0,
 * checkbox is unchecked, if value is bigger than 0, the value is checked;
 * but if the value is under 0, the checkbox display an "invalidate" status.
 * @code
 * checkbox check_item {
 *   cvar "*cvar foobar"
 *   pos  "410 100"
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

#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_behaviour.h"
#include "../ui_main.h"
#include "../ui_actions.h"
#include "../ui_render.h"
#include "ui_node_checkbox.h"
#include "ui_node_abstractnode.h"
#include "ui_node_abstractvalue.h"

#define EXTRADATA(node) UI_EXTRADATA(node, abstractValueExtraData_t)

static void UI_CheckBoxNodeDraw (uiNode_t* node)
{
	const float value = UI_GetReferenceFloat(node, EXTRADATA(node).value);
	vec2_t pos;
	const char *image = UI_GetReferenceString(node, node->image);
	int texx, texy;

	/* image set? */
	if (Q_strnull(image))
		return;

	/* outer status */
	if (node->disabled) {
		texy = 96;
	} else if (node->state) {
		texy = 32;
	} else {
		texy = 0;
	}

	/* inner status */
	if (value == 0) {
		texx = 0;
	} else if (value > 0) {
		texx = 32;
	} else { /* value < 0 */
		texx = 64;
	}

	UI_GetNodeAbsPos(node, pos);
	UI_DrawNormImageByName(qfalse, pos[0], pos[1], node->size[0], node->size[1],
		texx + node->size[0], texy + node->size[1], texx, texy, image);
}

/**
 * @brief Activate the node. Can be used without the mouse (ie. a button will execute onClick)
 */
static void UI_CheckBoxNodeActivate (uiNode_t *node)
{
	const float last = UI_GetReferenceFloat(node, EXTRADATA(node).value);
	float value;

	if (node->disabled)
		return;

	/* update value */
	value = (last > 0) ? 0 : 1;
	if (last == value)
		return;

	/* save result */
	EXTRADATA(node).lastdiff = value - last;
	if (Q_strstart((const char *)EXTRADATA(node).value, "*cvar:")) {
		Cvar_SetValue(&((const char*)EXTRADATA(node).value)[6], value);
	} else {
		*(float*) EXTRADATA(node).value = value;
	}

	/* fire change event */
	if (node->onChange) {
		UI_ExecuteEventActions(node, node->onChange);
	}
}

static void UI_CheckBoxNodeCallActivate (uiNode_t *node, const uiCallContext_t *context)
{
	UI_CheckBoxNodeActivate(node);
}

/**
 * @brief Handles checkboxes clicks
 */
static void UI_CheckBoxNodeClick (uiNode_t * node, int x, int y)
{
	if (node->onClick)
		UI_ExecuteEventActions(node, node->onClick);

	UI_CheckBoxNodeActivate(node);
}

/**
 * @brief Handled before the begin of the load of the node from the script
 */
static void UI_CheckBoxNodeLoading (uiNode_t *node)
{
}

void UI_RegisterCheckBoxNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "checkbox";
	behaviour->extends = "abstractvalue";
	behaviour->draw = UI_CheckBoxNodeDraw;
	behaviour->leftClick = UI_CheckBoxNodeClick;
	behaviour->loading = UI_CheckBoxNodeLoading;
	behaviour->activate = UI_CheckBoxNodeActivate;

	/* Texture used for the widget. Its a 128x128 template image with all
	 * three status according to the value, and four status according to the
	 * interaction. From left to right: unchecked, checked, and invalidate.
	 * From top to bottom: normal, hovered by the mouse, clicked, disabled.
	 * @image html http://ufoai.org/wiki/images/Checkbox_template.png
	 */
	UI_RegisterOveridedNodeProperty(behaviour, "image");

	/* Call it to toggle the node status. */
	UI_RegisterNodeMethod(behaviour, "toggle", UI_CheckBoxNodeCallActivate);
}
