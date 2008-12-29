/**
 * @file m_node_selectbox.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#ifndef CLIENT_MENU_M_NODE_SELECTBOX_H
#define CLIENT_MENU_M_NODE_SELECTBOX_H

#define MAX_SELECT_BOX_OPTIONS 128
#define SELECTBOX_MAX_VALUE_LENGTH 32
#define SELECTBOX_DEFAULT_HEIGHT 20.0f

struct menuIcon_s;

/** @brief Option definition */
typedef struct selectBoxOptions_s {
	char id[MAX_VAR];	/**< text for the select box - V_TRANSLATION_MANUAL_STRING */
	char label[SELECTBOX_MAX_VALUE_LENGTH];	/**< text for the select box - V_TRANSLATION_MANUAL_STRING */
	char action[MAX_VAR];	/**< execute this when the value is selected */
	char value[MAX_VAR];	/**< the value the cvar should get */
	struct selectBoxOptions_s *next;	/**< pointer to next option entry for this node
							 * NULL terminated for each node */
	struct menuIcon_s *icon;	/**< Facultative icon */
	qboolean hovered;		/**< current selected option entry selected? */
} selectBoxOptions_t;

typedef struct {
	selectBoxOptions_t *first;	/**< first option */
	int count;
} optionExtraData_t;

struct menuNode_s;
struct nodeBehaviour_s;

selectBoxOptions_t* MN_NodeAddOption(struct menuNode_s *node);
void MN_RegisterSelectBoxNode(struct nodeBehaviour_s *behaviour);
void MN_RegisterAbstractOptionNode(struct nodeBehaviour_s *behaviour);
#endif
