/**
 * @file m_node_selectbox.c
 * @todo change to mouse behaviour. It better to click to dropdown the list
 * @todo manage disabled option
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

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../m_main.h"
#include "../m_internal.h"
#include "../m_parse.h"
#include "../m_font.h"
#include "../m_input.h"
#include "../m_draw.h"
#include "m_node_abstractoption.h"
#include "m_node_abstractnode.h"

#define SELECTBOX_SIDE_WIDTH 7.0f
#define SELECTBOX_RIGHT_WIDTH 20.0f

#define SELECTBOX_SPACER 2.0f
#define SELECTBOX_BOTTOM_HEIGHT 4.0f

/**
 * @brief Remove the highter element (in alphabet) from a list
 */
static menuOption_t *MN_OptionNodeRemoveHighterOption (menuOption_t **option)
{
	menuOption_t *prev = NULL;
	menuOption_t *prevfind = NULL;
	menuOption_t *search = (*option)->next;
	char *label = (*option)->label;

	/* search the smaller element */
	while (search) {
		if (Q_strcmp(label, search->label) < 0) {
			prevfind = prev;
			label = search->label;
		}
		prev = search;
		search = search->next;
	}

	/* remove the first element */
	if (prevfind == NULL) {
		menuOption_t *tmp = *option;
		*option = (*option)->next;
		return tmp;
	} else {
		menuOption_t *tmp = prevfind->next;
		prevfind->next = tmp->next;
		return tmp;
	}
}

/**
 * @brief Sort options by alphabet
 */
void MN_OptionNodeSortOptions (menuNode_t *node)
{
	menuOption_t *option;
	assert(MN_NodeInstanceOf(node, "abstractoption"));

	/* unlink the unsorted list */
	option = node->u.option.first;
	if (option == NULL)
		return;
	node->u.option.first = NULL;

	/* construct a sorted list */
	while (option) {
		menuOption_t *element;
		element = MN_OptionNodeRemoveHighterOption(&option);
		element->next = node->u.option.first;
		node->u.option.first = element;
	}
}

/**
 * @brief Adds a new selectbox option to a selectbox node
 * @return NULL if menuSelectBoxes is 'full' - otherwise pointer to the selectBoxOption
 * @param[in] node The abstractoption where you want to append the option
 * @note You have to add the values manually to the option pointer
 */
menuOption_t* MN_NodeAddOption (menuNode_t *node)
{
	menuOption_t *selectBoxOption;

	assert(MN_NodeInstanceOf(node, "abstractoption"));

	if (mn.numSelectBoxes >= MAX_MENUOPTIONS) {
		Com_Printf("MN_AddSelectboxOption: numSelectBoxes exceeded - increase MAX_SELECT_BOX_OPTIONS\n");
		return NULL;
	}
	/* initial options entry */
	if (!node->u.option.first)
		node->u.option.first = &mn.menuSelectBoxes[mn.numSelectBoxes];
	else {
		/* link it in */
		for (selectBoxOption = node->u.option.first; selectBoxOption->next; selectBoxOption = selectBoxOption->next) {}
		selectBoxOption->next = &mn.menuSelectBoxes[mn.numSelectBoxes];
		selectBoxOption->next->next = NULL;
	}
	selectBoxOption = &mn.menuSelectBoxes[mn.numSelectBoxes];
	node->u.option.count++;
	mn.numSelectBoxes++;
	return selectBoxOption;
}

static const value_t properties[] = {
	/* very special attribute */
	{"option", V_SPECIAL_OPTIONNODE, offsetof(menuNode_t, u.option.first), 0},
	{"viewpos", V_INT, offsetof(menuNode_t, u.option.pos),  MEMBER_SIZEOF(menuNode_t, u.option.pos)},
	{"count", V_INT, offsetof(menuNode_t, u.option.count),  MEMBER_SIZEOF(menuNode_t, u.option.count)},
	{"viewchange", V_SPECIAL_ACTION, offsetof(menuNode_t, u.option.onViewChange), MEMBER_SIZEOF(menuNode_t, u.option.onViewChange)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractOptionNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractoption";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
}
