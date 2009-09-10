/**
 * @file src/client/menu/m_expression.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "m_expression.h"
#include "m_main.h"
#include "m_internal.h"
#include "m_parse.h"
#include "node/m_node_abstractnode.h"
#include "../../shared/parse.h"

/**
 * @return A float value, else 0
 */
float MN_GetFloatFromExpression (menuAction_t *expression, const menuNode_t *source)
{
	switch (expression->type & EA_HIGHT_MASK) {
	case EA_VALUE:
		switch (expression->type) {
		case EA_VALUE_STRING:
		case EA_VALUE_STRING_WITHINJECTION:
			{
				const char* string = expression->d.terminal.d1.string;
				if (expression->type == EA_VALUE_STRING_WITHINJECTION)
					string = MN_GenInjectedString(source, qtrue, string, qfalse);
				return atof(string);
			}
		case EA_VALUE_FLOAT:
			return expression->d.terminal.d1.number;
		case EA_VALUE_CVARNAME:
		case EA_VALUE_CVARNAME_WITHINJECTION:
			{
				cvar_t *cvar = NULL;
				const char *cvarName = expression->d.terminal.d1.string;
				if (expression->type == EA_VALUE_CVARNAME_WITHINJECTION)
					cvarName = MN_GenInjectedString(source, qtrue, cvarName, qfalse);
				cvar = Cvar_Get(cvarName, "", 0, "Cvar from menu expression");
				return cvar->value;
			}
		case EA_VALUE_PATHPROPERTY:
		case EA_VALUE_PATHPROPERTY_WITHINJECTION:
			{
				menuNode_t *node;
				const value_t *property;
				const char *path = expression->d.terminal.d1.string;
				if (expression->type == EA_VALUE_PATHPROPERTY_WITHINJECTION)
					path = MN_GenInjectedString(source, qtrue, path, qfalse);

				MN_ReadNodePath(path, source, &node, &property);
				if (!node) {
					Com_Printf("MN_GetFloatFromParam: Node '%s' wasn't found; '0' returned\n", path);
					return 0;
				}
				if (!property) {
					Com_Printf("MN_GetFloatFromParam: Property '%s' wasn't found; '0' returned\n", path);
					return 0;
				}
				return MN_GetFloatFromNodeProperty(node, property);
			}
		}
		break;
	case EA_OPERATOR_FLOAT2FLOAT:
		{
			const float value1 = MN_GetFloatFromExpression(expression->d.nonTerminal.left, source);
			const float value2 = MN_GetFloatFromExpression(expression->d.nonTerminal.right, source);

			switch (expression->type) {
			case EA_OPERATOR_ADD:
				return value1 + value2;
			case EA_OPERATOR_SUB:
				return value1 - value2;
			case EA_OPERATOR_MUL:
				return value1 * value2;
			case EA_OPERATOR_DIV:
				if (value2 == 0) {
					Com_Printf("MN_GetFloatFromExpression: Div by 0. '0' returned");
					return 0;
				} else
					return value1 / value2;
			case EA_OPERATOR_MOD:
				{
					const int v1 = value1;
					const int v2 = value2;
					/** @todo do we have some check to do? */
					return v1 % v2;
				}
			}
		}
		break;

	}

	Com_Printf("MN_GetFloatFromExpression: Unsupported expression type: %i. '0' returned", expression->type);
	return 0;
}

/**
 * @return A string, else an empty string
 * @todo this should not work very well, because too much va are used
 * then we should locally cache values, or manage a temporary string structure
 */
