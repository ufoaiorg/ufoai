/**
 * @file m_parse.c
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
#include "m_parse.h"
#include "m_main.h"
#include "m_actions.h"
#include "m_dragndrop.h"
#include "m_node_window.h"
#include "m_node_selectbox.h"

#define	V_ACTION	0xF000 /**< Identify an action type into the value_t structure */

/**
 * @brief valid node event ids
 * @todo merge it with the properties
 */
static const value_t eventProperties[] = {
	{"click", V_ACTION, offsetof(menuNode_t, click), MEMBER_SIZEOF(menuNode_t, click)},
	{"rclick", V_ACTION, offsetof(menuNode_t, rclick), MEMBER_SIZEOF(menuNode_t, rclick)},
	{"mclick", V_ACTION, offsetof(menuNode_t, mclick), MEMBER_SIZEOF(menuNode_t, mclick)},
	{"wheel", V_ACTION, offsetof(menuNode_t, wheel), MEMBER_SIZEOF(menuNode_t, wheel)},
	{"in", V_ACTION, offsetof(menuNode_t, mouseIn), MEMBER_SIZEOF(menuNode_t, mouseIn)},
	{"out", V_ACTION, offsetof(menuNode_t, mouseOut), MEMBER_SIZEOF(menuNode_t, mouseOut)},
	{"whup", V_ACTION, offsetof(menuNode_t, wheelUp), MEMBER_SIZEOF(menuNode_t, wheelUp)},
	{"whdown", V_ACTION, offsetof(menuNode_t, wheelDown), MEMBER_SIZEOF(menuNode_t, wheelDown)},
	{NULL, V_NULL, 0, 0}
};

/* =========================================================== */

/** @brief valid properties for a menu node */
static const value_t nps[] = {
	{"invis", V_BOOL, offsetof(menuNode_t, invis), MEMBER_SIZEOF(menuNode_t, invis)},
	{"mousefx", V_BOOL, offsetof(menuNode_t, mousefx), MEMBER_SIZEOF(menuNode_t, mousefx)},
	{"blend", V_BOOL, offsetof(menuNode_t, blend), MEMBER_SIZEOF(menuNode_t, blend)},
	{"disabled", V_BOOL, offsetof(menuNode_t, disabled), MEMBER_SIZEOF(menuNode_t, disabled)},
	{"texh", V_POS, offsetof(menuNode_t, texh), MEMBER_SIZEOF(menuNode_t, texh)},
	{"texl", V_POS, offsetof(menuNode_t, texl), MEMBER_SIZEOF(menuNode_t, texl)},
	{"border", V_INT, offsetof(menuNode_t, border), MEMBER_SIZEOF(menuNode_t, border)},
	{"padding", V_INT, offsetof(menuNode_t, padding), MEMBER_SIZEOF(menuNode_t, padding)},
	{"pos", V_POS, offsetof(menuNode_t, pos), MEMBER_SIZEOF(menuNode_t, pos)},
	{"size", V_POS, offsetof(menuNode_t, size), MEMBER_SIZEOF(menuNode_t, size)},
	{"format", V_POS, offsetof(menuNode_t, texh), MEMBER_SIZEOF(menuNode_t, texh)},
	{"scale", V_VECTOR, offsetof(menuNode_t, scale), MEMBER_SIZEOF(menuNode_t, scale)},
	{"num", V_MENUTEXTID, offsetof(menuNode_t, num), MEMBER_SIZEOF(menuNode_t, num)},
	{"baseid", V_BASEID, offsetof(menuNode_t, baseid), MEMBER_SIZEOF(menuNode_t, baseid)},
	{"height", V_INT, offsetof(menuNode_t, height), MEMBER_SIZEOF(menuNode_t, height)},
	{"text_scroll", V_INT, offsetof(menuNode_t, textScroll), MEMBER_SIZEOF(menuNode_t, textScroll)},
	{"longlines", V_LONGLINES, offsetof(menuNode_t, longlines), MEMBER_SIZEOF(menuNode_t, longlines)},
	{"timeout", V_INT, offsetof(menuNode_t, timeOut), MEMBER_SIZEOF(menuNode_t, timeOut)},
	{"timeout_once", V_BOOL, offsetof(menuNode_t, timeOutOnce), MEMBER_SIZEOF(menuNode_t, timeOutOnce)},
	{"bgcolor", V_COLOR, offsetof(menuNode_t, bgcolor), MEMBER_SIZEOF(menuNode_t, bgcolor)},
	{"bordercolor", V_COLOR, offsetof(menuNode_t, bordercolor), MEMBER_SIZEOF(menuNode_t, bordercolor)},
	{"key", V_STRING, offsetof(menuNode_t, key), 0},

	{"tooltip", V_LONGSTRING|V_MENU_COPY, offsetof(menuNode_t, tooltip), 0},	/* translated in MN_Tooltip */
	{"image", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataImageOrModel), 0},
	{"roq", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataImageOrModel), 0},
	{"md2", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataImageOrModel), 0},	/** @todo Rename into model */
	{"cvar", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataModelSkinOrCVar), 0},	/* for selectbox */
	{"skin", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataModelSkinOrCVar), 0},
	{"string", V_LONGSTRING|V_MENU_COPY, offsetof(menuNode_t, text), 0},	/* no gettext here - this can be a cvar, too */
	{"font", V_STRING|V_MENU_COPY, offsetof(menuNode_t, font), 0},
