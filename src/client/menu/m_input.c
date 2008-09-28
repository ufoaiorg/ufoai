/**
 * @file m_input.c
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
#include "../cl_map.h"
#include "m_main.h"
#include "m_parse.h"
#include "m_input.h"
#include "m_inventory.h"
#include "m_node_text.h"
#include "m_draw.h"
#include "../cl_global.h"

cvar_t *mn_inputlength;

/**
 * @sa MN_DisplayNotice
 */
static void MN_CheckCvar (const cvar_t *cvar)
{
	if (cvar->modified) {
		if (cvar->flags & CVAR_CONTEXT) {
			MN_DisplayNotice(_("This change requires a restart"), 2000);
		} else if (cvar->flags & CVAR_IMAGES) {
			MN_DisplayNotice(_("This change might require a restart"), 2000);
		}
	}
}

/**
 * @param[in] str Might be NULL if you want to set a float value
 */
static void MN_SetCvar (const char *name, const char *str, float value)
{
	const cvar_t *cvar;
	cvar = Cvar_FindVar(name);
	if (!cvar) {
		Com_Printf("Could not find cvar '%s'\n", name);
		return;
	}
	/* strip '*cvar ' from data[0] - length is already checked above */
	if (str)
		Cvar_Set(cvar->name, str);
	else
		Cvar_SetValue(cvar->name, value);
	MN_CheckCvar(cvar);
}

/**
 * @sa IN_Parse
 */
qboolean MN_CursorOnMenu (int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu = mn.menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next)
			if (MN_CheckNodeZone(node, x, y)) {
				/* found an element */
				/*MN_FocusSetNode(node);*/
				return qtrue;
			}

		if (menu->renderNode) {
			/* don't care about non-rendered windows */
			if (menu->renderNode->invis)
				return qtrue;
			else
				return qfalse;
		}
	}

	return qfalse;
}

/**
 * @brief Handles checkboxes clicks
 */
static void MN_CheckboxClick (menuNode_t * node)
{
	int value;
	const char *cvarName;

	assert(node->data[MN_DATA_MODEL_SKIN_OR_CVAR]);
	/* no cvar? */
	if (Q_strncmp(node->data[MN_DATA_MODEL_SKIN_OR_CVAR], "*cvar", 5))
		return;

	cvarName = &((const char *)node->data[MN_DATA_MODEL_SKIN_OR_CVAR])[6];
	value = Cvar_VariableInteger(cvarName) ^ 1;
	MN_SetCvar(cvarName, NULL, value);
}

/**
 * @brief Handles selectboxes clicks
 * @sa MN_SELECTBOX
 */
