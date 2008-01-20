/**
 * @file m_node_selectbox.c
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

#include "../client.h"
#include "m_main.h"

/**
 * @brief Adds a new selectbox option to a selectbox node
 * @sa MN_SELECTBOX
 * @return NULL if menuSelectBoxes is 'full' - otherwise pointer to the selectBoxOption
 * @param[in] node The node (must be of type MN_SELECTBOX) where you want to append
 * the option
 * @note You have to add the values manually to the option pointer
 */
selectBoxOptions_t* MN_AddSelectboxOption (menuNode_t *node)
{
	selectBoxOptions_t *selectBoxOption;

	assert(node->type == MN_SELECTBOX);

	if (mn.numSelectBoxes >= MAX_SELECT_BOX_OPTIONS) {
		Com_Printf("MN_AddSelectboxOption: numSelectBoxes exceeded - increase MAX_SELECT_BOX_OPTIONS\n");
		return NULL;
	}
	/* initial options entry */
	if (!node->options)
		node->options = &mn.menuSelectBoxes[mn.numSelectBoxes];
	else {
		/* link it in */
		for (selectBoxOption = node->options; selectBoxOption->next; selectBoxOption = selectBoxOption->next);
		selectBoxOption->next = &mn.menuSelectBoxes[mn.numSelectBoxes];
		selectBoxOption->next->next = NULL;
	}
	selectBoxOption = &mn.menuSelectBoxes[mn.numSelectBoxes];
	node->height++;
	mn.numSelectBoxes++;
	return selectBoxOption;
}

void MN_NodeSelectBoxInit (void)
{

}
