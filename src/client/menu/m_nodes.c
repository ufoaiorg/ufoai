/**
 * @file m_nodes.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "m_main.h"
#include "m_internal.h"
#include "m_nodes.h"
#include "m_parse.h"
#include "m_input.h"

#include "node/m_node_abstractnode.h"
#include "node/m_node_abstractscrollbar.h"
#include "node/m_node_abstractvalue.h"
#include "node/m_node_bar.h"
#include "node/m_node_base.h"
#include "node/m_node_button.h"
#include "node/m_node_checkbox.h"
#include "node/m_node_controls.h"
#include "node/m_node_cinematic.h"
#include "node/m_node_container.h"
#include "node/m_node_custombutton.h"
#include "node/m_node_editor.h"
#include "node/m_node_ekg.h"
#include "node/m_node_image.h"
#include "node/m_node_item.h"
#include "node/m_node_linechart.h"
#include "node/m_node_map.h"
#include "node/m_node_material_editor.h"
#include "node/m_node_messagelist.h"
#include "node/m_node_model.h"
#include "node/m_node_optionlist.h"
#include "node/m_node_optiontree.h"
#include "node/m_node_panel.h"
#include "node/m_node_radar.h"
#include "node/m_node_radiobutton.h"
#include "node/m_node_rows.h"
#include "node/m_node_selectbox.h"
#include "node/m_node_string.h"
#include "node/m_node_special.h"
#include "node/m_node_spinner.h"
#include "node/m_node_tab.h"
#include "node/m_node_tbar.h"
#include "node/m_node_text.h"
#include "node/m_node_textlist.h"
#include "node/m_node_textentry.h"
#include "node/m_node_keybinding.h"
#include "node/m_node_todo.h"
#include "node/m_node_vscrollbar.h"
#include "node/m_node_zone.h"

typedef void (*registerFunction_t)(nodeBehaviour_t *node);

/**
 * @brief List of functions to register nodes
 * @note Functions must be sorted by node name
 */
static const registerFunction_t registerFunctions[] = {
	MN_RegisterNullNode,
	MN_RegisterAbstractBaseNode,
	MN_RegisterAbstractNode,
	MN_RegisterAbstractOptionNode,
	MN_RegisterAbstractScrollableNode,
	MN_RegisterAbstractScrollbarNode,
	MN_RegisterAbstractValueNode,
	MN_RegisterBarNode,
	MN_RegisterBaseLayoutNode,
	MN_RegisterBaseMapNode,
	MN_RegisterButtonNode,
	MN_RegisterCheckBoxNode,
	MN_RegisterCinematicNode,
	MN_RegisterConFuncNode,
	MN_RegisterContainerNode,
	MN_RegisterControlsNode,
	MN_RegisterCustomButtonNode,
	MN_RegisterCvarFuncNode,
	MN_RegisterEditorNode,
	MN_RegisterEKGNode,
	MN_RegisterFuncNode,
	MN_RegisterItemNode,
	MN_RegisterKeyBindingNode,
	MN_RegisterLineChartNode,
	MN_RegisterMapNode,
	MN_RegisterMaterialEditorNode,
	MN_RegisterMessageListNode,
	MN_RegisterModelNode,
	MN_RegisterOptionListNode,
	MN_RegisterOptionTreeNode,
	MN_RegisterPanelNode,
	MN_RegisterImageNode,	/* pic */
	MN_RegisterRadarNode,
	MN_RegisterRadioButtonNode,
	MN_RegisterRowsNode,
	MN_RegisterSelectBoxNode,
	MN_RegisterSpecialNode,
	MN_RegisterSpinnerNode,
	MN_RegisterStringNode,
	MN_RegisterTabNode,
	MN_RegisterTBarNode,
	MN_RegisterTextNode,
	MN_RegisterTextEntryNode,
	MN_RegisterTextListNode,
	MN_RegisterTodoNode,
	MN_RegisterVScrollbarNode,
	MN_RegisterWindowNode,
	MN_RegisterZoneNode
};
#define NUMBER_OF_BEHAVIOURS lengthof(registerFunctions)

