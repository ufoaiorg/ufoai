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
#include "node/m_node_window.h"
#include "node/m_node_selectbox.h"


/** @brief valid properties for options of the selectbox and tab */
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
	"call",
	"*",
	"&"
};

#define EA_SPECIAL_NUM_EVENTACTION 1
#define EA_SPECIAL_TIMEOUT (EA_NUM_EVENTACTION + 1)

static const char *ea_special_strings[EA_SPECIAL_NUM_EVENTACTION] = {
	"timeout",
};

/**
 * @brief Find a value_t by name into a array of value_t
 * @param[in] propertyList Array of value_t, with null termination
 * @param[in] name Property name we search
 * @return A value_t with the requested name, else NULL
 */
const value_t* MN_FindPropertyByName (const value_t* propertyList, const char* name)
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
 * @brief Allocate a string into the menu memory
 * @node Its not a dynamic memory allocation. Please only use it at the loading time
 * @param[in] string Use to initialize the string
 * @param[in] size request a fixed memory size, if 0 the string size is used
 * @todo use it every where its possible (search mn.curadata)
 */
char* MN_AllocString (const char* string, int size)
{
	char* result = (char *)mn.curadata;
	if (size != 0) {
		assert(mn.curadata - mn.adata + size <= mn.adataize);
		strncpy((char *)mn.curadata, string, size);
		mn.curadata += size;
	} else {
		assert(mn.curadata - mn.adata + strlen(string) + 1 <= mn.adataize);
		mn.curadata += sprintf((char *)mn.curadata, "%s", string);
	}
	return result;
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

	/* prevent bad position */
	assert(*token[0] == '{');

	while (qtrue) {
		int type = EA_NULL;
		found = qfalse;

		/* get new token */
		*token = COM_EParse(text, errhead, NULL);
		if (!*token)
			return qfalse;

		if (*token[0] == '}')
			break;

		/* decode action type */
		for (i = 0; i < EA_NUM_EVENTACTION; i++) {
			if (i == EA_VAR || i == EA_NODE)
				continue;
			if (Q_strcasecmp(*token, ea_strings[i]) != 0)
				continue;
			type = i;
		}
		if (type == EA_NULL) {
			if (*token[0] == ea_strings[EA_VAR][0])
				type = EA_VAR;
			else if (*token[0] == ea_strings[EA_NODE][0]) {
				type = EA_NODE;
			}
		}
		if (type == EA_NULL) {
			for (i = 0; i < EA_SPECIAL_NUM_EVENTACTION; i++) {
				if (Q_strcasecmp(*token, ea_special_strings[i]) != 0)
					continue;
				type = EA_NUM_EVENTACTION + 1 + i;
			}
		}

		/* unknown, we break the parsing */
		if (type == EA_NULL) {
			Com_Printf("MN_ParseAction: unknown token \"%s\" ignored (in event) (node: %s.%s)\n", *token, menuNode->menu->name, menuNode->name);
			return qfalse;
		}

		/** @todo better to append the action after initialization */
		/* add the action */
		if (lastAction) {
			if (mn.numActions >= MAX_MENUACTIONS)
				Sys_Error("MN_ParseAction: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
			action = &mn.menuActions[mn.numActions++];
			memset(action, 0, sizeof(*action));
			lastAction->next = action;
		}
		action->type = type;

		/* decode action */
		switch (action->type) {
		case EA_CMD:
			/* get parameter values */
			*token = COM_EParse(text, errhead, NULL);
			if (!*text)
				return qfalse;

			/* get the value */
			action->data = mn.curadata;
			mn.curadata += Com_EParseValue(mn.curadata, *token, V_LONGSTRING, 0, 0);
			break;

		case EA_NODE:
			/* get the node name */
			{
				char cast[32] = "abstractnode";
				const char *nodeName = *token + 1;
				nodeBehaviour_t *castedBehaviour;

				/* check cast */
				if (nodeName[0] == '(') {
					char *end = strchr(nodeName, ')');
					assert(end != NULL);
					assert(end - nodeName < 32);

					/* copy the cast and update the node name */
					if (end != NULL) {
						strncpy(cast, nodeName + 1, end - nodeName - 1);
						cast[end - nodeName - 1] = '\0';
						nodeName = end + 1;
					}
				}

				/* copy the node */
				action->data = mn.curadata;
				strcpy((char *) mn.curadata, nodeName);
				mn.curadata += ALIGN(strlen((char *) mn.curadata) + 1);

				/* get the node property */
				*token = COM_EParse(text, errhead, NULL);
				if (!*text)
					return qfalse;

				castedBehaviour = MN_GetNodeBehaviour(cast);
				val = MN_GetPropertyFromBehaviour(castedBehaviour, *token);
				if (!val) {
					/* do we ALREADY know this node? and his type */
					menuNode_t *node = MN_GetNode(menuNode->menu, action->data);
					if (node) {
						val = MN_NodeGetPropertyDefinition(node, *token);
					} else {
						Com_Printf("MN_ParseAction: node \"%s\" not already know (in event), you can cast it\n", *token);
					}
				}

				action->scriptValues = val;

				if (!val || !val->type) {
					Com_Printf("MN_ParseAction: token \"%s\" isn't a node property (in event)\n", *token);
					action->type = EA_NULL;
					break;
				}

				/* get the value */
				*token = COM_EParse(text, errhead, NULL);
				if (!*text)
					return qfalse;

				mn.curadata += Com_EParseValue(mn.curadata, *token, val->type & V_BASETYPEMASK, 0, val->size);
			}
			break;

		case EA_VAR:
			Com_Printf("MN_ParseAction: token \"%s\" ignored, EA_VAR not implemented (node: %s.%s)\n", *token, menuNode->menu->name, menuNode->name);
			break;

		case EA_CALL:
			/* get the function name */
			*token = COM_EParse(text, errhead, NULL);

			/* function calls */
			for (node = mn.menus[mn.numMenus - 1].firstChild; node; node = node->next) {
				if ((node->behaviour->id == MN_FUNC || node->behaviour->id == MN_CONFUNC || node->behaviour->id == MN_CVARFUNC)
					&& !Q_strncmp(node->name, *token, sizeof(node->name))) {

					action->data = mn.curadata;
					*(menuAction_t ***) mn.curadata = &node->onClick;
					mn.curadata += ALIGN(sizeof(menuAction_t *));

					lastAction = action;
					found = qtrue;
					break;
				}
			}
			break;

		case EA_SPECIAL_TIMEOUT:
			/* get new token */
			*token = COM_EParse(text, errhead, NULL);
			/** @todo use scanef instead of atoi, no need to check '}' */
			if (!*token || **token == '}') {
				Com_Printf("MN_ParseAction: timeout with no value (in event) (node: %s)\n", menuNode->name);
				return qfalse;
			}
			menuNode->timeOut = atoi(*token);

			/* no action for that */
			action->type = EA_NULL;
			break;

		default:
			assert(qfalse);
		}

		/* step */
		lastAction = action;
	}

	assert(*token[0] == '}');

	return qtrue;
}

static qboolean MN_ParseOption (menuNode_t * node, const char **text, const char **token, const char *errhead)
{
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
	if (*token[0] != '{') {
		Com_Printf("MN_ParseOption: node with bad option definition ignored (node \"%s\", menu %s)\n", node->name, node->menu->name);
		return qfalse;
	}

	if (mn.numSelectBoxes >= MAX_SELECT_BOX_OPTIONS) {
		FS_SkipBlock(text);
		Com_Printf("MN_ParseOption: Too many option entries for node %s (menu %s)\n", node->name, node->menu->name);
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
		const value_t *property;

		*token = COM_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;
		if (*token[0] == '}')
			break;

		property = MN_FindPropertyByName(selectBoxValues, *token);
		if (!property) {
			Com_Printf("MN_ParseOption: unknown options property: '%s' - ignore it\n", *token);
			return qfalse;
		}

		/* get parameter values */
		*token = COM_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;

		if (property->type == V_TRANSLATION_MANUAL_STRING) {
			if (property->size) {
				/* selectbox values are static arrays */
				char *target = ((char*)&mn.menuSelectBoxes[mn.numSelectBoxes] + property->ofs);
				const char *translateableToken = *token;
				if (translateableToken[0] == '_')
					translateableToken++;
				Q_strncpyz(target, translateableToken, property->size);
			}
			/* otherwise fall through */
		} else {
			int result;
			size_t bytes;
			result = Com_ParseValue(&mn.menuSelectBoxes[mn.numSelectBoxes], *token, property->type, property->ofs, property->size, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("MN_ParseOption: Invalid value for property '%s': %s\n", property->string, Com_GetError());
				return qfalse;
			}
		}
	} while (**token != '}');
	MN_NodeAddOption(node);
	return qtrue;
}

static qboolean MN_ParseExcludeRect (menuNode_t * node, const char **text, const char **token, const char *errhead)
{
	excludeRect_t rect;

	/* get parameters */
	*token = COM_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;
	if (**token != '{') {
		Com_Printf("MN_ParseExcludeRect: node with bad excluderect ignored (node \"%s\", menu %s)\n", node->name, node->menu->name);
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
			Com_EParseValue(&rect, *token, V_POS, offsetof(excludeRect_t, pos), sizeof(vec2_t));
		} else if (!Q_strcmp(*token, "size")) {
			*token = COM_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Com_EParseValue(&rect, *token, V_POS, offsetof(excludeRect_t, size), sizeof(vec2_t));
		}
	} while (**token != '}');


	if (node->excludeRectNum >= MAX_EXLUDERECTS) {
		Com_Printf("MN_ParseExcludeRect: exluderect limit exceeded (max: %i)\n", MAX_EXLUDERECTS);
		return qfalse;
	}

	/* copy the rect into the global structure */
	mn.excludeRect[mn.numExcludeRect] = rect;

	/* link only the first element */
	if (node->excludeRect == NULL) {
		node->excludeRect = &mn.excludeRect[mn.numExcludeRect];
	}

	mn.numExcludeRect++;
	node->excludeRectNum++;

	return qtrue;
}

