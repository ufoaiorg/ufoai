/**
 * @file cl_menu.c
 * @brief Client menu functions.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "client.h"
#include "cl_global.h"
#include "menu/m_main.h"
#include "menu/m_popup.h"
#include "menu/m_parse.h"
#include "menu/m_inventory.h"
#include "menu/m_font.h"
#include "menu/m_tooltip.h"
#include "menu/m_messages.h"
#include "../renderer/r_mesh.h"
#include "../renderer/r_draw.h"
#include "../renderer/r_mesh_anim.h"

static cvar_t *mn_debugmenu;
cvar_t *mn_inputlength;

/**
 * @brief Update the menu values with current gametype values
 */
static void MN_UpdateGametype_f (void)
{
	Com_SetGameType();
}

/**
 * @brief Switch to the next multiplayer game type
 * @sa MN_PrevGametype_f
 */
static void MN_ChangeGametype_f (void)
{
	int i, newType;
	gametype_t* gt;
	mapDef_t *md;
	const char *newGameTypeID = NULL;
	qboolean next = qtrue;

	/* no types defined or parsed */
	if (numGTs == 0)
		return;

	md = &csi.mds[ccs.multiplayerMapDefinitionIndex];
	if (!md || !md->multiplayer) {
		Com_Printf("MN_ChangeGametype_f: No mapdef for the map\n");
		return;
	}

	/* previous? */
	if (!Q_strcmp(Cmd_Argv(0), "mn_prevgametype")) {
		next = qfalse;
	}

	if (md->gameTypes) {
		linkedList_t *list = md->gameTypes;
		linkedList_t *old = NULL;
		while (list) {
			if (!Q_strcmp((const char*)list->data, gametype->string)) {
				if (next) {
					if (list->next)
						newGameTypeID = (const char *)list->next->data;
					else
						newGameTypeID = (const char *)md->gameTypes->data;
				} else {
					/* only one or the first entry */
					if (old) {
						newGameTypeID = (const char *)old->data;
					} else {
						while (list->next)
							list = list->next;
						newGameTypeID = (const char *)list->data;
					}
				}
				break;
			}
			old = list;
			list = list->next;
		}
		/* current value is not valid for this map, set to first valid gametype */
		if (!list)
			newGameTypeID = (const char *)md->gameTypes->data;
	} else {
		for (i = 0; i < numGTs; i++) {
			gt = &gts[i];
			if (!Q_strncmp(gt->id, gametype->string, MAX_VAR)) {
				if (next) {
					newType = (i + 1);
					if (newType >= numGTs)
						newType = 0;
				} else {
					newType = (i - 1);
					if (newType < 0)
						newType = numGTs - 1;
				}

				newGameTypeID = gts[newType].id;
				break;
			}
		}
	}
	if (newGameTypeID) {
		Cvar_Set("gametype", newGameTypeID);
		Com_SetGameType();
	}
}

/**
 * @brief Starts a server and checks if the server loads a team unless he is a dedicated
 * server admin
 */
static void MN_StartServer_f (void)
{
	char map[MAX_VAR];
	mapDef_t *md;
	cvar_t* mn_serverday = Cvar_Get("mn_serverday", "1", 0, "Decides whether the server starts the day or the night version of the selected map");
	aircraft_t *aircraft;

	if (ccs.singleplayer)
		return;

	aircraft = AIR_AircraftGetFromIdx(0);
	assert(aircraft);

	if (!sv_dedicated->integer && !B_GetNumOnTeam(aircraft)) {
		Com_Printf("MN_StartServer_f: Multiplayer team not loaded, please choose your team now.\n");
		Cmd_ExecuteString("assign_initial");
		return;
	}

	if (Cvar_VariableInteger("sv_teamplay")
		&& Cvar_VariableValue("sv_maxsoldiersperplayer") > Cvar_VariableValue("sv_maxsoldiersperteam")) {
		MN_Popup(_("Settings doesn't make sense"), _("Set soldiers per player lower than soldiers per team"));
		return;
	}

	md = &csi.mds[ccs.multiplayerMapDefinitionIndex];
	if (!md || !md->multiplayer)
		return;
	assert(md->map);

	Com_sprintf(map, sizeof(map), "map %s%c %s", md->map, mn_serverday->integer ? 'd' : 'n', md->param ? md->param : "");

	/* let the (local) server know which map we are running right now */
	csi.currentMD = md;

	Cmd_ExecuteString(map);

	Cvar_Set("mn_main", "multiplayerInGame");
	MN_PushMenu("multiplayer_wait");
	Cvar_Set("mn_active", "multiplayer_wait");
}

/**
 * @brief Determine the position and size of render
 * @param[in] menu : use its position and size properties
 */
void MN_SetViewRect (const menu_t* menu)
{
	menuNode_t* menuNode = menu ? (menu->renderNode ? menu->renderNode : (menu->popupNode ? menu->popupNode : NULL)): NULL;

	if (!menuNode) {
		/* render the full screen */
		viddef.x = viddef.y = 0;
		viddef.viewWidth = viddef.width;
		viddef.viewHeight = viddef.height;
	} else if (menuNode->invis) {
		/* don't draw the scene */
		viddef.x = viddef.y = 0;
		viddef.viewWidth = viddef.viewHeight = 0;
	} else {
		/* the menu has a view size specified */
		viddef.x = menuNode->pos[0] * viddef.rx;
		viddef.y = menuNode->pos[1] * viddef.ry;
		viddef.viewWidth = menuNode->size[0] * viddef.rx;
		viddef.viewHeight = menuNode->size[1] * viddef.ry;
	}
}


/**
 * @brief Left click on the basemap
 */