/**
 * @brief List of all node behaviours, indexes by nodetype num.
 */
static nodeBehaviour_t nodeBehaviourList[NUMBER_OF_BEHAVIOURS];

/**
 * @brief Get a property from a behaviour or his inheritance
 * @param[in] behaviour Context behaviour
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else NULL
 */
const value_t *MN_GetPropertyFromBehaviour (const nodeBehaviour_t *behaviour, const char* name)
{
	for (; behaviour; behaviour = behaviour->super) {
		const value_t *result;
		if (behaviour->properties == NULL)
			continue;
		result = MN_FindPropertyByName(behaviour->properties, name);
		if (result)
			return result;
	}
	return NULL;
}

/**
 * @brief Check the if conditions for a given node
 * @sa V_UI_IF
 * @returns qfalse if the node is not drawn due to not meet if conditions
 */
qboolean MN_CheckVisibility (menuNode_t *node)
{
	menuCallContext_t context;
	if (!node->visibilityCondition)
		return qtrue;
	context.source = node;
	context.useCmdParam = qfalse;
	return MN_GetBooleanFromExpression(node->visibilityCondition, &context);
}

/**
 * @brief Return a path from a menu to a node
 * @return A path "menuname.nodename.nodename.givennodename"
 * @note Use a static buffer for the result
 */
const char* MN_GetPath (const menuNode_t* node)
{
	static char result[MAX_VAR];
	const menuNode_t* nodes[8];
	int i = 0;

	while (node) {
		assert(i < 8);
		nodes[i] = node;
		node = node->parent;
		i++;
	}

	/** @todo we can use something faster than cat */
	result[0] = '\0';
	while (i) {
		i--;
		Q_strcat(result, nodes[i]->name, sizeof(result));
		if (i > 0)
			Q_strcat(result, ".", sizeof(result));
	}

	return result;
}

/**
 * @brief Read a path and return every we can use (node and property)
 * @details The path token must be a menu name, and then node child.
 * Reserved token 'menu' and 'parent' can be used to navigate.
 * If relativeNode is set, the path can start with reserved token
 * 'this', 'menu' and 'parent' (relative to this node).
 * The function can return a node property by using a '\@',
 * the path 'foo\@pos' will return the menu foo and the property 'pos'
 * from the 'window' behaviour.
 * @param[in] path Path to read. Contain a node location with dot seprator and a facultative property
 * @param[in] relativeNode relative node where the path start. It allow to use facultative command to start the path (this, parent, menu).
 * @param[out] resultNode Node found. Else NULL.
 * @param[out] resultProperty Property found. Else NULL.
 */
void MN_ReadNodePath (const char* path, const menuNode_t *relativeNode, menuNode_t **resultNode, const value_t **resultProperty)
{
	char name[MAX_VAR];
	menuNode_t* node = NULL;
	const char* nextName;
	char nextCommand = '^';

	*resultNode = NULL;
	if (resultProperty)
		*resultProperty = NULL;

	nextName = path;
	while (nextName && nextName[0] != '\0') {
		const char* begin = nextName;
		char command = nextCommand;
		nextName = strpbrk(begin, ".@");
		if (!nextName) {
			Q_strncpyz(name, begin, sizeof(name));
			nextCommand = '\0';
		} else {
			assert(nextName - begin + 1 <= sizeof(name));
			Q_strncpyz(name, begin, nextName - begin + 1);
			nextCommand = *nextName;
			nextName++;
		}

		switch (command) {
		case '^':	/* first string */
			if (!strcmp(name, "this")) {
				if (relativeNode == NULL)
					return;
				/** @todo find a way to fix the bad cast. only here to remove "discards qualifiers" warning */
				node = *(menuNode_t**) ((void*)&relativeNode);
			} else if (!strcmp(name, "parent")) {
				if (relativeNode == NULL)
					return;
				node = relativeNode->parent;
			} else if (!strcmp(name, "root")) {
				if (relativeNode == NULL)
					return;
				node = relativeNode->root;
			} else
				node = MN_GetWindow(name);
			break;
		case '.':	/* child node */
			if (!strcmp(name, "parent"))
				node = node->parent;
			else if (!strcmp(name, "root"))
				node = node->root;
			else
				node = MN_GetNode(node, name);
			break;
		case '@':	/* property */
			assert(nextCommand == '\0');
			*resultProperty = MN_GetPropertyFromBehaviour(node->behaviour, name);
			*resultNode = node;
			return;
		}

		if (!node)
			return;
	}

	*resultNode = node;
	return;
}

