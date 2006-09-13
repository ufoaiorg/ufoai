/**
 * @file cl_menu.c
 * @brief Client menu functions.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

static vec4_t tooltipBG = { 0.0f, 0.0f, 0.0f, 0.7f };
static vec4_t tooltipColor = { 0.0f, 0.8f, 0.0f, 1.0f };

/* =========================================================== */

typedef enum ea_s {
	EA_NULL,
	EA_CMD,

	EA_CALL,
	EA_NODE,
	EA_VAR,

	EA_NUM_EVENTACTION
} ea_t;

static char *ea_strings[EA_NUM_EVENTACTION] = {
	"",
	"cmd",

	"",
	"*",
	"&"
};

static int ea_values[EA_NUM_EVENTACTION] = {
	V_NULL,
	V_LONGSTRING,

	V_NULL,
	V_NULL,
	V_NULL
};

/* =========================================================== */

typedef enum ne_s {
	NE_NULL,
	NE_CLICK,
	NE_RCLICK,
	NE_MCLICK,
	NE_MOUSEIN,
	NE_MOUSEOUT,

	NE_NUM_NODEEVENT
} ne_t;

static char *ne_strings[NE_NUM_NODEEVENT] = {
	"",
	"click",
	"rclick",
	"mclick",
	"in",
	"out"
};

size_t ne_values[NE_NUM_NODEEVENT] = {
	0,
	offsetof(menuNode_t, click),
	offsetof(menuNode_t, rclick),
	offsetof(menuNode_t, mclick),
	offsetof(menuNode_t, mouseIn),
	offsetof(menuNode_t, mouseOut)
};

/* =========================================================== */

static value_t nps[] = {
	{"invis", V_BOOL, offsetof(menuNode_t, invis)},
	{"mousefx", V_BOOL, offsetof(menuNode_t, mousefx)},
	{"blend", V_BOOL, offsetof(menuNode_t, blend)},
	{"texh", V_POS, offsetof(menuNode_t, texh)},
	{"texl", V_POS, offsetof(menuNode_t, texl)},
	{"pos", V_POS, offsetof(menuNode_t, pos)},
	{"size", V_POS, offsetof(menuNode_t, size)},
	{"format", V_POS, offsetof(menuNode_t, texh)},
	{"origin", V_VECTOR, offsetof(menuNode_t, origin)},
	{"center", V_VECTOR, offsetof(menuNode_t, center)},
	{"scale", V_VECTOR, offsetof(menuNode_t, scale)},
	{"angles", V_VECTOR, offsetof(menuNode_t, angles)},
	{"num", V_INT, offsetof(menuNode_t, num)},
	{"height", V_INT, offsetof(menuNode_t, height)},
	{"text_scroll", V_INT, offsetof(menuNode_t, textScroll)},
	{"timeout", V_INT, offsetof(menuNode_t, timeOut)},
	{"bgcolor", V_COLOR, offsetof(menuNode_t, bgcolor)},
	/* 0, -1, -2, -3, -4, -5 fills the data array in menuNode_t */
	{"tooltip", V_STRING, -5},	/* translated in MN_Tooltip */
	{"image", V_STRING, 0},
	{"md2", V_STRING, 0},
	{"anim", V_STRING, -1},
	{"tag", V_STRING, -2},
	{"skin", V_STRING, -3},
	/* -4 is animation state */
	{"string", V_STRING, 0},	/* no gettext here - this can be a cvar, too */
	{"font", V_STRING, -1},
	{"max", V_FLOAT, 0},
	{"min", V_FLOAT, -1},
	{"current", V_FLOAT, -2},
	{"weapon", V_STRING, 0},
	{"color", V_COLOR, offsetof(menuNode_t, color)},
	{"align", V_ALIGN, offsetof(menuNode_t, align)},
	{"if", V_IF, offsetof(menuNode_t, depends)},

	{NULL, V_NULL, 0},
};

static value_t menuModelValues[] = {
	{"model", V_STRING, offsetof(menuModel_t, model)},
	{"need", V_NULL, 0},
	{"anim", V_STRING, offsetof(menuModel_t, anim)},
	{"skin", V_INT, offsetof(menuModel_t, skin)},
	{"origin", V_VECTOR, offsetof(menuModel_t, origin)},
	{"center", V_VECTOR, offsetof(menuModel_t, center)},
	{"scale", V_VECTOR, offsetof(menuModel_t, scale)},
	{"angles", V_VECTOR, offsetof(menuModel_t, angles)},
	{"color", V_COLOR, offsetof(menuModel_t, color)},

	{NULL, V_NULL, 0},
};

/* =========================================================== */

typedef enum mn_s {
	MN_NULL,
	MN_CONFUNC,
	MN_CVARFUNC,
	MN_FUNC,
	MN_ZONE,
	MN_PIC,
	MN_STRING,
	MN_TEXT,
	MN_BAR,
	MN_MODEL,
	MN_CONTAINER,
	MN_ITEM,
	MN_MAP,
	MN_3DMAP,
	MN_BASEMAP,

	MN_NUM_NODETYPE
} mn_t;

static char *nt_strings[MN_NUM_NODETYPE] = {
	"",
	"confunc",
	"cvarfunc",
	"func",
	"zone",
	"pic",
	"string",
	"text",
	"bar",
	"model",
	"container",
	"item",
	"map",
	"3dmap",
	"basemap"
};


/* =========================================================== */


static menuModel_t menuModels[MAX_MENUMODELS];
static menuAction_t menuActions[MAX_MENUACTIONS];
static menuNode_t menuNodes[MAX_MENUNODES];
static menu_t menus[MAX_MENUS];

static int numActions;
static int numNodes;
static int numMenus;
static int numMenuModels;
static int numTutorials;

static byte *adata, *curadata;
static int adataize = 0;

static menu_t *menuStack[MAX_MENUSTACK];
static int menuStackPos;

inventory_t *menuInventory = NULL;
char *menuText[MAX_MENUTEXTS];

cvar_t *mn_escpop;


/*
==============================================================

ACTION EXECUTION

==============================================================
*/

/**
 * @brief Searches all menus for the specified one
 */
menu_t *MN_GetMenu(char *name)
{
	int i;

	for (i = 0; i < numMenus; i++)
		if (!Q_strncmp(menus[i].name, name, MAX_VAR))
			return &menus[i];

	Sys_Error("Could not find menu '%s'\n", name);
	return NULL;
}

/**
 * @brief Searches all nodes in the given menu for a given nodename
 */
menuNode_t *MN_GetNode(const menu_t* const menu, char *name)
{
	menuNode_t *node;

	for (node = menu->firstNode; node; node = node->next)
		if (!Q_strncmp(name, node->name, sizeof(node->name)))
			break;

	return node;
}

/**
 * @brief Searches a given node in the current menu
 */
menuNode_t* MN_GetNodeFromCurrentMenu(char*name)
{
	return MN_GetNode(menuStack[menuStackPos-1], name);
}

/**
 * @brief Sets new x and y coordinates for a given node
 */
void MN_SetNewNodePos (menuNode_t* node, int x, int y)
{
	if (node) {
		node->pos[0] = x;
		node->pos[1] = y;
	}
}

/**
 * @brief
 */
static char *MN_GetReferenceString(const menu_t* const menu, char *ref)
{
	if (!ref)
		return NULL;
	if (*ref == '*') {
		char ident[MAX_VAR];
		char *text;
		char *token;

		/* get the reference and the name */
		text = ref + 1;
		token = COM_Parse(&text);
		if (!text)
			return NULL;
		Q_strncpyz(ident, token, MAX_VAR);
		token = COM_Parse(&text);
		if (!text)
			return NULL;

		if (!Q_strncmp(ident, "cvar", 4)) {
			/* get the cvar value */
			return Cvar_VariableString(token);
		} else {
			menuNode_t *refNode;
			value_t *val;

			/* draw a reference to a node property */
			refNode = MN_GetNode(menu, ident);
			if (!refNode)
				return NULL;

			/* get the property */
			for (val = nps; val->type; val++)
				if (!Q_stricmp(token, val->string))
					break;

			if (!val->type)
				return NULL;

			/* get the string */
			/* 0, -1, -2, -3, -4, -5 fills the data array in menuNode_t */
			if ((val->ofs > 0) && (val->ofs < (size_t)-5))
				return Com_ValueToStr(refNode, val->type, val->ofs);
			else
				return Com_ValueToStr(refNode->data[-(val->ofs)], val->type, 0);
		}
	} else if (*ref == '_') {
		ref++;
		return _(ref);
	} else {
		/* just get the data */
		return ref;
	}
}


/**
 * @brief
 */
static float MN_GetReferenceFloat(const menu_t* const menu, void *ref)
{
	if (!ref)
		return 0.0;
	if (*(char *) ref == '*') {
		char ident[MAX_VAR];
		char *text;
		char *token;

		/* get the reference and the name */
		text = (char *) ref + 1;
		token = COM_Parse(&text);
		if (!text)
			return 0.0;
		Q_strncpyz(ident, token, MAX_VAR);
		token = COM_Parse(&text);
		if (!text)
			return 0.0;

		if (!Q_strncmp(ident, "cvar", 4)) {
			/* get the cvar value */
			return Cvar_VariableValue(token);
		} else {
			menuNode_t *refNode;
			value_t *val;

			/* draw a reference to a node property */
			refNode = MN_GetNode(menu, ident);
			if (!refNode)
				return 0.0;

			/* get the property */
			for (val = nps; val->type; val++)
				if (!Q_stricmp(token, val->string))
					break;

			if (val->type != V_FLOAT)
				return 0.0;

			/* get the string */
			/* 0, -1, -2, -3, -4, -5 fills the data array in menuNode_t */
			if ((val->ofs > 0) && (val->ofs < (size_t)-5))
				return *(float *) ((byte *) refNode + val->ofs);
			else
				return *(float *) refNode->data[-(val->ofs)];
		}
	} else {
		/* just get the data */
		return *(float *) ref;
	}
}

/**
 * @brief Starts a server and checks if the server loads a team unless he is a dedicated
 * server admin
 */
static void MN_StartServer(void)
{
	if (Cmd_Argc() <= 1) {
		Com_Printf("Usage: mn_startserver <name>\n");
		return;
	} else
		Com_Printf("MN_StartServer\n");

	if (Cvar_VariableValue("dedicated") > 0)
		Com_DPrintf("Dedicated server needs no team\n");
	/* FIXME: Spectator */
	else if (!B_GetNumOnTeam()) {
		MN_Popup(_("Error"), _("Assemble a team first"));
		return;
	} else
		Com_DPrintf("There are %i members on team\n", B_GetNumOnTeam());

	if (Cvar_VariableValue("sv_teamplay")
		&& Cvar_VariableValue("maxsoldiersperplayer") > Cvar_VariableValue("maxsoldiers")) {
		MN_Popup(_("Settings doesn't make sense"), _("Set playersoldiers lower than teamsoldiers"));
		return;
	}

	Cbuf_ExecuteText(EXEC_NOW, va("map %s\n", Cmd_Argv(1)));
}

/**
 * @brief Will merge the second shape (=itemshape) into the first one on the position x/y
 * @param[in] shape Pointer to 'int shape[16]'
 * @param[in] itemshape
 * @param[in] x
 * @param[in] y
 */
static void Com_MergeShapes(int* const shape, int itemshape, int x, int y)
{
	int i;

	for (i = 0; (i < 4) && (y + i < 16); i++)
		shape[y + i] |= ((itemshape >> i * 8) & 0xFF) << x;
}

/**
 * @brief Checks the shape if there is a 1-bit on the position x/y.
 * @param[in] shape
 * @param[in] x
 * @param[in] y
 * @note Make sure, that y is not bigger than 15
 */
