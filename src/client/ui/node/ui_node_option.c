/**
 * @file ui_node_option.c
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

#include "../ui_main.h"
#include "../ui_parse.h"
#include "ui_node_abstractnode.h"
#include "ui_node_option.h"

#include "../../client.h" /* gettext _() */

/**
 * Allow to check if a node is an option without string check
 */
const uiBehaviour_t *ui_optionBehaviour = NULL;

#define EXTRADATA_TYPE optionExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

static const value_t *propertyCollapsed;


/**
 * @brief update option cache about child, according to collapse and visible status
 * @note can be a common function for all option node
 * @return number of visible elements
 */
int UI_OptionUpdateCache (uiNode_t* option)
{
	int count = 0;
	while (option) {
		int localCount = 0;
		assert(option->behaviour == ui_optionBehaviour);
		if (option->invis) {
			option = option->next;
			continue;
		}
		if (OPTIONEXTRADATA(option).collapsed) {
			OPTIONEXTRADATA(option).childCount = 0;
			option = option->next;
			count++;
			continue;
		}
		if (option->firstChild)
			localCount = UI_OptionUpdateCache(option->firstChild);
		OPTIONEXTRADATA(option).childCount = localCount;
		count += 1 + localCount;
		option = option->next;
	}
	return count;
}

static void UI_OptionDoLayout (uiNode_t *node) {
	uiNode_t *child = node->firstChild;
	int count = 0;

	while(child && child->behaviour == ui_optionBehaviour) {
		UI_Validate(child);
		if (!child->invis) {
			if (EXTRADATA(child).collapsed)
				count += 1 + EXTRADATA(child).childCount;
			else
				count += 1;
		}
		child = child->next;
	}
	EXTRADATA(node).childCount = count;
	node->invalidated = qfalse;
}

static void UI_OptionPropertyChanged (uiNode_t *node, const value_t *property)
{
	if (property == propertyCollapsed) {
		UI_Invalidate(node);
		return;
	}
	ui_optionBehaviour->super->propertyChanged(node, property);
}

/** @brief valid properties for options (used by selectbox, tab, optonlist and optiontree) */
static const value_t properties[] = {
	/**
	 * Displayed text
	 */
	{"label", V_STRING, UI_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, label), 0},

	/**
	 * Value of the option
	 */
	{"value", V_STRING, UI_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, value), 0},

	/**
	 * If true, child are not displayed
	 */
	{"collapsed", V_BOOL, UI_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, collapsed), MEMBER_SIZEOF(EXTRADATA_TYPE, collapsed)},

	/* Icon used to display the node
	 */
	{"icon", V_UI_SPRITEREF, UI_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, icon), MEMBER_SIZEOF(EXTRADATA_TYPE, icon)},

	{NULL, V_NULL, 0, 0},
};

void UI_RegisterOptionNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "option";
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->doLayout = UI_OptionDoLayout;
	behaviour->propertyChanged = UI_OptionPropertyChanged;

	propertyCollapsed = UI_GetPropertyFromBehaviour(behaviour, "collapsed");

	ui_optionBehaviour = behaviour;
}