/**
 * @brief Return a node by a path name (names with dot separation)
 * @return The requested node, else NULL if not found
 * @code
 * // get keylist node from options_keys node from options menu
 * node = MN_GetNodeByPath("options.options_keys.keylist");
 * @endcode
 */
menuNode_t* MN_GetNodeByPath (const char* path)
{
	char name[MAX_VAR];
	menuNode_t* node = NULL;
	const char* nextName;

	nextName = path;
	while (nextName && nextName[0] != '\0') {
		const char* begin = nextName;
		nextName = strstr(begin, ".");
		if (!nextName) {
			Q_strncpyz(name, begin, sizeof(name));
		} else {
			assert(nextName - begin + 1 <= sizeof(name));
			Q_strncpyz(name, begin, nextName - begin + 1);
			nextName++;
		}

		if (node == NULL)
			node = MN_GetWindow(name);
		else
			node = MN_GetNode(node, name);

		if (!node)
			return NULL;
	}

	return node;
}

/**
 * @brief Allocate a node into the UI memory (do notr call behaviour->new)
 * @note It's not a dynamic memory allocation. Please only use it at the loading time
 * @todo Assert out when we are not in parsing/loading stage
 * @param[in] name Name of the new node, else NULL if we dont want to edit it.
 * @param[in] type Name of the node behavior
 * @param[in] isDynamic Allocate a node in static or dynamic memory
 */
static menuNode_t* MN_AllocNodeWithoutNew (const char* name, const char* type, qboolean isDynamic)
{
	menuNode_t* node;

	if (!isDynamic) {
		if (mn.numNodes >= MAX_MENUNODES)
			Com_Error(ERR_FATAL, "MN_AllocStaticNode: MAX_MENUNODES hit");
		node = &mn.nodes[mn.numNodes++];
		memset(node, 0, sizeof(*node));
	} else {
		node = (menuNode_t*)Mem_PoolAlloc(sizeof(*node), mn_dynPool, 0);
		memset(node, 0, sizeof(*node));
		node->dynamic = qtrue;
	}

	node->behaviour = MN_GetNodeBehaviour(type);
	if (node->behaviour == NULL)
		Com_Error(ERR_FATAL, "MN_AllocNode: Node behaviour '%s' doesn't exist", type);
#ifdef DEBUG
	node->behaviour->count++;
#endif
	if (node->behaviour->isAbstract)
		Com_Error(ERR_FATAL, "MN_AllocNode: Node behavior '%s' is abstract. We can't instantiate it.", type);

	if (name != NULL) {
		Q_strncpyz(node->name, name, sizeof(node->name));
		if (strlen(node->name) != strlen(name))
			Com_Printf("MN_AllocNode: Node name \"%s\" truncated. New name is \"%s\"\n", name, node->name);
	}

	/* initialize default properties */
	if (node->behaviour->loading)
		node->behaviour->loading(node);

	return node;
}

/**
 * @brief Allocate a node into the UI memory
 * @note It's not a dynamic memory allocation. Please only use it at the loading time
 * @todo Assert out when we are not in parsing/loading stage
 * @param[in] name Name of the new node, else NULL if we dont want to edit it.
 * @param[in] type Name of the node behavior
 * @param[in] isDynamic Allocate a node in static or dynamic memory
 */
menuNode_t* MN_AllocNode (const char* name, const char* type, qboolean isDynamic)
{
	menuNode_t* node = MN_AllocNodeWithoutNew (name, type, isDynamic);

	/* allocate memories */
	if (node->dynamic && node->behaviour->new)
		node->behaviour->new(node);

	return node;
}

/**
 * @brief Return the first visible node at a position
 * @param[in] node Node where we must search
 * @param[in] rx Relative x position to the parent of the node
 * @param[in] ry Relative y position to the parent of the node
 * @return The first visible node at position, else NULL
 */
