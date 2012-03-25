/**
 * @file ui_node.h
 * @brief C interface to allow to access to cpp node code.
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

#ifndef CLIENT_UI_UI_NODE_H
#define CLIENT_UI_UI_NODE_H

/* prototype */
struct uiNode_s;

qboolean UI_Node_IsVirtual (const struct uiNode_s *node);
qboolean UI_Node_IsDrawable (const struct uiNode_s *node);
qboolean UI_Node_IsAbstract (const struct uiNode_s *node);
qboolean UI_Node_IsFunction (const struct uiNode_s *node);
qboolean UI_Node_IsScrollableContainer (const struct uiNode_s *node);
qboolean UI_Node_IsDrawItselfChild (const struct uiNode_s *node);

const char* UI_Node_GetWidgetName (const struct uiNode_s *node);
intptr_t UI_Node_GetMemorySize (const struct uiNode_s *node);

void UI_Node_Draw (struct uiNode_s *node);
void UI_Node_DrawTooltip (struct uiNode_s *node, int x, int y);
void UI_Node_DrawOverWindow (struct uiNode_s *node);
void UI_Node_LeftClick (struct uiNode_s *node, int x, int y);
void UI_Node_RightClick (struct uiNode_s *node, int x, int y);
void UI_Node_MiddleClick (struct uiNode_s *node, int x, int y);
qboolean UI_Node_Scroll (struct uiNode_s *node, int deltaX, int deltaY);
void UI_Node_MouseMove (struct uiNode_s *node, int x, int y);
void UI_Node_MouseDown (struct uiNode_s *node, int x, int y, int button);
void UI_Node_MouseUp (struct uiNode_s *node, int x, int y, int button);
void UI_Node_CapturedMouseMove (struct uiNode_s *node, int x, int y);
void UI_Node_CapturedMouseLost (struct uiNode_s *node);
void UI_Node_Loading (struct uiNode_s *node);
void UI_Node_Loaded (struct uiNode_s *node);
void UI_Node_Clone (const struct uiNode_s *source, struct uiNode_s *clone);
void UI_Node_NewNode (struct uiNode_s *node);
void UI_Node_DeleteNode (struct uiNode_s *node);
void UI_Node_WindowOpened (struct uiNode_s *node, linkedList_t *params);
void UI_Node_WindowClosed (struct uiNode_s *node);
void UI_Node_DoLayout (struct uiNode_s *node);
void UI_Node_Activate (struct uiNode_s *node);
void UI_Node_PropertyChanged (struct uiNode_s *node, const value_t *property);
void UI_Node_SizeChanged (struct uiNode_s *node);
void UI_Node_GetClientPosition (const struct uiNode_s *node, vec2_t position);
qboolean UI_Node_DndEnter (struct uiNode_s *node);
qboolean UI_Node_DndMove (struct uiNode_s *node, int x, int y);
void UI_Node_DndLeave (struct uiNode_s *node);
qboolean UI_Node_DndDrop (struct uiNode_s *node, int x, int y);
qboolean UI_Node_DndFinished (struct uiNode_s *node, qboolean isDroped);
void UI_Node_FocusGained (struct uiNode_s *node);
void UI_Node_FocusLost (struct uiNode_s *node);
qboolean UI_Node_KeyPressed (struct uiNode_s *node, unsigned int key, unsigned short unicode);
qboolean UI_Node_KeyReleased (struct uiNode_s *node, unsigned int key, unsigned short unicode);
int UI_Node_GetCellWidth (struct uiNode_s *node);
int UI_Node_GetCellHeight (struct uiNode_s *node);

#ifdef DEBUG
void UI_Node_DebugCountWidget (struct uiNode_s *node, int count);
#endif

#endif
