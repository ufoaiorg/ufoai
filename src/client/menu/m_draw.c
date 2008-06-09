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
cvar_t *mn_show_tooltips;

static void MN_DrawBorder (const menuNode_t *node)
{
	/* FIXME: use GL_LINE_LOOP + array here */
	/* left */
	R_DrawFill(node->pos[0] - node->padding - node->border, node->pos[1] - node->padding - node->border,
		node->border, node->size[1] + (node->padding*2) + (node->border*2), node->align, node->bordercolor);
	/* right */
	R_DrawFill(node->pos[0] + node->size[0] + node->padding, node->pos[1] - node->padding - node->border,
		node->border, node->size[1] + (node->padding*2) + (node->border*2), node->align, node->bordercolor);
	/* top */
	R_DrawFill(node->pos[0] - node->padding, node->pos[1] - node->padding - node->border,
		node->size[0] + (node->padding*2), node->border, node->align, node->bordercolor);
	/* down */
	R_DrawFill(node->pos[0] - node->padding, node->pos[1] + node->size[1] + node->padding,
		node->size[0] + (node->padding*2), node->border, node->align, node->bordercolor);
}

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
	vec4_t color;
	int mouseOver = 0;
	int y, i;
	message_t *message;
	int width, height;
	const invList_t *itemHover = NULL;
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

	/* Reset info for preview rendering of dragged items. */
	dragInfo.toNode = NULL;

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
					|| node->type == MN_CONTAINER || node->type == MN_TEXT || node->type == MN_BASELAYOUT || node->type == MN_BASEMAP || node->type == MN_MAP)
					|| node->type == MN_CHECKBOX || node->type == MN_SELECTBOX || node->type == MN_LINESTRIP
					|| ((node->type == MN_ZONE || node->type == MN_CONTAINER) && node->bgcolor[3]) || node->type == MN_RADAR)) {
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
					if (node->type != MN_BASELAYOUT)
						R_DrawFill(node->pos[0] - node->padding, node->pos[1] - node->padding,
							node->size[0] + (node->padding * 2), node->size[1] + (node->padding * 2), 0, node->bgcolor);
				}

				if (node->border && node->bordercolor && node->size[0] && node->size[1] && node->pos)
					MN_DrawBorder(node);

				/* mouse darken effect */
				VectorScale(node->color, 0.8, color);
				color[3] = node->color[3];
				if (node->mousefx && node->type == MN_PIC && mouseOver && sp > pp)
					R_ColorBlend(color);
				else if (node->type != MN_SELECTBOX)
					R_ColorBlend(node->color);

				/* get the reference */
				if (node->type != MN_BAR && node->type != MN_CONTAINER && node->type != MN_BASEMAP && node->type != MN_BASELAYOUT
					&& node->type != MN_TEXT && node->type != MN_MAP && node->type != MN_ZONE && node->type != MN_LINESTRIP
					&& node->type != MN_RADAR) {
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
					MN_DrawCheckBoxNode(menu, node, ref);
					break;

				case MN_SELECTBOX:
					if (node->data[MN_DATA_MODEL_SKIN_OR_CVAR])
						MN_DrawSelectBoxNode(node, ref);
					break;

				case MN_RADAR:
					if (cls.state == ca_active && !cl.skipRadarNodes)
						MN_DrawRadar(node);
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

				case MN_TBAR:
					{
						/* data[0] is the texture name, data[1] is the minimum value, data[2] is the current value */
						float ps, shx;
						if (node->pointWidth) {
							ps = MN_GetReferenceFloat(menu, node->data[2]) - MN_GetReferenceFloat(menu, node->data[1]);
							shx = node->texl[0] + round(ps * node->pointWidth) + (ps > 0 ? floor((ps - 1) / 10) * node->gapWidth : 0);
						} else
							shx = node->texh[0];
						R_DrawNormPic(node->pos[0], node->pos[1], node->size[0], node->size[1],	shx, node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, ref);
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
						const invList_t *itemHover_temp = MN_DrawContainerNode(node);
						if (itemHover_temp)
							itemHover = itemHover_temp;

						/* Store information for preview drawing of dragged items. */
						if (MN_CheckNodeZone(node, mousePosX, mousePosY)) {
							dragInfo.toNode = node;
							dragInfo.to = &csi.ids[node->mousefx];

							dragInfo.toX = (mousePosX - node->pos[0]) / C_UNIT;
							dragInfo.toY = (mousePosY - node->pos[1]) / C_UNIT;
							/* We calculate the position of the top-left corner of the dragged
							 * item in oder to compensate for the centered-drawn cursor-item. */
							if (dragInfo.item.t) {
								dragInfo.toX -= dragInfo.item.t->sx / 2;
								dragInfo.toY -= dragInfo.item.t->sy / 2;
							}

							/** Search for a suitable position to render the item at if
							 * the container is "single" or the calculated values are out of bounds.
							 * @todo I hope this is the same behaviour that is used in MN_Drag - a special function
							 *		to simply calculate the final x/y values would be a good idea here. */
							if (dragInfo.item.t && (dragInfo.to->single
							 || dragInfo.toX  < 0 || dragInfo.toY < 0
							 || dragInfo.toX >= SHAPE_BIG_MAX_WIDTH || dragInfo.toY >= SHAPE_BIG_MAX_HEIGHT)) {
								Com_FindSpace(menuInventory, &dragInfo.item, dragInfo.to, &dragInfo.toX, &dragInfo.toY);
							}
						}
					}
					break;

				case MN_ITEM:
					if (ref && *ref)
						MN_DrawItemNode(node, ref);
					break;

				case MN_MODEL:
					if (*source)
						MN_DrawModelNode(menu, node, ref, source);
					break;

				case MN_MAP:
					if (curCampaign) {
						/* don't run the campaign in console mode */
						if (cls.key_dest != key_console)
							CL_CampaignRun();	/* advance time */
						MAP_DrawMap(node); /* Draw geoscape */
					}
					break;

				case MN_BASELAYOUT:
					MN_BaseMapLayout(node);
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
							CIN_SetParameters(node->pos[0], node->pos[1], node->size[0], node->size[1], CIN_STATUS_MENU, qtrue);
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

				R_ColorBlend(NULL);
			}	/* if */

		}	/* for */
		if (sp == mn.menuStackPos && menu->hoverNode && mn_show_tooltips->integer) {
			/* We are hovering over an item and also want to display it.
	 		 * Make sure that we draw this on top of every other node. */

			if (itemHover) {
				char tooltiptext[MAX_VAR * 2];
				const int itemToolTipWidth = 250;

				/* Get name and info about item */
				MN_GetItemTooltip(itemHover->item, tooltiptext, sizeof(tooltiptext));

				MN_DrawTooltip("f_small", tooltiptext, mousePosX, mousePosY, itemToolTipWidth, 0);
			} else {
				MN_Tooltip(menu, menu->hoverNode, mousePosX, mousePosY);
				menu->hoverNode = NULL;
			}
		}
	}

	/* draw a special notice */
	if (cl.time < cl.msgTime) {
		menu = MN_GetActiveMenu();
		if (menu->noticePos[0] || menu->noticePos[1])
			MN_DrawNotice(menu->noticePos[0], menu->noticePos[1]);
		else
			MN_DrawNotice(500, 110);
	}

	R_ColorBlend(NULL);
}

void MN_DrawMenusInit (void)
{
	mn_debugmenu = Cvar_Get("mn_debugmenu", "0", 0, "Prints node names for debugging purposes");
	mn_show_tooltips = Cvar_Get("mn_show_tooltips", "1", CVAR_ARCHIVE, "Show tooltips in menus and hud");
}

/**
 * @brief Displays a message over all menus.
 * @sa SCR_DisplayHudMessage
 * @param[in] time is a ms values
 * @param[in] text text is already translated here
 */
void MN_DisplayNotice (const char *text, int time)
{
	cl.msgTime = cl.time + time;
	Q_strncpyz(cl.msgText, text, sizeof(cl.msgText));
}
