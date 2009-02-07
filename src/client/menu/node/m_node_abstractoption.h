/**
 * @file m_node_abstractoption.h
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

#ifndef CLIENT_MENU_M_NODE_ABSTRACTOPTION_H
#define CLIENT_MENU_M_NODE_ABSTRACTOPTION_H

#include "../../../shared/shared.h"

#define MAX_MENUOPTIONS 128
#define OPTION_MAX_VALUE_LENGTH 32

struct menuIcon_s;
struct menuAction_s;

/** @brief Option definition */
typedef struct menuOption_s {
	char id[MAX_VAR];	/**< text for the select box - V_TRANSLATION_STRING */
	char label[OPTION_MAX_VALUE_LENGTH];	/**< text for the select box - V_TRANSLATION_STRING */
	char action[MAX_VAR];	/**< execute this when the value is selected */
	char value[MAX_VAR];	/**< the value the cvar should get */
	struct menuIcon_s *icon;	/**< Facultative icon */
	qboolean disabled;		/**< If true, the option is not selectable */

	/**
	 * @frief True if the element is hovered
	 * @todo remove it, deprecated, we should use optionExtraData_t->hovered
	 */
	qboolean hovered;

	struct menuOption_s *next;	/**< Next element into a linked list of option */

} menuOption_t;

typedef struct {
	/* link to shared data (can be used if internal data is null) */
	int dataId;							/**< Shared data id where we can find option */
	int versionId;						/**< Cached version of the shared data, to check update */

	/* link to internal data */
	menuOption_t *first;				/**< first option */

	/* information */
	menuOption_t *selected;				/**< current selected option */
	menuOption_t *hovered;				/**< current hovered option */
	int count;							/**< number of elements */
	int pos;							/**< position of the view */
	struct menuAction_s *onViewChange;	/**< called when view change (number of elements...) */
} optionExtraData_t;

struct menuNode_s;
struct nodeBehaviour_s;

menuOption_t* MN_NodeAppendOption(struct menuNode_s *node, menuOption_t* option);
void MN_OptionNodeSortOptions(struct menuNode_s *node);
void MN_RegisterAbstractOptionNode(struct nodeBehaviour_s *behaviour);

#endif