static menuNode_t *MN_GetNodeInTreeAtPosition (menuNode_t *node, int rx, int ry)
{
	menuNode_t *find;
	int i;

	if (node->invis || node->behaviour->isVirtual || !MN_CheckVisibility(node))
		return NULL;

	/* relative to the node */
	rx -= node->pos[0];
	ry -= node->pos[1];

	/* check bounding box */
	if (rx < 0 || ry < 0 || rx >= node->size[0] || ry >= node->size[1])
		return NULL;

	/** @todo we should improve the loop (last-to-first) */
	find = NULL;
	if (node->firstChild) {
		menuNode_t *child;
		vec2_t clientPosition = {0, 0};

		if (node->behaviour->getClientPosition)
			node->behaviour->getClientPosition(node, clientPosition);

		rx -= clientPosition[0];
		ry -= clientPosition[1];

		for (child = node->firstChild; child; child = child->next) {
			menuNode_t *tmp;
			tmp = MN_GetNodeInTreeAtPosition(child, rx, ry);
			if (tmp)
				find = tmp;
		}

		rx += clientPosition[0];
		ry += clientPosition[1];
	}
	if (find)
		return find;

	/* disable ghost/excluderect in debug mode 2 */
	if (MN_DebugMode() != 2) {
		/* is the node tangible */
		if (node->ghost)
			return NULL;

		/* check excluded box */
		for (i = 0; i < node->excludeRectNum; i++) {
			if (rx >= node->excludeRect[i].pos[0]
			 && rx < node->excludeRect[i].pos[0] + node->excludeRect[i].size[0]
			 && ry >= node->excludeRect[i].pos[1]
			 && ry < node->excludeRect[i].pos[1] + node->excludeRect[i].size[1])
				return NULL;
		}
	}

	/* we are over the node */
	return node;
}

/**
 * @brief Return the first visible node at a position
 */
menuNode_t *MN_GetNodeAtPosition (int x, int y)
{
	int pos;

	/* find the first menu under the mouse */
	for (pos = mn.windowStackPos - 1; pos >= 0; pos--) {
		menuNode_t *menu = mn.windowStack[pos];
		menuNode_t *find;

		/* update the layout */
		menu->behaviour->doLayout(menu);

		find = MN_GetNodeInTreeAtPosition(menu, x, y);
		if (find)
			return find;

		/* we must not search anymore */
		if (menu->u.window.dropdown)
			break;
		if (menu->u.window.modal)
			break;
		if (MN_WindowIsFullScreen(menu))
			break;
	}

	return NULL;
}

/**
 * @brief Return a node behaviour by name
 * @note Use a dichotomic search. nodeBehaviourList must be sorted by name.
 * @param[in] name Behaviour name requested
 * @return The bahaviour found, else NULL
 */
nodeBehaviour_t* MN_GetNodeBehaviour (const char* name)
{
	unsigned char min = 0;
	unsigned char max = NUMBER_OF_BEHAVIOURS;

	while (min != max) {
		const int mid = (min + max) >> 1;
		const char diff = strcmp(nodeBehaviourList[mid].name, name);
		assert(mid < max);
		assert(mid >= min);

		if (diff == 0)
			return &nodeBehaviourList[mid];

		if (diff > 0)
			max = mid;
		else
			min = mid + 1;
	}

	return NULL;
}

nodeBehaviour_t* MN_GetNodeBehaviourByIndex(int index)
{
	return &nodeBehaviourList[index];
}

int MN_GetNodeBehaviourCount(void)
{
	return NUMBER_OF_BEHAVIOURS;
}

/**
 * @brief Remove all child from a node (only remove dynamic memory allocation nodes)
 * @param node The node we want to clean
 */
void MN_DeleteAllChild (menuNode_t* node)
{
	menuNode_t *child;
	child = node->firstChild;
	while (child) {
		menuNode_t *next = child->next;
		MN_DeleteNode(child);
		child = next;
	}
}

/**
 * Delete the node and remove it from his parent
 * @param node The node we want to delete
 */
