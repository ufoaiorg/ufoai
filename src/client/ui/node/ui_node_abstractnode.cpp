/**
 * @file
 * @brief Every node extends this node
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

#include "ui_node_abstractnode.h"
#include "../ui_actions.h"
#include "../ui_tooltip.h"
#include "../ui_behaviour.h"
#include "../ui_components.h"
#include "../ui_parse.h"
#include "../ui_sound.h"

#ifdef DEBUG
/**
 * @brief display value of a node property from the command line
 */
static void UI_NodeGetProperty_f (void)
{
	uiNode_t* node;
	const value_t* property;
	const char* sValue;
	float fValue;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <nodepath@prop>\n", Cmd_Argv(0));
		return;
	}

	UI_ReadNodePath(Cmd_Argv(1), nullptr, nullptr, &node, &property);

	if (node == nullptr) {
		Com_Printf("UI_NodeGetProperty_f: Node from path '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}

	if (property == nullptr) {
		Com_Printf("UI_NodeGetProperty_f: Property from path '%s' doesn't exist\n", Cmd_Argv(1));
		return;
	}

	/* check string value */
	sValue = UI_GetStringFromNodeProperty(node, property);
	if (sValue) {
		Com_Printf("\"%s\" is \"%s\"\n", Cmd_Argv(1), sValue);
		return;
	}

	/* check float value */
	fValue = UI_GetFloatFromNodeProperty(node, property);
	Com_Printf("\"%s\" is \"%f\"\n", Cmd_Argv(1), fValue);
}

/**
 * @brief set a node property from the command line
 * @todo Unify path syntaxe to allow to create a common autocompletion
 */
static void UI_NodeSetProperty_f (void)
{
	uiNode_t* node;
	const value_t* property;

	if (Cmd_Argc() != 4) {
		Com_Printf("Usage: %s <nodepath> <prop> <value>\n", Cmd_Argv(0));
		return;
	}

	node = UI_GetNodeByPath(Cmd_Argv(1));
	if (!node) {
		Com_Printf("UI_NodeSetProperty_f: Node '%s' not found\n", Cmd_Argv(1));
		return;
	}

	property = UI_GetPropertyFromBehaviour(node->behaviour, Cmd_Argv(2));
	if (!property) {
		Com_Printf("Property '%s@%s' doesn't exist\n", UI_GetPath(node), Cmd_Argv(2));
		return;
	}

	UI_NodeSetProperty(node, property, Cmd_Argv(3));
}
#endif

/**
 * Mouse enter on the node (a child node is part of the node)
 */
void uiLocatedNode::onMouseEnter(uiNode_t* node)
{
	UI_ExecuteEventActions(node, node->onMouseEnter);
}

/**
 * Mouse leave the node (a child node is part of the node)
 */
void uiLocatedNode::onMouseLeave(uiNode_t* node)
{
	UI_ExecuteEventActions(node, node->onMouseLeave);
}

bool uiLocatedNode::onDndEnter (uiNode_t* node)
{
	return false;
}

bool uiLocatedNode::onDndMove (uiNode_t* node, int x, int y)
{
	return true;
}

void uiLocatedNode::onDndLeave (uiNode_t* node)
{
}

bool uiLocatedNode::onDndDrop (uiNode_t* node, int x, int y)
{
	return true;
}

bool uiLocatedNode::onDndFinished (uiNode_t* node, bool isDroped)
{
	return isDroped;
}

/**
 * @brief Activate the node. Can be used without the mouse (ie. a button will execute onClick)
 */
void uiNode::onActivate (uiNode_t* node)
{
	if (node->onClick)
		UI_ExecuteEventActions(node, node->onClick);
}

/**
 * @brief Call to update the node layout. This common code revalidates the node tree.
 */
void uiLocatedNode::doLayout (uiNode_t* node)
{
	if (!node->invalidated)
		return;

	for (uiNode_t* child = node->firstChild; child; child = child->next) {
		UI_Node_DoLayout(child);
	}

	node->invalidated = false;
}

void uiNode::onWindowOpened (uiNode_t* node, linkedList_t* params)
{
	for (uiNode_t* child = node->firstChild; child; child = child->next) {
		UI_Node_WindowOpened(child, nullptr);
	}
}

void uiNode::onWindowClosed (uiNode_t* node)
{
	for (uiNode_t* child = node->firstChild; child; child = child->next) {
		UI_Node_WindowClosed(child);
	}
}

void uiNode::onWindowActivate (uiNode_t* node)
{
	for (uiNode_t* child = node->firstChild; child; child = child->next) {
		UI_Node_WindowActivate(child);
	}
}

/**
 * @brief Callback stub
 */
void uiLocatedNode::onSizeChanged (uiNode_t* node)
{
	if (node->firstChild != nullptr)
		UI_Invalidate(node);
}

static void UI_AbstractNodeVisibilityChange (uiNode_t* node)
{
	if (node->parent != nullptr)
		UI_Invalidate(node->parent);
}

static const value_t* propertyWidth;
static const value_t* propertyHeight;
static const value_t* propertySize;
static const value_t* propertyInvis;

void uiNode::onPropertyChanged (uiNode_t* node, const value_t* property)
{
	if (property == propertyWidth || property == propertyHeight || property == propertySize) {
		UI_Node_SizeChanged(node);
	} else if (property == propertyInvis) {
		UI_AbstractNodeVisibilityChange(node);
	}
}

static void UI_AbstractNodeCallRemovaAllChild (uiNode_t* node, const uiCallContext_t* context)
{
	if (UI_GetParamNumber(context) != 0) {
		Com_Printf("UI_AbstractNodeCallRemovaAllChild: Invalid number of parameters\n");
		return;
	}
	UI_DeleteAllChild(node);
}

static void UI_AbstractNodeCallCreateChild (uiNode_t* node, const uiCallContext_t* context)
{
	uiNode_t* child;
	uiNode_t* component;
	const char* name;
	const char* type;

	if (UI_GetParamNumber(context) != 2) {
		Com_Printf("UI_AbstractNodeCallCreateChild: Invalid number of parameters\n");
		return;
	}

	name = UI_GetParam(context, 1);
	type = UI_GetParam(context, 2);

	uiNode_t* existingNode = UI_GetNode(node, name);
	if (existingNode != nullptr) {
		Com_Printf("UI_AbstractNodeCallCreateChild: Node with name '%s' already exists\n", name);
	}

	component = UI_GetComponent(type);
	if (component) {
		child = UI_CloneNode(component, node->root, true, name, true);
	} else {
		child = UI_AllocNode(name, type, true);
	}

	if (child == nullptr) {
		Com_Printf("UI_AbstractNodeCallCreateChild: Impossible to create the node\n");
		return;
	}

	UI_AppendNode(node, child);
}

static void UI_AbstractNodeCallDelete (uiNode_t* node, const uiCallContext_t* context)
{
	if (UI_GetParamNumber(context) != 0) {
		Com_Printf("UI_AbstractNodeCallDelete: Invalid number of parameters\n");
		return;
	}
	UI_DeleteNode(node);
}

static void UI_AbstractNodeCallDeleteTimed (uiNode_t* node, const uiCallContext_t* context)
{
	if (UI_GetParamNumber(context) != 1) {
		Com_Printf("UI_AbstractNodeCallDeleteTimed: Invalid number of parameters\n");
		return;
	}
	const char* msStr = UI_GetParam(context, 1);
	const int ms = atoi(msStr);
	if (ms <= 0) {
		UI_DeleteNode(node);
	} else {
		node->deleteTime = CL_Milliseconds() + ms;
	}
}

bool uiLocatedNode::onScroll (uiNode_t* node, int deltaX, int deltaY)
{
	if (node->onWheelUp && deltaY < 0) {
		UI_ExecuteEventActions(node, node->onWheelUp);
		return true;
	}
	if (node->onWheelDown && deltaY > 0) {
		UI_ExecuteEventActions(node, node->onWheelDown);
		return true;
	}
	if (node->onWheel && deltaY != 0) {
		UI_ExecuteEventActions(node, node->onWheel);
		return true;
	}
	return false;
}

void uiLocatedNode::drawTooltip (const uiNode_t* node, int x, int y) const
{
	UI_Tooltip(node, x, y);
}

void uiLocatedNode::onLeftClick (uiNode_t* node, int x, int y)
{
	if (node->onClick != nullptr) {
		UI_ExecuteEventActions(node, node->onClick);
		UI_PlaySound("click1");
	}
}

void uiLocatedNode::onRightClick (uiNode_t* node, int x, int y)
{
	if (node->onRightClick != nullptr) {
		UI_ExecuteEventActions(node, node->onRightClick);
		UI_PlaySound("click1");
	}
}

void uiLocatedNode::onMiddleClick (uiNode_t* node, int x, int y)
{
	if (node->onMiddleClick != nullptr) {
		UI_ExecuteEventActions(node, node->onMiddleClick);
		UI_PlaySound("click1");
	}
}

void UI_RegisterAbstractNode (uiBehaviour_t* behaviour)
{
	behaviour->name = "abstractnode";
	behaviour->isAbstract = true;
	behaviour->manager = UINodePtr(new uiLocatedNode());

	/* Top-left position of the node */
	UI_RegisterNodeProperty(behaviour, "pos", V_POS, uiNode_t, box.pos);
	/* Size of the node */
	propertySize = UI_RegisterNodeProperty(behaviour, "size", V_POS, uiNode_t, box.size);
	/* Width of the node (see also <code>size</code>) */
	propertyWidth = UI_RegisterNodeProperty(behaviour, "width", V_FLOAT, uiNode_t, box.size[0]);
	/* Height of the node (see also <code>size</code>) */
	propertyHeight = UI_RegisterNodeProperty(behaviour, "height", V_FLOAT, uiNode_t, box.size[1]);
	/* Left position of the node (see also <code>pos</code>) */
	UI_RegisterNodeProperty(behaviour, "left", V_FLOAT, uiNode_t, box.pos[0]);
	/* Top position of the node (see also <code>pos</code>) */
	UI_RegisterNodeProperty(behaviour, "top", V_FLOAT, uiNode_t, box.pos[1]);

	/* If true, the node name is indexed into the window. We can access to the node with
	 * the path "windowName#nodeName"
	 */
	UI_RegisterNodeProperty(behaviour, "indexed", V_BOOL, uiNode_t, indexed);
	/* If true, the node is not displayed nor or activatable. */
	propertyInvis = UI_RegisterNodeProperty(behaviour, "invis", V_BOOL, uiNode_t, invis);
	/* If true, the node is disabled. Few nodes support it, fell free to request an update. */
	UI_RegisterNodeProperty(behaviour, "disabled", V_BOOL, uiNode_t, disabled);
	/* Text color the node will use when something is disabled. */
	UI_RegisterNodeProperty(behaviour, "disabledcolor", V_COLOR, uiNode_t, disabledColor);
	/* If true, the node is not ''tangible''. We click through it, then it will not receive mouse event. */
	UI_RegisterNodeProperty(behaviour, "ghost", V_BOOL, uiNode_t, ghost);
	/* Flashing effect. */
	UI_RegisterNodeProperty(behaviour, "flash", V_BOOL, uiNode_t, flash);
	/* Speed of the flashing effect */
	UI_RegisterNodeProperty(behaviour, "flashspeed", V_FLOAT, uiNode_t, flashSpeed);
	/* Border size we want to display. */
	UI_RegisterNodeProperty(behaviour, "border", V_INT, uiNode_t, border);
	/* Padding size we want to use. Few node support it. */
	UI_RegisterNodeProperty(behaviour, "padding", V_INT, uiNode_t, padding);
	/* Background color we want to display. */
	UI_RegisterNodeProperty(behaviour, "bgcolor", V_COLOR, uiNode_t, bgcolor);
	/* Border color we want to display. */
	UI_RegisterNodeProperty(behaviour, "bordercolor", V_COLOR, uiNode_t, bordercolor);

	/*
	 * Used to set the position of the node when the parent use a layout manager.
	 * Else it do nothing.
	 * Available values are: LAYOUTALIGN_TOPLEFT, LAYOUTALIGN_TOP, LAYOUTALIGN_TOPRIGHT,
	 * LAYOUTALIGN_LEFT, LAYOUTALIGN_MIDDLE, LAYOUTALIGN_RIGHT, LAYOUTALIGN_BOTTOMLEFT,
	 * LAYOUTALIGN_BOTTOM, LAYOUTALIGN_BOTTOMRIGHT, LAYOUTALIGN_FILL.
	 * Allowed value depend the layout manager used. The update to date list is into
	 * ui_node_panel.c
	 * @image html http://ufoai.org/wiki/images/Layout.png
	 */
	UI_RegisterNodeProperty(behaviour, "align", V_INT, uiNode_t, align);

	/*
	 * Used share an int, only used by 1 behaviour
	 * @todo move it to the right behaviour, delete it
	 */
	UI_RegisterNodeProperty(behaviour, "num", V_INT, uiNode_t, num);

	/* Tooltip we want to use. */
	UI_RegisterNodeProperty(behaviour, "tooltip", V_CVAR_OR_LONGSTRING, uiNode_t, tooltip);
	/* Text the node will display.
	 */
	UI_RegisterNodeProperty(behaviour, "string", V_CVAR_OR_LONGSTRING, uiNode_t, text);
	/* Text font the node will use.
	 * @todo use V_REF_OF_STRING when its possible ('font' is never a cvar).
	 */
	UI_RegisterNodeProperty(behaviour, "font", V_CVAR_OR_STRING, uiNode_t, font);

	/* Text color the node will use. */
	UI_RegisterNodeProperty(behaviour, "color", V_COLOR, uiNode_t, color);
	/* Text color the node will use when something is selected. */
	UI_RegisterNodeProperty(behaviour, "selectcolor", V_COLOR, uiNode_t, selectedColor);
	/* Flashing color */
	UI_RegisterNodeProperty(behaviour, "flashcolor", V_COLOR, uiNode_t, flashColor);
	/* Alignement of the text into the node, or elements into blocks. */
	UI_RegisterNodeProperty(behaviour, "contentalign", V_UI_ALIGN, uiNode_t, contentAlign);
	/* When <code>invis</code> property is false (default value);
	 * this condition say if the node is visible or not. It use a script expression.
	 */
	UI_RegisterNodeProperty(behaviour, "visiblewhen", V_UI_IF, uiNode_t, visibilityCondition);

	/* Called when the user click with left button into the node. */
	UI_RegisterNodeProperty(behaviour, "onclick", V_UI_ACTION, uiNode_t, onClick);
	/* Called when the user click with right button into the node. */
	UI_RegisterNodeProperty(behaviour, "onrclick", V_UI_ACTION, uiNode_t, onRightClick);
	/* Called when the user click with middle button into the node. */
	UI_RegisterNodeProperty(behaviour, "onmclick", V_UI_ACTION, uiNode_t, onMiddleClick);
	/* Called when the user use the mouse wheel over the node. */
	UI_RegisterNodeProperty(behaviour, "onwheel", V_UI_ACTION, uiNode_t, onWheel);
	/* Called when the user use the mouse wheel up over the node. */
	UI_RegisterNodeProperty(behaviour, "onwheelup", V_UI_ACTION, uiNode_t, onWheelUp);
	/* Called when the user use the mouse wheel down over the node. */
	UI_RegisterNodeProperty(behaviour, "onwheeldown", V_UI_ACTION, uiNode_t, onWheelDown);
	/* Called when the mouse enter over the node. */
	UI_RegisterNodeProperty(behaviour, "onmouseenter", V_UI_ACTION, uiNode_t, onMouseEnter);
	/* Called when the mouse go out of the node. */
	UI_RegisterNodeProperty(behaviour, "onmouseleave", V_UI_ACTION, uiNode_t, onMouseLeave);
	/* Called when the internal content of the nde change. Each behaviour use it how they need it.
	 * @todo Move it where it is need.
	 */
	UI_RegisterNodeProperty(behaviour, "onchange", V_UI_ACTION, uiNode_t, onChange);

	/* Special attribute only use into the node description to exclude part of the node
	 * (see also <code>ghost</code>). Rectangle position is relative to the node. */
	UI_RegisterNodeProperty(behaviour, "excluderect", V_UI_EXCLUDERECT, uiNode_t, firstExcludeRect);

	/* Remove all child from the node (only dynamic allocated nodes). */
	UI_RegisterNodeMethod(behaviour, "removeallchild", UI_AbstractNodeCallRemovaAllChild);

	/* Create a new child with name and type. */
	UI_RegisterNodeMethod(behaviour, "createchild", UI_AbstractNodeCallCreateChild);

	/* Delete the node and remove it from his parent. */
	UI_RegisterNodeMethod(behaviour, "delete", UI_AbstractNodeCallDelete);

	/* Delete the node in x ms and remove it from his parent. */
	UI_RegisterNodeMethod(behaviour, "deletetimed", UI_AbstractNodeCallDeleteTimed);

	/** @todo move it into common? */
	Com_RegisterConstInt("ALIGN_UL", ALIGN_UL);
	Com_RegisterConstInt("ALIGN_UC", ALIGN_UC);
	Com_RegisterConstInt("ALIGN_UR", ALIGN_UR);
	Com_RegisterConstInt("ALIGN_CL", ALIGN_CL);
	Com_RegisterConstInt("ALIGN_CC", ALIGN_CC);
	Com_RegisterConstInt("ALIGN_CR", ALIGN_CR);
	Com_RegisterConstInt("ALIGN_LL", ALIGN_LL);
	Com_RegisterConstInt("ALIGN_LC", ALIGN_LC);
	Com_RegisterConstInt("ALIGN_LR", ALIGN_LR);

	/* some commands */
#ifdef DEBUG
	Cmd_AddCommand("debug_mnsetnodeproperty", UI_NodeSetProperty_f, "Set a node property");
	Cmd_AddCommand("debug_mngetnodeproperty", UI_NodeGetProperty_f, "Get a node property");
#endif
}
