/**
 * @file ui_node_abstractnode.h
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

#ifndef CLIENT_UI_UI_NODE_ABSTRACTNODE_H
#define CLIENT_UI_UI_NODE_ABSTRACTNODE_H

#include "../ui_nodes.h"

void UI_RegisterAbstractNode(struct uiBehaviour_s *);

struct uiNode_s;

qboolean UI_NodeInstanceOf(const uiNode_t *node, const char* behaviourName);
qboolean UI_NodeInstanceOfPointer(const uiNode_t *node, const struct uiBehaviour_s* behaviour);
qboolean UI_NodeSetProperty(uiNode_t* node, const value_t *property, const char* value);
void UI_NodeSetPropertyFromRAW(uiNode_t* node, const value_t *property, const void* rawValue, int rawType);
float UI_GetFloatFromNodeProperty(const struct uiNode_s* node, const value_t* property);
const char* UI_GetStringFromNodeProperty(const uiNode_t* node, const value_t* property);

/* visibility */
void UI_UnHideNode(struct uiNode_s *node);
void UI_HideNode(struct uiNode_s *node);
void UI_Invalidate(struct uiNode_s *node);
void UI_Validate(struct uiNode_s *node);
void UI_NodeSetSize(uiNode_t* node, vec2_t size);

/* position */
void UI_GetNodeAbsPos(const struct uiNode_s* node, vec2_t pos);
void UI_GetNodeScreenPos(const uiNode_t* node, vec2_t pos);
void UI_NodeAbsoluteToRelativePos(const struct uiNode_s* node, int *x, int *y);
void UI_NodeRelativeToAbsolutePoint(const uiNode_t* node, vec2_t pos);
void UI_NodeGetPoint(const uiNode_t* node, vec2_t pos, int pointDirection);

/* navigation */
struct uiNode_s *UI_GetNode(const struct uiNode_s* const node, const char *name);
void UI_InsertNode(struct uiNode_s* const node, struct uiNode_s *prevNode, struct uiNode_s *newNode);
void UI_AppendNode(struct uiNode_s* const node, struct uiNode_s *newNode);
uiNode_t* UI_RemoveNode(uiNode_t* const node, uiNode_t *child);
void UI_UpdateRoot(uiNode_t *node, uiNode_t *newRoot);

#endif