static qboolean Com_CheckShape(int shape[16], int x, int y)
{
	int row = shape[y];
	int position = pow(2, x);

	if ((row & position) == 0)
		return qfalse;
	else
		return qtrue;
}

/**
 * @brief Draws the rectangle in a 'free' style on position posx/posy (pixel) in the size sizex/sizey (pixel)
 */
static void MN_DrawFree(int posx, int posy, int sizex, int sizey)
{
	static vec4_t color = { 0.0f, 1.0f, 0.0f, 0.7f };
	re.DrawFill(posx, posy, sizex, sizey, ALIGN_UL, color);
	re.DrawColor(NULL);
}

/**
 * @brief Draws the free and useable inventory positions when dragging an item.
 */
static void MN_InvDrawFree(inventory_t * inv, menuNode_t * node)
{
	/* get the 'type' of the dragged item */
	int item = dragItem.t;
	int container = node->mousefx;
	int itemshape;

	/* The shape of the free positions. */
	int free[16];
	int x, y;

	/* Draw only in dragging-mode and not for the equip-floor */
	if (mouseSpace == MS_DRAG) {
		assert(inv);
		memset(free, 0, sizeof(free));
#if 0
		/* TODO: add armor support */
		if (csi.ids[container].armor) {
		} else
#endif
			/* if  single container (hands) */
		if (csi.ids[container].single) {
			/* if container is free or the dragged-item is in it */
			if (node->mousefx == dragFrom || Com_CheckToInventory(inv, item, container, 0, 0))
				MN_DrawFree(node->pos[0], node->pos[1], node->size[0], node->size[1]);
		} else {
			for (y = 0; y < 16; y++) {
				for (x = 0; x < 32; x++) {
					/* Check if the current position is useable (topleft of the item) */
					if (Com_CheckToInventory(inv, item, container, x, y)) {
						itemshape = csi.ods[dragItem.t].shape;
						/* add '1's to each position the item is 'blocking' */
						Com_MergeShapes(free, itemshape, x, y);
					}
					/* Only draw on existing positions */
					if (Com_CheckShape(csi.ids[container].shape, x, y)) {
						if (Com_CheckShape(free, x, y))
							MN_DrawFree(node->pos[0] + x * C_UNIT, node->pos[1] + y * C_UNIT, C_UNIT, C_UNIT);
					}
				}	/* for x */
			}	/* for y */
		}
	}
}


/**
 * @brief Popup in geoscape
 */
void MN_Popup(const char *title, const char *text)
{
	menuText[TEXT_POPUP] = (char *) title;
	menuText[TEXT_POPUP_INFO] = (char *) text;
	Cbuf_ExecuteText(EXEC_NOW, "game_timestop");
	MN_PushMenu("popup");
}

/**
 * @brief
 */
static void MN_ExecuteActions(const menu_t* const menu, menuAction_t* const first)
{
	menuAction_t *action;
	byte *data;

	for (action = first; action; action = action->next)
		switch (action->type) {
		case EA_NULL:
			/* do nothing */
			break;
		case EA_CMD:
			/* execute a command */
			if (action->data)
				Cbuf_AddText(va("%s\n", action->data));
			break;
		case EA_CALL:
			/* call another function */
			MN_ExecuteActions(menu, **(menuAction_t ***) action->data);
			break;
		case EA_NODE:
			/* set a property */
			if (action->data) {
				menuNode_t *node;
				int np;

				data = action->data;
				data += strlen(action->data) + 1;
				np = *((int *) data);
				data += sizeof(int);

				/* search the node */
				node = MN_GetNode(menu, (char *) action->data);

				if (!node) {
					/* didn't find node -> "kill" action and print error */
					action->type = EA_NULL;
					Com_Printf("MN_ExecuteActions: node \"%s\" doesn't exist\n", (char *) action->data);
					break;
				}

				/* 0, -1, -2, -3, -4, -5 fills the data array in menuNode_t */
				if ((nps[np].ofs > 0) && (nps[np].ofs < (size_t)-5))
					Com_SetValue(node, (char *) data, nps[np].type, nps[np].ofs);
				else
					node->data[-(nps[np].ofs)] = data;
			}
			break;
		default:
			Sys_Error("unknown action type\n");
			break;
		}
}


/**
 * @brief
 */
static void MN_Command(void)
{
	menuNode_t *node;
	char *name;
	int i;

	name = Cmd_Argv(0);
	for (i = 0; i < numMenus; i++)
		for (node = menus[i].firstNode; node; node = node->next)
			if (node->type == MN_CONFUNC && !Q_strncmp(node->name, name, sizeof(node->name))) {
				/* found the node */
				MN_ExecuteActions(&menus[i], node->click);
				return;
			}
}


/*
==============================================================

MENU ZONE DETECTION

==============================================================
*/

/**
 * @brief
 */
static void MN_FindContainer(menuNode_t* const node)
{
	invDef_t *id;
	int i, j;

	for (i = 0, id = csi.ids; i < csi.numIDs; id++, i++)
		if (!Q_strncmp(node->name, id->name, sizeof(node->name)))
			break;

	if (i == csi.numIDs)
		node->mousefx = NONE;
	else
		node->mousefx = id - csi.ids;

	for (i = 31; i >= 0; i--) {
		for (j = 0; j < 16; j++)
			if (id->shape[j] & (1 << i))
				break;
		if (j < 16)
			break;
	}
	node->size[0] = C_UNIT * (i + 1) + 0.01;

	for (i = 15; i >= 0; i--)
		if (id->shape[i] & 0xFFFFFFFF)
			break;
	node->size[1] = C_UNIT * (i + 1) + 0.01;
}

/**
 * @brief
 */
static qboolean MN_CheckNodeZone(menuNode_t* const node, int x, int y)
{
	int sx, sy, tx, ty;

	if (*node->depends.var) {
		/* menuIfCondition_t */
		switch (node->depends.cond) {
		case IF_EQ:
			if (atof(node->depends.value) != Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value)
				return qfalse;
			break;
		case IF_LE:
			if (Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value > atof(node->depends.value))
				return qfalse;
			break;
		case IF_GE:
			if (Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value < atof(node->depends.value))
				return qfalse;
			break;
		case IF_GT:
			if (Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value <= atof(node->depends.value))
				return qfalse;
			break;
		case IF_LT:
			if (Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value >= atof(node->depends.value))
				return qfalse;
			break;
		case IF_NE:
			if (Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value == atof(node->depends.value))
				return qfalse;
			break;
		default:
			Sys_Error("Unknown condition for if statement: %i\n", node->depends.cond);
			break;
		}
	}

	/* containers */
	if (node->type == MN_CONTAINER) {
		if (node->mousefx == C_UNDEFINED)
			MN_FindContainer(node);
		if (node->mousefx == NONE)
			return qfalse;

		/* check bounding box */
		if (x < node->pos[0] || x > node->pos[0] + node->size[0] || y < node->pos[1] || y > node->pos[1] + node->size[1])
			return qfalse;

		/* found a container */
		return qtrue;
	}

	/* check for click action */
	if (node->invis || (!node->click && !node->rclick && !node->mclick && !node->mouseIn && !node->mouseOut))
		return qfalse;

	if (!node->size[0] || !node->size[1]) {
		if (node->type == MN_PIC && node->data[0]) {
			if (node->texh[0] && node->texh[1]) {
				sx = node->texh[0] - node->texl[0];
				sy = node->texh[1] - node->texl[1];
			} else
				re.DrawGetPicSize(&sx, &sy, node->data[0]);
		} else
			return qfalse;
	} else {
		sx = node->size[0];
		sy = node->size[1];
	}

	tx = x - node->pos[0];
	ty = y - node->pos[1];
	if ( node->align > 0 && node->align < ALIGN_LAST ) {
		switch ( node->align % 3 ) {
		/* center */
		case 1: tx = x - node->pos[0] + sx / 2; break;
		/* right */
		case 2: tx = x - node->pos[0] + sx; break;
		}
	}

	if (tx < 0 || ty < 0 || tx > sx || ty > sy)
		return qfalse;

	/* on the node */
	if (node->type == MN_TEXT) {
		assert(node->texh[0]);
		return (int) (ty / node->texh[0]) + 1;
	} else
		return qtrue;
}


/**
 * @brief
 */
