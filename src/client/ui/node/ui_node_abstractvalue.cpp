/**
 * @file
 * @brief The abstractvalue node is an abstract node (we can't instanciate it).
 * It provide common properties to concrete nodes, to manage a value in a range.
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

#include "../ui_nodes.h"
#include "../ui_parse.h"
#include "../ui_internal.h"
#include "ui_node_abstractvalue.h"

#include "../../input/cl_input.h"
#include "../../input/cl_keys.h"

#define EXTRADATA_TYPE abstractValueExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static inline void UI_InitCvarOrFloat (float** adress, float defaultValue)
{
	if (*adress == nullptr) {
		*adress = UI_AllocStaticFloat(1);
		**adress = defaultValue;
	}
}

void uiAbstractValueNode::onLoading (uiNode_t* node)
{
	EXTRADATA(node).shiftIncreaseFactor = 2.0F;
}

void uiAbstractValueNode::onLoaded (uiNode_t* node)
{
	UI_InitCvarOrFloat((float**)&EXTRADATA(node).value, 0);
	UI_InitCvarOrFloat((float**)&EXTRADATA(node).delta, 1);
	UI_InitCvarOrFloat((float**)&EXTRADATA(node).max, 0);
	UI_InitCvarOrFloat((float**)&EXTRADATA(node).min, 0);
}

void uiAbstractValueNode::newNode (uiNode_t* node)
{
	EXTRADATA(node).value = Mem_PoolAllocType(float, ui_dynPool);
	EXTRADATA(node).delta = Mem_PoolAllocType(float, ui_dynPool);
	EXTRADATA(node).max   = Mem_PoolAllocType(float, ui_dynPool);
	EXTRADATA(node).min   = Mem_PoolAllocType(float, ui_dynPool);
}

void uiAbstractValueNode::deleteNode (uiNode_t* node)
{
	Mem_Free(EXTRADATA(node).value);
	Mem_Free(EXTRADATA(node).delta);
	Mem_Free(EXTRADATA(node).max);
	Mem_Free(EXTRADATA(node).min);
	EXTRADATA(node).value = nullptr;
	EXTRADATA(node).delta = nullptr;
	EXTRADATA(node).max = nullptr;
	EXTRADATA(node).min = nullptr;
}

static void UI_CloneCvarOrFloat (const uiNode_t* source, uiNode_t* clone, const float*const* sourceData, float** cloneData)
{
	/* dont update cvar */
	if (Q_strstart(*(const char*const*)sourceData, "*cvar:")) {
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

float uiAbstractValueNode::getFactorFloat (const uiNode_t* node)
{
	if (!Key_IsDown(K_SHIFT))
		return 1.0F;

	return EXTRADATACONST(node).shiftIncreaseFactor;
}

void uiAbstractValueNode::setRange(uiNode_t* node, float min, float max)
{
	if (EXTRADATA(node).min == nullptr) {
		UI_InitCvarOrFloat((float**)&EXTRADATA(node).min, min);
	}
	if (EXTRADATA(node).max == nullptr) {
		UI_InitCvarOrFloat((float**)&EXTRADATA(node).max, max);
	}
}

bool uiAbstractValueNode::setValue(uiNode_t* node, float value)
{
	const float last = UI_GetReferenceFloat(node, EXTRADATA(node).value);
	const float max = UI_GetReferenceFloat(node, EXTRADATA(node).max);
	const float min = UI_GetReferenceFloat(node, EXTRADATA(node).min);

	/* ensure sane values */
	if (value < min)
		value = min;
	else if (value > max)
		value = max;

	/* nothing change? */
	if (last == value) {
		return false;
	}

	/* save result */
	EXTRADATA(node).lastdiff = value - last;
	const char* cvar = Q_strstart((char*)EXTRADATA(node).value, "*cvar:");
	if (cvar)
		Cvar_SetValue(cvar, value);
	else
		*(float*) EXTRADATA(node).value = value;

	/* fire change event */
	if (node->onChange)
		UI_ExecuteEventActions(node, node->onChange);

	return true;
}

bool uiAbstractValueNode::incValue(uiNode_t* node)
{
	float value = UI_GetReferenceFloat(node, EXTRADATA(node).value);
	const float delta = getFactorFloat(node) * UI_GetReferenceFloat(node, EXTRADATA(node).delta);
	return setValue(node, value + delta);
}

bool uiAbstractValueNode::decValue(uiNode_t* node)
{
	float value = UI_GetReferenceFloat(node, EXTRADATA(node).value);
	const float delta = getFactorFloat(node) * UI_GetReferenceFloat(node, EXTRADATA(node).delta);
	return setValue(node, value - delta);
}

float uiAbstractValueNode::getMin (uiNode_t const* node)
{
	return UI_GetReferenceFloat(node, EXTRADATACONST(node).min);
}

float uiAbstractValueNode::getMax (uiNode_t const* node)
{
	return UI_GetReferenceFloat(node, EXTRADATACONST(node).max);
}

float uiAbstractValueNode::getDelta (uiNode_t const* node)
{
	return UI_GetReferenceFloat(node, EXTRADATACONST(node).delta);
}

float uiAbstractValueNode::getValue (uiNode_t const* node)
{
	return UI_GetReferenceFloat(node, EXTRADATACONST(node).value);
}

/**
 * @brief Call to update a cloned node
 */
void uiAbstractValueNode::clone (const uiNode_t* source, uiNode_t* clone)
{
	uiLocatedNode::clone(source, clone);
	UI_CloneCvarOrFloat(source, clone, (const float*const*)&EXTRADATACONST(source).value, (float**)&EXTRADATA(clone).value);
	UI_CloneCvarOrFloat(source, clone, (const float*const*)&EXTRADATACONST(source).delta, (float**)&EXTRADATA(clone).delta);
	UI_CloneCvarOrFloat(source, clone, (const float*const*)&EXTRADATACONST(source).max, (float**)&EXTRADATA(clone).max);
	UI_CloneCvarOrFloat(source, clone, (const float*const*)&EXTRADATACONST(source).min, (float**)&EXTRADATA(clone).min);
}

void UI_RegisterAbstractValueNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "abstractvalue";
	behaviour->manager = UINodePtr(new uiAbstractValueNode());
	behaviour->isAbstract = true;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Current value of the node. It should be a cvar */
	UI_RegisterExtradataNodeProperty(behaviour, "current", V_CVAR_OR_FLOAT, abstractValueExtraData_t, value);
	/* Value of a positive step. Must be bigger than 1. */
	UI_RegisterExtradataNodeProperty(behaviour, "delta", V_CVAR_OR_FLOAT, abstractValueExtraData_t, delta);
	/* Maximum value we can set to the node. It can be a cvar. Default value is 0. */
	UI_RegisterExtradataNodeProperty(behaviour, "max", V_CVAR_OR_FLOAT, abstractValueExtraData_t, max);
	/* Minimum value we can set to the node. It can be a cvar. Default value is 1. */
	UI_RegisterExtradataNodeProperty(behaviour, "min", V_CVAR_OR_FLOAT, abstractValueExtraData_t, min);
	/* Defines a factor that is applied to the delta value when the shift key is held down. */
	UI_RegisterExtradataNodeProperty(behaviour, "shiftincreasefactor", V_FLOAT, abstractValueExtraData_t, shiftIncreaseFactor);

	/* Callback value set when before calling onChange. It is used to know the change apply by the user
	 * @Deprecated
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "lastdiff", V_FLOAT, abstractValueExtraData_t, lastdiff);

}
