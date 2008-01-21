/**
 * @file m_draw.c
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
#include "m_draw.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_inventory.h"
#include "m_tooltip.h"

static cvar_t *mn_debugmenu;
static cvar_t *mn_show_tooltips;

/**
 * @brief Draws the menu stack
 * @sa SCR_UpdateScreen
 */
void MN_DrawMenus (void)
{
	menuNode_t *node;
	menu_t *menu;
	const char *ref;
	const char *font;
	char source[MAX_VAR] = "";
	int sp, pp;
	item_t item = {1, NONE, NONE, 0, 0}; /* 1 so it's not reddish; fake item anyway */
	vec4_t color;
	int mouseOver = 0;
	int y, i;
	message_t *message;
	int width, height;
	invList_t *itemHover = NULL;
	char *tab, *end;

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

				/* FIXME: use GL_LINE_LOOP + array here */
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
					if (ref && *ref)
						MN_DrawImageNode(node, ref, cl.time);
					break;

				case MN_CHECKBOX:
					MN_DrawCheckBoxNode(node, ref);
					break;

				case MN_SELECTBOX:
					if (node->data[MN_DATA_MODEL_SKIN_OR_CVAR])
						MN_DrawSelectBoxNode(node, ref);
					break;

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
						if (mouseSpace != MS_DRAG && node->state && mn_show_tooltips->integer) {
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
					MN_DrawModelNode(node, ref, source);
					break;

				case MN_MAP:
					if (curCampaign) {
						CL_CampaignRun();	/* advance time */
						MAP_DrawMap(node); /* Draw geoscape */
					}
					break;

				case MN_BASEMAP:
					MN_BaseMapDraw(node);
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

				if (mn_debugmenu->integer)
					MN_DrawTooltip("f_small", node->name, node->pos[0], node->pos[1], node->size[1], 0);
			}	/* if */

		}	/* for */
		if (sp == mn.menuStackPos && menu->hoverNode && mn_show_tooltips->integer) {
			/* we are hovering an item and also want to display it
	 		 * make sure that we draw this on top of every other node */
			if (itemHover) {
				char tooltiptext[MAX_VAR * 2] = "";
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

void MN_DrawMenusInit (void)
{
	mn_debugmenu = Cvar_Get("mn_debugmenu", "0", 0, "Prints node names for debugging purposes");
	mn_show_tooltips = Cvar_Get("mn_show_tooltips", "1", CVAR_ARCHIVE, "Show tooltips in menus and hud");
}