qboolean MN_CursorOnMenu(int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp;

	sp = menuStackPos;

	while (sp > 0) {
		menu = menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next)
			if (MN_CheckNodeZone(node, x, y)) {
				/* found an element */
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
 * @brief
 * @note: node->mousefx is the container id
 */
static void MN_Drag(const menuNode_t* const node, int x, int y)
{
	int px, py;

	if (!menuInventory)
		return;

	if (mouseSpace == MS_MENU) {
		invList_t *ic;

		px = (int) (x - node->pos[0]) / C_UNIT;
		py = (int) (y - node->pos[1]) / C_UNIT;

		/* start drag (mousefx represents container number) */
		ic = Com_SearchInInventory(menuInventory, node->mousefx, px, py);
		if (ic) {
			/* found item to drag */
			mouseSpace = MS_DRAG;
			dragItem = ic->item;
			dragFrom = node->mousefx;
			dragFromX = ic->x;
			dragFromY = ic->y;
			CL_ItemDescription(ic->item.t);
		}
	} else {
		/* end drag */
		mouseSpace = MS_NULL;
		px = (int) ((x - node->pos[0] - C_UNIT * ((csi.ods[dragItem.t].sx - 1) / 2.0)) / C_UNIT);
		py = (int) ((y - node->pos[1] - C_UNIT * ((csi.ods[dragItem.t].sy - 1) / 2.0)) / C_UNIT);

		/* tactical mission */
		if (selActor)
			MSG_Write_PA(PA_INVMOVE, selActor->entnum, dragFrom, dragFromX, dragFromY, node->mousefx, px, py);
		/* menu */
		else {
			invList_t *i = NULL;
			int et = -1, sel;

			if (node->mousefx == csi.idEquip) {
				/* a hack to add the equipment correctly into buy categories;
				   it is valid only due to the following property: */
				assert (MAX_CONTAINERS >= NUM_BUYTYPES);

				i = Com_SearchInInventory(menuInventory, dragFrom, dragFromX, dragFromY);
				if (i) {
					et = csi.ods[i->item.t].buytype;
					if (et != baseCurrent->equipType) {
						menuInventory->c[csi.idEquip] = baseCurrent->equipByBuyType.c[et];
						Com_FindSpace(menuInventory, i->item.t, csi.idEquip, &px, &py);
						if (px >= 32 && py >= 16) {
							menuInventory->c[csi.idEquip] = baseCurrent->equipByBuyType.c[baseCurrent->equipType];
							return;
						}
					}
				}
			}

			/* move the item */
			Com_MoveInInventory(menuInventory, dragFrom, dragFromX, dragFromY, node->mousefx, px, py, NULL, NULL);

			/* end of hack */
			if (i && et != baseCurrent->equipType) {
				baseCurrent->equipByBuyType.c[et] = menuInventory->c[csi.idEquip];
				menuInventory->c[csi.idEquip] = baseCurrent->equipByBuyType.c[baseCurrent->equipType];
			} else
				baseCurrent->equipByBuyType.c[baseCurrent->equipType] = menuInventory->c[csi.idEquip];

			/* update character info (for armor changes) */
			sel = cl_selected->value;
			if (sel >= 0 && sel < gd.numEmployees[EMPL_SOLDIER]) {
				if ( baseCurrent->curTeam[sel]->fieldSize == ACTOR_SIZE_NORMAL )
					CL_CharacterCvars(baseCurrent->curTeam[sel]);
				else
					CL_UGVCvars(baseCurrent->curTeam[sel]);
			}
		}
	}

}


/**
 * @brief
 */
static void MN_BarClick(menu_t * menu, menuNode_t * node, int x)
{
	char var[MAX_VAR];
	float frac, min;

	if (!node->mousefx)
		return;

	Q_strncpyz(var, node->data[2], MAX_VAR);
	if (Q_strncmp(var, "*cvar", 5))
		return;

	frac = (float) (x - node->pos[0]) / node->size[0];
	min = MN_GetReferenceFloat(menu, node->data[1]);
	Cvar_SetValue(&var[6], min + frac * (MN_GetReferenceFloat(menu, node->data[0]) - min));
}

/**
 * @brief
 */
static void MN_BaseMapClick(menuNode_t * node, int x, int y)
{
	int row, col;
	building_t	*entry;

	assert(baseCurrent);

	if (baseCurrent->buildingCurrent && baseCurrent->buildingCurrent->buildingStatus == B_STATUS_NOT_SET) {
		for (row = 0; row < BASE_SIZE; row++)
			for (col = 0; col < BASE_SIZE; col++)
				if (baseCurrent->map[row][col] == -1 && x >= baseCurrent->posX[row][col]
					&& x < baseCurrent->posX[row][col] + node->size[0] / BASE_SIZE && y >= baseCurrent->posY[row][col]
					&& y < baseCurrent->posY[row][col] + node->size[1] / BASE_SIZE) {
					/* Set position for a new building */
					B_SetBuildingByClick(row, col);
					return;
				}
	}

	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++)
			if (baseCurrent->map[row][col] != -1 && x >= baseCurrent->posX[row][col]
				&& x < baseCurrent->posX[row][col] + node->size[0] / BASE_SIZE && y >= baseCurrent->posY[row][col]
				&& y < baseCurrent->posY[row][col] + node->size[1] / BASE_SIZE) {
				entry = B_GetBuildingByIdx(baseCurrent->map[row][col]);
				if (!entry)
					Sys_Error("MN_BaseMapClick: no entry at %i:%i\n", x, y);

				if (*entry->onClick)
					Cbuf_ExecuteText(EXEC_NOW, entry->onClick);
#if 0
				else {
					/* Click on building : display its properties in the building menu */
					MN_PushMenu("buildings");
					baseCurrent->buildingCurrent = entry;
					B_BuildingStatus();
				}
#else
				else
					UP_OpenWith(entry->pedia);
#endif
				return;
			}
}


/**
 * @brief
 */
static void MN_ModelClick(menuNode_t * node)
{
	mouseSpace = MS_ROTATE;
	rotateAngles = node->angles;
}


/**
 * @brief Calls the script command for a text node that is clickable
 * @note The node must have the click parameter
 * @sa MN_TextRightClick
 */
static void MN_TextClick(menuNode_t * node, int mouseOver)
{
	Cbuf_AddText(va("%s_click %i\n", node->name, mouseOver - 1));
}

/**
 * @brief Calls the script command for a text node that is clickable via right mouse button
 * @note The node must have the rclick parameter
 * @sa MN_TextClick
 */
static void MN_TextRightClick(menuNode_t * node, int mouseOver)
{
	Cbuf_AddText(va("%s_rclick %i\n", node->name, mouseOver - 1));
}

/**
 * @brief
 * @sa MN_ModelClick
 * @sa MN_TextRightClick
 * @sa MN_TextClick
 * @sa MN_Drag
 * @sa MN_BarClick
 * @sa MN_BaseMapClick
 * @sa MAP_3DMapClick
 * @sa MAP_MapClick
 * @sa MN_ExecuteActions
 * @sa MN_RightClick
 */
void MN_Click(int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp, mouseOver;

	sp = menuStackPos;

	while (sp > 0) {
		menu = menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next) {
			if (node->type != MN_CONTAINER && !node->click)
				continue;

			/* check whether mouse is over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);

			if (!mouseOver)
				continue;

			/* found a node -> do actions */
			switch (node->type) {
			case MN_CONTAINER:
				MN_Drag(node, x, y);
				break;
			case MN_BAR:
				MN_BarClick(menu, node, x);
				break;
			case MN_BASEMAP:
				MN_BaseMapClick(node, x, y);
				break;
			case MN_MAP:
				MAP_MapClick(node, x, y);
				break;
			case MN_3DMAP:
				MAP_3DMapClick(node, x, y);
				break;
			case MN_MODEL:
				MN_ModelClick(node);
				break;
			case MN_TEXT:
				MN_TextClick(node, mouseOver);
				break;
			default:
				MN_ExecuteActions(menu, node->click);
				break;
			}
		}

		/* don't care about non-rendered windows */
		if (menu->renderNode || menu->popupNode)
			return;
	}
}

/**
 * @brief Scrolls the text in a textbox up/down.
 * @param[in] node The node of the text to be scrolled.
 * @param[in] offset Number of lines to scroll. Positive values scroll down, negative up.
 * @return Returns qtrue if scrolling was possible otherwise qfalse.
 */
static qboolean MN_TextScroll(menuNode_t *node, int offset)
{
	int textScroll_new;

	if (!node)
		return qfalse;

	if (abs(offset) >= node->height) {
		/* Offset value is bigger than textbox height. */
		node->textScroll = 0;
		return qfalse;
	}

	if (node->textLines <= node->height ) {
		/* Number of lines are less tehan the height of the textbox. */
		node->textScroll = 0;
		return qfalse;
	}

	textScroll_new = node->textScroll + offset;

	if (textScroll_new <= 0) {
		/* Goto top line, no matter how big the offset was. */
		node->textScroll = 0;
		return qtrue;

	} else if (textScroll_new >= (node->textLines + 1 - node->height)) {
		/* Goto last possible line line, no matter how big the offset was. */
		node->textScroll = node->textLines + 1 - node->height;
		return qtrue;

	} else {
		node->textScroll = textScroll_new;
		return qtrue;
	}
}

/**
 * @brief Scriptfunction that gets the wanted text node and scrolls the text.
 */
static void MN_TextScroll_f(void)
{
	int offset = 0;
	menuNode_t *node = NULL;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: textscroll <nodename> <+/-offset>\n");
		return;
	}

	node = MN_GetNodeFromCurrentMenu(Cmd_Argv(1));

	if ( !node ) {
		Com_DPrintf("MN_TextScroll_f: Node '%s' not found.\n", Cmd_Argv(1));
		return;
	}

	offset = atoi(Cmd_Argv(2));

	if ( offset == 0 )
		return;

	MN_TextScroll(node, offset);
}

/**
 * @brief
 * @sa MAP_ResetAction
 * @sa MN_TextRightClick
 * @sa MN_ExecuteActions
 * @sa MN_Click
 */
