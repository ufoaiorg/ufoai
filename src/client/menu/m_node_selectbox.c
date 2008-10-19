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
#include "m_parse.h"
#include "m_font.h"
#include "m_input.h"

const menuNode_t *selectBoxNode;

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

	assert(node->type == MN_SELECTBOX || node->type == MN_TAB);

	if (mn.numSelectBoxes >= MAX_SELECT_BOX_OPTIONS) {
		Com_Printf("MN_AddSelectboxOption: numSelectBoxes exceeded - increase MAX_SELECT_BOX_OPTIONS\n");
		return NULL;
	}
	/* initial options entry */
	if (!node->options)
		node->options = &mn.menuSelectBoxes[mn.numSelectBoxes];
	else {
		/* link it in */
		for (selectBoxOption = node->options; selectBoxOption->next; selectBoxOption = selectBoxOption->next) {}
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
	selectBoxNode = NULL;
}

void MN_DrawSelectBoxNode (menuNode_t *node)
{
	selectBoxOptions_t* selectBoxOption;
	int selBoxX, selBoxY;
	const char *ref;
	const char *font;
	vec2_t nodepos;

	if (!node->dataModelSkinOrCVar)
		return;

	MN_GetNodeAbsPos(node, nodepos);
	const char* image = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	if (!image)
		image = "menu/selectbox";

	ref = MN_GetReferenceString(node->menu, node->dataModelSkinOrCVar);

	font = MN_GetFont(node->menu, node);
	selBoxX = nodepos[0] + SELECTBOX_SIDE_WIDTH;
	selBoxY = nodepos[1] + SELECTBOX_SPACER;

	/* left border */
	R_DrawNormPic(nodepos[0], nodepos[1], SELECTBOX_SIDE_WIDTH, node->size[1],
		SELECTBOX_SIDE_WIDTH, 20.0f, 0.0f, 0.0f, node->align, node->blend, image);
	/* stretched middle bar */
	R_DrawNormPic(nodepos[0] + SELECTBOX_SIDE_WIDTH, nodepos[1], node->size[0], node->size[1],
		12.0f, 20.0f, 7.0f, 0.0f, node->align, node->blend, image);
	/* right border (arrow) */
	R_DrawNormPic(nodepos[0] + SELECTBOX_SIDE_WIDTH + node->size[0], nodepos[1], SELECTBOX_DEFAULT_HEIGHT, node->size[1],
		32.0f, 20.0f, 12.0f, 0.0f, node->align, node->blend, image);
	/* draw the label for the current selected option */
	for (selectBoxOption = node->options; selectBoxOption; selectBoxOption = selectBoxOption->next) {
		if (!Q_strcmp(selectBoxOption->value, ref)) {
			R_FontDrawString(font, node->align, selBoxX, selBoxY,
				selBoxX, selBoxY, node->size[0] - 4, 0,
				node->texh[0], _(selectBoxOption->label), 0, 0, NULL, qfalse);
		}
	}

	/* active? */
	if (node->state) {
		selBoxY += node->size[1];
		selectBoxNode = node;

		/* drop down menu */
		/* left side */
		R_DrawNormPic(nodepos[0], nodepos[1] + node->size[1], SELECTBOX_SIDE_WIDTH, node->size[1] * node->height,
			7.0f, 28.0f, 0.0f, 21.0f, node->align, node->blend, image);

		/* stretched middle bar */
		R_DrawNormPic(nodepos[0] + SELECTBOX_SIDE_WIDTH, nodepos[1] + node->size[1], node->size[0], node->size[1] * node->height,
			16.0f, 28.0f, 7.0f, 21.0f, node->align, node->blend, image);

		/* right side */
		R_DrawNormPic(nodepos[0] + SELECTBOX_SIDE_WIDTH + node->size[0], nodepos[1] + node->size[1], SELECTBOX_SIDE_WIDTH, node->size[1] * node->height,
			23.0f, 28.0f, 16.0f, 21.0f, node->align, node->blend, image);

		/* now draw all available options for this selectbox */
		for (selectBoxOption = node->options; selectBoxOption; selectBoxOption = selectBoxOption->next) {
			/* draw the hover effect */
			if (selectBoxOption->hovered)
				R_DrawFill(selBoxX, selBoxY, node->size[0], SELECTBOX_DEFAULT_HEIGHT, ALIGN_UL, node->color);
			/* print the option label */
			R_FontDrawString(font, node->align, selBoxX, selBoxY,
				selBoxX, nodepos[1] + node->size[1], node->size[0] - 4, 0,
				node->texh[0], _(selectBoxOption->label), 0, 0, NULL, qfalse);
			/* next entries' position */
			selBoxY += node->size[1];
			/* reset the hovering - will be recalculated next frame */
			selectBoxOption->hovered = qfalse;
		}
		/* left side */
		R_DrawNormPic(nodepos[0], selBoxY - SELECTBOX_SPACER, SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
			7.0f, 32.0f, 0.0f, 28.0f, node->align, node->blend, image);

		/* stretched middle bar */
		R_DrawNormPic(nodepos[0] + SELECTBOX_SIDE_WIDTH, selBoxY - SELECTBOX_SPACER, node->size[0], SELECTBOX_BOTTOM_HEIGHT,
			16.0f, 32.0f, 7.0f, 28.0f, node->align, node->blend, image);

		/* right bottom side */
		R_DrawNormPic(nodepos[0] + SELECTBOX_SIDE_WIDTH + node->size[0], selBoxY - SELECTBOX_SPACER,
			SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
			23.0f, 32.0f, 16.0f, 28.0f, node->align, node->blend, image);
	}
}

