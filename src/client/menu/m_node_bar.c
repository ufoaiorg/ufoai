/**
 * @file m_node_bar.c
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

static void MN_DrawBarNode (menuNode_t *node)
{
	vec4_t color;
	float fac, bar_width;
	vec2_t nodepos;
	menu_t *menu = node->menu;
	float min = MN_GetReferenceFloat(menu, node->u.abstractvalue.min);
	float max = MN_GetReferenceFloat(menu, node->u.abstractvalue.max);
	float value = MN_GetReferenceFloat(menu, node->u.abstractvalue.value);
	MN_GetNodeAbsPos(node, nodepos);
	VectorScale(node->color, 0.8, color);
	color[3] = node->color[3];

	/* in the case of MN_BAR the first three data array values are float values - see menuDataValues_t */
	fac = node->size[0] / (max - min);
	bar_width = (value - min) * fac;
	R_DrawFill(nodepos[0], nodepos[1], bar_width, node->size[1], node->align, node->state ? color : node->color);
}

/**
 * @brief Handles the bar cvar values
 * @sa Key_Message
 */
static void MN_BarClick (menuNode_t * node, int x, int y)
{
	char var[MAX_VAR];
	vec2_t pos;
	menu_t * menu = node->menu;

	if (!node->mousefx)
		return;

	MN_GetNodeAbsPos(node, pos);
	Q_strncpyz(var, node->u.abstractvalue.value, sizeof(var));
	/* no cvar? */
	if (!Q_strncmp(var, "*cvar", 5)) {
		/* normalize it */
		const float frac = (float) (x - pos[0]) / node->size[0];
		const float min = MN_GetReferenceFloat(menu, node->u.abstractvalue.min);
		const float value = min + frac * (MN_GetReferenceFloat(menu, node->u.abstractvalue.max) - min);
		/* in the case of MN_BAR the first three data array values are float values - see menuDataValues_t */
		MN_SetCvar(&var[6], NULL, value);
	}
}

void MN_RegisterNodeBar (nodeBehaviour_t *behaviour)
{
	behaviour->name = "bar";
	behaviour->draw = MN_DrawBarNode;
	behaviour->leftClick = MN_BarClick;
}