void MN_RightClick(int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp, mouseOver;

	sp = menuStackPos;

	while (sp > 0) {
		menu = menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next) {
			/* no right click for this node defined */
			if (!node->rclick)
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			/* found a node -> do actions */
			switch (node->type) {
			case MN_MAP:
				MAP_ResetAction();
				mouseSpace = MS_SHIFTMAP;
				break;
			case MN_3DMAP:
				MAP_ResetAction();
				mouseSpace = MS_SHIFT3DMAP;
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
 * @brief
 */
void MN_MiddleClick(int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp, mouseOver;

	sp = menuStackPos;

	while (sp > 0) {
		menu = menuStack[--sp];
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
			case MN_3DMAP:
				mouseSpace = MS_ZOOM3DMAP;
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
 * @brief Determine the position and size of render
 * @param[in] menu : use its position and size properties
 */
void MN_SetViewRect(const menu_t* menu)
{
	menuNode_t* menuNode = menu ? (menu->renderNode ? menu->renderNode : (menu->popupNode ? menu->popupNode : NULL)): NULL;

	if (!menuNode) {
		/* render the full screen */
		scr_vrect.x = scr_vrect.y = 0;
		scr_vrect.width = viddef.width;
		scr_vrect.height = viddef.height;
	} else if (menuNode->invis) {
		/* don't draw the scene */
		memset(&scr_vrect, 0, sizeof(scr_vrect));
	} else {
		/* the menu has a view size specified */
		scr_vrect.x = menuNode->pos[0] * viddef.rx;
		scr_vrect.y = menuNode->pos[1] * viddef.ry;
		scr_vrect.width = menuNode->size[0] * viddef.rx;
		scr_vrect.height = menuNode->size[1] * viddef.ry;
	}
}


/*
==============================================================
MENU DRAWING
==============================================================
*/

/**
 * @brief Draws an item to the screen
 *
 * @param[in] org Node position on the screen (pixel)
 * @param[in] item The item to draw
 * @param[in] sx Size in x direction (no pixel but container space)
 * @param[in] sy Size in y direction (no pixel but container space)
 * @param[in] x Position in container
 * @param[in] y Position in container
 * @param[in] scale
 * @param[in] color
 *
 * Used to draw an item to the equipment containers. First look whether the objDef_t
 * includes an image - if there is none then draw the model
 */
void MN_DrawItem(vec3_t org, item_t item, int sx, int sy, int x, int y, vec3_t scale, vec4_t color)
{
	modelInfo_t mi;
	objDef_t *od;
	vec3_t angles = { -10, 160, 70 };
	vec3_t origin;
	vec3_t size;
	vec4_t col;

	assert (item.t != NONE);
	od = &csi.ods[item.t];

	if (od->image[0]) {
		/* draw the image */
		re.DrawNormPic(
			org[0] + C_UNIT / 2.0 * sx + C_UNIT * x,
			org[1] + C_UNIT / 2.0 * sy + C_UNIT * y,
			C_UNIT * sx, C_UNIT * sy, 0, 0, 0, 0, ALIGN_CC, qtrue, od->image);
	} else if (od->model[0]) {
		/* draw model, if there is no image */
		mi.name = od->model;
		mi.origin = origin;
		mi.angles = angles;
		mi.center = od->center;
		mi.scale = size;

		if (od->scale) {
			VectorScale(scale, od->scale, size);
		} else {
			VectorCopy(scale, size);
		}

		VectorCopy(org, origin);
		if (x || y || sx || sy) {
			origin[0] += C_UNIT / 2.0 * sx;
			origin[1] += C_UNIT / 2.0 * sy;
			origin[0] += C_UNIT * x;
			origin[1] += C_UNIT * y;
		}

		mi.frame = 0;
		mi.oldframe = 0;
		mi.backlerp = 0;
		mi.skin = 0;

		Vector4Copy(color, col);
		if (od->weapon && od->reload && !item.a) {
			col[1] *= 0.5;
			col[2] *= 0.5;
		}

		mi.color = col;

		/* draw the model */
		re.DrawModelDirect(&mi, NULL, NULL);
	}
}



/**
 * @brief Generic tooltip function
 */
static void MN_DrawTooltip(char *font, char *string, int x, int y)
{
	int width = 0, height = 0;

	re.FontLength(font, string, &width, &height);
	if (!width)
		return;

	x += 5;
	y += 5;
	if (x + width > VID_NORM_WIDTH)
		x -= (width + 10);
	re.DrawFill(x - 1, y - 1, width, height, 0, tooltipBG);
	re.DrawColor(tooltipColor);
	re.FontDrawString(font, 0, x + 1, y + 1, x + 1, y + 1, width, 0, height, string,0,0,NULL);
	re.DrawColor(NULL);
}

/**
 * @brief Wrapper for menu tooltips
 */
static void MN_Tooltip(menuNode_t * node, int x, int y)
{
	char *tooltip;

	/* tooltips
	   data[5] is a char pointer to the tooltip text
	   see value_t nps for more info */
	if (node->data[5]) {
		tooltip = (char *) node->data[5];
		if (*tooltip == '_')
			tooltip++;
		MN_DrawTooltip("f_small", _(tooltip), x, y);
	}
}

/**
 * @brief
 */
void MN_PrecacheMenus(void)
{
	int i;
	menu_t *menu;
	menuNode_t *node;
	char *ref;
	char source[MAX_VAR];

	Com_Printf("...precaching %i menus\n", numMenus);

	for (i = 0; i < numMenus; i++) {
		menu = &menus[i];
		for (node = menu->firstNode; node; node = node->next) {
			if (!node->invis && node->data[0]) {
				ref = MN_GetReferenceString(menu, node->data[0]);
				if (!ref) {
					/* bad reference */
					node->invis = qtrue;
					Com_Printf("MN_PrecacheMenus: node \"%s\" bad reference \"%s\"\n", node->name, node->data);
					continue;
				}
				Q_strncpyz(source, ref, MAX_VAR);
				switch (node->type) {
				case MN_PIC:
					re.RegisterPic(ref);
					break;
				case MN_MODEL:
					/* FIXME: menuModel_t!! */
					re.RegisterModel(source);
					break;
				}
			}
		}
	}
}

/**
 * @brief Returns pointer to menu model
 * @param[in] menuModel menu model id from script files
 * @return menuModel_t pointer
 */
static menuModel_t *MN_GetMenuModel(char *menuModel)
{
	int i;
	menuModel_t *m;

	for (i = 0; i < numMenuModels; i++) {
		m = &menuModels[i];
		if (!Q_strncmp(m->id, menuModel, MAX_VAR))
			return m;
	}
	return NULL;
}

/**
 * @brief Return the font for a specific node or default font
 * @param[in] m The current menu pointer - if NULL we will use the current menuStack
 * @param[in] n The node to get the font for - if NULL f_small is returned
 * @return char pointer with font name (default is f_small)
 */
char *MN_GetFont(const menu_t *m, const menuNode_t *const n)
{
	if (!n || n->data[1]) {
		if (!m) {
			m = menuStack[menuStackPos-1];
		}
		return MN_GetReferenceString(m, n->data[1]);
	}
	return "f_small";
}

/**
 * @brief Draws the menu stack
 * @sa SCR_UpdateScreen
 */
void MN_DrawMenus(void)
{
	modelInfo_t mi = {0, };
	modelInfo_t pmi;
	menuNode_t *node;
	menu_t *menu;
	animState_t *as;
	char *ref, *font;
	char *anim;					/* model anim state */
	char source[MAX_VAR];
	static char oldSource[MAX_VAR] = "";
	int sp, pp;
	item_t item = {1,NONE,NONE}; /* 1 so it's not reddish; fake item anyway */
	vec4_t color;
	int mouseOver = 0;
	char *cur, *tab, *end;
	int y, x;
	message_t *message;
	menuModel_t *menuModel = NULL;
	int width, height;

	/* render every menu on top of a menu with a render node */
	pp = 0;
	sp = menuStackPos;
	while (sp > 0) {
		if (menuStack[--sp]->renderNode)
			break;
		if (menuStack[sp]->popupNode)
			pp = sp;
	}
	if (pp < sp)
		pp = sp;

	while (sp < menuStackPos) {
		menu = menuStack[sp++];
		for (node = menu->firstNode; node; node = node->next) {
			if (!node->invis && (node->data[0] /* 0 are images, models and strings e.g. */
					|| node->type == MN_CONTAINER || node->type == MN_TEXT || node->type == MN_BASEMAP || node->type == MN_MAP
					|| node->type == MN_3DMAP)) {
				/* if construct */
				if (*node->depends.var) {
					/* menuIfCondition_t */
					switch (node->depends.cond) {
					case IF_EQ:
						if (atof(node->depends.value) != Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value)
							continue;
						break;
					case IF_LE:
						if (Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value > atof(node->depends.value))
							continue;
						break;
					case IF_GE:
						if (Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value < atof(node->depends.value))
							continue;
						break;
					case IF_GT:
						if (Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value <= atof(node->depends.value))
							continue;
						break;
					case IF_LT:
						if (Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value >= atof(node->depends.value))
							continue;
						break;
					case IF_NE:
						if (Cvar_Get(node->depends.var, node->depends.value, 0, NULL)->value == atof(node->depends.value))
							continue;
						break;
					default:
						Sys_Error("Unknown condition for if statement: %i\n", node->depends.cond);
						break;
					}
				}

				/* mouse effects */
				if (sp > pp) {
					/* in and out events */
					mouseOver = MN_CheckNodeZone(node, mx, my);
					if (mouseOver != node->state) {
						/* maybe we are leaving to another menu */
						menu->hoverNode = NULL;
						if (mouseOver)
							MN_ExecuteActions(menu, node->mouseIn);
						else
							MN_ExecuteActions(menu, node->mouseOut);
						node->state = mouseOver;
					}
				}

				/* check node size x and y value to check whether they are zero */
				if (node->bgcolor && node->size[0] && node->size[1] && node->pos)
					re.DrawFill(node->pos[0] - 3, node->pos[1] - 3, node->size[0] + 6, node->size[1] + 6, 0, node->bgcolor);

				/* mouse darken effect */
				VectorScale(node->color, 0.8, color);
				color[3] = node->color[3];
				if (node->mousefx && node->type == MN_PIC && mouseOver && sp > pp)
					re.DrawColor(color);
				else
					re.DrawColor(node->color);

				/* get the reference */
				if (node->type != MN_BAR && node->type != MN_CONTAINER && node->type != MN_BASEMAP && node->type != MN_TEXT && node->type != MN_MAP
					&& node->type != MN_3DMAP) {
					ref = MN_GetReferenceString(menu, node->data[0]);
					if (!ref) {
						/* bad reference */
						node->invis = qtrue;
						Com_Printf("MN_DrawActiveMenus: node \"%s\" bad reference \"%s\"\n", node->name, node->data);
						continue;
					}
					Q_strncpyz(source, ref, MAX_VAR);
				} else
					ref = NULL;

				switch (node->type) {
				case MN_PIC:
					/* hack for ekg pics */
					if (!Q_strncmp(node->name, "ekg_", 4)) {
						int pt;

						if (node->name[4] == 'm')
							pt = Cvar_VariableValue("mn_morale") / 2;
						else
							pt = Cvar_VariableValue("mn_hp");
						node->texl[1] = (3 - (int) (pt < 60 ? pt : 60) / 20) * 32;
						node->texh[1] = node->texl[1] + 32;
						node->texl[0] = -(int) (0.01 * (node->name[4] - 'a') * cl.time) % 64;
						node->texh[0] = node->texl[0] + node->size[0];
					}
					if (ref)
						re.DrawNormPic(node->pos[0], node->pos[1], node->size[0], node->size[1],
								node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, ref);
					break;

				case MN_STRING:
					font = MN_GetFont(menu, node);
					if (!node->mousefx || cl.time % 1000 < 500)
						re.FontDrawString(font, node->align, node->pos[0], node->pos[1], node->pos[0], node->pos[1], node->size[0], 0, node->texh[0], ref,0,0,NULL);
					else
						re.FontDrawString(font, node->align, node->pos[0], node->pos[1], node->pos[0], node->pos[1], node->size[0], node->size[1], node->texh[0], va("%s*\n", ref),0,0,NULL);
					break;

				case MN_TEXT:
					if (menuText[node->num]) {
						char textCopy[MAX_MENUTEXTLEN];

						Q_strncpyz(textCopy, menuText[node->num], MAX_MENUTEXTLEN);
						font = MN_GetFont(menu, node);

						cur = textCopy;
						y = node->pos[1];
						node->textLines = 0; /* these are lines only in one-line texts! */
						/* but it's easy to fix, just change FontDrawString
							so that it returns a pair, #lines and height
							and add that to variable line; the only other file
							using FontDrawString result is client/cl_sequence.c
							and there just ignore #lines */
						do {
							/* new line starts from node x position */
							x = node->pos[0];

							/* get the position of the next newline - otherwise end will be null */
							end = strchr(cur, '\n');
							if (end)
								/* set the \n to \0 to draw only this part (before the \n) with our font renderer */
								/* let end point to the next char after the \n (or \0 now) */
								*end++ = '\0';

							/* FIXME: only works with one-line texts right now: */
							if (node->mousefx && node->textLines == mouseOver)
								re.DrawColor(color);

							/* we assume all the tabs fit on a single line */
							do {
								tab = strchr(cur, '\t');
								/* if this string does not contain any tabstops break this do while ... */
								if (!tab)
									break;
								/* ... otherwise set the \t to \0 and increase the tab pointer to the next char */
								/* after the tab (needed for the next loop in this (the inner) do while) */
								*tab++ = '\0';
								re.FontDrawString(font, node->align, x, y, node->pos[0], node->pos[1], node->size[0], node->size[1], node->texh[0], cur, node->height, node->textScroll, &node->textLines );
								/* increase the x value as given via menu definition format string */
								/* or use 1/3 of the node size (width) */
								if (!node->texh[1])
									x += (node->size[0] / 3);
								else
									x += node->texh[1];
								/* now set cur to the first char after the \t */
								cur = tab;
							} while (1);

							/* the conditional expression at the end is a hack to draw "/n/n" as a blank line */
							y += re.FontDrawString(font, node->align, x, y, node->pos[0], node->pos[1], node->size[0], node->size[1], node->texh[0], (*cur ? cur : " "),0,0,NULL);

							if (node->mousefx && node->textLines == mouseOver)
								re.DrawColor(node->color); /* why is this repeated? */

							/* now set cur to the next char after the \n (see above) */
							cur = end;
						} while (cur);
					} else if (node->num == TEXT_MESSAGESYSTEM) {
						if (node->data[1])
							font = MN_GetReferenceString(menu, node->data[1]);
						else
							font = "f_small";

						y = node->pos[1];
						node->textLines = 0;
						message = messageStack;
						while (message) {
							if (node->textLines >= node->height) {
								/* TODO: Draw the scrollbars */
								break;
							}
							node->textLines++;

							/* maybe due to scrolling this line is not visible */
							if (node->textLines > node->textScroll) {
								char text[TIMESTAMP_TEXT + MAX_MESSAGE_TEXT];
								MN_TimestampedText(text, sizeof(text), message);
								re.FontLength(font, text, &width, &height);
								if (!width)
									break;
								if (width > node->pos[0] + node->size[0]) {
									/* TODO: not tested this.... */
									/* we use a backslash to determine where to break the line */
									tab = text;
									while ((end = strstr(tab, "\\")) != NULL) {
										*end++ = '\0';
										y += re.FontDrawString(font, ALIGN_UL, node->pos[0], y, node->pos[0], node->pos[1], node->size[0], node->size[1], node->texh[0], tab,0,0,NULL);
										tab = end;
										node->textLines++;
										if (node->textLines >= node->height)
											break;
									}
								} else {
									/* newline to space - we don't need this */
									while ((end = strstr(text, "\\")) != NULL)
										*end = ' ';

									y += re.FontDrawString(font, ALIGN_UL, node->pos[0], y, node->pos[0], node->pos[1], node->size[0], node->size[1], node->texh[0], text,0,0,NULL);
								}
							}

							message = message->next;
						}
					}
					break;

				case MN_BAR:
					{
						float fac, width;

						fac = node->size[0] / (MN_GetReferenceFloat(menu, node->data[0]) - MN_GetReferenceFloat(menu, node->data[1]));
						width = (MN_GetReferenceFloat(menu, node->data[2]) - MN_GetReferenceFloat(menu, node->data[1])) * fac;
						re.DrawFill(node->pos[0], node->pos[1], width, node->size[1], node->align, mouseOver ? color : node->color);
					}
					break;

				case MN_CONTAINER:
					if (menuInventory) {
						vec3_t scale = {3.5, 3.5, 3.5};
						invList_t *ic;

						color[0] = color[1] = color[2] = 0.5;
						color[3] = 1;

						if (node->mousefx == C_UNDEFINED)
							MN_FindContainer(node);
						if (node->mousefx == NONE)
							break;

						if (csi.ids[node->mousefx].single) {
							/* single item container (special case for left hand) */
							if (node->mousefx == csi.idLeft && !menuInventory->c[csi.idLeft]) {
								color[3] = 0.5;
								if (menuInventory->c[csi.idRight] && csi.ods[menuInventory->c[csi.idRight]->item.t].twohanded)
									MN_DrawItem(node->pos, menuInventory->c[csi.idRight]->item, node->size[0] / C_UNIT, node->size[1] / C_UNIT, 0, 0, scale,
												color);
							} else if (menuInventory->c[node->mousefx])
								MN_DrawItem(node->pos, menuInventory->c[node->mousefx]->item, node->size[0] / C_UNIT, node->size[1] / C_UNIT, 0, 0, scale,
											color);
						} else {
							/* standard container */
							for (ic = menuInventory->c[node->mousefx]; ic; ic = ic->next) {
								MN_DrawItem(node->pos, ic->item, csi.ods[ic->item.t].sx, csi.ods[ic->item.t].sy, ic->x, ic->y, scale, color);
							}
						}
						/* draw free space if dragging - but not for idEquip */
						if (node->mousefx != csi.idEquip)
							MN_InvDrawFree(menuInventory, node);
					}
					break;

				case MN_ITEM:
					color[0] = color[1] = color[2] = 0.5;
					color[3] = 1;

					if (node->mousefx == C_UNDEFINED)
						MN_FindContainer(node);
					if (node->mousefx == NONE) {
						Com_Printf("no container...\n");
						break;
					}

					for (item.t = 0; item.t < csi.numODs; item.t++)
						if (!Q_strncmp(ref, csi.ods[item.t].kurz, MAX_VAR))
							break;
					if (item.t == csi.numODs)
						break;

					MN_DrawItem(node->pos, item, 0, 0, 0, 0, node->scale, color);
					break;

				case MN_MODEL:
					/* set model properties */
					node->menuModel = MN_GetMenuModel(source);
					if (!node->menuModel) {
						menuModel = NULL;
						mi.model = re.RegisterModel(source);
						if (!mi.model)
							break;
					} else
						menuModel = node->menuModel;

					mi.name = source;

					mi.origin = node->origin;
					mi.angles = node->angles;
					mi.scale = node->scale;
					mi.center = node->center;
					mi.color = node->color;
					/* no animation */
					mi.frame = 0;
					mi.oldframe = 0;
					mi.backlerp = 0;

					/* autoscale? */
					if (!node->scale[0]) {
						mi.scale = NULL;
						mi.center = node->size;
					}

					do {
						if (menuModel) {
							mi.model = re.RegisterModel(menuModel->model);
							mi.name = menuModel->model;
							if (VectorNotEmpty(menuModel->origin))
								mi.origin = menuModel->origin;
							if (VectorNotEmpty(menuModel->angles))
								mi.angles = menuModel->angles;
							if (VectorNotEmpty(menuModel->scale))
								mi.scale = menuModel->scale;
							if (VectorNotEmpty(menuModel->center))
								mi.center = menuModel->center;
							if (Vector4NotEmpty(menuModel->color))
								mi.color = menuModel->color;
							if (!mi.model) {
								menuModel = menuModel->next;
								if (!menuModel)
									break;
								continue;
							}
							mi.skin = menuModel->skin;
							if (*menuModel->anim) {
								as = &menuModel->animState;
								anim = re.AnimGetName(as, mi.model);
								if (anim && Q_strncmp(anim, ref, MAX_VAR))
									re.AnimChange(as, mi.model, ref);
								re.AnimRun(as, mi.model, cls.frametime * 1000);
							}
							re.DrawModelDirect(&mi, NULL, NULL);
						} else {
							/* get skin */
							if (node->data[3] && *(char *) node->data[3])
								mi.skin = atoi(MN_GetReferenceString(menu, node->data[3]));
							else
								mi.skin = 0;

							/* do animations */
							if (node->data[1] && *(char *) node->data[1]) {
								ref = MN_GetReferenceString(menu, node->data[1]);
								if (!node->data[2] && Q_strncmp(oldSource, source, MAX_VAR)) {
									Q_strncpyz(oldSource, source, MAX_VAR);
									node->data[4] = NULL;
								}
								if (!node->data[4]) {
									/* new anim state */
									as = (animState_t *) curadata;
									curadata += sizeof(animState_t);
									memset(as, 0, sizeof(animState_t));
									re.AnimChange(as, mi.model, ref);
									node->data[4] = as;
								} else {
									/* change anim if needed */
									as = node->data[4];
									anim = re.AnimGetName(as, mi.model);
									if (anim && Q_strncmp(anim, ref, MAX_VAR))
										re.AnimChange(as, mi.model, ref);
									re.AnimRun(as, mi.model, cls.frametime * 1000);
								}

								mi.frame = as->frame;
								mi.oldframe = as->oldframe;
								mi.backlerp = as->backlerp;
							}

							/* place on tag */
							if (node->data[2]) {
								menuNode_t *search;
								char parent[MAX_VAR];
								char *tag;

								Q_strncpyz(parent, MN_GetReferenceString(menu, node->data[2]), MAX_VAR);
								tag = parent;
								while (*tag && *tag != ' ')
									tag++;
								*tag++ = 0;

								for (search = menu->firstNode; search != node && search; search = search->next)
									if (search->type == MN_MODEL && !Q_strncmp(search->name, parent, MAX_VAR)) {
										char model[MAX_VAR];

										Q_strncpyz(model, MN_GetReferenceString(menu, search->data[0]), MAX_VAR);

										pmi.model = re.RegisterModel(model);
										if (!pmi.model)
											break;

										pmi.name = model;
										pmi.origin = search->origin;
										pmi.angles = search->angles;
										pmi.scale = search->scale;
										pmi.center = search->center;

										/* autoscale? */
										if (!node->scale[0]) {
											mi.scale = NULL;
											mi.center = node->size;
										}

										as = search->data[4];
										pmi.frame = as->frame;
										pmi.oldframe = as->oldframe;
										pmi.backlerp = as->backlerp;

										re.DrawModelDirect(&mi, &pmi, tag);
										break;
									}
							} else
								re.DrawModelDirect(&mi, NULL, NULL);
						}

						if (menuModel)
							menuModel = menuModel->next;
					} while (menuModel != NULL);
					break;

				case MN_3DMAP:
				case MN_MAP:
					if (curCampaign) {
						CL_CampaignRun();	/* advance time */
						MAP_DrawMap(node, node->type == MN_3DMAP); /* Draw geoscape */
					}
					break;

				case MN_BASEMAP:
					B_DrawBase(node);
					break;
				}				/* switch */

				/* mouseover? */
				if (node->state == qtrue)
					menu->hoverNode = node;
			}					/* if */
		}						/* for */
		if (sp == menuStackPos && menu->hoverNode && cl_show_tooltips->value) {
			MN_Tooltip(menu->hoverNode, mx, my);
			menu->hoverNode = NULL;
		}
	}
	re.DrawColor(NULL);
}


/*
==============================================================

GENERIC MENU FUNCTIONS

==============================================================
*/

/**
 * @brief
 */
static void MN_DeleteMenu(menu_t * menu)
{
	int i;

	for (i = 0; i < menuStackPos; i++)
		if (menuStack[i] == menu) {
			for (menuStackPos--; i < menuStackPos; i++)
				menuStack[i] = menuStack[i + 1];
			return;
		}
}

/**
 * @brief Get the current active menu
 * @return menu_t pointer from menu stack
 */
menu_t* MN_ActiveMenu(void)
{
	if (menuStackPos >= 0)
		return menuStack[menuStackPos-1];

	return NULL;
}

/**
 * @brief Push a menu onto the menu stack
 * @param[in] name Name of the menu to push onto menu stack
 * @return pointer to menu_t
 */
menu_t* MN_PushMenuDelete(char *name, qboolean delete)
{
	int i;

	for (i = 0; i < numMenus; i++)
		if (!Q_strncmp(menus[i].name, name, MAX_VAR)) {
			/* found the correct add it to stack or bring it on top */
			if (delete)
				MN_DeleteMenu(&menus[i]);

			if (menuStackPos < MAX_MENUSTACK)
				menuStack[menuStackPos++] = &menus[i];
			else
				Com_Printf("Menu stack overflow\n");

			/* initialize it */
			if (menus[i].initNode)
				MN_ExecuteActions(&menus[i], menus[i].initNode->click);

			cls.key_dest = key_game;
			return menus + i;
		}

	Com_Printf("Didn't find menu \"%s\"\n", name);
	return NULL;
}

/**
 * @brief Push a menu onto the menu stack
 * @param[in] name Name of the menu to push onto menu stack
 * @return pointer to menu_t
 */
menu_t* MN_PushMenu(char *name)
{
	return MN_PushMenuDelete(name, qtrue);
}

/**
 * @brief Console function to push a menu onto the menu stack
 * @sa MN_PushMenu
 */
static void MN_PushMenu_f(void)
{
	if (Cmd_Argc() > 1)
		MN_PushMenu(Cmd_Argv(1));
	else
		Com_Printf("usage: mn_push <name>\n");
}

/**
 * @brief Console function to push a menu, without deleting its copies
 * @sa MN_PushMenu
 */
static void MN_PushCopyMenu_f(void)
{
	if (Cmd_Argc() > 1) {
		Cvar_SetValue("mn_escpop", mn_escpop->value + 1);
		MN_PushMenuDelete(Cmd_Argv(1), qfalse);
	} else {
		Com_Printf("usage: mn_push_copy <name>\n");
	}
}


/**
 * @brief Pops a menu from the menu stack
 * @param[in] all If true pop all menus from stack
 * @sa MN_PopMenu_f
 */
void MN_PopMenu(qboolean all)
{
	if (all)
		while (menuStackPos > 0) {
			menuStackPos--;
			if (menuStack[menuStackPos]->closeNode)
				MN_ExecuteActions(menuStack[menuStackPos], menuStack[menuStackPos]->closeNode->click);
		}

	if (menuStackPos > 0) {
		menuStackPos--;
		if (menuStack[menuStackPos]->closeNode)
			MN_ExecuteActions(menuStack[menuStackPos], menuStack[menuStackPos]->closeNode->click);
	}

	if (!all && menuStackPos == 0) {
		if (!Q_strncmp(menuStack[0]->name, mn_main->string, MAX_VAR)) {
			if (*mn_active->string)
				MN_PushMenu(mn_active->string);
			if (!menuStackPos)
				MN_PushMenu(mn_main->string);
		} else {
			if (*mn_main->string)
				MN_PushMenu(mn_main->string);
			if (!menuStackPos)
				MN_PushMenu(mn_active->string);
		}
	}

	cls.key_dest = key_game;
}

/**
 * @brief Console function to pop a menu from the menu stack
 * @sa MN_PopMenu
 * @note The cvar mn_escpop defined how often the MN_PopMenu function is called.
 * This is useful for e.g. nodes that doesn't have a render node (for example: video options)
 */
static void MN_PopMenu_f(void)
{
	if (Cmd_Argc() < 2 || Q_strncmp(Cmd_Argv(1), "esc", 3)) {
		MN_PopMenu(qfalse);
	} else {
		int i;

		for (i = 0; i < (int) mn_escpop->value; i++)
			MN_PopMenu(qfalse);

		Cvar_Set("mn_escpop", "1");
	}
}


/**
 * @brief
 */
static void MN_Modify_f(void)
{
	float value;

	if (Cmd_Argc() < 5)
		Com_Printf("usage: mn_modify <name> <amount> <min> <max>\n");

	value = Cvar_VariableValue(Cmd_Argv(1));
	value += atof(Cmd_Argv(2));
	if (value < atof(Cmd_Argv(3)))
		value = atof(Cmd_Argv(3));
	else if (value > atof(Cmd_Argv(4)))
		value = atof(Cmd_Argv(4));

	Cvar_SetValue(Cmd_Argv(1), value);
}


/**
 * @brief
 */
static void MN_ModifyWrap_f(void)
{
	float value;

	if (Cmd_Argc() < 5)
		Com_Printf("usage: mn_modifywrap <name> <amount> <min> <max>\n");

	value = Cvar_VariableValue(Cmd_Argv(1));
	value += atof(Cmd_Argv(2));
	if (value < atof(Cmd_Argv(3)))
		value = atof(Cmd_Argv(4));
	else if (value > atof(Cmd_Argv(4)))
		value = atof(Cmd_Argv(3));

	Cvar_SetValue(Cmd_Argv(1), value);
}


/**
 * @brief
 */
static void MN_ModifyString_f(void)
{
	qboolean next;
	char *current, *list, *tp;
	char token[MAX_VAR], last[MAX_VAR], first[MAX_VAR];
	int add;

	if (Cmd_Argc() < 4)
		Com_Printf("usage: mn_modifystring <name> <amount> <list>\n");

	current = Cvar_VariableString(Cmd_Argv(1));
	add = atoi(Cmd_Argv(2));
	list = Cmd_Argv(3);
	last[0] = 0;
	first[0] = 0;
	next = qfalse;

	while (add) {
		tp = token;
		while (*list && *list != ':')
			*tp++ = *list++;
		if (*list)
			list++;
		*tp = 0;

		if (*token && !first[0])
			Q_strncpyz(first, token, MAX_VAR);

		if (!*token) {
			if (add < 0 || next)
				Cvar_Set(Cmd_Argv(1), last);
			else
				Cvar_Set(Cmd_Argv(1), first);
			return;
		}

		if (next) {
			Cvar_Set(Cmd_Argv(1), token);
			return;
		}

		if (!Q_strncmp(token, current, MAX_VAR)) {
			if (add < 0) {
				if (last[0])
					Cvar_Set(Cmd_Argv(1), last);
				else
					Cvar_Set(Cmd_Argv(1), first);
				return;
			} else
				next = qtrue;
		}
		Q_strncpyz(last, token, MAX_VAR);
	}
}


/**
 * @brief
 *
 * Shows the corresponding strings in menu
 * Example: Optionsmenu - fullscreen: yes
 */
static void MN_Translate_f(void)
{
	qboolean next;
	char *current, *list, *orig, *trans;
	char original[MAX_VAR], translation[MAX_VAR];

	if (Cmd_Argc() < 4)
		Com_Printf("usage: mn_translate <source> <dest> <list>\n");

	current = Cvar_VariableString(Cmd_Argv(1));
	list = Cmd_Argv(3);
	next = qfalse;

	while (*list) {
		orig = original;
		while (*list && *list != ':')
			*orig++ = *list++;
		*orig = 0;
		list++;

		trans = translation;
		while (*list && *list != ',')
			*trans++ = *list++;
		*trans = 0;
		list++;

		if (!Q_strncmp(current, original, MAX_VAR)) {
			Cvar_Set(Cmd_Argv(2), _(translation));
			return;
		}
	}

	/* nothing found, copy value */
	Cvar_Set(Cmd_Argv(2), _(current));
}

#define MAX_TUTORIALLIST 512
static char tutorialList[MAX_TUTORIALLIST];
/**
 * @brief
 */
static void MN_GetTutorials_f(void)
{
	int i;
	tutorial_t *t;

	menuText[TEXT_LIST] = tutorialList;
	tutorialList[0] = 0;
	for (i = 0; i < numTutorials; i++) {
		t = &tutorials[i];
		Q_strcat(tutorialList, va("%s\n", t->name), sizeof(tutorialList));
	}
}

/**
 * @brief
 */
static void MN_ListTutorials_f(void)
{
	int i;

	Com_Printf("Tutorials: %i\n", numTutorials);
	for (i = 0; i < numTutorials; i++) {
		Com_Printf("tutorial: %s\n", tutorials[i].name);
		Com_Printf("..sequence: %s\n", tutorials[i].sequence);
	}
}

/**
 * @brief click function for text tutoriallist in menu_tutorials.ufo
 */
static void MN_TutorialListClick_f(void)
{
	int num;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: tutoriallist_click <num>\n");
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= numTutorials)
		return;

	Cbuf_ExecuteText(EXEC_NOW, va("seq_start %s", tutorials[num].sequence));
}

/**
 * @brief
 */
static void MN_ListMenuModels_f(void)
{
	int i;

	/* search for menumodels with same name */
	Com_Printf("menu models: %i\n", numMenuModels);
	for (i = 0; i < numMenuModels; i++)
		Com_Printf("id: %s\n...model: %s\n...need: %s\n\n", menuModels[i].id, menuModels[i].model, menuModels[i].need);
}

/**
 * @brief
 */
void MN_ResolutionChange_f (void)
{
	char* action;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: mn_resolution_change [-|+]\n");
		return;
	}
	action = Cmd_Argv(1);
	switch (*action) {
	case '+':
		Cbuf_ExecuteText(EXEC_NOW, va("mn_modify mn_glmode 1 0 %i;", maxVidModes-1));
		break;
	case '-':
		Cbuf_ExecuteText(EXEC_NOW, va("mn_modify mn_glmode -1 0 %i;", maxVidModes-1));
		break;
	}
}

/**
 * @brief
 */
void MN_InitKeyList_f (void)
{
	static char keylist[2048];
	int i;

	*keylist = '\0';

	for (i = 0; i < 256; i++)
		if (keybindings[i] && keybindings[i][0]) {
			Com_Printf("%s - %s\n", Key_KeynumToString(i), keybindings[i]);
			Q_strcat(keylist, va("%s\t%s\n", Key_KeynumToString(i), Cmd_GetCommandDesc(keybindings[i])), sizeof(keylist));
		}

	menuText[TEXT_STANDARD] = keylist;
}

/**
 * @brief
 */
void MN_ResetMenus(void)
{
	int i;

	/* reset menu structures */
	numActions = 0;
	numNodes = 0;
	numMenus = 0;
	numMenuModels = 0;
	menuStackPos = 0;
	numTutorials = 0;

	/* add commands */
	mn_escpop = Cvar_Get("mn_escpop", "1", 0, NULL);
	Cvar_Set("mn_main", "main");
	Cvar_Set("mn_sequence", "sequence");

	/* textbox */
	Cmd_AddCommand("textscroll", MN_TextScroll_f, NULL);

	/* print the keybindings to menuText */
	Cmd_AddCommand("mn_init_keylist", MN_InitKeyList_f, NULL);

	Cmd_AddCommand("mn_resolution_change", MN_ResolutionChange_f, NULL);

	/* tutorial stuff */
	Cmd_AddCommand("listtutorials", MN_ListTutorials_f, NULL);
	Cmd_AddCommand("gettutorials", MN_GetTutorials_f, NULL);
	Cmd_AddCommand("tutoriallist_click", MN_TutorialListClick_f, NULL);

	Cmd_AddCommand("getmaps", MN_GetMaps_f, NULL);
	Cmd_AddCommand("mn_startserver", MN_StartServer, NULL);
	Cmd_AddCommand("mn_nextmap", MN_NextMap, NULL);
	Cmd_AddCommand("mn_prevmap", MN_PrevMap, NULL);
	Cmd_AddCommand("mn_push", MN_PushMenu_f, NULL);
	Cmd_AddCommand("mn_push_copy", MN_PushCopyMenu_f, NULL);
	Cmd_AddCommand("mn_pop", MN_PopMenu_f, NULL);
	Cmd_AddCommand("mn_modify", MN_Modify_f, NULL);
	Cmd_AddCommand("mn_modifywrap", MN_ModifyWrap_f, NULL);
	Cmd_AddCommand("mn_modifystring", MN_ModifyString_f, NULL);
	Cmd_AddCommand("mn_translate", MN_Translate_f, NULL);
	Cmd_AddCommand("menumodelslist", MN_ListMenuModels_f, NULL);
	/* get action data memory */
	if (adataize)
		memset(adata, 0, adataize);
	else {
		Hunk_Begin(0x40000);
		adata = Hunk_Alloc(0x40000);
		adataize = Hunk_End();
	}
	curadata = adata;

	/* reset menu texts */
	for (i = 0; i < MAX_MENUTEXTS; i++)
		menuText[i] = NULL;

	/* reset ufopedia & basemanagement */
	UP_ResetUfopedia();
	B_ResetBaseManagement();
	RS_ResetResearch();
	PR_ResetProduction();
	E_Reset();
	MAP_ResetAction();
	UFO_Reset();
}

/**
 * @brief
 */
void MN_Shutdown(void)
{
	/* free the memory */
	if (adataize)
		Hunk_Free(adata);
}

/*
==============================================================
MENU PARSING
==============================================================
*/

/**
 * @brief
 */
qboolean MN_ParseAction(menuNode_t * menuNode, menuAction_t * action, char **text, char **token)
{
	char *errhead = "MN_ParseAction: unexpected end of file (in event)";
	menuAction_t *lastAction;
	menuNode_t *node;
	qboolean found;
	value_t *val;
	int i;

	lastAction = NULL;

	do {
		/* get new token */
		*token = COM_EParse(text, errhead, NULL);
		if (!*token)
			return qfalse;

		/* get actions */
		do {
			found = qfalse;

			/* standard function execution */
			for (i = 0; i < EA_CALL; i++)
				if (!Q_stricmp(*token, ea_strings[i])) {
/*					Com_Printf( "   %s", *token ); */

					/* add the action */
					if (lastAction) {
						action = &menuActions[numActions++];
						memset(action, 0, sizeof(menuAction_t));
						lastAction->next = action;
					}
					action->type = i;

					if (ea_values[i] != V_NULL) {
						/* get parameter values */
						*token = COM_EParse(text, errhead, NULL);
						if (!*text)
							return qfalse;

/*						Com_Printf( " %s", *token ); */

						/* get the value */
						action->data = curadata;
						curadata += Com_ParseValue(curadata, *token, ea_values[i], 0);
					}

/*					Com_Printf( "\n" ); */

					/* get next token */
					*token = COM_EParse(text, errhead, NULL);
					if (!*text)
						return qfalse;

					lastAction = action;
					found = qtrue;
					break;
				}

			/* node property setting */
			if (**token == '*') {
/*				Com_Printf( "   %s", *token ); */

				/* add the action */
				if (lastAction) {
					action = &menuActions[numActions++];
					memset(action, 0, sizeof(menuAction_t));
					lastAction->next = action;
				}
				action->type = EA_NODE;

				/* get the node name */
				action->data = curadata;

				strcpy((char *) curadata, &(*token)[1]);
				curadata += strlen((char *) curadata) + 1;

				/* get the node property */
				*token = COM_EParse(text, errhead, NULL);
				if (!*text)
					return qfalse;

/*				Com_Printf( " %s", *token ); */

				for (val = nps, i = 0; val->type; val++, i++)
					if (!Q_stricmp(*token, val->string)) {
						*(int *) curadata = i;
						break;
					}

				if (!val->type) {
					Com_Printf("MN_ParseAction: token \"%s\" isn't a node property (in event)\n", *token);
					curadata = action->data;
					if (lastAction) {
						lastAction->next = NULL;
						numActions--;
					}
					break;
				}

				/* get the value */
				*token = COM_EParse(text, errhead, NULL);
				if (!*text)
					return qfalse;

/*				Com_Printf( " %s\n", *token ); */

				curadata += sizeof(int);
				curadata += Com_ParseValue(curadata, *token, val->type, 0);

				/* get next token */
				*token = COM_EParse(text, errhead, NULL);
				if (!*text)
					return qfalse;

				lastAction = action;
				found = qtrue;
			}

			/* function calls */
			for (node = menus[numMenus - 1].firstNode; node; node = node->next)
				if ((node->type == MN_FUNC || node->type == MN_CONFUNC) && !Q_strncmp(node->name, *token, sizeof(node->name))) {
/*					Com_Printf( "   %s\n", node->name ); */

					/* add the action */
					if (lastAction) {
						action = &menuActions[numActions++];
						memset(action, 0, sizeof(menuAction_t));
						lastAction->next = action;
					}
					action->type = EA_CALL;

					action->data = curadata;
					*(menuAction_t ***) curadata = &node->click;
					curadata += sizeof(menuAction_t *);

					/* get next token */
					*token = COM_EParse(text, errhead, NULL);
					if (!*text)
						return qfalse;

					lastAction = action;
					found = qtrue;
					break;
				}
		} while (found);

		/* test for end or unknown token */
		if (**token == '}') {
			/* finished */
			return qtrue;
		} else {
			/* unknown token, print message and continue */
			Com_Printf("MN_ParseAction: unknown token \"%s\" ignored (in event) (node: %s)\n", *token, menuNode->name);
		}
	} while (*text);

	return qfalse;
}

/**
 * @brief
 */
qboolean MN_ParseNodeBody(menuNode_t * node, char **text, char **token)
{
	char *errhead = "MN_ParseNodeBody: unexpected end of file (node";
	qboolean found;
	value_t *val;
	int i;

	/* functions are a special case */
	if (node->type == MN_CONFUNC || node->type == MN_FUNC) {
		menuAction_t **action;

		/* add new actions to end of list */
		action = &node->click;
		for (; *action; action = &(*action)->next);

		*action = &menuActions[numActions++];
		memset(*action, 0, sizeof(menuAction_t));

		if (node->type == MN_CONFUNC)
			Cmd_AddCommand(node->name, MN_Command, NULL);

		return MN_ParseAction(node, *action, text, token);
	}

	do {
		/* get new token */
		*token = COM_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;

		/* get properties, events and actions */
		do {
			found = qfalse;

			for (val = nps; val->type; val++)
				if (!Q_strcmp(*token, val->string)) {
/*					Com_Printf( "  %s", *token ); */

					if (val->type != V_NULL) {
						/* get parameter values */
						*token = COM_EParse(text, errhead, node->name);
						if (!*text)
							return qfalse;

/*						Com_Printf( " %s", *token ); */

						/* get the value */
						/* 0, -1, -2, -3, -4, -5 fills the data array in menuNode_t */
						if ((val->ofs > 0) && (val->ofs < (size_t)-5))
							Com_ParseValue(node, *token, val->type, val->ofs);
						else {
							/* a reference to data is handled like this */
/* 							Com_Printf( "%i %s\n", val->ofs, *token ); */
							node->data[-(val->ofs)] = curadata;
							if (**token == '*')
								curadata += Com_ParseValue(curadata, *token, V_STRING, 0);
							else
								curadata += Com_ParseValue(curadata, *token, val->type, 0);
						}
					}

/*					Com_Printf( "\n" ); */

					/* get next token */
					*token = COM_EParse(text, errhead, node->name);
					if (!*text)
						return qfalse;

					found = qtrue;
					break;
				}

			for (i = 0; i < NE_NUM_NODEEVENT; i++)
				if (!Q_strcmp(*token, ne_strings[i])) {
					menuAction_t **action;

/*					Com_Printf( "  %s\n", *token ); */

					/* add new actions to end of list */
					action = (menuAction_t **) ((byte *) node + ne_values[i]);
					for (; *action; action = &(*action)->next);

					*action = &menuActions[numActions++];
					memset(*action, 0, sizeof(menuAction_t));

					/* get the action body */
					*token = COM_EParse(text, errhead, node->name);
					if (!*text)
						return qfalse;

					if (**token == '{') {
						MN_ParseAction(node, *action, text, token);

						/* get next token */
						*token = COM_EParse(text, errhead, node->name);
						if (!*text)
							return qfalse;
					}

					found = qtrue;
					break;
				}
		} while (found);

		/* test for end or unknown token */
		if (**token == '}') {
			/* finished */
			return qtrue;
		} else {
			/* unknown token, print message and continue */
			Com_Printf("MN_ParseNodeBody: unknown token \"%s\" ignored (node \"%s\")\n", *token, node->name);
		}
	} while (*text);

	return qfalse;
}

/**
 * @brief Hides a given menu node
 * @note Sanity check whether node is null included
 */
void MN_HideNode ( menuNode_t* node )
{
	if ( node && node->invis == qtrue )
		node->invis = qfalse;
}

/**
 * @brief Unhides a given menu node
 * @note Sanity check whether node is null included
 */
void MN_UnHideNode ( menuNode_t* node )
{
	if ( node && node->invis == qfalse )
		node->invis = qtrue;
}

/**
 * @brief
 */
qboolean MN_ParseMenuBody(menu_t * menu, char **text)
{
	char *errhead = "MN_ParseMenuBody: unexpected end of file (menu";
	char *token;
	qboolean found;
	menuNode_t *node, *lastNode;
	int i;

	lastNode = NULL;

	do {
		/* get new token */
		token = COM_EParse(text, errhead, menu->name);
		if (!*text)
			return qfalse;

		/* get node type */
		do {
			found = qfalse;

			for (i = 0; i < MN_NUM_NODETYPE; i++)
				if (!Q_strcmp(token, nt_strings[i])) {
					/* found node */
					/* get name */
					token = COM_EParse(text, errhead, menu->name);
					if (!*text)
						return qfalse;

					/* test if node already exists */
					for (node = menu->firstNode; node; node = node->next)
						if (!Q_strncmp(token, node->name, sizeof(node->name))) {
							if (node->type != i)
								Com_Printf("MN_ParseMenuBody: node prototype type change (menu \"%s\")\n", menu->name);
							break;
						}

					/* initialize node */
					if (!node) {
						if (numNodes>=MAX_MENUNODES)
							Sys_Error("MAX_MENUNODES exceeded\n");
						node = &menuNodes[numNodes++];
						memset(node, 0, sizeof(menuNode_t));
						Q_strncpyz(node->name, token, MAX_VAR);

						/* link it in */
						if (lastNode)
							lastNode->next = node;
						else
							menu->firstNode = node;

						lastNode = node;
					}

					node->type = i;

/*					Com_Printf( " %s %s\n", nt_strings[i], *token ); */

					/* check for special nodes */
					switch (node->type) {
					case MN_FUNC:
						if (!Q_strncmp(node->name, "init", 4)) {
							if (!menu->initNode)
								menu->initNode = node;
							else
								Com_Printf("MN_ParseMenuBody: second init function ignored (menu \"%s\")\n", menu->name);
						} else if (!Q_strncmp(node->name, "close", 5)) {
							if (!menu->closeNode)
								menu->closeNode = node;
							else
								Com_Printf("MN_ParseMenuBody: second close function ignored (menu \"%s\")\n", menu->name);
						}
						break;
					case MN_ZONE:
						if (!Q_strncmp(node->name, "render", 6)) {
							if (!menu->renderNode)
								menu->renderNode = node;
							else
								Com_Printf("MN_ParseMenuBody: second render node ignored (menu \"%s\")\n", menu->name);
						} else if (!Q_strncmp(node->name, "popup", 5)) {
							if (!menu->popupNode)
								menu->popupNode = node;
							else
								Com_Printf("MN_ParseMenuBody: second popup node ignored (menu \"%s\")\n", menu->name);
						}
						break;
					case MN_CONTAINER:
						node->mousefx = C_UNDEFINED;
						break;
					}

					/* set standard texture coordinates */
/*					node->texl[0] = 0; node->texl[1] = 0; */
/*					node->texh[0] = 1; node->texh[1] = 1; */

					/* get parameters */
					token = COM_EParse(text, errhead, menu->name);
					if (!*text)
						return qfalse;

					if (*token == '{') {
						if (!MN_ParseNodeBody(node, text, &token)) {
							Com_Printf("MN_ParseMenuBody: node with bad body ignored (menu \"%s\")\n", menu->name);
							numNodes--;
							continue;
						}

						token = COM_EParse(text, errhead, menu->name);
						if (!*text)
							return qfalse;
					}

					/* set standard color */
					if (!node->color[3])
						node->color[0] = node->color[1] = node->color[2] = node->color[3] = 1.0f;

					found = qtrue;
					break;
				}
		} while (found);

		/* test for end or unknown token */
		if (*token == '}') {
			/* finished */
			return qtrue;
		} else {
			/* unknown token, print message and continue */
			Com_Printf("MN_ParseMenuBody: unknown token \"%s\" ignored (menu \"%s\")\n", token, menu->name);
		}

	} while (*text);

	return qfalse;
}

/**
 * @brief parses the models.ufo and all files where menu_models are defined
 */
void MN_ParseMenuModel(char *name, char **text)
{
	menuModel_t *menuModel;
	char *token;
	int i;
	value_t *v = NULL;
	char *errhead = "MN_ParseMenuModel: unexptected end of file (names ";

	/* search for menumodels with same name */
	for (i = 0; i < numMenuModels; i++)
		if (!Q_strncmp(menuModels[i].id, name, MAX_VAR)) {
			Com_Printf("MN_ParseMenuModel: menu_model \"%s\" with same name found, second ignored\n", name);
			return;
		}

	if (numMenuModels >= MAX_MENUMODELS) {
		Com_Printf("MN_ParseMenuModel: Max menu models reached\n", name);
		return;
	}

	/* initialize the menu */
	menuModel = &menuModels[numMenuModels];
	memset(menuModel, 0, sizeof(menuModel_t));

	Q_strncpyz(menuModel->id, name, MAX_VAR);
	Com_DPrintf("Found menu model %s (%i)\n", menuModel->id, numMenuModels);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseMenuModel: menu \"%s\" without body ignored\n", menuModel->id);
		return;
	}

	numMenuModels++;

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (v = menuModelValues; v->string; v++)
			if (!Q_strncmp(token, v->string, sizeof(v->string))) {
				/* found a definition */
				if (!Q_strncmp(v->string, "need", 4)) {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
					menuModel->next = MN_GetMenuModel(token);
					if (!menuModel->next)
						Com_Printf("Could not find menumodel %s", token);
					Q_strncpyz(menuModel->need, token, MAX_QPATH);
				} else {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;

					Com_ParseValue(menuModel, token, v->type, v->ofs);
				}
				break;
			}

		if (!v->string)
			Com_Printf("MN_ParseMenuModel: unknown token \"%s\" ignored (menu_model %s)\n", token, name);

	} while (*text);
}