static qboolean MN_ParseProperty (menuNode_t * node, const value_t *val, const char **text, const char **token, const char *errhead)
{
	size_t bytes;
	int result;
	node->scriptValues = val;

	/* Com_Printf("  %s", *token); */

	if (val->type == V_NULL) {
		return qtrue;
	}

	/* get parameter values */
	*token = COM_EParse(text, errhead, node->name);
	if (!*text)
		return qfalse;

	/* Com_Printf(" %s", *token); */

	/* get the value */
	if (!(val->type & V_MENU_COPY)) {
		result = Com_ParseValue(node, *token, val->type, val->ofs, val->size, &bytes);
		if (result != RESULT_OK) {
			Com_Printf("Invalid value for property '%s': %s\n", val->string, Com_GetError());
			return qfalse;
		}
	} else {
		/* a reference to data is handled like this */
		*(byte **) ((byte *) node + val->ofs) = mn.curadata;
		/* references are parsed as string */
		if (**token == '*') {
			/* sanity check */
			if (strlen(*token) > MAX_VAR - 1) {
				Com_Printf("MN_ParseProperty: Value '%s' is too long (key %s)\n", *token, val->string);
				return qfalse;
			}

			result = Com_ParseValue(mn.curadata, *token, V_STRING, 0, 0, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("Invalid value for property '%s': %s\n", val->string, Com_GetError());
				return qfalse;
			}
			mn.curadata += bytes;
		} else {
			/* sanity check */
			if (val->type == V_STRING && strlen(*token) > MAX_VAR - 1) {
				Com_Printf("MN_ParseProperty: Value '%s' is too long (key %s)\n", *token, val->string);
				return qfalse;
			}

			result = Com_ParseValue(mn.curadata, *token, val->type & V_BASETYPEMASK, 0, val->size, &bytes);
			if (result != RESULT_OK) {
				Com_Printf("Invalid value for property '%s': %s\n", val->string, Com_GetError());
				return qfalse;
			}
			mn.curadata += bytes;
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
		Sys_Error("MN_ParseEventProperty: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
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
	if (node->behaviour->id == MN_CONFUNC || node->behaviour->id == MN_FUNC || node->behaviour->id == MN_CVARFUNC) {
		menuAction_t **action;

		/* add new actions to end of list */
		action = &node->onClick;
		for (; *action; action = &(*action)->next) {}

		if (mn.numActions >= MAX_MENUACTIONS)
			Sys_Error("MN_ParseNodeBody: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
		*action = &mn.menuActions[mn.numActions++];
		memset(*action, 0, sizeof(**action));

		/* register confunc non inherited */
		if (node->super == NULL && node->behaviour->id == MN_CONFUNC) {
			/* don't add a callback twice */
			if (!Cmd_Exists(node->name)) {
				Cmd_AddCommand(node->name, MN_ConfuncCommand_f, "Confunc callback");
				Cmd_AddUserdata(node->name, node);
			} else {
				Com_Printf("MN_ParseNodeBody: Command name for confunc '%s.%s' already registered\n", node->menu->name, node->name);
			}
		}

		return MN_ParseAction(node, *action, text, token);
	}

	do {
		const value_t *val;
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

		/* find the property */

		val = MN_NodeGetPropertyDefinition(node, *token);

		if (!val) {
			/* unknown token, print message and continue */
			Com_Printf("MN_ParseNodeBody: unknown property \"%s\" ignored (node \"%s\", menu %s)\n", *token, node->name, node->menu->name);
			return qfalse;
		}


		/* if its not a special case */
		if (!(val->type & V_SPECIAL)) {
			result = MN_ParseProperty(node, val, text, token, errhead);
			if (!result)
				return qfalse;

		/* is it an event property */
		} else if (val->type == V_SPECIAL_ACTION) {
			result = MN_ParseEventProperty(node, val, text, token, errhead);
			if (!result)
				return qfalse;
			nextTokenAlreadyRead = qtrue;

		} else if (val->type == V_SPECIAL_EXCLUDERECT) {
			result = MN_ParseExcludeRect(node, text, token, errhead);
			if (!result)
				return qfalse;

		/* for MN_SELECTBOX */
		} else if (val->type == V_SPECIAL_OPTIONNODE) {
			result = MN_ParseOption(node, text, token, errhead);
			if (!result)
				return qfalse;

		} else {
			/* unknown val->type !!!!! */
			Com_Printf("MN_ParseNodeBody: unknown val->type \"%d\" ignored (node \"%s\", menu %s)\n", val->type, node->name, node->menu->name);
			return qfalse;
		}
	} while (*text);

	return qfalse;
}

/**
 * @brief parse a node and complet the menu with it
 * @sa MN_ParseMenuBody
 * @sa MN_ParseNodeBody
 * @todo we can think about merging MN_ParseNodeBody here
 * @node first token already read
 * @node dont read more than the need token (last right token is '}' of end of node)
 */
static qboolean MN_ParseNode (menu_t * menu, const char **text, const char **token, const char *errhead)
{
	menuNode_t *node;
	nodeBehaviour_t *behaviour;
	qboolean result;

	/* allow to begin with the identifier "node" before the behaviour name */
	if (!Q_strcasecmp(*token, "node")) {
		*token = COM_EParse(text, errhead, menu->name);
		if (!*text)
			return qfalse;
	}

	/* get the behaviour */
	behaviour = MN_GetNodeBehaviour(*token);
	if (behaviour == NULL) {
		Com_Printf("MN_ParseNode: node behaviour '%s' doesn't exist (menu \"%s\")\n", *token, menu->name);
		return qfalse;
	}

	/* get the name */
	*token = COM_EParse(text, errhead, menu->name);
	if (!*text)
		return qfalse;

	/* test if node already exists */
	node = MN_GetNode(menu, *token);
	if (node) {
		if (node->behaviour->id != behaviour->id) {
			Com_Printf("MN_ParseNode: we can't change node type (node \"%s.%s\")\n", menu->name, node->name);
			return qfalse;
		}
		Com_DPrintf(DEBUG_CLIENT, "... over-riding node %s.%s\n", menu->name, node->name);
		/* reset action list of node */
		node->onClick = NULL;	/**< @todo understand why this strange hack (there is a lot of over actions) */

	/* else initialize node */
	} else {
		node = MN_AllocNode(behaviour->name);
		node->menu = menu;
		Q_strncpyz(node->name, *token, sizeof(node->name));
		MN_AppendNode(menu, node);
	}

	/* node default values */
	/** @todo move it into the respective "loading" function (for those nodes, that need it) */
	node->padding = 3;

	/* throw to the node, we begin to read attributes */
	if (node->behaviour->loading)
		node->behaviour->loading(node);

	/* get body */
	*token = COM_EParse(text, errhead, menu->name);
	if (!*text)
		return qfalse;

	/* node without body is out of spec */
	if (*token[0] != '{') {
		Com_Printf("MN_ParseNode: node dont have body, token '%s' read (node \"%s.%s\")\n", *token, menu->name, node->name);
		mn.numNodes--;
		return qfalse;
	}

	result = MN_ParseNodeBody(node, text, token);
	if (!result) {
		Com_Printf("MN_ParseNode: node with bad body ignored (node \"%s.%s\")\n", menu->name, node->name);
		mn.numNodes--;
		return qfalse;
	}

	/* already check on MN_ParseNodeBody */
	assert(*token[0] == '}');

	/* init the node according to his behaviour */
	if (node->behaviour->loaded) {
		node->behaviour->loaded(node);
	}
	return qtrue;
}

static qboolean MN_ParseMenuProperties (menu_t * menu, const char **text, const char **token, const char *errhead)
{
	const nodeBehaviour_t* menuBehaviour = MN_GetNodeBehaviour("menu");

	/* there are no menu properties */
	if (*token[0] != '{') {
		return qtrue;
	}

	/* get new token */
	*token = COM_EParse(text, errhead, menu->name);
	if (!*text)
		return qfalse;

	while (*token[0] != '}') {
		const value_t* property;
		int result;
		size_t bytes;

		property = MN_FindPropertyByName(menuBehaviour->properties, *token);
		if (!property) {
			Com_Printf("Unknown menu property '%s' find\n", *token);
			return qfalse;
		}

		/* get new token */
		*token = COM_EParse(text, errhead, menu->name);
		if (!*text)
			return qfalse;

		/* check value */
		result = Com_ParseValue(menu, *token, property->type, property->ofs, property->size, &bytes);
		if (result != RESULT_OK) {
			Com_Printf("Invalid value for property '%s': %s\n", property->string, Com_GetError());
			return qfalse;
		}

		/* get new token */
		*token = COM_EParse(text, errhead, menu->name);
		if (!*text)
			return qfalse;
	}

	/* get new token */
	*token = COM_EParse(text, errhead, menu->name);
	if (!*text)
		return qfalse;

	return qtrue;
}

/**
 * @sa MN_ParseNodeBody
 * @todo merge it with MN_ParseMenu
 */
static qboolean MN_ParseMenuBody (menu_t * menu, const char **text)
{
	const char *errhead = "MN_ParseMenuBody: unexpected end of file (menu";
	const char *token;
	qboolean result;

	Vector2Set(menu->pos, 0, 0);

	/* get new token */
	token = COM_EParse(text, errhead, menu->name);
	if (!*text)
		return qfalse;

	/* parse special menu values */
	result = MN_ParseMenuProperties(menu, text, &token, errhead);
	if (!result)
		return qfalse;

	/* for each nodes */
	while (token[0] != '}') {
		result = MN_ParseNode(menu, text, &token, errhead);
		if (!result)
			return qfalse;

		token = COM_EParse(text, errhead, menu->name);
		if (!*text)
			return qfalse;
	}

	return qtrue;
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
							Com_EParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].scale, token, V_VECTOR, 0, sizeof(vec3_t));
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
							Com_EParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].angles, token, V_VECTOR, 0, sizeof(vec3_t));
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
							Com_EParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].origin, token, V_VECTOR, 0, sizeof(vec3_t));
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
						Com_EParseValue(menuModel, token, v->type, v->ofs, v->size);
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
	}

	if (mn.numMenus >= MAX_MENUS) {
		Sys_Error("MN_ParseMenu: max menus exceeded (%i) - ignore '%s'\n", MAX_MENUS, name);
		return;	/* never reached */
	}

	/* initialize the menu */
	menu = &mn.menus[mn.numMenus++];
	memset(menu, 0, sizeof(*menu));

	Q_strncpyz(menu->name, name, sizeof(menu->name));

	/** @todo waithing for the merge of menu and node */
	/* menuBehaviour->loading(menu); */
	MN_WindowNodeLoading(menu);

	/* get it's body */
	token = COM_Parse(text);

	/* does this menu inherit data from another menu? */
	if (!Q_strncmp(token, "extends", 7)) {
		menu_t *superMenu;
		menuNode_t *newNode;

		token = COM_Parse(text);
		Com_DPrintf(DEBUG_CLIENT, "MN_ParseMenus: menu \"%s\" inheriting menu \"%s\"\n", name, token);
		superMenu = MN_GetMenu(token);
		if (!superMenu)
			Sys_Error("MN_ParseMenu: menu '%s' can't inherit from menu '%s' - because '%s' was not found\n", name, token, token);
		*menu = *superMenu;
		menu->super = superMenu;
		Q_strncpyz(menu->name, name, sizeof(menu->name));

		/* start a new list */
		menu->firstChild = NULL;
		menu->lastChild = NULL;

		/* clone all super menu's nodes */
		for (node = superMenu->firstChild; node; node = node->next) {
			if (mn.numNodes >= MAX_MENUNODES)
				Sys_Error("MAX_MENUNODES exceeded\n");

			newNode = MN_CloneNode(node, menu, qtrue);
			newNode->super = node;
			MN_AppendNode(menu, newNode);

			/* update special links */
			if (superMenu->renderNode == node)
				menu->renderNode = newNode;
			else if (superMenu->popupNode == node)
				menu->popupNode = newNode;
			else if (superMenu->onTimeOut == node->onClick) {
				menu->onTimeOut = newNode->onClick;
				menu->eventNode = newNode;
			}
		}

		token = COM_Parse(text);
	}

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseMenu: menu \"%s\" without body ignored\n", menu->name);
		mn.numMenus--;
		return;
	}

	/* parse it's body */
	if (!MN_ParseMenuBody(menu, text)) {
		Sys_Error("MN_ParseMenu: menu \"%s\" have a bad body\n", menu->name);
		return;	/* never reached */
	}

	/** @todo waithing for the merge of menu and node */
	/* menuBehaviour->loaded(menu); */
	MN_WindowNodeLoaded(menu);
}

/**
 * @sa COM_MacroExpandString
 */
const char *MN_GetReferenceString (const menu_t* const menu, const char *ref)
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
			val = MN_NodeGetPropertyDefinition(refNode, token);
			if (!val)
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
			val = MN_NodeGetPropertyDefinition(refNode, token);
			if (!val || val->type != V_FLOAT)
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
	return qtrue;
}
