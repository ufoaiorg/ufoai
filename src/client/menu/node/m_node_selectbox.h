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
struct menuAction_s;

/** @brief Option definition */
typedef struct selectBoxOptions_s {
	char id[MAX_VAR];	/**< text for the select box - V_TRANSLATION_STRING */
	char label[SELECTBOX_MAX_VALUE_LENGTH];	/**< text for the select box - V_TRANSLATION_STRING */
	char action[MAX_VAR];	/**< execute this when the value is selected */
	char value[MAX_VAR];	/**< the value the cvar should get */
	struct menuIcon_s *icon;	/**< Facultative icon */
	qboolean disabled;		/**< If true, the option is not selectable */

	/**
	 * @frief True if the element is hovered
	 * @todo remove it, deprecated, we should use optionExtraData_t->hovered
	 */
	qboolean hovered;

	struct selectBoxOptions_s *next;	/**< Next element into a linked list of option */

} selectBoxOptions_t;

typedef struct {
	selectBoxOptions_t *first;			/**< first option */
	selectBoxOptions_t *selected;		/**< current selected option */
	selectBoxOptions_t *hovered;		/**< current hovered option */
	int count;							/**< number of elements */
	int pos;							/**< position of the view */
	struct menuAction_s *onViewChange;	/**< called when view change (number of elements...) */
} optionExtraData_t;

struct menuNode_s;
struct nodeBehaviour_s;

selectBoxOptions_t* MN_NodeAddOption(struct menuNode_s *node);
void MN_OptionNodeSortOptions(struct menuNode_s *node);
void MN_RegisterSelectBoxNode(struct nodeBehaviour_s *behaviour);
void MN_RegisterAbstractOptionNode(struct nodeBehaviour_s *behaviour);

#endif