void MN_DeleteNode (menuNode_t* node)
{
	nodeBehaviour_t *behaviour;

	if (!node->dynamic)
		return;

	MN_DeleteAllChild(node);
	if (node->firstChild != NULL) {
		Com_Printf("MN_DeleteNode: Node '%s' contain static nodes. We can't delete it.", MN_GetPath(node));
		return;
	}

	if (node->parent)
		MN_RemoveNode(node->parent, node);

	/* delete all allocated properties */
	for (behaviour = node->behaviour; behaviour; behaviour = behaviour->super) {
		const value_t *property = behaviour->properties;
		if (property == NULL)
			continue;
		while (property->string != NULL) {
			if ((property->type & V_UI_MASK) == V_UI_CVAR) {
				void *mem = ((byte *) node + property->ofs);
				if (*(void**)mem != NULL) {
					MN_FreeStringProperty(*(void**)mem);
					*(void**)mem = NULL;
				}
			}

			/** @todo We must delete all EA_LISTENER too */

			property++;
		}
	}

	if (node->behaviour->delete)
		node->behaviour->delete(node);
}

/**
 * @brief Clone a node
 * @param[in] node Node to clone
 * @param[in] recursive True if we also must clone subnodes
 * @param[in] newMenu Menu where the nodes must be add (this function only link node into menu, note menu into the new node)
 * @param[in] newName New node name, else NULL to use the source name
 * @param[in] isDynamic Allocate a node in static or dynamic memory
 * @todo exclude rect is not safe cloned.
 * @todo actions are not cloned. It is be a problem if we use add/remove listener into a cloned node.
 */
menuNode_t* MN_CloneNode (const menuNode_t* node, menuNode_t *newMenu, qboolean recursive, const char *newName, qboolean isDynamic)
{
	menuNode_t* newNode = MN_AllocNodeWithoutNew(NULL, node->behaviour->name, isDynamic);

	/* clone all data */
	*newNode = *node;
	newNode->dynamic = isDynamic;

	/* custom name */
	if (newName != NULL) {
		Q_strncpyz(newNode->name, newName, sizeof(newNode->name));
		if (strlen(newNode->name) != strlen(newName))
			Com_Printf("MN_CloneNode: Node name \"%s\" truncated. New name is \"%s\"\n", newName, newNode->name);
	}

	/* clean up node navigation */
	if (node->root == node && newMenu == NULL)
		newMenu = newNode;
	newNode->root = newMenu;
	newNode->parent = NULL;
	newNode->firstChild = NULL;
	newNode->lastChild = NULL;
	newNode->next = NULL;
	newNode->super = *(menuNode_t**) ((void*)&node);

	/* clone child */
	if (recursive) {
		menuNode_t* childNode;
		for (childNode = node->firstChild; childNode; childNode = childNode->next) {
			menuNode_t* newChildNode = MN_CloneNode(childNode, newMenu, recursive, NULL, isDynamic);
			MN_AppendNode(newNode, newChildNode);
		}
	}

	/* allocate memories */
	if (newNode->dynamic && newNode->behaviour->new)
		newNode->behaviour->new(newNode);

	newNode->behaviour->clone(node, newNode);

	return newNode;
}

