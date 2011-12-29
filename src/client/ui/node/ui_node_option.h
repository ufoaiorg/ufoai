/**
 * @file ui_node_option.h
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

#ifndef CLIENT_UI_UI_NODE_OPTION_H
#define CLIENT_UI_UI_NODE_OPTION_H

#include "../ui_nodes.h"

void UI_RegisterOptionNode(struct uiBehaviour_s *behaviour);

extern const struct uiBehaviour_s *ui_optionBehaviour;

#define OPTIONEXTRADATA_TYPE optionExtraData_t
#define OPTIONEXTRADATA(node) UI_EXTRADATA(node, OPTIONEXTRADATA_TYPE)
#define OPTIONEXTRADATACONST(node) UI_EXTRADATACONST(node, OPTIONEXTRADATA_TYPE)

/** @brief Option definition */
typedef struct optionExtraData_s {
	char label[MAX_VAR];	/**< text for the select box */
	char value[MAX_VAR];	/**< the value the cvar should get */

	struct uiSprite_s *icon;	/**< Link to an icon */
	qboolean flipIcon;		/**< Flip the icon rendering (horizontal) */

	/* status */
	qboolean collapsed;		/**< If true, child are not displayed */

	/* cache */
	qboolean truncated;		/**< If true, the label is not fully displayed */
	int childCount;			/**< Number of visible recursive child */
	qboolean hovered;		/**< true if the element is hovered. Deprecated */

} optionExtraData_t;

int UI_OptionUpdateCache(struct uiNode_s* option);
uiNode_t *UI_AllocOptionNode(const char* name, const char* label, const char* value);

#endif