static void MN_SelectboxClick (menu_t * menu, menuNode_t * node, int y)
{
	selectBoxOptions_t* selectBoxOption;
	int clickedAtOption;

	clickedAtOption = (y - node->pos[1]);

	if (node->size[1])
		clickedAtOption = (clickedAtOption - node->size[1]) / node->size[1];
	else
		clickedAtOption = (clickedAtOption - SELECTBOX_DEFAULT_HEIGHT) / SELECTBOX_DEFAULT_HEIGHT; /* default height - see selectbox.tga */

	if (clickedAtOption < 0 || clickedAtOption >= node->height)
		return;

	/* the cvar string is stored in data[MN_DATA_MODEL_SKIN_OR_CVAR] */
	/* no cvar given? */
	if (!node->data[MN_DATA_MODEL_SKIN_OR_CVAR] || !*(char*)node->data[MN_DATA_MODEL_SKIN_OR_CVAR]) {
		Com_Printf("MN_SelectboxClick: node '%s' doesn't have a valid cvar assigned (menu %s)\n", node->name, node->menu->name);
		return;
	}

	/* no cvar? */
	if (Q_strncmp((const char *)node->data[MN_DATA_MODEL_SKIN_OR_CVAR], "*cvar", 5))
		return;

	/* only execute the click stuff if the selectbox is active */
	if (node->state) {
		selectBoxOption = node->options;
		for (; clickedAtOption > 0 && selectBoxOption; selectBoxOption = selectBoxOption->next) {
			clickedAtOption--;
		}
		if (selectBoxOption) {
			const char *cvarName = &((const char *)node->data[MN_DATA_MODEL_SKIN_OR_CVAR])[6];
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

/**
 * @brief Handles the bar cvar values
 * @sa Key_Message
 */
static void MN_BarClick (menu_t * menu, menuNode_t * node, int x)
{
	char var[MAX_VAR];

	if (!node->mousefx)
		return;

	Q_strncpyz(var, node->data[2], sizeof(var));
	/* no cvar? */
	if (!Q_strncmp(var, "*cvar", 5)) {
		/* normalize it */
		const float frac = (float) (x - node->pos[0]) / node->size[0];
		const float min = MN_GetReferenceFloat(menu, node->data[1]);
		const float value = min + frac * (MN_GetReferenceFloat(menu, node->data[0]) - min);
		/* in the case of MN_BAR the first three data array values are float values - see menuDataValues_t */
		MN_SetCvar(&var[6], NULL, value);
	}
}

/**
 * @brief Activates the model rotation
 * @note set the mouse space to MS_ROTATE
 * @sa rotateAngles
 */
static void MN_ModelClick (menuNode_t * node)
{
	mouseSpace = MS_ROTATE;
	/* modify node->angles (vec3_t) if you rotate the model */
	rotateAngles = node->angles;
}


/**
 * @brief Calls the script command for a text node that is clickable
 * @note The node must have the click parameter in it's menu definition or there
 * must be a console command that has the same name as the node has + _click
 * attached.
 * @sa MN_TextRightClick
 * @todo Check for scrollbars and when one would click them scroll according to
 * mouse movement, maybe implement a new mousespace (MS_* - @sa cl_input.c)
 */
void MN_TextClick (menuNode_t * node, int mouseOver)
{
	char cmd[MAX_VAR];
	Com_sprintf(cmd, sizeof(cmd), "%s_click", node->name);
	if (Cmd_Exists(cmd))
		Cbuf_AddText(va("%s %i\n", cmd, mouseOver - 1));
	else if (node->click && node->click->type == EA_CMD) {
		assert(node->click->data);
		Cbuf_AddText(va("%s %i\n", (const char *)node->click->data, mouseOver - 1));
	}
}

/**
 * @brief Calls the script command for a text node that is clickable via right mouse button
 * @note The node must have the rclick parameter
 * @sa MN_TextClick
 */
static void MN_TextRightClick (menuNode_t * node, int mouseOver)
{
	char cmd[MAX_VAR];
	Com_sprintf(cmd, sizeof(cmd), "%s_rclick", node->name);
	if (Cmd_Exists(cmd))
		Cbuf_AddText(va("%s %i\n", cmd, mouseOver - 1));
}

/**
 * @brief Left click on the basemap
 * @sa MN_BaseMapRightClick
 * @param[in] node The menu node definition for the base map
 * @param[in,out] base The base we are just viewing (and clicking)
 * @param[in] x The x screen coordinate
 * @param[in] y The y screen coordinate
 */
static void MN_BaseMapClick (const menuNode_t *node, base_t *base, int x, int y)
{
	int row, col;

	assert(base);

	if (gd.baseAction == BA_NEWBUILDING) {
		assert(base->buildingCurrent);
		for (row = 0; row < BASE_SIZE; row++)
			for (col = 0; col < BASE_SIZE; col++) {
				if (x >= base->map[row][col].posX
				 && x < base->map[row][col].posX + node->size[0] / BASE_SIZE
				 && y >= base->map[row][col].posY
				 && y < base->map[row][col].posY + node->size[1] / BASE_SIZE) {
					/* we're on the tile the player clicked */
					if (!base->map[row][col].building && !base->map[row][col].blocked) {
						if (!base->buildingCurrent->needs
						 || (col < BASE_SIZE - 1 && !base->map[row][col + 1].building && !base->map[row][col + 1].blocked)
						 || (col > 0 && !base->map[row][col - 1].building && !base->map[row][col - 1].blocked))
						/* Set position for a new building */
						B_SetBuildingByClick(base, base->buildingCurrent, row, col);
					}
					return;
				}
			}
	}

	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++)
			if (base->map[row][col].building && x >= base->map[row][col].posX
			 && x < base->map[row][col].posX + node->size[0] / BASE_SIZE && y >= base->map[row][col].posY
			 && y < base->map[row][col].posY + node->size[1] / BASE_SIZE) {
				const building_t *entry = base->map[row][col].building;
				if (!entry)
					Sys_Error("MN_BaseMapClick: no entry at %i:%i\n", x, y);

				assert(!base->map[row][col].blocked);

				B_BuildingOpenAfterClick(base, entry);
				gd.baseAction = BA_NONE;
				return;
			}
}

/**
 * @brief Right click on the basemap
 * @sa MN_BaseMapClick
 * @param[in] node The menu node definition for the base map
 * @param[in,out] base The base we are just viewing (and clicking)
 * @param[in] x The x screen coordinate
 * @param[in] y The y screen coordinate
 */
static void MN_BaseMapRightClick (const menuNode_t *node, base_t *base, int x, int y)
{
	int row, col;

	assert(base);

	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++)
			if (base->map[row][col].building && x >= base->map[row][col].posX
			 && x < base->map[row][col].posX + node->size[0] / BASE_SIZE && y >= base->map[row][col].posY
			 && y < base->map[row][col].posY + node->size[1] / BASE_SIZE) {
				building_t *entry = base->map[row][col].building;
				if (!entry)
					Sys_Error("MN_BaseMapRightClick: no entry at %i:%i\n", x, y);

				assert(!base->map[row][col].blocked);
				B_MarkBuildingDestroy(base, entry);
				return;
			}
}

/**
 * @brief Is called everytime one clickes on a menu/screen. Then checks if anything needs to be executed in the earea of the click (e.g. button-commands, inventory-handling, geoscape-stuff, etc...)
 * @sa MN_ModelClick
 * @sa MN_TextRightClick
 * @sa MN_TextClick
 * @sa MN_Drag
 * @sa MN_BarClick
 * @sa MN_CheckboxClick
 * @sa MN_BaseMapClick
 * @sa MAP_3DMapClick
 * @sa MAP_MapClick
 * @sa MN_ExecuteActions
 * @sa MN_RightClick
 * @sa Key_Message
 * @sa CL_MessageMenu_f
 * @note inline editing of cvars (e.g. save dialog) is done in Key_Message
 */
void MN_Click (int x, int y)
{
	menuNode_t *node;
	int sp, mouseOver;
	qboolean clickedInside = qfalse;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu_t *menu = mn.menuStack[--sp];
		menuNode_t *execute_node = NULL;
		for (node = menu->firstNode; node; node = node->next) {
			if (node->type != MN_CONTAINER && node->type != MN_CHECKBOX
			 && node->type != MN_SELECTBOX && !node->click)
				continue;

			/* check whether mouse is over this node */
			if (mouseSpace == MS_DRAGITEM && node->type == MN_CONTAINER && dragInfo.item.t) {
				int itemX = 0;
				int itemY = 0;

				/** We calculate the position of the top-left corner of the dragged
				 * item in oder to compensate for the centered-drawn cursor-item.
				 * Or to be more exact, we calculate the relative offset from the cursor
				 * location to the middle of the top-left square of the item.
				 * @sa m_draw.c:MN_DrawMenus:MN_CONTAINER */
				if (dragInfo.item.t) {
					itemX = C_UNIT * dragInfo.item.t->sx / 2;	/* Half item-width. */
					itemY = C_UNIT * dragInfo.item.t->sy / 2;	/* Half item-height. */

					/* Place relative center in the middle of the square. */
					itemX -= C_UNIT / 2;
					itemY -= C_UNIT / 2;
				}
				mouseOver = MN_CheckNodeZone(node, x, y) || MN_CheckNodeZone(node, x - itemX, y - itemY);
			} else {
				mouseOver = MN_CheckNodeZone(node, x, y);
			}

			if (!mouseOver)
				continue;

			/* check whether we clicked at least on one menu node */
			clickedInside = qtrue;

			/* found a node -> do actions */
			switch (node->type) {
			case MN_CONTAINER:
				MN_Drag(node, baseCurrent, x, y, qfalse);
				break;
			case MN_BAR:
				MN_BarClick(menu, node, x);
				break;
			case MN_BASEMAP:
				MN_BaseMapClick(node, baseCurrent, x, y);
				break;
			case MN_MAP:
				MAP_MapClick(node, x, y);
				break;
			case MN_CHECKBOX:
				MN_CheckboxClick(node);
				break;
			case MN_SELECTBOX:
				MN_SelectboxClick(menu, node, y);
				break;
			case MN_MODEL:
				MN_ModelClick(node);
				break;
			case MN_TEXT:
				MN_TextClick(node, mouseOver);
				execute_node = node;
				mn.mouseRepeat.textLine = mouseOver;
				break;
			default:
				/* Save the action for later execution. */
				if (node->click && (node->click->type != EA_NULL))
					execute_node = node;
				break;
			}
		}

		/* Only execute the last-found (i.e. from the top-most displayed node) action found.
		 * Especially needed for button-nodes that are (partially) overlayed and all have actions defined.
		 * e.g. the firemode checkboxes.
		 */
		if (execute_node) {
			mn.mouseRepeat.node = execute_node;
			mn.mouseRepeat.lastclicked = cls.realtime;
			if (execute_node->repeat) {
				mouseSpace = MS_LHOLD;
				/* Reset number of click */
				mn.mouseRepeat.numClick = 1;
				if (execute_node->clickDelay)
					mn.mouseRepeat.clickDelay = execute_node->clickDelay;
				else
					mn.mouseRepeat.clickDelay = 300;
				/** Delay between the 2 first actions is longer than the delay between other actions (see IN_Parse()) */
				mn.mouseRepeat.nexttime = cls.realtime + mn.mouseRepeat.clickDelay;
				mn.mouseRepeat.menu = menu;
				mn.mouseRepeat.action = execute_node->click;
			}
			if (execute_node->type != MN_TEXT)
				MN_ExecuteActions(menu, execute_node->click);
		}

		/** @todo maybe we should also check sp == mn.menuStackPos here */
		if (!clickedInside && menu->leaveNode)
			MN_ExecuteActions(menu, menu->leaveNode->click);

		/* don't care about non-rendered windows */
		if (menu->renderNode || menu->popupNode)
			return;
	}
}