#if 0 /* never use */
	{"weapon", V_STRING|V_MENU_COPY, offsetof(menuNode_t, dataImageOrModel), 0},
#endif

	/* specific for model
	 * @todo move it into the node behaviour
	 */
	{"anim", V_STRING|V_MENU_COPY, offsetof(menuNode_t, u.model.animation), MEMBER_SIZEOF(menuNode_t, u.model.animation)},
	{"angles", V_VECTOR, offsetof(menuNode_t, u.model.angles), MEMBER_SIZEOF(menuNode_t, u.model.angles)},
	{"center", V_VECTOR, offsetof(menuNode_t, u.model.center), MEMBER_SIZEOF(menuNode_t, u.model.center)},
	{"origin", V_VECTOR, offsetof(menuNode_t, u.model.origin), MEMBER_SIZEOF(menuNode_t, u.model.origin)},
	{"tag", V_STRING|V_MENU_COPY, offsetof(menuNode_t, u.model.tag), 0},

	{"color", V_COLOR, offsetof(menuNode_t, color), MEMBER_SIZEOF(menuNode_t, color)},
	{"selectcolor", V_COLOR, offsetof(menuNode_t, selectedColor), MEMBER_SIZEOF(menuNode_t, selectedColor)},
	{"align", V_ALIGN, offsetof(menuNode_t, align), MEMBER_SIZEOF(menuNode_t, align)},
	{"if", V_IF, offsetof(menuNode_t, depends), 0},
	{"repeat", V_BOOL, offsetof(menuNode_t, repeat), MEMBER_SIZEOF(menuNode_t, repeat)},
	{"clickdelay", V_INT, offsetof(menuNode_t, clickDelay), MEMBER_SIZEOF(menuNode_t, clickDelay)},
	{"scrollbar", V_BOOL, offsetof(menuNode_t, scrollbar), MEMBER_SIZEOF(menuNode_t, scrollbar)},
	{"scrollbarleft", V_BOOL, offsetof(menuNode_t, scrollbarLeft), MEMBER_SIZEOF(menuNode_t, scrollbarLeft)},
	{"point_width", V_FLOAT, offsetof(menuNode_t, pointWidth), MEMBER_SIZEOF(menuNode_t, pointWidth)},
	{"gap_width", V_INT, offsetof(menuNode_t, gapWidth), MEMBER_SIZEOF(menuNode_t, gapWidth)},

	{NULL, V_NULL, 0, 0},
};

/** @brief valid properties for a select box */
static const value_t selectBoxValues[] = {
	{"label", V_TRANSLATION_MANUAL_STRING, offsetof(selectBoxOptions_t, label), sizeof(char) * SELECTBOX_MAX_VALUE_LENGTH},
	{"action", V_STRING, offsetof(selectBoxOptions_t, action), 0},
	{"value", V_STRING, offsetof(selectBoxOptions_t, value), 0},

	{NULL, V_NULL, 0, 0},
};

/** @brief valid properties for a menu model definition */
static const value_t menuModelValues[] = {
	{"model", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, model), 0},
	{"need", V_NULL, 0, 0},
	{"menutransform", V_NULL, 0, 0},
	{"anim", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, anim), 0},
	{"skin", V_INT, offsetof(menuModel_t, skin), sizeof(int)},
	{"origin", V_VECTOR, offsetof(menuModel_t, origin), sizeof(vec3_t)},
	{"center", V_VECTOR, offsetof(menuModel_t, center), sizeof(vec3_t)},
	{"scale", V_VECTOR, offsetof(menuModel_t, scale), sizeof(vec3_t)},
	{"angles", V_VECTOR, offsetof(menuModel_t, angles), sizeof(vec3_t)},
	{"color", V_COLOR, offsetof(menuModel_t, color), sizeof(vec4_t)},
	{"tag", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, tag), 0},
	{"parent", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, parent), 0},

	{NULL, V_NULL, 0, 0},
};

/* =========================================================== */

/** @brief valid node event actions */
static const char *ea_strings[EA_NUM_EVENTACTION] = {
	"",
	"cmd",

	"",
	"*",
	"&"
};