void MN_BaseMapClick (menuNode_t *node, int x, int y)
{
	int row, col;
	building_t	*entry;

	assert(baseCurrent);

	if (baseCurrent->buildingCurrent && baseCurrent->buildingCurrent->buildingStatus == B_STATUS_NOT_SET) {
		for (row = 0; row < BASE_SIZE; row++)
			for (col = 0; col < BASE_SIZE; col++)
				if (baseCurrent->map[row][col] == BASE_FREESLOT && x >= baseCurrent->posX[row][col]
					&& x < baseCurrent->posX[row][col] + node->size[0] / BASE_SIZE && y >= baseCurrent->posY[row][col]
					&& y < baseCurrent->posY[row][col] + node->size[1] / BASE_SIZE) {
					/* Set position for a new building */
					B_SetBuildingByClick(row, col);
					return;
				}
	}

	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++)
			if (baseCurrent->map[row][col] > BASE_FREESLOT && x >= baseCurrent->posX[row][col]
				&& x < baseCurrent->posX[row][col] + node->size[0] / BASE_SIZE && y >= baseCurrent->posY[row][col]
				&& y < baseCurrent->posY[row][col] + node->size[1] / BASE_SIZE) {
				entry = B_GetBuildingByIdx(baseCurrent, baseCurrent->map[row][col]);
				if (!entry)
					Sys_Error("MN_BaseMapClick: no entry at %i:%i\n", x, y);

				if (*entry->onClick) {
					baseCurrent->buildingCurrent = entry;
					Cmd_ExecuteString(va("%s %i", entry->onClick, baseCurrent->idx));
					baseCurrent->buildingCurrent = NULL;
					gd.baseAction = BA_NONE;
				}
#if 0
				else {
					/* Click on building : display its properties in the building menu */
					MN_PushMenu("buildings");
					baseCurrent->buildingCurrent = entry;
					B_BuildingStatus(baseCurrent, baseCurrent->buildingCurrent);
				}
#else
				else
					UP_OpenWith(entry->pedia);
#endif
				return;
			}
}

/**
 * @brief Right click on the basemap
 */
void MN_BaseMapRightClick (menuNode_t *node, int x, int y)
{
	int row, col;
	building_t	*entry;

	assert(baseCurrent);

	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++)
			if (baseCurrent->map[row][col] > BASE_FREESLOT && x >= baseCurrent->posX[row][col]
				&& x < baseCurrent->posX[row][col] + node->size[0] / BASE_SIZE && y >= baseCurrent->posY[row][col]
				&& y < baseCurrent->posY[row][col] + node->size[1] / BASE_SIZE) {
				entry = B_GetBuildingByIdx(baseCurrent, baseCurrent->map[row][col]);
				if (!entry)
					Sys_Error("MN_BaseMapClick: no entry at %i:%i\n", x, y);
				B_MarkBuildingDestroy(baseCurrent, entry);
				return;
			}
}

/*
==============================================================
MENU DRAWING
==============================================================
*/

/**
 * @brief Draws the menu stack
 * @sa SCR_UpdateScreen
 */
