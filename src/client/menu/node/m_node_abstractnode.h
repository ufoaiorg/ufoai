/**
 * @file m_node_abstractnode.h
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

#ifndef CLIENT_MENU_M_NODE_ABSTRACTNODE_H
#define CLIENT_MENU_M_NODE_ABSTRACTNODE_H

#include "../m_nodes.h"

void MN_RegisterAbstractNode(uiBehaviour_t *);

struct uiNode_s;

qboolean MN_NodeInstanceOf(const uiNode_t *node, const char* behaviourName);
qboolean MN_NodeInstanceOfPointer(const uiNode_t *node, const uiBehaviour_t* behaviour);
qboolean MN_NodeSetProperty(uiNode_t* node, const value_t *property, const char* value);
void MN_NodeSetPropertyFromRAW(uiNode_t* node, const value_t *property, void* rawValue, int rawType);
float MN_GetFloatFromNodeProperty(const struct uiNode_s* node, const value_t* property);
const char* MN_GetStringFromNodeProperty(const uiNode_t* node, const value_t* property);

/* visibility */
void MN_UnHideNode(struct uiNode_s *node);
void MN_HideNode(struct uiNode_s *node);
void MN_Invalidate(struct uiNode_s *node);
void MN_Validate(struct uiNode_s *node);
void MN_NodeSetSize(uiNode_t* node, vec2_t size);

/* position */
void MN_GetNodeAbsPos(const struct uiNode_s* node, vec2_t pos);
void MN_NodeAbsoluteToRelativePos(const struct uiNode_s* node, int *x, int *y);
void MN_NodeRelativeToAbsolutePoint(const uiNode_t* node, vec2_t pos);
void MN_NodeGetPoint(const uiNode_t* node, vec2_t pos, byte pointDirection);

/* navigation */
struct uiNode_s *MN_GetNode(const struct uiNode_s* const node, const char *name);
void MN_InsertNode(struct uiNode_s* const node, struct uiNode_s *prevNode, struct uiNode_s *newNode);
void MN_AppendNode(struct uiNode_s* const node, struct uiNode_s *newNode);
uiNode_t* MN_RemoveNode(uiNode_t* const node, uiNode_t *child);
void MN_UpdateRoot(uiNode_t *node, uiNode_t *newRoot);

#endif