/**
 * @sa MAP_ResetAction
 * @sa MN_TextRightClick
 * @sa MN_ExecuteActions
 * @sa MN_Click
 * @sa MN_MiddleClick
 * @sa MN_MouseWheel
 */
void MN_RightClick (int x, int y)
{
	int mouseOver;
	int sp = mn.menuStackPos;

	while (sp > 0) {
		menu_t *menu = mn.menuStack[--sp];
		menuNode_t *node;

		for (node = menu->firstNode; node; node = node->next) {
			/* no right click for this node defined */
			if (node->type != MN_CONTAINER && !node->rclick)
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			/* found a node -> do actions */
			switch (node->type) {
			case MN_CONTAINER:
				MN_Drag(node, baseCurrent, x, y, qtrue);
				break;
			case MN_BASEMAP:
				MN_BaseMapRightClick(node, baseCurrent, x, y);
				break;
			case MN_MAP:
				if (!gd.combatZoomOn || !gd.combatZoomedUfo) {
					if (!cl_3dmap->integer)
						mouseSpace = MS_SHIFTMAP;
					else
						mouseSpace = MS_SHIFT3DMAP;
					MAP_StopSmoothMovement();
				}
				break;
			case MN_TEXT:
				MN_TextRightClick(node, mouseOver);
				break;
			default:
				MN_ExecuteActions(menu, node->rclick);
				break;
			}
		}

		if (menu->renderNode || menu->popupNode)
			/* don't care about non-rendered windows */
			return;
	}
}

