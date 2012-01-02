/**
 * @file ui_behaviour.c
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

#include "ui_main.h"
#include "ui_internal.h"
#include "ui_behaviour.h"
#include "ui_parse.h"

/**
 * FIXME rework that bad structure, move it to hunk memeory
 */
#define LOCAL_PROPERTY_SIZE	128
#define GLOBAL_PROPERTY_SIZE	512
static value_t properties[GLOBAL_PROPERTY_SIZE];
static int propertyCount;

/**
 * @brief Register a property to a behaviour.
 * It should not be used in the code
 * @param behaviour Target behaviour
 * @param name Name of the property
 * @param type Type of the property
 * @param pos position of the attribute (which store property memory) into the node structure
 * @param size size of the attribute (which store property memory) into the node structure
 * @see UI_RegisterNodeProperty
 * @see UI_RegisterExtradataNodeProperty
 * @return A link to the node property
 */
const struct value_s *UI_RegisterNodePropertyPosSize_ (struct uiBehaviour_s *behaviour, const char* name, int type, int pos, int size)
{
	if (propertyCount >= GLOBAL_PROPERTY_SIZE) {
		Com_Error(ERR_FATAL, "UI_RegisterNodePropertyPosSize_: Global property memory is full.");
	}
	value_t *property = &properties[propertyCount++];

	if (type == V_STRING || type == V_LONGSTRING || type == V_CVAR_OR_LONGSTRING || V_CVAR_OR_STRING) {
		size = 0;
	}

	property->string = name;
	property->type = type;
	property->ofs = pos;
	property->size = size;

	if (behaviour->localProperties == NULL) {
		/** FIXME should be into hunk memory, and unlimited */
		behaviour->localProperties = (const value_t **)Mem_PoolAlloc(sizeof(*behaviour->localProperties) * LOCAL_PROPERTY_SIZE, ui_sysPool, 0);
	}
	if (behaviour->propertyCount >= LOCAL_PROPERTY_SIZE-1) {
		Com_Error(ERR_FATAL, "UI_RegisterNodePropertyPosSize_: Property memory of behaviour %s is full.", behaviour->name);
	}
	behaviour->localProperties[behaviour->propertyCount++] = property;
	behaviour->localProperties[behaviour->propertyCount] = NULL;

	return property;
}

/**
 * @brief Register a node method to a behaviour.
 * @param behaviour Target behaviour
 * @param name Name of the property
 * @param function function to execute the node method
 * @return A link to the node property
 */
const struct value_s *UI_RegisterNodeMethod (struct uiBehaviour_s *behaviour, const char* name, uiNodeMethod_t function)
{
	return UI_RegisterNodePropertyPosSize_(behaviour, name, V_UI_NODEMETHOD, (size_t)function, 0);
}

/**
 * @brief Get a property from a behaviour or his inheritance
 * @param[in] behaviour Context behaviour
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else NULL
 */
const value_t *UI_GetPropertyFromBehaviour (const uiBehaviour_t *behaviour, const char* name)
{
	for (; behaviour; behaviour = behaviour->super) {
		const value_t **current;
		if (behaviour->localProperties == NULL)
			continue;
		current = behaviour->localProperties;
		while (*current) {
			if (!Q_strcasecmp(name, (*current)->string))
				return *current;
			current++;
		}
	}
	return NULL;
}