void MN_DrawMenus (void)
{
	modelInfo_t mi;
	modelInfo_t pmi;
	menuNode_t *node;
	menu_t *menu;
	animState_t *as;
	const char *ref;
	const char *font;
	char *anim;					/* model anim state */
	char source[MAX_VAR] = "";
	int sp, pp;
	item_t item = {1, NONE, NONE, 0, 0}; /* 1 so it's not reddish; fake item anyway */
	vec4_t color;
	int mouseOver = 0;
	int y, i;
	message_t *message;
	menuModel_t *menuModel = NULL, *menuModelParent = NULL;
	int width, height;
	invList_t *itemHover = NULL;
	char *tab, *end;
	static menuNode_t *selectBoxNode = NULL;

	/* render every menu on top of a menu with a render node */
	pp = 0;
	sp = mn.menuStackPos;
	while (sp > 0) {
		if (mn.menuStack[--sp]->renderNode)
			break;
		if (mn.menuStack[sp]->popupNode)
			pp = sp;
	}
	if (pp < sp)
		pp = sp;

	while (sp < mn.menuStackPos) {
		menu = mn.menuStack[sp++];
		/* event node */
		if (menu->eventNode) {
			if (menu->eventNode->timeOut > 0 && (menu->eventNode->timeOut == 1 || (!menu->eventTime || (menu->eventTime + menu->eventNode->timeOut < cls.realtime)))) {
				menu->eventTime = cls.realtime;
				MN_ExecuteActions(menu, menu->eventNode->click);
#ifdef DEBUG
				Com_DPrintf(DEBUG_CLIENT, "Event node '%s' '%i\n", menu->eventNode->name, menu->eventNode->timeOut);
#endif
			}
		}
		for (node = menu->firstNode; node; node = node->next) {
			if (!node->invis && ((node->data[MN_DATA_STRING_OR_IMAGE_OR_MODEL] /* 0 are images, models and strings e.g. */
					|| node->type == MN_CONTAINER || node->type == MN_TEXT || node->type == MN_BASEMAP || node->type == MN_MAP)
					|| node->type == MN_CHECKBOX || node->type == MN_SELECTBOX || node->type == MN_LINESTRIP
					|| ((node->type == MN_ZONE || node->type == MN_CONTAINER) && node->bgcolor[3]))) {
				/* if construct */
				if (!MN_CheckCondition(node))
					continue;

				/* timeout? */
				if (node->timePushed) {
					if (node->timePushed + node->timeOut < cls.realtime) {
						node->timePushed = 0;
						node->invis = qtrue;
						/* only timeout this once, otherwise there is a new timeout after every new stack push */
						if (node->timeOutOnce)
							node->timeOut = 0;
						Com_DPrintf(DEBUG_CLIENT, "MN_DrawMenus: timeout for node '%s'\n", node->name);
						continue;
					}
				}

				/* mouse effects */
				/* don't allow to open more than one selectbox */
				if (selectBoxNode && selectBoxNode != node)
					node->state = qfalse;
				else if (sp > pp) {
					/* in and out events */
					mouseOver = MN_CheckNodeZone(node, mousePosX, mousePosY);
					if (mouseOver != node->state) {
						/* maybe we are leaving to another menu */
						menu->hoverNode = NULL;
						if (mouseOver)
							MN_ExecuteActions(menu, node->mouseIn);
						else
							MN_ExecuteActions(menu, node->mouseOut);
						node->state = mouseOver;
						selectBoxNode = NULL;
					}
				}

				/* check node size x and y value to check whether they are zero */
				if (node->bgcolor && node->size[0] && node->size[1] && node->pos) {
					R_DrawFill(node->pos[0] - node->padding, node->pos[1] - node->padding, node->size[0] + (node->padding*2), node->size[1] + (node->padding*2), 0, node->bgcolor);
				}

				if (node->border && node->bordercolor && node->size[0] && node->size[1] && node->pos) {
					/* left */
					R_DrawFill(node->pos[0] - node->padding - node->border, node->pos[1] - node->padding - node->border,
						node->border, node->size[1] + (node->padding*2) + (node->border*2), 0, node->bordercolor);
					/* right */
					R_DrawFill(node->pos[0] + node->size[0] + node->padding, node->pos[1] - node->padding - node->border,
						node->border, node->size[1] + (node->padding*2) + (node->border*2), 0, node->bordercolor);
					/* top */
					R_DrawFill(node->pos[0] - node->padding, node->pos[1] - node->padding - node->border,
						node->size[0] + (node->padding*2), node->border, 0, node->bordercolor);
					/* down */
					R_DrawFill(node->pos[0] - node->padding, node->pos[1] + node->size[1] + node->padding,
						node->size[0] + (node->padding*2), node->border, 0, node->bordercolor);
				}

				/* mouse darken effect */
				VectorScale(node->color, 0.8, color);
				color[3] = node->color[3];
				if (node->mousefx && node->type == MN_PIC && mouseOver && sp > pp)
					R_ColorBlend(color);
				else if (node->type != MN_SELECTBOX)
					R_ColorBlend(node->color);

				/* get the reference */
				if (node->type != MN_BAR && node->type != MN_CONTAINER && node->type != MN_BASEMAP
					&& node->type != MN_TEXT && node->type != MN_MAP && node->type != MN_ZONE && node->type != MN_LINESTRIP) {
					ref = MN_GetReferenceString(menu, node->data[MN_DATA_STRING_OR_IMAGE_OR_MODEL]);
					if (!ref) {
						/* these have default values for their image */
						if (node->type != MN_SELECTBOX && node->type != MN_CHECKBOX) {
							/* bad reference */
							node->invis = qtrue;
							Com_Printf("MN_DrawActiveMenus: node \"%s\" bad reference \"%s\" (menu %s)\n", node->name, (char*)node->data, node->menu->name);
							continue;
						}
					} else
						Q_strncpyz(source, ref, MAX_VAR);
				} else {
					ref = NULL;
					*source = '\0';
				}

				switch (node->type) {
				case MN_PIC:
					/* hack for ekg pics */
					if (!Q_strncmp(node->name, "ekg_", 4)) {
						int pt;

						if (node->name[4] == 'm')
							pt = Cvar_VariableInteger("mn_morale") / 2;
						else
							pt = Cvar_VariableInteger("mn_hp");
						node->texl[1] = (3 - (int) (pt < 60 ? pt : 60) / 20) * 32;
						node->texh[1] = node->texl[1] + 32;
						node->texl[0] = -(int) (0.01 * (node->name[4] - 'a') * cl.time) % 64;
						node->texh[0] = node->texl[0] + node->size[0];
					}
					if (ref && *ref)
						R_DrawNormPic(node->pos[0], node->pos[1], node->size[0], node->size[1],
								node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, ref);
					break;

				case MN_CHECKBOX:
					{
						const char *image;
						/* image set? */
						if (ref && *ref)
							image = ref;
						else
							image = "menu/checkbox";
						ref = MN_GetReferenceString(menu, node->data[MN_DATA_MODEL_SKIN_OR_CVAR]);
						switch (*ref) {
						case '0':
							R_DrawNormPic(node->pos[0], node->pos[1], node->size[0], node->size[1],
								node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, va("%s_unchecked", image));
							break;
						case '1':
							R_DrawNormPic(node->pos[0], node->pos[1], node->size[0], node->size[1],
								node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, va("%s_checked", image));
							break;
						default:
							Com_Printf("Error - invalid value for MN_CHECKBOX node - only 0/1 allowed\n");
						}
						break;
					}

				case MN_SELECTBOX:
					{
						selectBoxOptions_t* selectBoxOption;
						int selBoxX, selBoxY;
						const char *image = ref;
						if (!image)
							image = "menu/selectbox";
						if (!node->data[MN_DATA_MODEL_SKIN_OR_CVAR]) {
							Com_Printf("MN_DrawMenus: skip node '%s' (MN_SELECTBOX) - no cvar given (menu %s)\n", node->name, node->menu->name);
							node->invis = qtrue;
							break;
						}
						ref = MN_GetReferenceString(menu, node->data[MN_DATA_MODEL_SKIN_OR_CVAR]);

						font = MN_GetFont(menu, node);
						selBoxX = node->pos[0] + SELECTBOX_SIDE_WIDTH;
						selBoxY = node->pos[1] + SELECTBOX_SPACER;

						/* left border */
						R_DrawNormPic(node->pos[0], node->pos[1], SELECTBOX_SIDE_WIDTH, node->size[1],
							SELECTBOX_SIDE_WIDTH, 20.0f, 0.0f, 0.0f, node->align, node->blend, image);
						/* stretched middle bar */
						R_DrawNormPic(node->pos[0] + SELECTBOX_SIDE_WIDTH, node->pos[1], node->size[0], node->size[1],
							12.0f, 20.0f, 7.0f, 0.0f, node->align, node->blend, image);
						/* right border (arrow) */
						R_DrawNormPic(node->pos[0] + SELECTBOX_SIDE_WIDTH + node->size[0], node->pos[1], SELECTBOX_DEFAULT_HEIGHT, node->size[1],
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
							R_DrawNormPic(node->pos[0], node->pos[1] + node->size[1], SELECTBOX_SIDE_WIDTH, node->size[1] * node->height,
								7.0f, 28.0f, 0.0f, 21.0f, node->align, node->blend, image);

							/* stretched middle bar */
							R_DrawNormPic(node->pos[0] + SELECTBOX_SIDE_WIDTH, node->pos[1] + node->size[1], node->size[0], node->size[1] * node->height,
								16.0f, 28.0f, 7.0f, 21.0f, node->align, node->blend, image);

							/* right side */
							R_DrawNormPic(node->pos[0] + SELECTBOX_SIDE_WIDTH + node->size[0], node->pos[1] + node->size[1], SELECTBOX_SIDE_WIDTH, node->size[1] * node->height,
								23.0f, 28.0f, 16.0f, 21.0f, node->align, node->blend, image);

							/* now draw all available options for this selectbox */
							for (selectBoxOption = node->options; selectBoxOption; selectBoxOption = selectBoxOption->next) {
								/* draw the hover effect */
								if (selectBoxOption->hovered)
									R_DrawFill(selBoxX, selBoxY, node->size[0], SELECTBOX_DEFAULT_HEIGHT, ALIGN_UL, node->color);
								/* print the option label */
								R_FontDrawString(font, node->align, selBoxX, selBoxY,
									selBoxX, node->pos[1] + node->size[1], node->size[0] - 4, 0,
									node->texh[0], _(selectBoxOption->label), 0, 0, NULL, qfalse);
								/* next entries' position */
								selBoxY += node->size[1];
								/* reset the hovering - will be recalculated next frame */
								selectBoxOption->hovered = qfalse;
							}
							/* left side */
							R_DrawNormPic(node->pos[0], selBoxY - SELECTBOX_SPACER, SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
								7.0f, 32.0f, 0.0f, 28.0f, node->align, node->blend, image);

							/* stretched middle bar */
							R_DrawNormPic(node->pos[0] + SELECTBOX_SIDE_WIDTH, selBoxY - SELECTBOX_SPACER, node->size[0], SELECTBOX_BOTTOM_HEIGHT,
								16.0f, 32.0f, 7.0f, 28.0f, node->align, node->blend, image);

							/* right bottom side */
							R_DrawNormPic(node->pos[0] + SELECTBOX_SIDE_WIDTH + node->size[0], selBoxY - SELECTBOX_SPACER,
								SELECTBOX_SIDE_WIDTH, SELECTBOX_BOTTOM_HEIGHT,
								23.0f, 32.0f, 16.0f, 28.0f, node->align, node->blend, image);
						}
						break;
					}

				case MN_STRING:
					font = MN_GetFont(menu, node);
					ref += node->horizontalScroll;
					/* blinking */
					if (!node->mousefx || cl.time % 1000 < 500)
						R_FontDrawString(font, node->align, node->pos[0], node->pos[1], node->pos[0], node->pos[1], node->size[0], 0, node->texh[0], ref, 0, 0, NULL, qfalse);
					else
						R_FontDrawString(font, node->align, node->pos[0], node->pos[1], node->pos[0], node->pos[1], node->size[0], node->size[1], node->texh[0], va("%s*\n", ref), 0, 0, NULL, qfalse);
					break;

				case MN_TEXT:
					if (mn.menuText[node->num]) {
						font = MN_GetFont(menu, node);
						MN_DrawTextNode(mn.menuText[node->num], NULL, font, node, node->pos[0], node->pos[1], node->size[0], node->size[1]);
					} else if (mn.menuTextLinkedList[node->num]) {
						font = MN_GetFont(menu, node);
						MN_DrawTextNode(NULL, mn.menuTextLinkedList[node->num], font, node, node->pos[0], node->pos[1], node->size[0], node->size[1]);
					} else if (node->num == TEXT_MESSAGESYSTEM) {
						if (node->data[MN_DATA_ANIM_OR_FONT])
							font = MN_GetReferenceString(menu, node->data[MN_DATA_ANIM_OR_FONT]);
						else
							font = "f_small";

						y = node->pos[1];
						node->textLines = 0;
						message = mn.messageStack;
						while (message) {
							if (node->textLines >= node->height) {
								/* @todo: Draw the scrollbars */
								break;
							}
							node->textLines++;

							/* maybe due to scrolling this line is not visible */
							if (node->textLines > node->textScroll) {
								int offset = 0;
								char text[TIMESTAMP_TEXT + MAX_MESSAGE_TEXT];
								/* get formatted date text and pixel width of text */
								MN_TimestampedText(text, message, sizeof(text));
								R_FontLength(font, text, &offset, &height);
								/* append remainder of message */
								Q_strcat(text, message->text, sizeof(text));
								R_FontLength(font, text, &width, &height);
								if (!width)
									break;
								if (width > node->pos[0] + node->size[0]) {
									int indent = node->pos[0];
									tab = text;
									while (qtrue) {
										y += R_FontDrawString(font, ALIGN_UL, indent, y, node->pos[0], node->pos[1], node->size[0], node->size[1], node->texh[0], tab, 0, 0, NULL, qfalse);
										/* we use a backslash to determine where to break the line */
										end = strstr(tab, "\\");
										if (!end)
											break;
										*end++ = '\0';
										tab = end;
										node->textLines++;
										if (node->textLines >= node->height)
											break;
										indent = offset;
									}
								} else {
									/* newline to space - we don't need this */
									while ((end = strstr(text, "\\")) != NULL)
										*end = ' ';

									y += R_FontDrawString(font, ALIGN_UL, node->pos[0], y, node->pos[0], node->pos[1], node->size[0], node->size[1], node->texh[0], text, 0, 0, NULL, qfalse);
								}
							}

							message = message->next;
						}
					}
					break;

				case MN_BAR:
					{
						float fac, bar_width;

						/* in the case of MN_BAR the first three data array values are float values - see menuDataValues_t */
						fac = node->size[0] / (MN_GetReferenceFloat(menu, node->data[0]) - MN_GetReferenceFloat(menu, node->data[1]));
						bar_width = (MN_GetReferenceFloat(menu, node->data[2]) - MN_GetReferenceFloat(menu, node->data[1])) * fac;
						R_DrawFill(node->pos[0], node->pos[1], bar_width, node->size[1], node->align, mouseOver ? color : node->color);
					}
					break;

				case MN_LINESTRIP:
					if (node->linestrips.numStrips > 0) {
						/* Draw all linestrips. */
						for (i = 0; i < node->linestrips.numStrips; i++) {
							/* Draw this line if it's valid. */
							if (node->linestrips.pointList[i] && (node->linestrips.numPoints[i] > 0)) {
								R_ColorBlend(node->linestrips.color[i]);
								R_DrawLineStrip(node->linestrips.numPoints[i], node->linestrips.pointList[i]);
							}
						}
					}
					break;

				case MN_CONTAINER:
					if (menuInventory) {
						vec3_t scale = {3.5, 3.5, 3.5};
						invList_t *ic;

						Vector4Set(color, 1, 1, 1, 1);

						if (node->mousefx == C_UNDEFINED)
							MN_FindContainer(node);
						if (node->mousefx == NONE)
							break;

						if (csi.ids[node->mousefx].single) {
							/* single item container (special case for left hand) */
							if (node->mousefx == csi.idLeft && !menuInventory->c[csi.idLeft]) {
								color[3] = 0.5;
								if (menuInventory->c[csi.idRight] && csi.ods[menuInventory->c[csi.idRight]->item.t].holdTwoHanded)
									MN_DrawItem(node->pos, menuInventory->c[csi.idRight]->item, node->size[0] / C_UNIT,
												node->size[1] / C_UNIT, 0, 0, scale, color);
							} else if (menuInventory->c[node->mousefx]) {
								if (node->mousefx == csi.idRight &&
										csi.ods[menuInventory->c[csi.idRight]->item.t].fireTwoHanded &&
										menuInventory->c[csi.idLeft]) {
									color[0] = color[1] = color[2] = color[3] = 0.5;
									MN_DrawDisabled(node);
								}
								MN_DrawItem(node->pos, menuInventory->c[node->mousefx]->item, node->size[0] / C_UNIT,
											node->size[1] / C_UNIT, 0, 0, scale, color);
							}
						} else {
							/* standard container */
							for (ic = menuInventory->c[node->mousefx]; ic; ic = ic->next) {
								MN_DrawItem(node->pos, ic->item, csi.ods[ic->item.t].sx, csi.ods[ic->item.t].sy, ic->x, ic->y, scale, color);
							}
						}
						/* draw free space if dragging - but not for idEquip */
						if (mouseSpace == MS_DRAG && node->mousefx != csi.idEquip)
							MN_InvDrawFree(menuInventory, node);

						/* Draw tooltip for weapon or ammo */
						if (mouseSpace != MS_DRAG && node->state && cl_show_tooltips->integer) {
							/* Find out where the mouse is */
							itemHover = Com_SearchInInventory(menuInventory,
								node->mousefx, (mousePosX - node->pos[0]) / C_UNIT, (mousePosY - node->pos[1]) / C_UNIT);
						}
					}
					break;

				case MN_ITEM:
					Vector4Copy(color_white, color);

					if (node->mousefx == C_UNDEFINED)
						MN_FindContainer(node);
					if (node->mousefx == NONE) {
						Com_Printf("no container for drawing this item (%s)...\n", ref);
						break;
					}

					for (item.t = 0; item.t < csi.numODs; item.t++)
						if (!Q_strncmp(ref, csi.ods[item.t].id, MAX_VAR))
							break;
					if (item.t == csi.numODs)
						break;

					MN_DrawItem(node->pos, item, 0, 0, 0, 0, node->scale, color);
					break;

				case MN_MODEL:
				{
					/* if true we have to reset the anim state and make sure the correct model is shown */
					qboolean updateModel = qfalse;
					/* set model properties */
					if (!*source)
						break;
					node->menuModel = MN_GetMenuModel(source);

					/* direct model name - no menumodel definition */
					if (!node->menuModel) {
						/* prevent the searching for a menumodel def in the next frame */
						menuModel = NULL;
						mi.model = R_RegisterModelShort(source);
						mi.name = source;
						if (!mi.model) {
							Com_Printf("Could not find model '%s'\n", source);
							break;
						}
					} else
						menuModel = node->menuModel;

					/* check whether the cvar value changed */
					if (Q_strncmp(node->oldRefValue, source, sizeof(node->oldRefValue))) {
						Q_strncpyz(node->oldRefValue, source, sizeof(node->oldRefValue));
						updateModel = qtrue;
					}

					mi.origin = node->origin;
					mi.angles = node->angles;
					mi.scale = node->scale;
					mi.center = node->center;
					mi.color = node->color;

					/* autoscale? */
					if (!node->scale[0]) {
						mi.scale = NULL;
						mi.center = node->size;
					}

					do {
						/* no animation */
						mi.frame = 0;
						mi.oldframe = 0;
						mi.backlerp = 0;
						if (menuModel) {
							assert(menuModel->model);
							mi.model = R_RegisterModelShort(menuModel->model);
							if (!mi.model) {
								menuModel = menuModel->next;
								/* list end */
								if (!menuModel)
									break;
								continue;
							}

							mi.skin = menuModel->skin;
							mi.name = menuModel->model;

							/* set mi pointers to menuModel */
							mi.origin = menuModel->origin;
							mi.angles = menuModel->angles;
							mi.center = menuModel->center;
							mi.color = menuModel->color;
							mi.scale = menuModel->scale;

							/* no tag and no parent means - base model or single model */
							if (!menuModel->tag && !menuModel->parent) {
								if (menuModel->menuTransformCnt) {
									for (i = 0; i < menuModel->menuTransformCnt; i++) {
										if (menu == menuModel->menuTransform[i].menuPtr) {
											/* Use menu scale if defined. */
											if (menuModel->menuTransform[i].useScale) {
												VectorCopy(menuModel->menuTransform[i].scale, mi.scale);
											} else {
												VectorCopy(node->scale, mi.scale);
											}

											/* Use menu angles if defined. */
											if (menuModel->menuTransform[i].useAngles) {
												VectorCopy(menuModel->menuTransform[i].angles, mi.angles);
											} else {
												VectorCopy(node->angles, mi.angles);
											}

											/* Use menu origin if defined. */
											if (menuModel->menuTransform[i].useOrigin) {
												VectorAdd(node->origin, menuModel->menuTransform[i].origin, mi.origin);
											} else {
												VectorCopy(node->origin, mi.origin);
											}
											break;
										}
									}
									/* not for this menu */
									if (i == menuModel->menuTransformCnt) {
										VectorCopy(node->scale, mi.scale);
										VectorCopy(node->angles, mi.angles);
										VectorCopy(node->origin, mi.origin);
									}
								} else {
									VectorCopy(node->scale, mi.scale);
									VectorCopy(node->angles, mi.angles);
									VectorCopy(node->origin, mi.origin);
								}
								Vector4Copy(node->color, mi.color);
								VectorCopy(node->center, mi.center);

								/* get the animation given by menu node properties */
								if (node->data[MN_DATA_ANIM_OR_FONT] && *(char *) node->data[MN_DATA_ANIM_OR_FONT]) {
									ref = MN_GetReferenceString(menu, node->data[MN_DATA_ANIM_OR_FONT]);
								/* otherwise use the standard animation from modelmenu defintion */
								} else
									ref = menuModel->anim;

								/* only base models have animations */
								if (ref && *ref) {
									as = &menuModel->animState;
									anim = R_AnimGetName(as, mi.model);
									/* initial animation or animation change */
									if (!anim || (anim && Q_strncmp(anim, ref, MAX_VAR)))
										R_AnimChange(as, mi.model, ref);
									else
										R_AnimRun(as, mi.model, cls.frametime * 1000);

									mi.frame = as->frame;
									mi.oldframe = as->oldframe;
									mi.backlerp = as->backlerp;
								}
								R_DrawModelDirect(&mi, NULL, NULL);
							/* tag and parent defined */
							} else {
								/* place this menumodel part on an already existing menumodel tag */
								assert(menuModel->parent);
								assert(menuModel->tag);
								menuModelParent = MN_GetMenuModel(menuModel->parent);
								if (!menuModelParent) {
									Com_Printf("Menumodel: Could not get the menuModel '%s'\n", menuModel->parent);
									break;
								}
								pmi.model = R_RegisterModelShort(menuModelParent->model);
								if (!pmi.model) {
									Com_Printf("Menumodel: Could not get the model '%s'\n", menuModelParent->model);
									break;
								}

								pmi.name = menuModelParent->model;
								pmi.origin = menuModelParent->origin;
								pmi.angles = menuModelParent->angles;
								pmi.scale = menuModelParent->scale;
								pmi.center = menuModelParent->center;
								pmi.color = menuModelParent->color;

								/* autoscale? */
								if (!mi.scale[0]) {
									mi.scale = NULL;
									mi.center = node->size;
								}

								as = &menuModelParent->animState;
								pmi.frame = as->frame;
								pmi.oldframe = as->oldframe;
								pmi.backlerp = as->backlerp;

								R_DrawModelDirect(&mi, &pmi, menuModel->tag);
							}
							menuModel = menuModel->next;
						} else {
							/* get skin */
							if (node->data[MN_DATA_MODEL_SKIN_OR_CVAR] && *(char *) node->data[MN_DATA_MODEL_SKIN_OR_CVAR])
								mi.skin = atoi(MN_GetReferenceString(menu, node->data[MN_DATA_MODEL_SKIN_OR_CVAR]));
							else
								mi.skin = 0;

							/* do animations */
							if (node->data[MN_DATA_ANIM_OR_FONT] && *(char *) node->data[MN_DATA_ANIM_OR_FONT]) {
								ref = MN_GetReferenceString(menu, node->data[MN_DATA_ANIM_OR_FONT]);
								if (updateModel) {
									/* model has changed but mem is already reserved in pool */
									if (node->data[MN_DATA_MODEL_ANIMATION_STATE]) {
										Mem_Free(node->data[MN_DATA_MODEL_ANIMATION_STATE]);
										node->data[MN_DATA_MODEL_ANIMATION_STATE] = NULL;
									}
								}
								if (!node->data[MN_DATA_MODEL_ANIMATION_STATE]) {
									as = (animState_t *) Mem_PoolAlloc(sizeof(animState_t), cl_genericPool, CL_TAG_NONE);
									R_AnimChange(as, mi.model, ref);
									node->data[MN_DATA_MODEL_ANIMATION_STATE] = as;
								} else {
									/* change anim if needed */
									as = node->data[MN_DATA_MODEL_ANIMATION_STATE];
									anim = R_AnimGetName(as, mi.model);
									if (anim && Q_strncmp(anim, ref, MAX_VAR))
										R_AnimChange(as, mi.model, ref);
									R_AnimRun(as, mi.model, cls.frametime * 1000);
								}

								mi.frame = as->frame;
								mi.oldframe = as->oldframe;
								mi.backlerp = as->backlerp;
							}

							/* place on tag */
							if (node->data[MN_DATA_MODEL_TAG]) {
								menuNode_t *search;
								char parent[MAX_VAR];
								char *tag;

								Q_strncpyz(parent, MN_GetReferenceString(menu, node->data[MN_DATA_MODEL_TAG]), MAX_VAR);
								tag = parent;
								/* tag "menuNodeName modelTag" */
								while (*tag && *tag != ' ')
									tag++;
								/* split node name and tag */
								*tag++ = 0;

								for (search = menu->firstNode; search != node && search; search = search->next)
									if (search->type == MN_MODEL && !Q_strncmp(search->name, parent, MAX_VAR)) {
										char model[MAX_VAR];

										Q_strncpyz(model, MN_GetReferenceString(menu, search->data[MN_DATA_STRING_OR_IMAGE_OR_MODEL]), MAX_VAR);

										pmi.model = R_RegisterModelShort(model);
										if (!pmi.model)
											break;

										pmi.name = model;
										pmi.origin = search->origin;
										pmi.angles = search->angles;
										pmi.scale = search->scale;
										pmi.center = search->center;
										pmi.color = search->color;

										/* autoscale? */
										if (!node->scale[0]) {
											mi.scale = NULL;
											mi.center = node->size;
										}

										as = search->data[MN_DATA_MODEL_ANIMATION_STATE];
										pmi.frame = as->frame;
										pmi.oldframe = as->oldframe;
										pmi.backlerp = as->backlerp;
										R_DrawModelDirect(&mi, &pmi, tag);
										break;
									}
							} else
								R_DrawModelDirect(&mi, NULL, NULL);
						}
					/* for normal models (normal = no menumodel definition) this
					 * menuModel pointer is null - the do-while loop is only
					 * ran once */
					} while (menuModel != NULL);
					break;
				}

				case MN_MAP:
					if (curCampaign) {
						CL_CampaignRun();	/* advance time */
						MAP_DrawMap(node); /* Draw geoscape */
					}
					break;

				case MN_BASEMAP:
					B_DrawBase(node);
					break;

				case MN_CINEMATIC:
					if (node->data[MN_DATA_STRING_OR_IMAGE_OR_MODEL]) {
						assert(cls.playingCinematic != CIN_STATUS_FULLSCREEN);
						if (cls.playingCinematic == CIN_STATUS_NONE)
							CIN_PlayCinematic(node->data[MN_DATA_STRING_OR_IMAGE_OR_MODEL]);
						if (cls.playingCinematic) {
							/* only set replay to true if video was found and is running */
							CIN_SetParameters(node->pos[0], node->pos[1], node->size[0], node->size[1], CIN_STATUS_MENU);
							CIN_RunCinematic();
						}
					}
					break;
				}	/* switch */

				/* mouseover? */
				if (node->state == qtrue)
					menu->hoverNode = node;

				if (mn_debugmenu->integer) {
					MN_DrawTooltip("f_small", node->name, node->pos[0], node->pos[1], node->size[1], 0);
/*					R_FontDrawString("f_verysmall", ALIGN_UL, node->pos[0], node->pos[1], node->pos[0], node->pos[1], node->size[0], 0, node->texh[0], node->name, 0, 0, NULL, qfalse);*/
				}
			}	/* if */

		}	/* for */
		if (sp == mn.menuStackPos && menu->hoverNode && cl_show_tooltips->integer) {
			/* we are hovering an item and also want to display it
	 		 * make sure that we draw this on top of every other node */
			if (itemHover) {
				char tooltiptext[MAX_VAR*2] = "";
				int x = mousePosX, y = mousePosY;
				const int itemToolTipWidth = 250;
				int linenum;
				int itemToolTipHeight;
				/* Get name and info about item */
				linenum = MN_GetItemTooltip(itemHover->item, tooltiptext, sizeof(tooltiptext));
				itemToolTipHeight = linenum * 25; /** @todo make this a constant/define? */
#if 1
				if (x + itemToolTipWidth > VID_NORM_WIDTH)
					x = VID_NORM_WIDTH - itemToolTipWidth;
				if (y + itemToolTipHeight > VID_NORM_HEIGHT)
					y = VID_NORM_HEIGHT - itemToolTipHeight;
				MN_DrawTextNode(tooltiptext, NULL, "f_small", menu->hoverNode, x, y, itemToolTipWidth, itemToolTipHeight);
#else
				MN_DrawTooltip("f_small", tooltiptext, x, y, itemToolTipWidth, itemToolTipHeight);
#endif
			} else {
				MN_Tooltip(menu, menu->hoverNode, mousePosX, mousePosY);
				menu->hoverNode = NULL;
			}
		}
	}

	R_ColorBlend(NULL);
}