/**
 * @sa MN_Click
 * @sa MN_RightClick
 * @sa MN_MouseWheel
 */
void MN_MiddleClick (int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp, mouseOver;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu = mn.menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next) {
			/* no middle click for this node defined */
			if (!node->mclick)
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			/* found a node -> do actions */
			switch (node->type) {
			case MN_MAP:
				mouseSpace = MS_ZOOMMAP;
				break;
			default:
				MN_ExecuteActions(menu, node->mclick);
				break;
			}
		}

		if (menu->renderNode || menu->popupNode)
			/* don't care about non-rendered windows */
			return;
	}
}

/**
 * @brief Called when we are in menu mode and scroll via mousewheel
 * @note The geoscape zooming code is here, too (also in CL_ParseInput)
 * @sa MN_Click
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 * @sa MN_Click
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseWheel (qboolean down, int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp, mouseOver;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu = mn.menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next) {
			/* no middle click for this node defined */
			/* both wheelUp & wheelDown required */
			if (!node->wheel && !(node->wheelUp && node->wheelDown))
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			/* found a node -> do actions */
			switch (node->type) {
			case MN_MAP:
				if (gd.combatZoomOn  && gd.combatZoomedUfo && !down) {
					return;
				} else if (gd.combatZoomOn && gd.combatZoomedUfo && down) {
					MAP_TurnCombatZoomOff();
					return;
				}
				ccs.zoom *= pow(0.995, (down ? 10: -10));
				if (ccs.zoom < cl_mapzoommin->value)
					ccs.zoom = cl_mapzoommin->value;
				else if (ccs.zoom > cl_mapzoommax->value) {
					ccs.zoom = cl_mapzoommax->value;
					if (!down) {
						MAP_TurnCombatZoomOn();
					}
				}

				if (!cl_3dmap->integer) {
					if (ccs.center[1] < 0.5 / ccs.zoom)
						ccs.center[1] = 0.5 / ccs.zoom;
					if (ccs.center[1] > 1.0 - 0.5 / ccs.zoom)
						ccs.center[1] = 1.0 - 0.5 / ccs.zoom;
				}
				MAP_StopSmoothMovement();
				break;
			case MN_TEXT:
				if (node->wheelUp && node->wheelDown) {
					MN_ExecuteActions(menu, (down ? node->wheelDown : node->wheelUp));
				} else {
					MN_TextScroll(node, (down ? 1 : -1));
					/* they can also have script commands assigned */
					MN_ExecuteActions(menu, node->wheel);
				}
				break;
			default:
				if (node->wheelUp && node->wheelDown)
					MN_ExecuteActions(menu, (down ? node->wheelDown : node->wheelUp));
				else
					MN_ExecuteActions(menu, node->wheel);
				break;
			}
		}

		if (menu->renderNode || menu->popupNode)
			/* don't care about non-rendered windows */
			return;
	}
}