/**
 * @brief
 */
void MN_ParseMenu(char *name, char **text)
{
	menu_t *menu;
	menuNode_t *node;
	char *token;
	int i;

	/* search for menus with same name */
	for (i = 0; i < numMenus; i++)
		if (!Q_strncmp(name, menus[i].name, MAX_VAR))
			break;

	if (i < numMenus) {
		Com_Printf("MN_ParseMenus: menu \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the menu */
	menu = &menus[numMenus++];
	memset(menu, 0, sizeof(menu_t));

	Q_strncpyz(menu->name, name, MAX_VAR);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseMenus: menu \"%s\" without body ignored\n", menu->name);
		numMenus--;
		return;
	}

	/* parse it's body */
	if (!MN_ParseMenuBody(menu, text)) {
		Com_Printf("MN_ParseMenus: menu \"%s\" with bad body ignored\n", menu->name);
		numMenus--;
		return;
	}

	for (i = 0; i < numMenus; i++)
		for (node = menus[i].firstNode; node; node = node->next)
			if (node->num >= MAX_MENUTEXTS)
				Sys_Error("Error in menu %s - max menu num exeeded (%i)", menus[i].name, MAX_MENUTEXTS);

	Com_DPrintf("Nodes: %4i Menu data: %i\n", numNodes, curadata - adata);
}