/**
 * @brief Find a value_t by name into a array of value_t
 * @param[in] propertyList Array of value_t, with null termination
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else NULL
 */
static const value_t* findPropertyByName (const value_t* propertyList, const char* name)
{
	const value_t* current = propertyList;
	while (current->string != NULL) {
		if (!Q_strcasecmp(name, current->string))
			return current;
		current++;
	}
	return NULL;
}

/**
 * @brief
 * @sa MN_ExecuteActions
 */
static qboolean MN_ParseAction (menuNode_t *menuNode, menuAction_t *action, const char **text, const const char **token)
{
	const char *errhead = "MN_ParseAction: unexpected end of file (in event)";
	menuAction_t *lastAction;
	menuNode_t *node;
	qboolean found;
	const value_t *val;
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
				if (!Q_strcasecmp(*token, ea_strings[i])) {
/*					Com_Printf("   %s", *token); */

					/* add the action */
					if (lastAction) {
						if (mn.numActions >= MAX_MENUACTIONS)
							Sys_Error("MN_ParseAction: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
						action = &mn.menuActions[mn.numActions++];
						memset(action, 0, sizeof(*action));
						lastAction->next = action;
					}
					action->type = i;

					if (ea_values[i] != V_NULL) {
						/* get parameter values */
						*token = COM_EParse(text, errhead, NULL);
						if (!*text)
							return qfalse;

/*						Com_Printf(" %s", *token); */

						/* get the value */
						action->data = mn.curadata;
						mn.curadata += Com_ParseValue(mn.curadata, *token, ea_values[i], 0, 0);
					}

/*					Com_Printf("\n"); */

					/* get next token */
					*token = COM_EParse(text, errhead, NULL);
					if (!*text)
						return qfalse;

					lastAction = action;
					found = qtrue;
					break;
				}

			/* node property setting */
			switch (**token) {
			case '*':
/*				Com_Printf("   %s", *token); */

				/* add the action */
				if (lastAction) {
					if (mn.numActions >= MAX_MENUACTIONS)
						Sys_Error("MN_ParseAction: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
					action = &mn.menuActions[mn.numActions++];
					memset(action, 0, sizeof(*action));
					lastAction->next = action;
				}
				action->type = EA_NODE;

				/* get the node name */
				action->data = mn.curadata;

				strcpy((char *) mn.curadata, &(*token)[1]);
				mn.curadata += ALIGN(strlen((char *) mn.curadata) + 1);

				/* get the node property */
				*token = COM_EParse(text, errhead, NULL);
				if (!*text)
					return qfalse;

/*				Com_Printf(" %s", *token); */

				for (val = nps, i = 0; val->type; val++, i++)
					if (!Q_strcasecmp(*token, val->string))
						break;

				action->scriptValues = val;

				if (!val->type) {
					Com_Printf("MN_ParseAction: token \"%s\" isn't a node property (in event)\n", *token);
					mn.curadata = action->data;
					if (lastAction) {
						lastAction->next = NULL;
						mn.numActions--;
					}
					break;
				}

				/* get the value */
				*token = COM_EParse(text, errhead, NULL);
				if (!*text)
					return qfalse;

/*				Com_Printf(" %s\n", *token); */

				mn.curadata += Com_ParseValue(mn.curadata, *token, val->type & V_BASETYPEMASK, 0, val->size);

				/* get next token */
				*token = COM_EParse(text, errhead, NULL);
				if (!*text)
					return qfalse;

				lastAction = action;
				found = qtrue;
				break;
			case '&':
				action->type = EA_VAR;
				break;
			}

			/* function calls */
			for (node = mn.menus[mn.numMenus - 1].firstNode; node; node = node->next)
				if ((node->type == MN_FUNC || node->type == MN_CONFUNC || node->type == MN_CVARFUNC)
					&& !Q_strncmp(node->name, *token, sizeof(node->name))) {
/*					Com_Printf("   %s\n", node->name); */

					/* add the action */
					if (lastAction) {
						if (mn.numActions >= MAX_MENUACTIONS)
							Sys_Error("MN_ParseAction: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
						action = &mn.menuActions[mn.numActions++];
						memset(action, 0, sizeof(*action));
						lastAction->next = action;
					}
					action->type = EA_CALL;

					action->data = mn.curadata;
					*(menuAction_t ***) mn.curadata = &node->click;
					mn.curadata += ALIGN(sizeof(menuAction_t *));

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
			if (!Q_strcmp(*token, "timeout")) {
				/* get new token */
				*token = COM_EParse(text, errhead, NULL);
				if (!*token || **token == '}') {
					Com_Printf("MN_ParseAction: timeout with no value (in event) (node: %s)\n", menuNode->name);
					return qfalse;
				}
				menuNode->timeOut = atoi(*token);
			} else {
				/* unknown token, print message and continue */
				Com_Printf("MN_ParseAction: unknown token \"%s\" ignored (in event) (node: %s, menu %s)\n", *token, menuNode->name, menuNode->menu->name);
			}
		}
	} while (*text);

	return qfalse;
}

static qboolean MN_ParseOption (menuNode_t * node, const char **text, const char **token, const char *errhead)
{
	const value_t *val;

	/* get parameters */
	*token = COM_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;
	/** @todo mem corruption in case of too many select box options */
	Q_strncpyz(mn.menuSelectBoxes[mn.numSelectBoxes].id, *token, sizeof(mn.menuSelectBoxes[mn.numSelectBoxes].id));
	Com_DPrintf(DEBUG_CLIENT, "...found selectbox: '%s'\n", *token);

	*token = COM_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;
	if (**token != '{') {
		Com_Printf("MN_ParseNodeBody: node with bad option definition ignored (node \"%s\", menu %s)\n", node->name, node->menu->name);
		return qtrue;
	}

	if (mn.numSelectBoxes >= MAX_SELECT_BOX_OPTIONS) {
		FS_SkipBlock(text);
		Com_Printf("MN_ParseNodeBody: Too many option entries for node %s (menu %s)\n", node->name, node->menu->name);
		return qfalse;
	}

	/**
	 * the options data can be defined like this
	 * @code
	 * option string {
	 *  value "value"
	 *  action "command string"
	 * }
	 * @endcode
	 * The strings will appear in the drop down list of the select box
	 * if you select the 'string', the 'cvarname' will be set to 'value'
	 * and if action is defined (which is a console/script command string)
	 * this one is executed on each selection
	 */

	do {
		*token = COM_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
		if (**token == '}')
			break;

		val = findPropertyByName(selectBoxValues, *token);
		if (val) {
			/* get parameter values */
			*token = COM_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			switch (val->type) {
			case V_TRANSLATION_MANUAL_STRING:
				if (val->size) {
					/* selectbox values are static arrays */
					char *target = ((char*)&mn.menuSelectBoxes[mn.numSelectBoxes] + val->ofs);
					const char *translateableToken = *token;
					if (*translateableToken == '_')
						translateableToken++;
					Q_strncpyz(target, translateableToken, val->size);
					break;
				}
				/* otherwise fall through */
			default:
				Com_ParseValue(&mn.menuSelectBoxes[mn.numSelectBoxes], *token, val->type, val->ofs, val->size);
				break;
			}
		}
		if (!val || !val->string)
			Com_Printf("MN_ParseNodeBody: unknown options value: '%s' - ignore it\n", *token);
	} while (**token != '}');
	MN_AddSelectboxOption(node);
	return qtrue;
}

static qboolean MN_ParseExcludeRect (menuNode_t * node, const char **text, const char **token, const char *errhead)
{
	/* get parameters */
	*token = COM_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;
	if (**token != '{') {
		Com_Printf("MN_ParseNodeBody: node with bad excluderect ignored (node \"%s\", menu %s)\n", node->name, node->menu->name);
		return qtrue;
	}

	do {
		*token = COM_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
		if (!Q_strcmp(*token, "pos")) {
			*token = COM_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Com_ParseValue(&node->exclude[node->excludeNum], *token, V_POS, offsetof(excludeRect_t, pos), sizeof(vec2_t));
		} else if (!Q_strcmp(*token, "size")) {
			*token = COM_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Com_ParseValue(&node->exclude[node->excludeNum], *token, V_POS, offsetof(excludeRect_t, size), sizeof(vec2_t));
		}
	} while (**token != '}');
	if (node->excludeNum < MAX_EXLUDERECTS - 1)
		node->excludeNum++;
	else
		Com_Printf("MN_ParseNodeBody: exluderect limit exceeded (max: %i)\n", MAX_EXLUDERECTS);
	return qtrue;
}

static qboolean MN_ParseProperty (menuNode_t * node, const value_t *val, const char **text, const char **token, const char *errhead)
{
	node->scriptValues = val;

	/* Com_Printf("  %s", *token); */

	if (val->type != V_NULL) {
		/* get parameter values */
		*token = COM_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;

		/* Com_Printf(" %s", *token); */

		/* get the value */
		if (!(val->type & V_MENU_COPY)) {
			if (Com_ParseValue(node, *token, val->type, val->ofs, val->size) == -1)
				Com_Printf("MN_ParseNodeBody: Wrong size for value %s\n", val->string);
		} else {
			/* a reference to data is handled like this */
			/* Com_Printf("%i %s\n", val->ofs, *token); */
			*(byte **) ((byte *) node + val->ofs) = mn.curadata;
			/* we read it, we no more need extra flag */
			/* *((int*)&val->type) = val->type & V_BASETYPEMASK; */
			/* references are parsed as string */
			if (**token == '*') {
				/* sanity check */
				if (strlen(*token) > MAX_VAR - 1)
					Com_Printf("MN_ParseNodeBody: Value '%s' is too long (key %s)\n", *token, val->string);
				mn.curadata += Com_ParseValue(mn.curadata, *token, V_STRING, 0, 0);
			} else {
				/* sanity check */
				if (val->type == V_STRING && strlen(*token) > MAX_VAR - 1)
					Com_Printf("MN_ParseNodeBody: Value '%s' is too long (key %s)\n", *token, val->string);
				mn.curadata += Com_ParseValue(mn.curadata, *token, val->type & V_BASETYPEMASK, 0, val->size);
			}
		}
	}
	/* Com_Printf("\n"); */
	return qtrue;
}


static qboolean MN_ParseEventProperty (menuNode_t * node, const value_t *event, const char **text, const char **token, const char *errhead)
{
	menuAction_t **action;

	/* Com_Printf("  %s\n", *token); */

	/* add new actions to end of list */
	action = (menuAction_t **) ((byte *) node + event->ofs);
	for (; *action; action = &(*action)->next) {}

	if (mn.numActions >= MAX_MENUACTIONS)
		Sys_Error("MN_ParseNodeBody: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
	*action = &mn.menuActions[mn.numActions++];
	memset(*action, 0, sizeof(**action));

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
	return qtrue;
}

/**
 * @sa MN_ParseMenuBody
 */
static qboolean MN_ParseNodeBody (menuNode_t * node, const char **text, const char **token)
{
	const char *errhead = "MN_ParseNodeBody: unexpected end of file (node";
	qboolean nextTokenAlreadyRead = qfalse;

	/* functions are a special case */
	if (node->type == MN_CONFUNC || node->type == MN_FUNC || node->type == MN_CVARFUNC) {
		menuAction_t **action;

		/* add new actions to end of list */
		action = &node->click;
		for (; *action; action = &(*action)->next) {}

		if (mn.numActions >= MAX_MENUACTIONS)
			Sys_Error("MN_ParseNodeBody: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
		*action = &mn.menuActions[mn.numActions++];
		memset(*action, 0, sizeof(**action));

		if (node->type == MN_CONFUNC) {
			/* don't add a callback twice */
			if (!Cmd_Exists(node->name))
				Cmd_AddCommand(node->name, MN_Command_f, "Confunc callback");
			else
				Com_DPrintf(DEBUG_CLIENT, "MN_ParseNodeBody: skip confunc '%s' - already added (menu %s)\n", node->name, node->menu->name);
		}

		return MN_ParseAction(node, *action, text, token);
	}

	do {
		const value_t *val;
		const value_t *event;
		qboolean result;

		/* get new token */
		if (!nextTokenAlreadyRead) {
			*token = COM_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
		} else {
			nextTokenAlreadyRead = qfalse;
		}

		/* is finished */
		if (**token == '}') {
			return qtrue;
		}

		/* test token vs know properties */
		val = findPropertyByName(nps, *token);
		if (!val && nodeBehaviourList[node->type].properties)
			val = findPropertyByName(nodeBehaviourList[node->type].properties, *token);
		if (!val)
			event = findPropertyByName(eventProperties, *token);

		/* is it a property */
		if (val) {
			result = MN_ParseProperty(node, val, text, token, errhead);
			if (!result)
				return qfalse;

		/* is it an event property */
		} else if (event) {
			result = MN_ParseEventProperty(node, event, text, token, errhead);
			if (!result)
				return qfalse;
			nextTokenAlreadyRead = qtrue;

		} else if (!Q_strncmp(*token, "excluderect", 12)) {
			result = MN_ParseExcludeRect(node, text, token, errhead);
			if (!result)
				return qfalse;

		/* for MN_SELECTBOX */
		} else if (!Q_strncmp(*token, "option", 7)) {
			result = MN_ParseOption(node, text, token, errhead);
			if (!result)
				return qfalse;

		} else {
			/* unknown token, print message and continue */
			Com_Printf("MN_ParseNodeBody: unknown token \"%s\" ignored (node \"%s\", menu %s)\n", *token, node->name, node->menu->name);
		}
	} while (*text);

	return qfalse;
}

/**
 * @sa MN_ParseNodeBody
 */
static qboolean MN_ParseMenuBody (menu_t * menu, const char **text)
{
	const char *errhead = "MN_ParseMenuBody: unexpected end of file (menu";
	const char *token;
	qboolean found;
	menuNode_t *node, *lastNode, *iNode;
	int i;

	lastNode = NULL;
	Vector2Set(menu->pos, 0, 0);

	/* if inheriting another menu, link in the super menu's nodes */
	for (node = menu->firstNode; node; node = node->next) {
		if (mn.numNodes >= MAX_MENUNODES)
			Sys_Error("MAX_MENUNODES exceeded\n");
		iNode = MN_AllocNode(node->type);
		*iNode = *node;
		iNode->menu = menu;
		/* link it in */
		if (lastNode)
			lastNode->next = iNode;
		else
			menu->firstNode = iNode;
		lastNode = iNode;
	}

	lastNode = NULL;

	do {
		/* get new token */
		token = COM_EParse(text, errhead, menu->name);
		if (!*text)
			return qfalse;

		/* get node type */
		do {
			/* parse special menu values */
			if (token[0] == '{') {
				do {
					const value_t* property;
					found = qfalse;
					/* get new token */
					token = COM_EParse(text, errhead, menu->name);
					if (!*text)
						return qfalse;
					if (token[0] == '}')
						break;

					property = findPropertyByName(menuBehaviour.properties, token);
					if (property) {
						/* get new token */
						token = COM_EParse(text, errhead, menu->name);
						if (!*text)
							return qfalse;
						Com_ParseValue(menu, token, property->type, property->ofs, property->size);
						found = qtrue;
					} else {
						Com_Printf("Invalid special menu value '%s'\n", token);
					}
				} while (found);

				/* get new token */
				token = COM_EParse(text, errhead, menu->name);
				if (!*text)
					return qfalse;
			}
			found = qfalse;
			for (i = 0; i < MN_NUM_NODETYPE; i++) {
				if (nodeBehaviourList[i].name == NULL)
					continue;
				if (!Q_strcmp(token, nodeBehaviourList[i].name)) {
					/* found node */
					/* get name */
					token = COM_EParse(text, errhead, menu->name);
					if (!*text)
						return qfalse;

					/* test if node already exists */
					for (node = menu->firstNode; node; node = node->next) {
						if (!Q_strncmp(token, node->name, sizeof(node->name))) {
							if (node->type != i)
								Com_Printf("MN_ParseMenuBody: node prototype type change (menu \"%s\")\n", menu->name);
							Com_DPrintf(DEBUG_CLIENT, "... over-riding node %s in menu %s\n", node->name, menu->name);
							/* reset action list of node */
							node->click = NULL;
							break;
						}
						lastNode = node;
					}

					/* initialize node */
					if (!node) {
						if (mn.numNodes >= MAX_MENUNODES)
							Sys_Error("MAX_MENUNODES exceeded\n");
						node = MN_AllocNode(i);
						memset(node, 0, sizeof(*node));
						node->menu = menu;
						Q_strncpyz(node->name, token, sizeof(node->name));

						/* link it in */
						if (lastNode)
							lastNode->next = node;
						else
							menu->firstNode = node;

						lastNode = node;
					}

					node->type = i;
					if (nodeBehaviourList[node->type].loading)
						nodeBehaviourList[node->type].loading(node);

					/** node default values
					 * @todo move it into the respective "loading" function (where its need)
					 */
					node->padding = 3;

/*					Com_Printf(" %s %s\n", nodeBehaviourList[i].name, *token); */

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
						} else if (!Q_strncmp(node->name, "event", 5)) {
							if (!menu->eventNode) {
								menu->eventNode = node;
								menu->eventNode->timeOut = 2000; /* default value */
							} else
								Com_Printf("MN_ParseMenuBody: second event function ignored (menu \"%s\")\n", menu->name);
						} else if (!Q_strncmp(node->name, "leave", 5)) {
							if (!menu->leaveNode) {
								menu->leaveNode = node;
							} else
								Com_Printf("MN_ParseMenuBody: second leave function ignored (menu \"%s\")\n", menu->name);
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
						node->container = NULL;
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
							mn.numNodes--;
							continue;
						}

						token = COM_EParse(text, errhead, menu->name);
						if (!*text)
							return qfalse;
					}

					/** set standard color
					 * @todo move it on init behaviour only where it need
					 */
					if (!node->color[3])
						Vector4Set(node->color, 1, 1, 1, 1);

					/* init the node according to his behaviour */
					if (nodeBehaviourList[node->type].loaded) {
						nodeBehaviourList[node->type].loaded(node);
					}

					found = qtrue;
					break;
				}
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
 * @sa CL_ParseClientData
 */
void MN_ParseMenuModel (const char *name, const char **text)
{
	menuModel_t *menuModel;
	const char *token;
	int i;
	const value_t *v = NULL;
	const char *errhead = "MN_ParseMenuModel: unexpected end of file (names ";

	/* search for menumodels with same name */
	for (i = 0; i < mn.numMenuModels; i++)
		if (!Q_strcmp(mn.menuModels[i].id, name)) {
			Com_Printf("MN_ParseMenuModel: menu_model \"%s\" with same name found, second ignored\n", name);
			return;
		}

	if (mn.numMenuModels >= MAX_MENUMODELS) {
		Com_Printf("MN_ParseMenuModel: Max menu models reached\n");
		return;
	}

	/* initialize the menu */
	menuModel = &mn.menuModels[mn.numMenuModels];
	memset(menuModel, 0, sizeof(*menuModel));

	Vector4Set(menuModel->color, 1, 1, 1, 1);

	menuModel->id = Mem_PoolStrDup(name, cl_menuSysPool, CL_TAG_MENU);
	Com_DPrintf(DEBUG_CLIENT, "Found menu model %s (%i)\n", menuModel->id, mn.numMenuModels);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseMenuModel: menu \"%s\" without body ignored\n", menuModel->id);
		return;
	}

	mn.numMenuModels++;

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
					menuModel->need = Mem_PoolStrDup(token, cl_menuSysPool, CL_TAG_MENU);
				} else if (!Q_strncmp(v->string, "menutransform", 13)) {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
					if (*token != '{') {
						Com_Printf("Error in menumodel '%s' menutransform definition\n", name);
						break;
					}
					do {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}')
							break;
						menuModel->menuTransform[menuModel->menuTransformCnt].menuID = Mem_PoolStrDup(token, cl_menuSysPool, CL_TAG_MENU);

						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}') {
							Com_Printf("Error in menumodel '%s' menutransform definition - missing scale float value\n", name);
							break;
						}
						if (*token == '#') {
							menuModel->menuTransform[menuModel->menuTransformCnt].useScale = qfalse;
						} else {
							Com_ParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].scale, token, V_VECTOR, 0, sizeof(vec3_t));
							menuModel->menuTransform[menuModel->menuTransformCnt].useScale = qtrue;
						}

						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}') {
							Com_Printf("Error in menumodel '%s' menutransform definition - missing angles float value\n", name);
							break;
						}
						if (*token == '#') {
							menuModel->menuTransform[menuModel->menuTransformCnt].useAngles = qfalse;
						} else {
							Com_ParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].angles, token, V_VECTOR, 0, sizeof(vec3_t));
							menuModel->menuTransform[menuModel->menuTransformCnt].useAngles = qtrue;
						}

						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}') {
							Com_Printf("Error in menumodel '%s' menutransform definition - missing origin float value\n", name);
							break;
						}
						if (*token == '#') {
							menuModel->menuTransform[menuModel->menuTransformCnt].useOrigin = qfalse;
						} else {
							Com_ParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].origin, token, V_VECTOR, 0, sizeof(vec3_t));
							menuModel->menuTransform[menuModel->menuTransformCnt].useOrigin = qtrue;
						}

						menuModel->menuTransformCnt++;
					} while (*token != '}'); /* dummy condition - break is earlier here */
				} else {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;

					switch (v->type) {
					case V_CLIENT_HUNK_STRING:
						Mem_PoolStrDupTo(token, (char**) ((char*)menuModel + (int)v->ofs), cl_menuSysPool, CL_TAG_MENU);
						break;
					default:
						Com_ParseValue(menuModel, token, v->type, v->ofs, v->size);
					}
				}
				break;
			}

		if (!v->string)
			Com_Printf("MN_ParseMenuModel: unknown token \"%s\" ignored (menu_model %s)\n", token, name);

	} while (*text);
}

