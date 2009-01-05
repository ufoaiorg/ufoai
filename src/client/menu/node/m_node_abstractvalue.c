/**
 * @file m_node_abstractvalue.c
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "m_node_abstractvalue.h"

inline static void MN_InitCvarOrFloat (float** adress)
{
	if (*adress == NULL) {
		*adress = MN_AllocFloat(1);
		**adress = 0;
	}
}

static void MN_RegisterAbstractValueLoaded (menuNode_t * node)
{
	MN_InitCvarOrFloat((float**)&node->u.abstractvalue.value);
	MN_InitCvarOrFloat((float**)&node->u.abstractvalue.delta);
	MN_InitCvarOrFloat((float**)&node->u.abstractvalue.max);
	MN_InitCvarOrFloat((float**)&node->u.abstractvalue.min);
	*(float*)node->u.abstractvalue.delta = 1;
}

static const value_t properties[] = {
	{"current", V_CVAR_OR_FLOAT, offsetof(menuNode_t, u.abstractvalue.value), 0},
	{"delta", V_CVAR_OR_FLOAT, offsetof(menuNode_t, u.abstractvalue.delta), 0},
	{"max", V_CVAR_OR_FLOAT, offsetof(menuNode_t, u.abstractvalue.max), 0},
	{"min", V_CVAR_OR_FLOAT, offsetof(menuNode_t, u.abstractvalue.min), 0},
	{"lastdiff", V_FLOAT, offsetof(menuNode_t, u.abstractvalue.lastdiff), MEMBER_SIZEOF(menuNode_t, u.abstractvalue.lastdiff)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractValueNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractvalue";
	behaviour->loaded = MN_RegisterAbstractValueLoaded;
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
}