/**
 * @brief
 */
void CL_ListMaps_f(void)
{
	int i;

	if (!mapsInstalledInit)
		FS_GetMaps();

	for (i = 0; i < anzInstalledMaps - 1; i++)
		Com_Printf("%s\n", maps[i]);
}

/**
 * @brief
 */
void MN_MapInfo(void)
{
	char normalizedName[MAX_VAR];
	int length = 0;
	qboolean normalized = qfalse, found = qfalse;

	/* maybe mn_next/prev_map are called before getmaps??? */
	if (!mapsInstalledInit)
		FS_GetMaps();

	Cvar_Set("mn_mappic", "maps/shots/na.jpg");
	Cvar_Set("mn_mappic2", "maps/shots/na.jpg");
	Cvar_Set("mn_mappic3", "maps/shots/na.jpg");

	/* remove the day and night char */
	Q_strncpyz(normalizedName, maps[mapInstalledIndex], MAX_VAR);
	length = strlen(normalizedName);
	if (normalizedName[length - 1] == 'n' || normalizedName[length - 1] == 'd') {
		normalizedName[length - 1] = '\0';
		normalized = qtrue;
	}

	Cvar_ForceSet("mapname", maps[mapInstalledIndex]);
	if (FS_CheckFile(va("pics/maps/shots/%s.jpg", maps[mapInstalledIndex])) != -1) {
		Cvar_Set("mn_mappic", va("maps/shots/%s.jpg", maps[mapInstalledIndex]));
		found = qtrue;
	}

	if (FS_CheckFile(va("pics/maps/shots/%s_2.jpg", maps[mapInstalledIndex])) != -1) {
		Cvar_Set("mn_mappic2", va("maps/shots/%s_2.jpg", maps[mapInstalledIndex]));
		found = qtrue;
	}

	if (FS_CheckFile(va("pics/maps/shots/%s_3.jpg", maps[mapInstalledIndex])) != -1) {
		Cvar_Set("mn_mappic3", va("maps/shots/%s_3.jpg", maps[mapInstalledIndex]));
		found = qtrue;
	}

	/* then check the normalized names */
	if (!found && normalized) {
		if (FS_CheckFile(va("pics/maps/shots/%s.jpg", normalizedName)) != -1) {
			Cvar_Set("mn_mappic", va("maps/shots/%s.jpg", normalizedName));
			found = qtrue;
		}

		if (FS_CheckFile(va("pics/maps/shots/%s_2.jpg", normalizedName)) != -1) {
			Cvar_Set("mn_mappic2", va("maps/shots/%s_2.jpg", normalizedName));
			found = qtrue;
		}

		if (FS_CheckFile(va("pics/maps/shots/%s_3.jpg", normalizedName)) != -1) {
			Cvar_Set("mn_mappic3", va("maps/shots/%s_3.jpg", normalizedName));
			found = qtrue;
		}
	}
}