/** @brief position of virtual function into node behaviour */
static const int virtualFunctions[] = {
	offsetof(nodeBehaviour_t, draw),
	offsetof(nodeBehaviour_t, drawTooltip),
	offsetof(nodeBehaviour_t, leftClick),
	offsetof(nodeBehaviour_t, rightClick),
	offsetof(nodeBehaviour_t, middleClick),
	offsetof(nodeBehaviour_t, mouseWheel),
	offsetof(nodeBehaviour_t, mouseMove),
	offsetof(nodeBehaviour_t, mouseDown),
	offsetof(nodeBehaviour_t, mouseUp),
	offsetof(nodeBehaviour_t, capturedMouseMove),
	offsetof(nodeBehaviour_t, loading),
	offsetof(nodeBehaviour_t, loaded),
	offsetof(nodeBehaviour_t, init),
	offsetof(nodeBehaviour_t, close),
	offsetof(nodeBehaviour_t, clone),
	offsetof(nodeBehaviour_t, new),
	offsetof(nodeBehaviour_t, delete),
	offsetof(nodeBehaviour_t, activate),
	offsetof(nodeBehaviour_t, doLayout),
	offsetof(nodeBehaviour_t, dndEnter),
	offsetof(nodeBehaviour_t, dndMove),
	offsetof(nodeBehaviour_t, dndLeave),
	offsetof(nodeBehaviour_t, dndDrop),
	offsetof(nodeBehaviour_t, dndFinished),
	offsetof(nodeBehaviour_t, focusGained),
	offsetof(nodeBehaviour_t, focusLost),
	offsetof(nodeBehaviour_t, extraDataSize),
	offsetof(nodeBehaviour_t, sizeChanged),
	offsetof(nodeBehaviour_t, propertyChanged),
	offsetof(nodeBehaviour_t, getClientPosition),
	-1
};

/**
 * Initializes the inheritance (every node extends the abstract node)
 * @param behaviour The behaviour to initialize
 */
static void MN_InitializeNodeBehaviour (nodeBehaviour_t* behaviour)
{
	if (behaviour->isInitialized)
		return;

	/** @todo check (when its possible) properties are ordered by name */
	/* check and update properties data */
	if (behaviour->properties) {
		int num = 0;
		const value_t* current = behaviour->properties;
		while (current->string != NULL) {
			num++;
			current++;
		}
		behaviour->propertyCount = num;
	}

	/* everything inherits 'abstractnode' */
	if (behaviour->extends == NULL && strcmp(behaviour->name, "abstractnode") != 0) {
		behaviour->extends = "abstractnode";
	}

	if (behaviour->extends) {
		int i = 0;
		behaviour->super = MN_GetNodeBehaviour(behaviour->extends);
		MN_InitializeNodeBehaviour(behaviour->super);

		while (qtrue) {
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
	if (behaviour->super && behaviour->properties) {
		const value_t* current = behaviour->properties;
		while (current->string != NULL) {
			const value_t *p = MN_GetPropertyFromBehaviour(behaviour->super, current->string);
#if 0	/**< @todo not possible at the moment, not sure its the right way */
			const nodeBehaviour_t *b = MN_GetNodeBehaviour(current->string);
#endif
			if (p != NULL)
				Com_Error(ERR_FATAL, "MN_InitializeNodeBehaviour: property '%s' from node behaviour '%s' overwrite another property", current->string, behaviour->name);
#if 0	/**< @todo not possible at the moment, not sure its the right way */
			if (b != NULL)
				Com_Error(ERR_FATAL, "MN_InitializeNodeBehaviour: property '%s' from node behaviour '%s' use the name of an existing node behaviour", current->string, behaviour->name);
#endif
			current++;
		}
	}

	behaviour->isInitialized = qtrue;
}

void MN_InitNodes (void)
{
	int i = 0;
	nodeBehaviour_t *current = nodeBehaviourList;

	/* compute list of node behaviours */
	for (i = 0; i < NUMBER_OF_BEHAVIOURS; i++) {
		registerFunctions[i](current);
		current++;
	}

	/* check for safe data: list must be sorted by alphabet */
	current = nodeBehaviourList;
	assert(current);
	for (i = 0; i < NUMBER_OF_BEHAVIOURS - 1; i++) {
		const nodeBehaviour_t *a = current;
		const nodeBehaviour_t *b = current + 1;
		assert(b);
		if (strcmp(a->name, b->name) >= 0) {
#ifdef DEBUG
			Com_Error(ERR_FATAL, "MN_InitNodes: '%s' is before '%s'. Please order node behaviour registrations by name", a->name, b->name);
#else
			Com_Error(ERR_FATAL, "MN_InitNodes: Error: '%s' is before '%s'", a->name, b->name);
#endif
		}
		current++;
	}

	/* finalize node behaviour initialization */
	current = nodeBehaviourList;
	for (i = 0; i < NUMBER_OF_BEHAVIOURS; i++) {
		MN_InitializeNodeBehaviour(current);
		current++;
	}
}