/**
 * @brief Prints a list of tab and newline seperated string to keylist char array that hold the key and the command desc
 */
static void MN_InitKeyList_f (void)
{
	static char keylist[2048];
	int i;

	*keylist = '\0';

	for (i = 0; i < K_LAST_KEY; i++)
		if (keybindings[i] && keybindings[i][0]) {
			Com_Printf("%s - %s\n", Key_KeynumToString(i), keybindings[i]);
			Q_strcat(keylist, va("%s\t%s\n", Key_KeynumToString(i), Cmd_GetCommandDesc(keybindings[i])), sizeof(keylist));
		}

	mn.menuText[TEXT_LIST] = keylist;
}

/**
 * @brief Prints the map info
 */
static void MN_MapInfo (int step)
{
	int i = 0;
	mapDef_t *md;

	if (!csi.numMDs)
		return;

	ccs.multiplayerMapDefinitionIndex += step;

	if (ccs.multiplayerMapDefinitionIndex < 0)
		ccs.multiplayerMapDefinitionIndex = csi.numMDs - 1;

	ccs.multiplayerMapDefinitionIndex %= csi.numMDs;

	if (!ccs.singleplayer) {
		while (!csi.mds[ccs.multiplayerMapDefinitionIndex].multiplayer) {
			i++;
			ccs.multiplayerMapDefinitionIndex += (step ? step : 1);
			if (ccs.multiplayerMapDefinitionIndex < 0)
				ccs.multiplayerMapDefinitionIndex = csi.numMDs - 1;
			ccs.multiplayerMapDefinitionIndex %= csi.numMDs;
			if (i >= csi.numMDs)
				Sys_Error("MN_MapInfo: There is no multiplayer map in any mapdef\n");
		}
	}

	md = &csi.mds[ccs.multiplayerMapDefinitionIndex];

	Cvar_Set("mn_svmapname", md->map);
	if (FS_CheckFile(va("pics/maps/shots/%s.jpg", md->map)) != -1)
		Cvar_Set("mn_mappic", va("maps/shots/%s.jpg", md->map));
	else
		Cvar_Set("mn_mappic", va("maps/shots/na.jpg"));

	if (FS_CheckFile(va("pics/maps/shots/%s_2.jpg", md->map)) != -1)
		Cvar_Set("mn_mappic2", va("maps/shots/%s_2.jpg", md->map));
	else
		Cvar_Set("mn_mappic2", va("maps/shots/na.jpg"));

	if (FS_CheckFile(va("pics/maps/shots/%s_3.jpg", md->map)) != -1)
		Cvar_Set("mn_mappic3", va("maps/shots/%s_3.jpg", md->map));
	else
		Cvar_Set("mn_mappic3", va("maps/shots/na.jpg"));

	if (!ccs.singleplayer) {
		if (md->gameTypes) {
			linkedList_t *list = md->gameTypes;
			char buf[256] = "";
			while (list) {
				Q_strcat(buf, va("%s ", (const char *)list->data), sizeof(buf));
				list = list->next;
			}
			Cvar_Set("mn_mapgametypes", buf);
			list = md->gameTypes;
			while (list) {
				/* check whether current selected gametype is a valid one */
				if (!Q_strcmp((const char*)list->data, gametype->string)) {
					break;
				}
				list = list->next;
			}
			if (!list)
				MN_ChangeGametype_f();
		} else {
			Cvar_Set("mn_mapgametypes", _("all"));
		}
	}
}

