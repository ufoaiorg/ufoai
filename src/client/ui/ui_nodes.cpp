/**
 * @file
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

#include "ui_main.h"
#include "ui_internal.h"
#include "ui_node.h"
#include "ui_nodes.h"
#include "ui_parse.h"
#include "ui_input.h"

#include "node/ui_node_abstractnode.h"
#include "node/ui_node_abstractscrollbar.h"
#include "node/ui_node_abstractoption.h"
#include "node/ui_node_abstractvalue.h"
#include "node/ui_node_bar.h"
#include "node/ui_node_base.h"
#include "node/ui_node_baseinventory.h"
#include "node/ui_node_battlescape.h"
#include "node/ui_node_button.h"
#include "node/ui_node_checkbox.h"
#include "node/ui_node_controls.h"
#include "node/ui_node_container.h"
#include "node/ui_node_data.h"
#include "node/ui_node_editor.h"
#include "node/ui_node_ekg.h"
#include "node/ui_node_geoscape.h"
#include "node/ui_node_image.h"
#include "node/ui_node_item.h"
#include "node/ui_node_linechart.h"
#include "node/ui_node_material_editor.h"
#include "node/ui_node_messagelist.h"
#include "node/ui_node_model.h"
#include "node/ui_node_option.h"
#include "node/ui_node_optionlist.h"
#include "node/ui_node_optiontree.h"
#include "node/ui_node_panel.h"
#include "node/ui_node_radar.h"
#include "node/ui_node_radiobutton.h"
#include "node/ui_node_rows.h"
#include "node/ui_node_selectbox.h"
#include "node/ui_node_sequence.h"
#include "node/ui_node_string.h"
#include "node/ui_node_special.h"
#include "node/ui_node_spinner.h"
#include "node/ui_node_tab.h"
#include "node/ui_node_tbar.h"
#include "node/ui_node_text.h"
#include "node/ui_node_text2.h"
#include "node/ui_node_textlist.h"
#include "node/ui_node_textentry.h"
#include "node/ui_node_texture.h"
#include "node/ui_node_timer.h"
#include "node/ui_node_todo.h"
#include "node/ui_node_video.h"
#include "node/ui_node_vscrollbar.h"
#include "node/ui_node_zone.h"

typedef void (*registerFunction_t)(uiBehaviour_t* node);

/**
 * @brief List of functions to register nodes
 * @note Functions must be sorted by node name
 */
static const registerFunction_t registerFunctions[] = {
	UI_RegisterNullNode,
	UI_RegisterAbstractBaseNode,
	UI_RegisterAbstractNode,
	UI_RegisterAbstractOptionNode,
	UI_RegisterAbstractScrollableNode,
	UI_RegisterAbstractScrollbarNode,
	UI_RegisterAbstractValueNode,
	UI_RegisterBarNode,
	UI_RegisterBaseInventoryNode,
	UI_RegisterBaseLayoutNode,
	UI_RegisterBaseMapNode,
	UI_RegisterBattlescapeNode,
	UI_RegisterButtonNode,
	UI_RegisterCheckBoxNode,
	UI_RegisterConFuncNode,
	UI_RegisterContainerNode,
	UI_RegisterControlsNode,
	UI_RegisterCvarFuncNode,
	UI_RegisterDataNode,
	UI_RegisterEditorNode,
	UI_RegisterEKGNode,
	UI_RegisterFuncNode,
	UI_RegisterGeoscapeNode,
	UI_RegisterImageNode,
	UI_RegisterItemNode,
	UI_RegisterLineChartNode,
	UI_RegisterMaterialEditorNode,
	UI_RegisterMessageListNode,
	UI_RegisterModelNode,
	UI_RegisterOptionNode,
	UI_RegisterOptionListNode,
	UI_RegisterOptionTreeNode,
	UI_RegisterPanelNode,
	UI_RegisterRadarNode,
	UI_RegisterRadioButtonNode,
	UI_RegisterRowsNode,
	UI_RegisterSelectBoxNode,
	UI_RegisterSequenceNode,
	UI_RegisterSpinnerNode,
	UI_RegisterStringNode,
	UI_RegisterTabNode,
	UI_RegisterTBarNode,
	UI_RegisterTextNode,
	UI_RegisterText2Node,
	UI_RegisterTextEntryNode,
	UI_RegisterTextListNode,
	UI_RegisterTextureNode,
	UI_RegisterTimerNode,
	UI_RegisterTodoNode,
	UI_RegisterVideoNode,
	UI_RegisterVScrollbarNode,
	UI_RegisterWindowNode,
	UI_RegisterZoneNode
};
#define NUMBER_OF_BEHAVIOURS lengthof(registerFunctions)

