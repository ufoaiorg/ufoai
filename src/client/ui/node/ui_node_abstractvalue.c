/**
 * @file ui_node_abstractvalue.c
 * @brief The abstractvalue node is an abstract node (we can't instanciate it).
 * It provide common properties to concrete nodes, to manage a value in a range.
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
#include "../ui_internal.h"
#include "ui_node_abstractvalue.h"

#define EXTRADATA_TYPE abstractValueExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static const uiBehaviour_t const *localBehaviour;

static inline void UI_InitCvarOrFloat (float** adress, float defaultValue)
{
	if (*adress == NULL) {
		*adress = UI_AllocStaticFloat(1);
		**adress = defaultValue;
	}
}

static void UI_AbstractValueLoaded (uiNode_t * node)
{
	UI_InitCvarOrFloat((float**)&EXTRADATA(node).value, 0);
	UI_InitCvarOrFloat((float**)&EXTRADATA(node).delta, 1);
	UI_InitCvarOrFloat((float**)&EXTRADATA(node).max, 0);
	UI_InitCvarOrFloat((float**)&EXTRADATA(node).min, 0);
}

static void UI_AbstractValueNew (uiNode_t * node)
{
	EXTRADATA(node).value = Mem_PoolAlloc(sizeof(float), ui_dynPool, 0);
	EXTRADATA(node).delta = Mem_PoolAlloc(sizeof(float), ui_dynPool, 0);
	EXTRADATA(node).max = Mem_PoolAlloc(sizeof(float), ui_dynPool, 0);
	EXTRADATA(node).min = Mem_PoolAlloc(sizeof(float), ui_dynPool, 0);
}

static void UI_AbstractValueDelete (uiNode_t * node)
{
	Mem_Free(EXTRADATA(node).value);
	Mem_Free(EXTRADATA(node).delta);
	Mem_Free(EXTRADATA(node).max);
	Mem_Free(EXTRADATA(node).min);
	EXTRADATA(node).value = NULL;
	EXTRADATA(node).delta = NULL;
	EXTRADATA(node).max = NULL;
	EXTRADATA(node).min = NULL;
}

static void UI_CloneCvarOrFloat (const uiNode_t *source, uiNode_t *clone, const float*const* sourceData, float** cloneData)
{
	/* dont update cvar */
	if (Q_strstart(*(const char*const*)sourceData, "*cvar")) {
		/* thats anyway a const string */
		assert(!source->dynamic);
		if (clone->dynamic)
			Mem_Free(*(char**)cloneData);
		*(const char**)cloneData = *(const char*const*)sourceData;
	} else {
		/* clone float */
		if (!clone->dynamic)
			*cloneData = UI_AllocStaticFloat(1);
		**cloneData = **sourceData;
	}
}

/**
 * @brief Call to update a cloned node
 */
static void UI_AbstractValueClone (const uiNode_t *source, uiNode_t *clone)
{
	localBehaviour->super->clone(source, clone);
	UI_CloneCvarOrFloat(source, clone, (const float*const*)&EXTRADATACONST(source).value, (float**)&EXTRADATA(clone).value);
	UI_CloneCvarOrFloat(source, clone, (const float*const*)&EXTRADATACONST(source).delta, (float**)&EXTRADATA(clone).delta);
	UI_CloneCvarOrFloat(source, clone, (const float*const*)&EXTRADATACONST(source).max, (float**)&EXTRADATA(clone).max);
	UI_CloneCvarOrFloat(source, clone, (const float*const*)&EXTRADATACONST(source).min, (float**)&EXTRADATA(clone).min);
}

void UI_RegisterAbstractValueNode (uiBehaviour_t *behaviour)
{
	localBehaviour = behaviour;
	behaviour->name = "abstractvalue";
	behaviour->loaded = UI_AbstractValueLoaded;
	behaviour->clone = UI_AbstractValueClone;
	behaviour->newNode = UI_AbstractValueNew;
	behaviour->deleteNode = UI_AbstractValueDelete;
	behaviour->isAbstract = qtrue;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Current value of the node. It should be a cvar */
	UI_RegisterExtradataNodeProperty(behaviour, "current", V_CVAR_OR_FLOAT, abstractValueExtraData_t, value);
	/* Value of a positive step. Must be bigger than 1. */
	UI_RegisterExtradataNodeProperty(behaviour, "delta", V_CVAR_OR_FLOAT, abstractValueExtraData_t, delta);
	/* Maximum value we can set to the node. It can be a cvar. Default value is 0. */
	UI_RegisterExtradataNodeProperty(behaviour, "max", V_CVAR_OR_FLOAT, abstractValueExtraData_t, max);
	/* Minimum value we can set to the node. It can be a cvar. Default value is 1. */
	UI_RegisterExtradataNodeProperty(behaviour, "min", V_CVAR_OR_FLOAT, abstractValueExtraData_t, min);

	/* Callback value set when before calling onChange. It is used to know the change apply by the user
	 * @Deprecated
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "lastdiff", V_FLOAT, abstractValueExtraData_t, lastdiff);

}
