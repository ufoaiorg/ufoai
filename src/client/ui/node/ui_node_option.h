/**
 * @file
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

#include "../ui_nodes.h"
#include "ui_node_abstractnode.h"

class uiOptionNode : public uiLocatedNode {
	void onPropertyChanged(uiNode_t* node, const value_t* property) override;
	void doLayout(uiNode_t* node) override;
};

void UI_RegisterOptionNode(uiBehaviour_t* behaviour);

extern const uiBehaviour_t* ui_optionBehaviour;

#define OPTIONEXTRADATA_TYPE optionExtraData_t
#define OPTIONEXTRADATA(node) UI_EXTRADATA(node, OPTIONEXTRADATA_TYPE)
#define OPTIONEXTRADATACONST(node) UI_EXTRADATACONST(node, OPTIONEXTRADATA_TYPE)

/** @brief Option definition */
typedef struct optionExtraData_s {
	char label[MAX_VAR];	/**< text for the select box */
	char value[MAX_VAR];	/**< the value the cvar should get */

	uiSprite_t* icon;		/**< Link to an icon */
	bool flipIcon;			/**< Flip the icon rendering (horizontal) */

	/* status */
	bool collapsed;			/**< If true, child are not displayed */

	/* cache */
	bool truncated;			/**< If true, the label is not fully displayed */
	int childCount;			/**< Number of visible recursive child */
	bool hovered;			/**< true if the element is hovered. Deprecated */

} optionExtraData_t;

int UI_OptionUpdateCache(uiNode_t* option);
uiNode_t* UI_AllocOptionNode(const char* name, const char* label, const char* value);
