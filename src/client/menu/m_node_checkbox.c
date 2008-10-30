/**
 * @file m_node_checkbox.c
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

#include "m_nodes.h"
#include "m_parse.h"
#include "m_input.h"

/**
 * @brief Handled alfer the end of the load of the node from the script (all data and/or child are set)
 */
static void MN_CheckBoxNodeLoaded (menuNode_t *node) {
	/* update the size when its possible */
	if (node->size[0] == 0 && node->size[1] == 0) {
		if (node->texl[0] != 0 || node->texh[0] != 0) {
			node->size[0] = node->texh[0] - node->texl[0];
			node->size[1] = node->texh[1] - node->texl[1];
		} else {
			node->size[0] = 18;
			node->size[1] = 17;
		}
	}
}

static void MN_DrawCheckBoxNode (menuNode_t* node)
{
	const char *image;
	const char *ref;
	vec2_t nodepos;
	const menu_t* menu = node->menu;
	const char *checkBoxImage = MN_GetReferenceString(node->menu, node->dataImageOrModel);

	MN_GetNodeAbsPos(node, nodepos);
	/* image set? */
	if (checkBoxImage && *checkBoxImage)
		image = checkBoxImage;
	else
		image = "menu/checkbox";

	ref = MN_GetReferenceString(menu, node->dataModelSkinOrCVar);
	if (!ref)
		return;

	switch (*ref) {
	case '0':
		R_DrawNormPic(nodepos[0], nodepos[1], node->size[0], node->size[1],
			node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, va("%s_unchecked", image));
		break;
	case '1':
		R_DrawNormPic(nodepos[0], nodepos[1], node->size[0], node->size[1],
			node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, va("%s_checked", image));
		break;
	default:
		Com_Printf("Error - invalid value for MN_CHECKBOX node - only 0/1 allowed (%s)\n", ref);
	}
}

/**
 * @brief Handles checkboxes clicks
 */
static void MN_CheckboxClick (menuNode_t * node, int x, int y)
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

void MN_RegisterNodeCheckBox (nodeBehaviour_t *behaviour)
{
	behaviour->name = "checkbox";
	behaviour->draw = MN_DrawCheckBoxNode;
	behaviour->leftClick = MN_CheckboxClick;
	behaviour->loaded = MN_CheckBoxNodeLoaded;
}