/**
 * @sa CL_ParseClientData
 */
void MN_ParseMenu (const char *name, const char **text)
{
	menu_t *menu;
	menuNode_t *node;
	const char *token;
	int i;

	/* search for menus with same name */
	for (i = 0; i < mn.numMenus; i++)
		if (!Q_strncmp(name, mn.menus[i].name, MAX_VAR))
			break;

	if (i < mn.numMenus) {
		Com_Printf("MN_ParseMenus: menu \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (mn.numMenus >= MAX_MENUS) {
		Sys_Error("MN_ParseMenu: max menus exceeded (%i) - ignore '%s'\n", MAX_MENUS, name);
		return;	/* never reached */
	}

	/* initialize the menu */
	menu = &mn.menus[mn.numMenus++];
	memset(menu, 0, sizeof(*menu));

	Q_strncpyz(menu->name, name, sizeof(menu->name));

	MN_WindowNodeLoading(menu);

	/* get it's body */
	token = COM_Parse(text);

	/* does this menu inherit data from another menu? */
	if (!Q_strncmp(token, "extends", 7)) {
		menu_t *superMenu;
		token = COM_Parse(text);
		Com_DPrintf(DEBUG_CLIENT, "MN_ParseMenus: menu \"%s\" inheriting menu \"%s\"\n", name, token);
		superMenu = MN_GetMenu(token);
		if (!superMenu)
			Sys_Error("MN_ParseMenu: menu '%s' can't inherit from menu '%s' - because '%s' was not found\n", name, token, token);
		memcpy(menu, superMenu, sizeof(*menu));
		Q_strncpyz(menu->name, name, sizeof(menu->name));
		token = COM_Parse(text);
	}

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseMenus: menu \"%s\" without body ignored\n", menu->name);
		mn.numMenus--;
		return;
	}

	/* parse it's body */
	if (!MN_ParseMenuBody(menu, text)) {
		Com_Printf("MN_ParseMenus: menu \"%s\" with bad body ignored\n", menu->name);
		mn.numMenus--;
		return;
	}

	MN_WindowNodeLoaded(menu);

	for (node = menu->firstNode; node; node = node->next)
		if (node->num >= MAX_MENUTEXTS)
			Sys_Error("Error in menu %s - max menu num exeeded (num: %i, max: %i) in node '%s'", menu->name, node->num, MAX_MENUTEXTS, node->name);

}

