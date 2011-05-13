/**
 * @file ui_components.c
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

#include "ui_internal.h"
#include "ui_components.h"

/**
 * @brief Searches all components for the specified one
 * @param[in] name Name of the component we search
 * @return The component found, else NULL
 * @note Use dichotomic search
 */
uiNode_t *UI_GetComponent (const char *name)
{
	unsigned char min = 0;
	unsigned char max = ui_global.numComponents;

	while (min != max) {
		const int mid = (min + max) >> 1;
		const int diff = strcmp(ui_global.components[mid]->name, name);
		assert(mid < max);
		assert(mid >= min);

		if (diff == 0)
			return ui_global.components[mid];

		if (diff > 0)
			max = mid;
		else
			min = mid + 1;
	}

	return NULL;
}

/**
 * @brief Add a new component to the list of all components
 * @note Sort components by alphabet
 */
void UI_InsertComponent(uiNode_t* component)
{
	int pos = 0;
	int i;

	if (ui_global.numComponents >= UI_MAX_COMPONENTS)
		Com_Error(ERR_FATAL, "UI_InsertComponent: hit MAX_COMPONENTS");

	/* search the insertion position */
	for (pos = 0; pos < ui_global.numComponents; pos++) {
		const uiNode_t* node = ui_global.components[pos];
		if (strcmp(component->name, node->name) < 0)
			break;
	}

	/* create the space */
	for (i = ui_global.numComponents - 1; i >= pos; i--)
		ui_global.components[i + 1] = ui_global.components[i];

	/* insert */
	ui_global.components[pos] = component;
	ui_global.numComponents++;
}
