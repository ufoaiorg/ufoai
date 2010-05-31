/**
 * @file m_node_option.h
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

#ifndef CLIENT_MENU_M_NODE_OPTION_H
#define CLIENT_MENU_M_NODE_OPTION_H

#include "../m_nodes.h"

void MN_RegisterOptionNode(struct nodeBehaviour_s *behaviour);

extern const struct nodeBehaviour_s *optionBehaviour;

#define OPTIONEXTRADATA_TYPE optionExtraData_t
#define OPTIONEXTRADATA(node) MN_EXTRADATA(node, OPTIONEXTRADATA_TYPE)
#define OPTIONEXTRADATACONST(node) MN_EXTRADATACONST(node, OPTIONEXTRADATA_TYPE)

/** @brief Option definition */
typedef struct optionExtraData_s {
	char label[MAX_VAR];	/**< text for the select box - V_TRANSLATION_STRING */
	char value[MAX_VAR];	/**< the value the cvar should get */

	/* status */
	qboolean collapsed;		/**< If true, child are not displayed */

	/* cache */
	qboolean truncated;		/**< If true, the label is not fully displayed */
	int childCount;			/**< Number of visible recursive child */
	qboolean hovered;		/**< true if the element is hovered. Deprecated */

} optionExtraData_t;

int MN_OptionUpdateCache(struct menuNode_s* option);

#endif
