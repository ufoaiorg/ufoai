/**
 * @file m_node_window.h
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

#ifndef CLIENT_MENU_M_NODE_WINDOW_H
#define CLIENT_MENU_M_NODE_WINDOW_H

#include "../../../shared/mathlib.h"

/* prototype */
struct menuNode_s;
struct menuAction_s;
struct nodeBehaviour_s;
struct menuKeyBinding_s;

extern const struct nodeBehaviour_s const *windowBehaviour;

#define INDEXEDCHILD_HASH_SIZE 32

typedef struct node_index_s {
	struct menuNode_s *node;
	struct node_index_s *hash_next;
	struct node_index_s *next;
} node_index_t;

/**
 * @brief extradata for the window node
 */
typedef struct {
	int eventTime;
	vec2_t noticePos; 				/**< the position where the cl.msgText messages are rendered */
	qboolean dragButton;			/**< If true, we init the window with a header to move it */
	qboolean closeButton;			/**< If true, we init the window with a header button to close it */
	qboolean preventTypingEscape;	/**< If true, we can't use ESC button to close the window */
	qboolean modal;					/**< If true, we can't click outside the window */
	qboolean dropdown;				/**< very special property force the menu to close if we click outside */
	qboolean isFullScreen;			/**< Internal data to allow fullscreen windows without the same size */
	qboolean fill;					/**< If true, use all the screen space allowed */
	qboolean starLayout;			/**< If true, do a star layout (move child into a corner according to his num) */

	int timeOut;					/**< ms value until calling onTimeOut (see cl.time) */
	int lastTime;					/**< when a menu was pushed this value is set to cl.time */

	struct menuNode_s *parent;	/**< to create child window */

	/** @todo we can remove it if we create a node for the battlescape */
	struct menuNode_s *renderNode;

	struct menuKeyBinding_s *keyList;	/** list of key binding */

	/** @todo think about converting it to action instead of node */
	struct menuAction_s *onInit; 	/**< Call when the menu is push */
	struct menuAction_s *onClose;	/**< Call when the menu is pop */
	struct menuAction_s *onTimeOut;	/**< Call when the own timer of the window out */

	node_index_t *index;
	node_index_t *index_hash[INDEXEDCHILD_HASH_SIZE];

} windowExtraData_t;

void MN_RegisterWindowNode(struct nodeBehaviour_s *behaviour);

qboolean MN_WindowIsFullScreen(const struct menuNode_s* const menu);
qboolean MN_WindowIsDropDown(const struct menuNode_s* const menu);
qboolean MN_WindowIsModal(const struct menuNode_s* const menu);
void MN_WindowNodeRegisterKeyBinding(struct menuNode_s* menu, struct menuKeyBinding_s *binding);
struct menuKeyBinding_s *MN_WindowNodeGetKeyBinding(const struct menuNode_s* const node, unsigned int key);
void MN_WindowNodeSetRenderNode(struct menuNode_s *node, struct menuNode_s *renderNode);
vec_t *MN_WindowNodeGetNoticePosition(struct menuNode_s *node);
/* child index */
struct menuNode_s* MN_WindowNodeGetIndexedChild(struct menuNode_s* const node, const char* childName);
qboolean MN_WindowNodeAddIndexedNode(struct menuNode_s* const node, struct menuNode_s* const child);
qboolean MN_WindowNodeRemoveIndexedNode(struct menuNode_s* const node, struct menuNode_s* const child);

#endif