const char* MN_GetStringFromExpression (menuAction_t *expression, const menuNode_t *source)
{
	switch (expression->type & EA_HIGHT_MASK) {
	case EA_VALUE:
		switch (expression->type) {
		case EA_VALUE_STRING:
		case EA_VALUE_STRING_WITHINJECTION:
			{
				const char* string = expression->d.terminal.d1.string;
				if (expression->type == EA_VALUE_STRING_WITHINJECTION)
					string = MN_GenInjectedString(source, qtrue, string, qfalse);
				return string;
			}
		case EA_VALUE_FLOAT:
		{
			const float number = expression->d.terminal.d1.number;
			const int integer = number;
			/** @todo should we add a delta? */
			if (number == integer)
				return va("%i", integer);
			else
				return va("%f", number);
		}
		case EA_VALUE_CVARNAME:
		case EA_VALUE_CVARNAME_WITHINJECTION:
		{
			cvar_t *cvar = NULL;
			const char *cvarName = expression->d.terminal.d1.string;
			if (expression->type == EA_VALUE_CVARNAME_WITHINJECTION)
				cvarName = MN_GenInjectedString(source, qtrue, cvarName, qfalse);
			cvar = Cvar_Get(cvarName, "", 0, "Cvar from menu expression");
			return cvar->string;
		}
		case EA_VALUE_PATHPROPERTY:
		case EA_VALUE_PATHPROPERTY_WITHINJECTION:
			{
				menuNode_t *node;
				const value_t *property;
				const char* string;
				const char *path = expression->d.terminal.d1.string;
				if (expression->type == EA_VALUE_PATHPROPERTY_WITHINJECTION)
					path = MN_GenInjectedString(source, qtrue, path, qfalse);

				MN_ReadNodePath(expression->d.terminal.d1.string, source, &node, &property);
				if (!node) {
					Com_Printf("MN_GetStringFromParam: Node '%s' wasn't found; '' returned\n", path);
					return "";
				}
				if (!property) {
					Com_Printf("MN_GetStringFromParam: Property '%s' wasn't found; '' returned\n", path);
					return "";
				}
				string = MN_GetStringFromNodeProperty(node, property);
				if (string == NULL) {
					Com_Printf("MN_GetStringFromParam: String getter for '%s' property do not exists; '' returned\n", path);
					return "";
				}
				return string;
			}
			break;
		}

	case EA_OPERATOR_BOOLEAN2BOOLEAN:
	case EA_OPERATOR_FLOAT2BOOLEAN:
	case EA_OPERATOR_STRING2BOOLEAN:
		{
			const qboolean v = MN_GetBooleanFromExpression(expression, source);
			return (v)?"1":"0";
		}

	case EA_OPERATOR_FLOAT2FLOAT:
		{
			const float number = MN_GetFloatFromExpression(expression, source);
			const int integer = number;
			/** @todo should we add a delta? */
			if (number == integer)
				return va("%i", integer);
			else
				return va("%f", number);
		}
	}


	Com_Printf("MN_GetStringFromExpression: Unsupported expression type: %i", expression->type);
	return "";
}

/**
 * @brief Check if an expression is true
 * @return True if the expression is true
 */
qboolean MN_GetBooleanFromExpression (menuAction_t *expression, const menuNode_t *source)
{
	if (expression == NULL)
		return qfalse;

	switch (expression->type & EA_HIGHT_MASK) {
	case EA_VALUE:
		return MN_GetFloatFromExpression (expression, source) != 0;

	case EA_OPERATOR_BOOLEAN2BOOLEAN:
		{
			const qboolean value1 = MN_GetBooleanFromExpression(expression->d.nonTerminal.left, source);
			const qboolean value2 = MN_GetBooleanFromExpression(expression->d.nonTerminal.right, source);

			switch (expression->type) {
			case EA_OPERATOR_AND:
				return value1 && value2;
			case EA_OPERATOR_OR:
				return value1 || value2;
			case EA_OPERATOR_XOR:
				return value1 ^ value2;
			case EA_OPERATOR_NOT:
				return !value1;
			default:
				Com_Error(ERR_FATAL, "MN_GetBooleanFromExpression: (BOOL2BOOL) Invalid expression type");
			}
		}
		break;

	case EA_OPERATOR_FLOAT2BOOLEAN:
		{
			const float value1 = MN_GetFloatFromExpression(expression->d.nonTerminal.left, source);
			const float value2 = MN_GetFloatFromExpression(expression->d.nonTerminal.right, source);

			switch (expression->type) {
			case EA_OPERATOR_EQ:
				return value1 == value2;
			case EA_OPERATOR_LE:
				return value1 <= value2;
			case EA_OPERATOR_GE:
				return value1 >= value2;
			case EA_OPERATOR_GT:
				return value1 > value2;
			case EA_OPERATOR_LT:
				return value1 < value2;
			case EA_OPERATOR_NE:
				return value1 != value2;
			default:
				Com_Error(ERR_FATAL, "MN_GetBooleanFromExpression: (FLOAT2BOOL) Invalid expression type");
			}
		}
		break;

	case EA_OPERATOR_EXISTS:
		{
			const menuAction_t *e = expression->d.nonTerminal.left;
			const char* cvarName;
			assert(e);
			assert(e->type == EA_VALUE_CVARNAME || e->type == EA_VALUE_CVARNAME_WITHINJECTION);
			cvarName = e->d.terminal.d1.string;
			if (e->type == EA_VALUE_CVARNAME_WITHINJECTION)
				cvarName = MN_GenInjectedString(source, qtrue, cvarName, qfalse);
			return Cvar_FindVar(cvarName) != NULL;
		}
		break;

	case EA_OPERATOR_STRING2BOOLEAN:
		{
			const char* value1 = MN_GetStringFromExpression(expression->d.nonTerminal.left, source);
			const char* value2 = MN_GetStringFromExpression(expression->d.nonTerminal.right, source);

			switch (expression->type) {
			case EA_OPERATOR_STR_EQ:
				return strcmp(value1, value2) == 0;
			case EA_OPERATOR_STR_NE:
				return strcmp(value1, value2) != 0;
			default:
				Com_Error(ERR_FATAL, "MN_GetBooleanFromExpression: (STRING2BOOL) Invalid expression type");
			}
		}

	default:
		Com_Error(ERR_FATAL, "MN_GetBooleanFromExpression: Unsupported expression type: %i", expression->type);
	}
	return qfalse;
}