/**
 * @sa COM_MacroExpandString
 */
const char *MN_GetReferenceString (const menu_t* const menu, char *ref)
{
	if (!ref)
		return NULL;
	if (*ref == '*') {
		char ident[MAX_VAR];
		const char *text, *token;
		char command[MAX_VAR];
		char param[MAX_VAR];

		/* get the reference and the name */
		text = COM_MacroExpandString(ref);
		if (text)
			return text;

		text = ref + 1;
		token = COM_Parse(&text);
		if (!text)
			return NULL;
		Q_strncpyz(ident, token, sizeof(ident));
		token = COM_Parse(&text);
		if (!text)
			return NULL;

		if (!Q_strncmp(ident, "binding", 7)) {
			/* get the cvar value */
			if (*text && *text <= ' ') {
				/* check command and param */
				Q_strncpyz(command, token, sizeof(command));
				token = COM_Parse(&text);
				Q_strncpyz(param, token, sizeof(param));
				/*Com_sprintf(token, MAX_VAR, "%s %s", command, param);*/
			}
			return Key_GetBinding(token, (cls.state != ca_active ? KEYSPACE_MENU : KEYSPACE_GAME));
		} else {
			menuNode_t *refNode;
			const value_t *val;

			/* draw a reference to a node property */
			refNode = MN_GetNode(menu, ident);
			if (!refNode)
				return NULL;

			/* get the property */
			for (val = nps; val->type; val++)
				if (!Q_strcasecmp(token, val->string))
					break;

			if (!val->type)
				return NULL;

			/* get the string */
			return Com_ValueToStr(refNode, val->type & V_BASETYPEMASK, val->ofs);
		}
	} else if (*ref == '_') {
		ref++;
		return _(ref);
	} else {
		/* just get the data */
		return ref;
	}
}

