/**
 * @file m_node_checkbox.c
 * @brief The checkbox node is a three state widget. If the value is 0,
 * checkbox is unchecked, if value is bigger than 0, the value is checked;
 * but if the value is under 0, the checkbox display an "invalidate" status.
 * @code
 * checkbox check_item {
 *   cvar "*cvar mn_serverday"
 *   pos  "410 100"
 * }
 * @endcode
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

#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_main.h"
#include "../m_actions.h"
#include "../m_render.h"
#include "m_node_checkbox.h"
#include "m_node_abstractnode.h"

#define EXTRADATA(node) (node->u.abstractvalue)

static void MN_CheckBoxNodeDraw (menuNode_t* node)
{
	const float value = MN_GetReferenceFloat(node, EXTRADATA(node).value);
	vec2_t pos;
	const char *image = MN_GetReferenceString(node, node->image);
	int texx, texy;

	/* image set? */
	if (!image || image[0] == '\0')
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

	MN_GetNodeAbsPos(node, pos);
	MN_DrawNormImageByName(pos[0], pos[1], node->size[0], node->size[1],
		texx + node->size[0], texy + node->size[1], texx, texy, image);
}

/**
 * @brief Activate the node. Can be used without the mouse (ie. a button will execute onClick)
 */
static void MN_CheckBoxNodeActivate (menuNode_t *node)
{
	const float last = MN_GetReferenceFloat(node, EXTRADATA(node).value);
	float value;

	if (node->disabled)
		return;

	/* update value */
	value = (last > 0) ? 0 : 1;
	if (last == value)
		return;

	/* save result */
	EXTRADATA(node).lastdiff = value - last;
	if (!strncmp(EXTRADATA(node).value, "*cvar:", 6)) {
		MN_SetCvar(&((char*)EXTRADATA(node).value)[6], NULL, value);
	} else {
		*(float*) EXTRADATA(node).value = value;
	}

	/* fire change event */
	if (node->onChange) {
		MN_ExecuteEventActions(node, node->onChange);
	}
}

static void MN_CheckBoxNodeCallActivate (menuNode_t *node, const menuCallContext_t *context)
{
	MN_CheckBoxNodeActivate(node);
}

/**
 * @brief Handles checkboxes clicks
 */
static void MN_CheckBoxNodeClick (menuNode_t * node, int x, int y)
{
	MN_CheckBoxNodeActivate(node);
}

/**
 * @brief Handled before the begin of the load of the node from the script
 */
static void MN_CheckBoxNodeLoading (menuNode_t *node)
{
}

static const value_t properties[] = {
	/* @override image
	 * Texture used for the widget. Its a 128×128 template image with all
	 * three status according to the value, and four status according to the
	 * interaction. From left to right: unchecked, checked, and invalidate.
	 * From top to bottom: normal, hovered by the mouse, clicked, disabled.
	 * @image html http://ufoai.ninex.info/wiki/images/Checkbox_template.png
	 */

	/* Call it to toggle the node status. */
	{"toggle", V_UI_NODEMETHOD, ((size_t) MN_CheckBoxNodeCallActivate), 0},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterCheckBoxNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "checkbox";
	behaviour->extends = "abstractvalue";
	behaviour->draw = MN_CheckBoxNodeDraw;
	behaviour->leftClick = MN_CheckBoxNodeClick;
	behaviour->loading = MN_CheckBoxNodeLoading;
	behaviour->activate = MN_CheckBoxNodeActivate;
	behaviour->properties = properties;
}
