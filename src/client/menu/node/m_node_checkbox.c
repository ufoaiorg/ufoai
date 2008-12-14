/**
 * @file m_node_checkbox.c
 * @code
 * checkbox check_item {
 *   cvar "*cvar mn_serverday"
 *   pos  "410 100"
 * }
 * @endcode
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

#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_input.h"
#include "m_node_checkbox.h"

static void MN_CheckBoxNodeDraw (menuNode_t* node)
{
	const char *ref;
	vec2_t pos;
	const char *image = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	int texx, texy;

	/* image set? */
	if (!image || image[0] == '\0')
		return;

	ref = MN_GetReferenceString(node->menu, node->dataModelSkinOrCVar);
	if (!ref)
		return;

	/* outter status */
	if (node->disabled) {
		texy = 96;
	} else if (node->state) {
		texy = 32;
	} else {
		texy = 0;
	}

	/* inner status */
	switch (ref[0]) {
	case '0':
		texx = 0;
		break;
	case '1':
		texx = 32;
		break;
	case '-': /* negative */
		texx = 64;
		break;
	default:
		Com_Printf("Error - invalid value for MN_CHECKBOX node - only -1/0/1 allowed (%s)\n", ref);
	}

	MN_GetNodeAbsPos(node, pos);
	R_DrawNormPic(pos[0], pos[1], node->size[0], node->size[1],
		texx + node->size[0], texy + node->size[1], texx, texy, ALIGN_UL, node->blend, image);
}

/**
 * @brief Handles checkboxes clicks
 */
static void MN_CheckBoxNodeClick (menuNode_t * node, int x, int y)
{
	int value;
	const char *cvarName;

	assert(node->dataModelSkinOrCVar);
	/* no cvar? */
	if (Q_strncmp(node->dataModelSkinOrCVar, "*cvar", 5))
		return;

	cvarName = &((const char *)node->dataModelSkinOrCVar)[6];
	value = Cvar_VariableInteger(cvarName) ^ 1;
	MN_SetCvar(cvarName, NULL, value);
}

/**
 * @brief Handled before the begin of the load of the node from the script
 */
static void MN_CheckBoxNodeLoading (menuNode_t *node) {
	node->blend = qtrue;
}

void MN_RegisterCheckBoxNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "checkbox";
	behaviour->id = MN_CHECKBOX;
	behaviour->draw = MN_CheckBoxNodeDraw;
	behaviour->leftClick = MN_CheckBoxNodeClick;
	behaviour->loading = MN_CheckBoxNodeLoading;
}