/**
 * @brief List of all node behaviours, indexes by nodetype num.
 */
static uiBehaviour_t nodeBehaviourList[NUMBER_OF_BEHAVIOURS];

/**
 * @brief Check the if conditions for a given node
 * @sa V_UI_IF
 * @returns false if the node is not drawn due to not meet if conditions
 */
bool UI_CheckVisibility (uiNode_t* node)
{
	uiCallContext_t context;
	if (!node->visibilityCondition)
		return true;
	context.source = node;
	context.useCmdParam = false;
	return UI_GetBooleanFromExpression(node->visibilityCondition, &context);
}

/**
 * @brief Return a path from a window to a node
 * @return A path "windowname.nodename.nodename.givennodename"
 * @note Use a static buffer for the result
 */
const char* UI_GetPath (const uiNode_t* node)
{
	static char result[512];
	const uiNode_t* nodes[8];
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
		Q_strcat(result, sizeof(result), "%s", nodes[i]->name);
		if (i > 0)
			Q_strcat(result, sizeof(result), ".");
	}

	return result;
}

/**
 * @brief Read a path and return every we can use (node and property)
 * @details The path token must be a window name, and then node child.
 * Reserved token 'root' and 'parent' can be used to navigate.
 * If relativeNode is set, the path can start with reserved token
 * 'this', 'root', 'parent' and 'child' (relative to this node).
 * The function can return a node property by using a '\@',
 * the path 'foo\@pos' will return the window foo and the property 'pos'
 * from the 'window' behaviour.
 * @param[in] path Path to read. Contain a node location with dot seprator and a facultative property
 * @param[in] relativeNode relative node where the path start. It allow to use facultative command to start the path (this, parent, root).
 * @param[in] iterationNode relative node referencing child in 'forchildin' iteration, mapped to '*node:child', can be nullptr
 * @param[out] resultNode Node found. Else nullptr.
 * @param[out] resultProperty Property found. Else nullptr.
 * TODO Speed up, evilly used function, use strncmp instead of using buffer copy (name[MAX_VAR])
 */