/**
 * @brief Allocate and initialize an expression according to a string
 * @param[in] token String describing a condition
 * @param[out] condition Condition to initialize
 * @return The condition if everything is ok, NULL otherwise
 */
menuAction_t *MN_AllocStaticStringCondition (const char *description)
{
	const char *errhead = "MN_AllocStaticStringCondition: unexpected end of string (object";
	const char *text, *base;
	menuAction_t *expression;

	base = va("( %s )", description);
	text = base;
	expression = MN_ParseExpression(&text, errhead, NULL);
	if (!expression) {
		Com_Printf("MN_AllocStaticStringCondition: Parse error on expression \"%s\"\n", base);
		return NULL;
	}

	return expression;
}

/**
 * @brief Read a value from the stream and init an action with it
 * @return An initialized action else NULL
 */
static menuAction_t *MN_ParseValueExpression (const char **text, const char *errhead, const menuNode_t *source)
{
	const char* token;
	menuAction_t *expression = MN_AllocStaticAction();

	token = Com_Parse(text);
	if (*text == NULL) {
		Com_Printf("MN_ParseTerminalExpression: Token expected\n");
		return NULL;
	}

	/* it is a const string (or an injection tag for compatibility) */
	if (Com_ParsedTokenIsQuoted() || token[0] == '<') {
		expression->d.terminal.d1.string = MN_AllocStaticString(token, 0);
		if (MN_IsInjectedString(token))
			expression->type = EA_VALUE_STRING_WITHINJECTION;
		else
			expression->type = EA_VALUE_STRING;
		return expression;
	}

	/* it is a cvarname */
	if (!strncmp(token, "*cvar:", 6)) {
		const char* cvarName = token + 6;
		expression->d.terminal.d1.string = MN_AllocStaticString(cvarName, 0);
		if (MN_IsInjectedString(cvarName))
			expression->type = EA_VALUE_CVARNAME_WITHINJECTION;
		else
			expression->type = EA_VALUE_CVARNAME;
		return expression;
	}

	/* it is a node property or it is a OLD syntax node property */
	/** @todo We MUST remove the OLD code as fast as possible */
	if (!strncmp(token, "*node:", 6) || !strncmp(token, "*", 1)) {
		const char* path = token + 1;
		char cast[32] = "abstractnode";
		const nodeBehaviour_t *castedBehaviour;
		const char *propertyName;
		qboolean relativeToNode;
		const value_t *property;

		relativeToNode = !strncmp(path, "node:", 5);
		if (relativeToNode)
			path = path + 5;

		/* check cast */
		if (path[0] == '(') {
			const char *end = strchr(path, ')');
			assert(end != NULL);
			assert(end - path < sizeof(cast));

			/* copy the cast and update the path */
			if (end != NULL) {
				Q_strncpyz(cast, path + 1, end - path);
				path = end + 1;
			}
		}

		if (MN_IsInjectedString(path))
			expression->type = EA_VALUE_PATHPROPERTY_WITHINJECTION;
		else
			expression->type = EA_VALUE_PATHPROPERTY;
		if (!relativeToNode)
			path = va("root.%s", path);
		expression->d.terminal.d1.string = MN_AllocStaticString(path, 0);

		castedBehaviour = MN_GetNodeBehaviour(cast);
		if (castedBehaviour == NULL)
			Com_Error(ERR_FATAL, "MN_ParseValueExpression: Node behaviour cast '%s' doesn't exists\n", cast);

		/* get property name */
		propertyName = strchr(path, '@');
		if (propertyName == NULL)
			Com_Error(ERR_FATAL, "MN_ParseSetAction: Property setter without property ('@' not found in '%s')\n", path);
		propertyName++;

		property = MN_GetPropertyFromBehaviour(castedBehaviour, propertyName);
		if (!property && source) {
			menuNode_t *node;
			/* do we ALREADY know this node? and his type */
			MN_ReadNodePath(path, source, &node, &property);
			if (!node)
				Com_Printf("MN_ParseSetAction: node \"%s\" not already know (in event), you can cast it\n", path);
		}

		if (property && property->type) {
			expression->d.terminal.d2.constData = property;
		}

		return expression;
	}

	/* unsigned and signed number */
	if ( (token[0] >= '0' && token[0] <= '9')
		|| (token[0] == '-' && token[1] >= '0' && token[1] <= '9') ) {
		/* @todo use a better check */
		float f = atof(token);
		expression->d.terminal.d1.number = f;
		expression->type = EA_VALUE_FLOAT;
		return expression;
	}

	if (!strcmp(token, "true")) {
		expression->d.terminal.d1.number = 1.0;
		expression->type = EA_VALUE_FLOAT;
		return expression;
	}

	if (!strcmp(token, "false")) {
		expression->d.terminal.d1.number = 0.0;
		expression->type = EA_VALUE_FLOAT;
		return expression;
	}

	Com_Error(ERR_FATAL, "MN_ParseSetAction: Token \"%s\" unknown. String must use quotes, cvar and nodes must use prefix.\n", token);
}