/**
 * @brief
 */
void MN_GetMaps_f(void)
{
	if (!mapsInstalledInit)
		FS_GetMaps();

	MN_MapInfo();
}

/**
 * @brief
 */
void MN_NextMap(void)
{
	if (mapInstalledIndex < anzInstalledMaps - 1)
		mapInstalledIndex++;
	else
		mapInstalledIndex = 0;
	MN_MapInfo();
}

/**
 * @brief
 */
void MN_PrevMap(void)
{
	if (mapInstalledIndex > 0)
		mapInstalledIndex--;
	else
		mapInstalledIndex = anzInstalledMaps - 1;
	MN_MapInfo();
}

/**
 * @brief Script command to show all messages on the stack
 */
void CL_ShowMessagesOnStack(void)
{
	message_t *m = messageStack;

	while (m) {
		Com_Printf("%s: %s\n", m->title, m->text);
		m = m->next;
	}
}

/**
 * @brief Adds a new message to message stack
 * @note These are the messages that are displayed at geoscape
 * @param[in] title
 * @param[in] text
 * @param[in] popup
 * @param[in] type
 * @param[in] pedia
 * @return message_t pointer
 */
message_t *MN_AddNewMessage(const char *title, const char *text, qboolean popup, messagetype_t type, technology_t * pedia)
{
	message_t *mess;

	/* allocate memory for new message */
	mess = (message_t *) malloc(sizeof(message_t));

	/* push the new message at the beginning of the stack */
	mess->next = messageStack;
	messageStack = mess;

	mess->type = type;
	mess->pedia = pedia;		/* pointer to ufopedia */

	CL_DateConvert(&ccs.date, &mess->d, &mess->m);
	mess->y = ccs.date.day / 365;
	mess->h = ccs.date.sec / 3600;
	mess->min = (ccs.date.sec % 3600) / 60 / 10;
	mess->s = (ccs.date.sec % 3600) / 60 % 10;

	Q_strncpyz(mess->title, title, MAX_VAR);
	Q_strncpyz(mess->text, text, sizeof(mess->text));

	/* they need to be translated already */
	if (popup)
		MN_Popup(title, text);

	switch (type) {
	case MSG_DEBUG:
	case MSG_STANDARD:
	case MSG_TRANSFERFINISHED:
	case MSG_PROMOTION:
	case MSG_DEATH:
		break;
	case MSG_RESEARCH:
	case MSG_CONSTRUCTION:
	case MSG_UFOSPOTTED:
	case MSG_TERRORSITE:
	case MSG_BASEATTACK:
	case MSG_PRODUCTION:
		/*TODO: S_StartLocalSound(); */
		break;
	default:
		Com_Printf("Warning: Unhandled message type: %i\n", type);
	}

	return mess;
}

