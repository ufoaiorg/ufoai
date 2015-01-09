/**
 * @file
 * @brief C interface to allow to access to cpp node code.
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

#pragma once

/* prototype */
struct uiNode_t;

bool UI_Node_IsVirtual (uiNode_t const* node);
bool UI_Node_IsDrawable (uiNode_t const* node);
bool UI_Node_IsAbstract (uiNode_t const* node);
bool UI_Node_IsFunction (uiNode_t const* node);
bool UI_Node_IsWindow (uiNode_t const* node);
bool UI_Node_IsOptionContainer (uiNode_t const* node);
bool UI_Node_IsBattleScape (uiNode_t const* node);
bool UI_Node_IsScrollableContainer (uiNode_t const* node);
bool UI_Node_IsDrawItselfChild (uiNode_t const* node);

const char* UI_Node_GetWidgetName (uiNode_t const* node);
intptr_t UI_Node_GetMemorySize (uiNode_t const* node);

void UI_Node_Draw (uiNode_t* node);
void UI_Node_DrawTooltip (const uiNode_t* node, int x, int y);
void UI_Node_DrawOverWindow (uiNode_t* node);
void UI_Node_LeftClick (uiNode_t* node, int x, int y);
void UI_Node_RightClick (uiNode_t* node, int x, int y);
void UI_Node_MiddleClick (uiNode_t* node, int x, int y);
bool UI_Node_Scroll (uiNode_t* node, int deltaX, int deltaY);
void UI_Node_MouseMove (uiNode_t* node, int x, int y);
void UI_Node_MouseDown (uiNode_t* node, int x, int y, int button);
void UI_Node_MouseUp (uiNode_t* node, int x, int y, int button);
bool UI_Node_MouseLongPress (uiNode_t* node, int x, int y, int button);
void UI_Node_MouseEnter (uiNode_t* node);
void UI_Node_MouseLeave (uiNode_t* node);
bool UI_Node_StartDragging (uiNode_t* node, int startX, int startY, int currentX, int currentY, int button);
void UI_Node_CapturedMouseMove (uiNode_t* node, int x, int y);
void UI_Node_CapturedMouseLost (uiNode_t* node);
void UI_Node_Loading (uiNode_t* node);
void UI_Node_Loaded (uiNode_t* node);
void UI_Node_Clone (uiNode_t const* source, uiNode_t* clone);
void UI_Node_NewNode (uiNode_t* node);
void UI_Node_DeleteNode (uiNode_t* node);
void UI_Node_WindowOpened (uiNode_t* node, linkedList_t* params);
void UI_Node_WindowClosed (uiNode_t* node);
void UI_Node_WindowActivate (uiNode_t* node);
void UI_Node_DoLayout (uiNode_t* node);
void UI_Node_Activate (uiNode_t* node);
void UI_Node_PropertyChanged (uiNode_t* node, const value_t* property);
void UI_Node_SizeChanged (uiNode_t* node);
void UI_Node_GetClientPosition (uiNode_t const* node, vec2_t position);
bool UI_Node_DndEnter (uiNode_t* node);
bool UI_Node_DndMove (uiNode_t* node, int x, int y);
void UI_Node_DndLeave (uiNode_t* node);
bool UI_Node_DndDrop (uiNode_t* node, int x, int y);
bool UI_Node_DndFinished (uiNode_t* node, bool isDroped);
void UI_Node_FocusGained (uiNode_t* node);
void UI_Node_FocusLost (uiNode_t* node);
bool UI_Node_KeyPressed (uiNode_t* node, unsigned int key, unsigned short unicode);
bool UI_Node_KeyReleased (uiNode_t* node, unsigned int key, unsigned short unicode);
int UI_Node_GetCellWidth (uiNode_t* node);
int UI_Node_GetCellHeight (uiNode_t* node);

#ifdef DEBUG
void UI_Node_DebugCountWidget (uiNode_t* node, int count);
#endif

bool UI_NodeInstanceOf(const uiNode_t* node, const char* behaviourName);
bool UI_NodeInstanceOfPointer(const uiNode_t* node, const uiBehaviour_t* behaviour);
bool UI_NodeSetProperty(uiNode_t* node, const value_t* property, const char* value);
void UI_NodeSetPropertyFromRAW(uiNode_t* node, const value_t* property, const void* rawValue, int rawType);
float UI_GetFloatFromNodeProperty(uiNode_t const* node, const value_t* property);
const char* UI_GetStringFromNodeProperty(const uiNode_t* node, const value_t* property);

/* visibility */
void UI_UnHideNode(uiNode_t* node);
void UI_HideNode(uiNode_t* node);
void UI_Invalidate(uiNode_t* node);
void UI_Validate(uiNode_t* node);
void UI_NodeSetSize(uiNode_t* node, vec2_t size);

/* position */
void UI_GetNodeAbsPos(uiNode_t const* node, vec2_t pos);
void UI_GetNodeScreenPos(const uiNode_t* node, vec2_t pos);
void UI_NodeAbsoluteToRelativePos(uiNode_t const* node, int* x, int* y);
void UI_NodeRelativeToAbsolutePoint(const uiNode_t* node, vec2_t pos);
void UI_NodeGetPoint(const uiNode_t* node, vec2_t pos, int pointDirection);

/* navigation */
uiNode_t* UI_GetNode(uiNode_t const* node, const char* name);
void UI_InsertNode(uiNode_t* node, uiNode_t* prevNode, uiNode_t* newNode);
void UI_AppendNode(uiNode_t* node, uiNode_t* newNode);
uiNode_t* UI_RemoveNode(uiNode_t* node, uiNode_t* child);
void UI_UpdateRoot(uiNode_t* node, uiNode_t* newRoot);