float MN_GetReferenceFloat (const menu_t* const menu, void *ref)
{
	if (!ref)
		return 0.0;
	if (*(char *) ref == '*') {
		char ident[MAX_VAR];
		const char *token, *text;

		/* get the reference and the name */
		text = (char *) ref + 1;
		token = COM_Parse(&text);
		if (!text)
			return 0.0;
		Q_strncpyz(ident, token, sizeof(ident));
		token = COM_Parse(&text);
		if (!text)
			return 0.0;

		if (!Q_strncmp(ident, "cvar", 4)) {
			/* get the cvar value */
			return Cvar_VariableValue(token);
		} else {
			menuNode_t *refNode;
			const value_t *val;

			/* draw a reference to a node property */
			refNode = MN_GetNode(menu, ident);
			if (!refNode)
				return 0.0;

			/* get the property */
			for (val = nps; val->type; val++)
				if (!Q_strcasecmp(token, val->string))
					break;

			if (val->type != V_FLOAT)
				return 0.0;

			/* get the string */
			return *(float *) ((byte *) refNode + val->ofs);
		}
	} else {
		/* just get the data */
		return *(float *) ref;
	}
}

/**
 * @brief Checks the parsed menus for errors
 * @return false if there are errors - true otherwise
 */
qboolean MN_ScriptSanityCheck (void)
{
	int i, error = 0;
	menuNode_t* node;

	for (i = 0, node = mn.menuNodes; i < mn.numNodes; i++, node++) {
		switch (node->type) {
		case MN_TEXT:
			if (!node->height) {
				Com_Printf("MN_ParseNodeBody: node '%s' (menu: %s) has no height value but is a text node\n", node->name, node->menu->name);
				error++;
			} else if (node->texh[0] && node->height != (int)(node->size[1] / node->texh[0])) {
				/* if node->texh[0] == 0, the height of the font is used */
				Com_Printf("MN_ParseNodeBody: height value (%i) of node '%s' (menu: %s) differs from size (%.0f) and format (%.0f) values\n",
					node->height, node->name, node->menu->name, node->size[1], node->texh[0]);
				error++;
			}
			break;
		default:
			break;
		}
	}

	if (!error)
		return qtrue;
	else
		return qfalse;
}