/**
 * @brief Prepends timestamps to the message text displayed in the geoscape
 * @param[in] text Buffer to hold the final result
 * @param[in] max_text Total size of the text buffer
 * @param[in] message The message to convert into text
 */
void MN_TimestampedText(char *text, size_t max_text, message_t *message)
{
	Q_strncpyz(text, va(TIMESTAMP_FORMAT, message->d, CL_DateGetMonthName(message->m), message->y, message->h, message->min, message->s), max_text);
	Q_strcat(text, message->text, max_text);
}

/**
 * @brief
 * FIXME: This needs to be called at shutdown
 */
void MN_ShutdownMessageSystem(void)
{
	message_t *m = messageStack;
	message_t *d;

	while (m) {
		d = m;
		m = m->next;
		free(d);
	}
}

/**
 * @brief
 */
void MN_RemoveMessage(char *title)
{
	message_t *m = messageStack;
	message_t *prev = NULL;

	while (m) {
		if (!Q_strncmp(m->title, title, MAX_VAR)) {
			if (prev)
				prev->next = m->next;
			free(m);
			return;
		}
		prev = m;
		m = m->next;
	}
	Com_Printf("Could not remove message from stack - %s was not found\n", title);
}

/**
 * @brief Inits the message system with no messages
 */
void CL_InitMessageSystem(void)
{
	/* no messages on stack */
	messageStack = NULL;

	/* we will use the messages on the stack in this textfield */
	/* so be sure that this is null - don't change this */
	menuText[TEXT_MESSAGESYSTEM] = NULL;
}

/* ==================== USE_SDL_TTF stuff ===================== */

#define MAX_FONTS 16
int numFonts;
font_t fonts[MAX_FONTS];

font_t *fontBig;
font_t *fontSmall;

static value_t fontValues[] = {
	{"font", V_TRANSLATION2_STRING, offsetof(font_t, path)},
	{"size", V_INT, offsetof(font_t, size)},
	{"style", V_STRING, offsetof(font_t, style)},

	{NULL, V_NULL, 0},
};

/**
 * @brief
 */
void CL_GetFontData(char *name, int *size, char *path)
{
	int i;

	/* search for font with this name */
	for (i = 0; i < numFonts; i++)
		if (!Q_strncmp(fonts[i].name, name, MAX_VAR)) {
			*size = fonts[i].size;
			path = fonts[i].path;
			return;
		}
	Com_Printf("Font: %s not found\n", name);
}

/**
 * @brief
 */
void CL_ParseFont(char *name, char **text)
{
	font_t *font;
	char *errhead = "CL_ParseFont: unexpected end of file (font";
	char *token;
	int i;
	value_t *v = NULL;

	/* search for font with same name */
	for (i = 0; i < numFonts; i++)
		if (!Q_strncmp(fonts[i].name, name, MAX_VAR)) {
			Com_Printf("CL_ParseFont: font \"%s\" with same name found, second ignored\n", name);
			return;
		}

	if (numFonts >= MAX_FONTS) {
		Com_Printf("CL_ParseFont: Max fonts reached\n", name);
		return;
	}

	/* initialize the menu */
	font = &fonts[numFonts];
	memset(font, 0, sizeof(font_t));

	Q_strncpyz(font->name, name, MAX_VAR);

	if (!Q_strncmp(font->name, "f_small", MAX_VAR))
		fontSmall = font;
	else if (!Q_strncmp(font->name, "f_big", MAX_VAR))
		fontBig = font;

	Com_DPrintf("...found font %s (%i)\n", font->name, numFonts);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseFont: font \"%s\" without body ignored\n", name);
		return;
	}

	numFonts++;

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (v = fontValues; v->string; v++)
			if (!Q_strncmp(token, v->string, sizeof(v->string))) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_ParseValue(font, token, v->type, v->ofs);
				break;
			}

		if (!v->string)
			Com_Printf("CL_ParseFont: unknown token \"%s\" ignored (font %s)\n", token, name);

	} while (*text);

	if (FS_CheckFile(font->path) == -1)
		Sys_Error("...font file %s does not exist (font %s)\n", font->path, font->name);

	re.FontRegister(font->name, font->size, _(font->path), font->style);
}

/**
 * @brief after a vid restart we have to reinit the fonts
 */
void CL_InitFonts(void)
{
	int i;

	Com_Printf("...registering %i fonts\n", numFonts);
	for (i = 0; i < numFonts; i++)
		re.FontRegister(fonts[i].name, fonts[i].size, fonts[i].path, fonts[i].style);
}

/* ===================== USE_SDL_TTF stuff end ====================== */

static value_t tutValues[] = {
	{"name", V_TRANSLATION_STRING, offsetof(tutorial_t, name)}
	,
	{"sequence", V_STRING, offsetof(tutorial_t, sequence)}
	,
	{NULL, 0, 0}
};

tutorial_t tutorials[MAX_TUTORIALS];

/**
 * @brief
 */
void MN_ParseTutorials(char *title, char **text)
{
	tutorial_t *t = NULL;
	char *errhead = "MN_ParseTutorials: unexptected end of file (tutorial ";
	char *token;
	value_t *v;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseTutorials: tutorial \"%s\" without body ignored\n", title);
		return;
	}

	/* parse tutorials */
	if (numTutorials >= MAX_TUTORIALS) {
		Com_Printf("Too many tutorials, '%s' ignored.\n", title);
		numTutorials = MAX_TUTORIALS;
		return;
	}

	t = &tutorials[numTutorials++];
	memset(t, 0, sizeof(tutorial_t));
	do {
		/* get the name type */
		token = COM_EParse(text, errhead, title);
		if (!*text)
			break;
		if (*token == '}')
			break;
		for (v = tutValues; v->string; v++)
			if (!Q_strncmp(token, v->string, sizeof(v->string))) {
				/* found a definition */
				token = COM_EParse(text, errhead, title);
				if (!*text)
					return;
				Com_ParseValue(t, token, v->type, v->ofs);
				break;
			}
		if (!v->string)
			Com_Printf("MN_ParseTutorials: unknown token \"%s\" ignored (tutorial %s)\n", token, title);
	} while (*text);
}
