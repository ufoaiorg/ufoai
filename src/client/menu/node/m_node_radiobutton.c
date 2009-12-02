/**
 * @file m_node_radiobutton.c
 * @brief The radiobutton is a clickable widget. Commonly, with use it in
 * a group of radiobuttons; the user is allowed to choose only one button
 * from this set. The current implementation share the value of the group
 * with a cvar, and each button use is own value. When the cvar equals to
 * a button value, this button is selected.
 * @code
 * radiobutton foo {
 *   cvar "*cvar:mn_serverday"
 *   value 4
 *   icon boo
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

#include "../m_main.h"
#include "../m_actions.h"
#include "../m_icon.h"
#include "../m_parse.h"
#include "../m_input.h"
#include "../m_render.h"
#include "m_node_radiobutton.h"
#include "m_node_abstractnode.h"

#define EPSILON 0.001f

/** Height of a status in a 4 status 256*256 texture */
#define MN_4STATUS_TEX_HEIGHT 64

/**
 * @brief Handles RadioButton draw
 * @todo need to implement image. We can't do everything with only one icon (or use nother icon)
 */
static void MN_RadioButtonNodeDraw (menuNode_t *node)
{
	vec2_t pos;
	iconStatus_t iconStatus;
	const float current = MN_GetReferenceFloat(node, node->cvar);
	const qboolean disabled = node->disabled || node->parent->disabled;
	int texY;
	const char *image;

	if (disabled) {
		iconStatus = ICON_STATUS_DISABLED;
		texY = MN_4STATUS_TEX_HEIGHT * 2;
	} else if (current > node->extraData1 - EPSILON && current < node->extraData1 + EPSILON) {
		iconStatus = ICON_STATUS_CLICKED;
		texY = MN_4STATUS_TEX_HEIGHT * 3;
	} else if (node->state) {
		iconStatus = ICON_STATUS_HOVER;
		texY = MN_4STATUS_TEX_HEIGHT;
	} else {
		iconStatus = ICON_STATUS_NORMAL;
		texY = 0;
	}

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node, node->image);
	if (image) {
		const int texX = rint(node->texl[0]);
		texY += node->texl[1];
		MN_DrawNormImageByName(pos[0], pos[1], node->size[0], node->size[1],
			texX + node->size[0], texY + node->size[1], texX, texY, image);
	}

	if (node->icon) {
		MN_DrawIconInBox(node->icon, iconStatus, pos[0], pos[1], node->size[0], node->size[1]);
	}
}

/**
 * @brief Activate the node. Can be used without the mouse (ie. a button will execute onClick)
 */
static void MN_RadioButtonNodeActivate (menuNode_t * node)
{
	float current;

	/* no cvar given? */
	if (!node->cvar || !*(char*)node->cvar) {
		Com_Printf("MN_RadioButtonNodeClick: node '%s' doesn't have a valid cvar assigned\n", MN_GetPath(node));
		return;
	}

	/* its not a cvar! */
	/** @todo the parser should already check that the property value is a right cvar */
	if (strncmp((const char *)node->cvar, "*cvar", 5))
		return;

	current = MN_GetReferenceFloat(node, node->cvar);
	/* Is we click on the action button, we can continue */
	if (current > node->extraData1 - EPSILON && current < node->extraData1 + EPSILON)
		return;

	{
		const char *cvarName = &((const char *)node->cvar)[6];
		MN_SetCvar(cvarName, NULL, node->extraData1);
		if (node->onChange) {
			MN_ExecuteEventActions(node, node->onChange);
		}
	}
}

/**
 * @brief Handles radio button clicks
 */
static void MN_RadioButtonNodeClick (menuNode_t * node, int x, int y)
{
	MN_RadioButtonNodeActivate(node);
}

static const value_t properties[] = {
	/* Value defining the radiobutton. Cvar is updated with this value when the radio button is selected. */
	{"value", V_FLOAT, offsetof(menuNode_t, extraData1), MEMBER_SIZEOF(menuNode_t, extraData1)},
	/* Cvar name shared with the radio button group to identify when a radio button is selected. */
	{"cvar", V_UI_CVAR, offsetof(menuNode_t, cvar), 0},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterRadioButtonNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "radiobutton";
	behaviour->draw = MN_RadioButtonNodeDraw;
	behaviour->leftClick = MN_RadioButtonNodeClick;
	behaviour->activate = MN_RadioButtonNodeActivate;
	behaviour->properties = properties;
}