static void MN_GetMaps_f (void)
{
	MN_MapInfo(0);
}

static void MN_NextMap_f (void)
{
	MN_MapInfo(1);
}

static void MN_PrevMap_f (void)
{
	MN_MapInfo(-1);
}

/**
 * @brief Initialize the menu data hunk, add cvars and commands
 * @note Also calls the 'reset' functions for production, basemanagement,
 * aliencontainmenu, employee, hospital and a lot more subfunctions
 * @note This function is called once
 * @sa MN_Shutdown
 * @sa B_ResetBaseManagement
 * @sa CL_InitLocal
 */
void MN_ResetMenus (void)
{
	/* reset menu structures */
	memset(&mn, 0, sizeof(mn));

	/* add commands and cvars */
	mn_debugmenu = Cvar_Get("mn_debugmenu", "0", 0, "Prints node names for debugging purposes");
	Cvar_Set("mn_main", "main");
	Cvar_Set("mn_sequence", "sequence");

	/* print the keybindings to mn.menuText */
	Cmd_AddCommand("mn_init_keylist", MN_InitKeyList_f, NULL);

	Cmd_AddCommand("mn_startserver", MN_StartServer_f, NULL);
	Cmd_AddCommand("mn_updategametype", MN_UpdateGametype_f, "Update the menu values with current gametype values");
	Cmd_AddCommand("mn_nextgametype", MN_ChangeGametype_f, "Switch to the next multiplayer game type");
	Cmd_AddCommand("mn_prevgametype", MN_ChangeGametype_f, "Switch to the previous multiplayer game type");
	Cmd_AddCommand("mn_getmaps", MN_GetMaps_f, "The initial map to show");
	Cmd_AddCommand("mn_nextmap", MN_NextMap_f, "Switch to the next multiplayer map");
	Cmd_AddCommand("mn_prevmap", MN_PrevMap_f, "Switch to the previous multiplayer map");
	Cmd_AddCommand("mn_push", MN_PushMenu_f, "Push a menu to the menustack");
	Cmd_AddParamCompleteFunction("mn_push", MN_CompletePushMenu);
	Cmd_AddCommand("mn_push_copy", MN_PushCopyMenu_f, NULL);
	Cmd_AddCommand("mn_pop", MN_PopMenu_f, "Pops the current menu from the stack");
	Cmd_AddCommand("menumodelslist", MN_ListMenuModels_f, NULL);

	Cmd_AddCommand("hidehud", MN_PushNoHud_f, _("Hide the HUD (press ESC to reactivate HUD)"));

	MN_Init();

	/* reset ufopedia, basemanagement and other subsystems */
	UP_ResetUfopedia();
	B_ResetBaseManagement();
	RS_ResetResearch();
	PR_ResetProduction();
	E_Reset();
	HOS_Reset();
	AC_Reset();
	MAP_ResetAction();
	UFO_Reset();
	TR_Reset();
	BaseSummary_Reset();
}