void UI_ReadNodePath (const char* path, const uiNode_t* relativeNode, const uiNode_t* iterationNode, uiNode_t** resultNode, const value_t** resultProperty)
{
	char name[MAX_VAR];
	uiNode_t* node = nullptr;
	const char* nextName;
	char nextCommand = '^';

	*resultNode = nullptr;
	if (resultProperty)
		*resultProperty = nullptr;

	nextName = path;
	while (nextName && nextName[0] != '\0') {
		const char* begin = nextName;
		char command = nextCommand;
		nextName = strpbrk(begin, ".@#");
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
			if (Q_streq(name, "this")) {
				if (relativeNode == nullptr)
					return;
				node = const_cast<uiNode_t *>(relativeNode);
			} else if (Q_streq(name, "parent")) {
				if (relativeNode == nullptr)
					return;
				node = relativeNode->parent;
			} else if (Q_streq(name, "root")) {
				if (relativeNode == nullptr)
					return;
				node = relativeNode->root;
			} else if (Q_streq(name, "child")) {
				if (iterationNode == nullptr)
					return;
				node = const_cast<uiNode_t *>(iterationNode);
			} else
				node = UI_GetWindow(name);
			break;
		case '.':	/* child node */
			if (Q_streq(name, "parent"))
				node = node->parent;
			else if (Q_streq(name, "root"))
				node = node->root;
			else
				node = UI_GetNode(node, name);
			break;
		case '#':	/* window index */
			/** @todo FIXME use a warning instead of an assert */
			assert(UI_Node_IsWindow(node));
			node = UI_WindowNodeGetIndexedChild(node, name);
			break;
		case '@':	/* property */
			assert(nextCommand == '\0');
			*resultProperty = UI_GetPropertyFromBehaviour(node->behaviour, name);
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
 * It is a simplification facade over UI_ReadNodePath
 * @return The requested node, else nullptr if not found
 * @code
 * // get keylist node from options_keys node from options window
 * node = UI_GetNodeByPath("options.options_keys.keylist");
 * @sa UI_ReadNodePath
 * @endcode
 */
uiNode_t* UI_GetNodeByPath (const char* path)
{
	uiNode_t* node = nullptr;
	const value_t* property;
	UI_ReadNodePath(path, nullptr, nullptr, &node, &property);
	/** @todo FIXME warning if it return a property */
	return node;
}

/**
 * @brief Allocate a node into the UI memory (do not call behaviour->new)
 * @note It's not a dynamic memory allocation. Please only use it at the loading time
 * @todo Assert out when we are not in parsing/loading stage
 * @param[in] name Name of the new node, else nullptr if we don't want to edit it.
 * @param[in] type Name of the node behavior
 * @param[in] isDynamic Allocate a node in static or dynamic memory
 */
static uiNode_t* UI_AllocNodeWithoutNew (const char* name, const char* type, bool isDynamic)
{
	uiNode_t* node;
	uiBehaviour_t* behaviour;
	int nodeSize;

	behaviour = UI_GetNodeBehaviour(type);
	if (behaviour == nullptr)
		Com_Error(ERR_FATAL, "UI_AllocNodeWithoutNew: Node behaviour '%s' doesn't exist", type);

	nodeSize = sizeof(*node) + behaviour->extraDataSize;

	if (!isDynamic) {
		void* memory = UI_AllocHunkMemory(nodeSize, STRUCT_MEMORY_ALIGN, true);
		if (memory == nullptr)
			Com_Error(ERR_FATAL, "UI_AllocNodeWithoutNew: No more memory to allocate a new node - increase the cvar ui_hunksize");
		node = static_cast<uiNode_t*>(memory);
		ui_global.numNodes++;
	} else {
		node = static_cast<uiNode_t*>(Mem_PoolAlloc(nodeSize, ui_dynPool, 0));
		node->dynamic = true;
	}

	node->behaviour = behaviour;
#ifdef DEBUG
	UI_Node_DebugCountWidget(node, 1);
#endif
	if (UI_Node_IsAbstract(node))
		Com_Error(ERR_FATAL, "UI_AllocNodeWithoutNew: Node behavior '%s' is abstract. We can't instantiate it.", type);

	if (name != nullptr) {
		Q_strncpyz(node->name, name, sizeof(node->name));
		if (strlen(node->name) != strlen(name))
			Com_Printf("UI_AllocNodeWithoutNew: Node name \"%s\" truncated. New name is \"%s\"\n", name, node->name);
	}

	/* initialize default properties */
	UI_Node_Loading(node);

	return node;
}

/**
 * @brief Allocate a node into the UI memory
 * @note It's not a dynamic memory allocation. Please only use it at the loading time
 * @todo Assert out when we are not in parsing/loading stage
 * @param[in] name Name of the new node, else nullptr if we don't want to edit it.
 * @param[in] type Name of the node behavior
 * @param[in] isDynamic Allocate a node in static or dynamic memory
 */
uiNode_t* UI_AllocNode (const char* name, const char* type, bool isDynamic)
{
	uiNode_t* node = UI_AllocNodeWithoutNew(name, type, isDynamic);

	/* allocate memory */
	if (node->dynamic)
		UI_Node_NewNode(node);

	return node;
}

/**
 * @brief Return the first visible node at a position
 * @param[in] node Node where we must search
 * @param[in] rx Relative x position to the parent of the node
 * @param[in] ry Relative y position to the parent of the node
 * @return The first visible node at position, else nullptr
 */
static uiNode_t* UI_GetNodeInTreeAtPosition (uiNode_t* node, int rx, int ry)
{
	if (node->invis || UI_Node_IsVirtual(node) || !UI_CheckVisibility(node))
		return nullptr;

	/* relative to the node */
	rx -= node->box.pos[0];
	ry -= node->box.pos[1];

	/* check bounding box */
	if (rx < 0 || ry < 0 || rx >= node->box.size[0] || ry >= node->box.size[1])
		return nullptr;

	/** @todo we should improve the loop (last-to-first) */
	uiNode_t* find = nullptr;
	if (node->firstChild) {
		vec2_t clientPosition = {0, 0};

		if (UI_Node_IsScrollableContainer(node))
			UI_Node_GetClientPosition(node, clientPosition);

		rx -= clientPosition[0];
		ry -= clientPosition[1];

		for (uiNode_t* child = node->firstChild; child; child = child->next) {
			uiNode_t* tmp;
			tmp = UI_GetNodeInTreeAtPosition(child, rx, ry);
			if (tmp)
				find = tmp;
		}

		rx += clientPosition[0];
		ry += clientPosition[1];
	}
	if (find)
		return find;

	/* disable ghost/excluderect in debug mode 2 */
	if (UI_DebugMode() != 2) {
		/* is the node tangible */
		if (node->ghost)
			return nullptr;

		/* check excluded box */
		for (uiExcludeRect_t* excludeRect = node->firstExcludeRect; excludeRect != nullptr; excludeRect = excludeRect->next) {
			if (rx >= excludeRect->pos[0]
			 && rx < excludeRect->pos[0] + excludeRect->size[0]
			 && ry >= excludeRect->pos[1]
			 && ry < excludeRect->pos[1] + excludeRect->size[1])
				return nullptr;
		}
	}

	/* we are over the node */
	return node;
}

/**
 * @brief Return the first visible node at a position
 */
uiNode_t* UI_GetNodeAtPosition (int x, int y)
{
	/* find the first window under the mouse */
	for (int pos = ui_global.windowStackPos - 1; pos >= 0; pos--) {
		uiNode_t* window = ui_global.windowStack[pos];
		uiNode_t* find;

		/* update the layout */
		UI_Validate(window);

		find = UI_GetNodeInTreeAtPosition(window, x, y);
		if (find)
			return find;

		/* we must not search anymore */
		if (UI_WindowIsDropDown(window))
			break;
		if (UI_WindowIsModal(window))
			break;
		if (UI_WindowIsFullScreen(window))
			break;
	}

	return nullptr;
}

/**
 * @brief Return a node behaviour by name
 * @note Use a dichotomic search. nodeBehaviourList must be sorted by name.
 * @param[in] name Behaviour name requested
 * @return The bahaviour found, else nullptr
 */
uiBehaviour_t* UI_GetNodeBehaviour (const char* name)
{
	unsigned char min = 0;
	unsigned char max = NUMBER_OF_BEHAVIOURS;

	while (min != max) {
		const int mid = (min + max) >> 1;
		const int diff = strcmp(nodeBehaviourList[mid].name, name);
		assert(mid < max);
		assert(mid >= min);

		if (diff == 0)
			return &nodeBehaviourList[mid];

		if (diff > 0)
			max = mid;
		else
			min = mid + 1;
	}

	return nullptr;
}

uiBehaviour_t* UI_GetNodeBehaviourByIndex (int index)
{
	return &nodeBehaviourList[index];
}

int UI_GetNodeBehaviourCount (void)
{
	return NUMBER_OF_BEHAVIOURS;
}

/**
 * @brief Remove all child from a node (only remove dynamic memory allocation nodes)
 * @param node The node we want to clean
 */
void UI_DeleteAllChild (uiNode_t* node)
{
	uiNode_t* child;
	child = node->firstChild;
	while (child) {
		uiNode_t* next = child->next;
		UI_DeleteNode(child);
		child = next;
	}
}

static void UI_BeforeDeletingNode (const uiNode_t* node)
{
	if (UI_GetHoveredNode() == node) {
		UI_InvalidateMouse();
	}
}

/**
 * Delete the node and remove it from his parent
 * @param node The node we want to delete
 */
void UI_DeleteNode (uiNode_t* node)
{
	if (!node->dynamic) {
		Com_Printf("UI_DeleteNode: Can't delete the static node '%s'\n", UI_GetPath(node));
		return;
	}

	UI_BeforeDeletingNode(node);

	UI_DeleteAllChild(node);
	if (node->firstChild != nullptr) {
		Com_Printf("UI_DeleteNode: Node '%s' contain static nodes. We can't delete it.\n", UI_GetPath(node));
		return;
	}

	if (node->parent)
		UI_RemoveNode(node->parent, node);

	/* delete all allocated properties */
	for (uiBehaviour_t* behaviour = node->behaviour; behaviour; behaviour = behaviour->super) {
		const value_t** property = behaviour->localProperties;
		if (property == nullptr)
			continue;
		while (*property) {
			if (((*property)->type & V_UI_MASK) == V_UI_CVAR) {
				if (void*& mem = Com_GetValue<void*>(node, *property)) {
					UI_FreeStringProperty(mem);
					mem = 0;
				}
			}

			/** @todo We must delete all EA_LISTENER too */

			property++;
		}
	}

	UI_Node_DeleteNode(node);
}

/**
 * @brief Clone a node
 * @param[in] node Node to clone
 * @param[in] recursive True if we also must clone subnodes
 * @param[in] newWindow Window where the nodes must be add (this function only link node into window, not window into the new node)
 * @param[in] newName New node name, else nullptr to use the source name
 * @param[in] isDynamic Allocate a node in static or dynamic memory
 * @todo exclude rect is not safe cloned.
 * @todo actions are not cloned. It is be a problem if we use add/remove listener into a cloned node.
 */
uiNode_t* UI_CloneNode (const uiNode_t* node, uiNode_t* newWindow, bool recursive, const char* newName, bool isDynamic)
{
	uiNode_t* newNode = UI_AllocNodeWithoutNew(nullptr, UI_Node_GetWidgetName(node), isDynamic);

	/* clone all data */
	memcpy(newNode, node, UI_Node_GetMemorySize(node));
	newNode->dynamic = isDynamic;

	/* custom name */
	if (newName != nullptr) {
		Q_strncpyz(newNode->name, newName, sizeof(newNode->name));
		if (strlen(newNode->name) != strlen(newName))
			Com_Printf("UI_CloneNode: Node name \"%s\" truncated. New name is \"%s\"\n", newName, newNode->name);
	}

	/* clean up node navigation */
	if (node->root == node && newWindow == nullptr)
		newWindow = newNode;
	newNode->root = newWindow;
	newNode->parent = nullptr;
	newNode->firstChild = nullptr;
	newNode->lastChild = nullptr;
	newNode->next = nullptr;
	newNode->super = node;

	/* clone child */
	if (recursive) {
		for (uiNode_t* childNode = node->firstChild; childNode; childNode = childNode->next) {
			uiNode_t* newChildNode = UI_CloneNode(childNode, newWindow, recursive, nullptr, isDynamic);
			UI_AppendNode(newNode, newChildNode);
		}
	}

	/* allocate memories */
	if (newNode->dynamic)
		UI_Node_NewNode(newNode);

	UI_Node_Clone(node, newNode);

	return newNode;
}

void UI_InitNodes (void)
{
	int i = 0;
	uiBehaviour_t* current = nodeBehaviourList;

	/* compute list of node behaviours */
	for (i = 0; i < NUMBER_OF_BEHAVIOURS; i++) {
		OBJZERO(*current);
		current->registration = true;
		registerFunctions[i](current);
		current->registration = false;
		current++;
	}

	/* check for safe data: list must be sorted by alphabet */
	current = nodeBehaviourList;
	assert(current);
	for (i = 0; i < NUMBER_OF_BEHAVIOURS - 1; i++) {
		const uiBehaviour_t* a = current;
		const uiBehaviour_t* b = current + 1;
		assert(b);
		if (strcmp(a->name, b->name) >= 0) {
#ifdef DEBUG
			Com_Error(ERR_FATAL, "UI_InitNodes: '%s' is before '%s'. Please order node behaviour registrations by name", a->name, b->name);
#else
			Com_Error(ERR_FATAL, "UI_InitNodes: Error: '%s' is before '%s'", a->name, b->name);
#endif
		}
		current++;
	}

	/* finalize node behaviour initialization */
	current = nodeBehaviourList;
	for (i = 0; i < NUMBER_OF_BEHAVIOURS; i++) {
		UI_InitializeNodeBehaviour(current);
		current++;
	}
}

void uiBox_t::alignBox (uiBox_t& inner, align_t direction)
{
	switch (direction % 3) {
	case 0:	/* left */
		inner.pos[0] = this->pos[0];
		break;
	case 1:	/* middle */
		inner.pos[0] = this->pos[0] + (this->size[0] * 0.5) - (inner.size[0] * 0.5);
		break;
	case 2:	/* right */
		inner.pos[0] = this->pos[0] + this->size[0] - inner.size[0];
		break;
	}
	switch (direction / 3) {
	case 0:	/* top */
		inner.pos[1] = this->pos[1];
		break;
	case 1:	/* middle */
		inner.pos[1] = this->pos[1] + (this->size[1] * 0.5) - (inner.size[1] * 0.5);
		break;
	case 2:	/* bottom */
		inner.pos[1] = this->pos[1] + this->size[1] - inner.size[1];
		break;
	default:
		inner.pos[1] = this->pos[1];
		Com_Error(ERR_FATAL, "UI_ImageAlignBoxInBox: Align %d not supported\n", direction);
	}
}