menuAction_t *MN_ParseExpression (const char **text, const char *errhead, const menuNode_t *source)
{
	const char* token;

	token = Com_Parse(text);
	if (*text == NULL)
		return NULL;

	if (!strcmp(token, "(")) {
		menuAction_t *expression;
		menuAction_t *e;

		e = MN_ParseExpression(text, errhead, source);

		token = Com_Parse(text);
		if (*text == NULL)
			return NULL;

		/* unary operator or unneed "( ... )" */
		if (!strcmp(token, ")"))
			return e;

		/* then its an operator */
		expression = MN_AllocStaticAction();
		expression->d.nonTerminal.left = e;
		expression->type = MN_GetActionTokenType(token, EA_BINARYOPERATOR);
		if (expression->type == EA_NULL) {
			Com_Printf("MN_ParseExpression: Invalid 'expression' statement. Unknown '%s' operator\n", token);
			return NULL;
		}

		e = MN_ParseExpression(text, errhead, source);
		expression->d.nonTerminal.right = e;

		token = Com_Parse(text);
		if (*text == NULL)
			return NULL;
		if (strcmp(token, ")") != 0) {
			Com_Printf("MN_ParseExpression: Token ')' expected\n");
			return NULL;
		}

		return expression;
	} else {

		int type = MN_GetActionTokenType(token, EA_UNARYOPERATOR);
		if (type == EA_NULL) {
			Com_UnParseLastToken();
			return MN_ParseValueExpression(text, errhead, source);
		} else {
			menuAction_t *expression = MN_AllocStaticAction();
			menuAction_t *e;

			e = MN_ParseExpression(text, errhead, source);
			expression->type = type;
			expression->d.nonTerminal.left = e;

			if (expression->type == EA_OPERATOR_EXISTS) {
				if (e->type != EA_VALUE_CVARNAME && e->type != EA_VALUE_CVARNAME_WITHINJECTION) {
					Com_Printf("MN_ParseExpression: Cvar expected, but type %d found\n", e->type);
					return NULL;
				}
			}
			return expression;
		}
	}

#if 0
	/* prevent wrong code */
	if (condition->type.left == condition->type.right
		 && (condition->type.left == IF_VALUE_STRING || condition->type.left == IF_VALUE_FLOAT)) {
		Com_Printf("MN_InitCondition: '%s' dont use any var operand.\n", token);
	}
#endif
	return NULL;
}
