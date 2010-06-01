/**
 * @file m_node_option.c
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

#include "../m_main.h"
#include "../m_parse.h"
#include "m_node_abstractnode.h"
#include "m_node_option.h"

#include "../../client.h" /* gettext _() */

/**
 * Allow to check if a node is an option without string check
 */
const nodeBehaviour_t *optionBehaviour = NULL;

#define EXTRADATA_TYPE optionExtraData_t
#define EXTRADATA(node) MN_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) MN_EXTRADATACONST(node, EXTRADATA_TYPE)

static const value_t *propertyCollapsed;


/**
 * @brief update option cache about child, according to collapse and visible status
 * @note can be a common function for all option node
 * @return number of visible elements
 */
int MN_OptionUpdateCache (menuNode_t* option)
{
	int count = 0;
	while (option) {
		int localCount = 0;
		assert(option->behaviour == optionBehaviour);
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
			localCount = MN_OptionUpdateCache(option->firstChild);
		OPTIONEXTRADATA(option).childCount = localCount;
		count += 1 + localCount;
		option = option->next;
	}
	return count;
}

static void MN_OptionDoLayout (menuNode_t *node) {
	menuNode_t *child = node->firstChild;
	int count = 0;

	while(child && child->behaviour == optionBehaviour) {
		MN_Validate(child);
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

static void MN_OptionPropertyChanged (menuNode_t *node, const value_t *property)
{
	if (property == propertyCollapsed) {
		MN_Invalidate(node);
		return;
	}
	optionBehaviour->super->propertyChanged(node, property);
}

/** @brief valid properties for options (used by selectbox, tab, optonlist and optiontree) */
static const value_t properties[] = {
	/**
	 * Displayed text
	 */
	{"label", V_TRANSLATION_STRING, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, label), MEMBER_SIZEOF(EXTRADATA_TYPE, label)},

	/**
	 * Value of the option
	 */
	{"value", V_STRING, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, value), 0},

	/**
	 * If true, child are not displayed
	 */
	{"collapsed", V_BOOL, MN_EXTRADATA_OFFSETOF(EXTRADATA_TYPE, collapsed), MEMBER_SIZEOF(EXTRADATA_TYPE, collapsed)},

	{NULL, V_NULL, 0, 0},
};

void MN_RegisterOptionNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "option";
	behaviour->properties = properties;
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);
	behaviour->doLayout = MN_OptionDoLayout;
	behaviour->propertyChanged = MN_OptionPropertyChanged;

	propertyCollapsed = MN_GetPropertyFromBehaviour(behaviour, "collapsed");

	optionBehaviour = behaviour;
}