/** @brief position of virtual function into node behaviour */
static const int virtualFunctions[] = {
	offsetof(uiBehaviour_t, draw),
	offsetof(uiBehaviour_t, drawTooltip),
	offsetof(uiBehaviour_t, leftClick),
	offsetof(uiBehaviour_t, rightClick),
	offsetof(uiBehaviour_t, middleClick),
	offsetof(uiBehaviour_t, scroll),
	offsetof(uiBehaviour_t, mouseMove),
	offsetof(uiBehaviour_t, mouseDown),
	offsetof(uiBehaviour_t, mouseUp),
	offsetof(uiBehaviour_t, capturedMouseMove),
	offsetof(uiBehaviour_t, capturedMouseLost),
	offsetof(uiBehaviour_t, loading),
	offsetof(uiBehaviour_t, loaded),
	offsetof(uiBehaviour_t, windowOpened),
	offsetof(uiBehaviour_t, windowClosed),
	offsetof(uiBehaviour_t, clone),
	offsetof(uiBehaviour_t, newNode),
	offsetof(uiBehaviour_t, deleteNode),
	offsetof(uiBehaviour_t, activate),
	offsetof(uiBehaviour_t, doLayout),
	offsetof(uiBehaviour_t, dndEnter),
	offsetof(uiBehaviour_t, dndMove),
	offsetof(uiBehaviour_t, dndLeave),
	offsetof(uiBehaviour_t, dndDrop),
	offsetof(uiBehaviour_t, dndFinished),
	offsetof(uiBehaviour_t, focusGained),
	offsetof(uiBehaviour_t, focusLost),
	offsetof(uiBehaviour_t, extraDataSize),
	offsetof(uiBehaviour_t, sizeChanged),
	offsetof(uiBehaviour_t, propertyChanged),
	offsetof(uiBehaviour_t, getClientPosition),
	-1
};

/**
 * @brief Initialize a node behaviour memory, after registration, and before unsing it.
 * @param behaviour Behaviour to initialize
 */
void UI_InitializeNodeBehaviour (uiBehaviour_t* behaviour)
{
	if (behaviour->isInitialized)
		return;

	/* everything inherits 'abstractnode' */
	if (behaviour->extends == NULL && !Q_streq(behaviour->name, "abstractnode")) {
		behaviour->extends = "abstractnode";
	}

	if (behaviour->extends) {
		int i = 0;

		/** TODO Find a way to remove that, if possible */
		behaviour->super = UI_GetNodeBehaviour(behaviour->extends);
		UI_InitializeNodeBehaviour(behaviour->super);

		for (;;) {
			const size_t pos = virtualFunctions[i];
			uintptr_t superFunc;
			uintptr_t func;
			if (pos == -1)
				break;

			/* cache super function if we don't overwrite it */
			superFunc = *(uintptr_t*)((byte*)behaviour->super + pos);
			func = *(uintptr_t*)((byte*)behaviour + pos);
			if (func == 0 && superFunc != 0)
				*(uintptr_t*)((byte*)behaviour + pos) = superFunc;

			i++;
		}
	}

	/* property must not overwrite another property */
	if (behaviour->super && behaviour->localProperties) {
		const value_t** property = behaviour->localProperties;
		while (*property) {
			const value_t *p = UI_GetPropertyFromBehaviour(behaviour->super, (*property)->string);
#if 0	/**< @todo not possible at the moment, not sure its the right way */
			const uiBehaviour_t *b = UI_GetNodeBehaviour(current->string);
#endif
			if (p != NULL)
				Com_Error(ERR_FATAL, "UI_InitializeNodeBehaviour: property '%s' from node behaviour '%s' overwrite another property", (*property)->string, behaviour->name);
#if 0	/**< @todo not possible at the moment, not sure its the right way */
			if (b != NULL)
				Com_Error(ERR_FATAL, "UI_InitializeNodeBehaviour: property '%s' from node behaviour '%s' use the name of an existing node behaviour", (*property)->string, behaviour->name);
#endif
			property++;
		}
	}

	/* Sanity: A property must not be outside the node memory */
	if (behaviour->localProperties) {
		const int size = sizeof(uiNode_t) + behaviour->extraDataSize;
		const value_t** property = behaviour->localProperties;
		while (*property) {
			if ((*property)->type != V_UI_NODEMETHOD && (*property)->ofs + (*property)->size > size)
				Com_Error(ERR_FATAL, "UI_InitializeNodeBehaviour: property '%s' from node behaviour '%s' is outside the node memory. The C code need a fix.", (*property)->string, behaviour->name);
			property++;
		}
	}

	behaviour->isInitialized = qtrue;
}