/**
 * @brief Handles selectboxes clicks
 * @sa MN_SELECTBOX
 */
static void MN_SelectboxClick (menuNode_t * node, int x, int y)
{
	selectBoxOptions_t* selectBoxOption;
	int clickedAtOption;
	vec2_t pos;

	MN_GetNodeAbsPos(node, pos);
	clickedAtOption = (y - pos[1]);

	if (node->size[1])
		clickedAtOption = (clickedAtOption - node->size[1]) / node->size[1];
	else
		clickedAtOption = (clickedAtOption - SELECTBOX_DEFAULT_HEIGHT) / SELECTBOX_DEFAULT_HEIGHT; /* default height - see selectbox.tga */

	if (clickedAtOption < 0 || clickedAtOption >= node->height)
		return;

	/* the cvar string is stored in dataModelSkinOrCVar */
	/* no cvar given? */
	if (!node->dataModelSkinOrCVar || !*(char*)node->dataModelSkinOrCVar) {
		Com_Printf("MN_SelectboxClick: node '%s' doesn't have a valid cvar assigned (menu %s)\n", node->name, node->menu->name);
		return;
	}

	/* no cvar? */
	if (Q_strncmp((const char *)node->dataModelSkinOrCVar, "*cvar", 5))
		return;

	/* only execute the click stuff if the selectbox is active */
	if (node->state) {
		selectBoxOption = node->options;
		for (; clickedAtOption > 0 && selectBoxOption; selectBoxOption = selectBoxOption->next) {
			clickedAtOption--;
		}
		if (selectBoxOption) {
			const char *cvarName = &((const char *)node->dataModelSkinOrCVar)[6];
			MN_SetCvar(cvarName, selectBoxOption->value, 0);
			if (*selectBoxOption->action) {
#ifdef DEBUG
				if (selectBoxOption->action[strlen(selectBoxOption->action) - 1] != ';')
					Com_Printf("selectbox option with none terminated action command");
#endif
				Cbuf_AddText(selectBoxOption->action);
			}
		}
	}
}

void MN_RegisterNodeSelectBox (nodeBehaviour_t *behaviour)
{
	behaviour->name = "selectbox";
	behaviour->draw = MN_DrawSelectBoxNode;
	behaviour->leftClick = MN_SelectboxClick;
}
