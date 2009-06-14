/**
 * @file m_condition.c
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

#include "m_expression.h"
#include "m_main.h"
#include "m_internal.h"
#include "m_parse.h"
#include "node/m_node_abstractnode.h"
#include "../../shared/parse.h"

typedef struct {
	int operator;
	char* string;
} menuActionTypeList_t;

static const menuActionTypeList_t operatorList[] = {
	{EA_OPERATOR_EQ, "=="},
	{EA_OPERATOR_LE, "<="},
	{EA_OPERATOR_GE, ">="},
	{EA_OPERATOR_GT, ">"},
	{EA_OPERATOR_LT, "<"},
	{EA_OPERATOR_NE, "!="},
	{EA_OPERATOR_STR_EQ, "eq"},
	{EA_OPERATOR_STR_NE, "ne"},
	{EA_NULL, ""}
};

/**
 * @todo code the FLOAT/STRING type into the opcode; we should not check it like that
 */
static inline qboolean MN_IsFloatOperator (int op)
{
	return op == EA_OPERATOR_EQ
	 || op == EA_OPERATOR_LE
	 || op == EA_OPERATOR_GE
	 || op == EA_OPERATOR_GT
	 || op == EA_OPERATOR_LT
	 || op == EA_OPERATOR_NE;
}

/**
 * @return A float value, else 0
 */
static float MN_GetFloatFromExpression (menuAction_t *expression, const menuNode_t *source)
{
	switch (expression->type.op) {
	case EA_NULL:
		return qfalse;
	case EA_VALUE_STRING:
		return atof(expression->d.terminal.d1.string);
	case EA_VALUE_FLOAT:
		return expression->d.terminal.d1.number;
	case EA_VALUE_CVARNAME:
		{
			cvar_t *cvar = NULL;
			cvar = Cvar_Get(expression->d.terminal.d1.string, "", 0, "Menu if condition cvar");
			return cvar->value;
		}
	case EA_VALUE_NODEPROPERTY:
		{
			menuNode_t *node;
			const value_t *property;
			const char *path = expression->d.terminal.d1.string;
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
	return 0;
}

/**
 * @return A string, else an empty string
 */
static const char* MN_GetStringFromExpression (menuAction_t *expression, const menuNode_t *source)
{
	switch (expression->type.op) {
	case EA_VALUE_STRING:
		return expression->d.terminal.d1.string;
	case EA_VALUE_FLOAT:
		assert(qfalse);
	case EA_VALUE_CVARNAME:
	{
		cvar_t *cvar = NULL;
		cvar = Cvar_Get(expression->d.terminal.d1.string, "", 0, "Menu if condition cvar");
		return cvar->string;
	}
	case EA_VALUE_NODEPROPERTY:
		{
			menuNode_t *node;
			const value_t *property;
			const char* string;
			const char *path = expression->d.terminal.d1.string;
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

	Com_Printf("MN_GetStringFromExpression: Unsupported expression type: %i", expression->type.op);
	return "";
}

/**
 * @return A boolean, else 0
 */
static qboolean MN_GetBooleanFromExpression (menuAction_t *expression, const menuNode_t *source)
{
	switch (expression->type.op) {
	case EA_VALUE_STRING:
	case EA_VALUE_FLOAT:
	case EA_VALUE_CVARNAME:
	case EA_VALUE_NODEPROPERTY:
		return MN_GetFloatFromExpression (expression, source) != 0;
	}

	if (MN_IsFloatOperator(expression->type.op)) {
		const float value1 = MN_GetFloatFromExpression(expression->d.nonTerminal.left, source);
		const float value2 = MN_GetFloatFromExpression(expression->d.nonTerminal.right, source);

		switch (expression->type.op) {
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
			assert(qfalse);
		}
	}

	if (expression->type.op == EA_OPERATOR_EXISTS) {
		const menuAction_t *e = expression->d.nonTerminal.left;
		assert(e);
		assert(e->type.op == EA_VALUE_CVARNAME);
		return Cvar_FindVar(e->d.terminal.d1.string) != NULL;
	}

	if (expression->type.op == EA_OPERATOR_STR_EQ || expression->type.op == EA_OPERATOR_STR_NE) {
		const char* value1 = MN_GetStringFromExpression(expression->d.nonTerminal.left, source);
		const char* value2 = MN_GetStringFromExpression(expression->d.nonTerminal.right, source);

		switch (expression->type.op) {
		case EA_OPERATOR_STR_EQ:
			return strcmp(value1, value2) == 0;
		case EA_OPERATOR_STR_NE:
			return strcmp(value1, value2) != 0;
		default:
			assert(qfalse);
		}
	}

	Com_Error(ERR_FATAL, "MN_GetBooleanFromExpression: Unsupported expression type: %i", expression->type.op);
}

/**
 * @brief Check the if conditions for a given node
 * @return True if the condition is qfalse if the node is not drawn
 */
qboolean MN_CheckBooleanExpression (menuAction_t *expression, const menuNode_t *source)
{
	return MN_GetBooleanFromExpression(expression, source);
}

/**
 * @brief Translate the condition string to menuIfCondition_t enum value
 * @param[in] conditionString The string from scriptfiles (see if_strings)
 * @return menuIfCondition_t value
 * @return enum value for condition string
 * @note Produces a Sys_Error if conditionString was not found in if_strings array
 * @todo dichotomic search
 */
static int MN_GetOperatorByName (const char* operatorName)
{
	int i = 0;
	while (qtrue) {
		if (operatorList[i].operator == EA_NULL)
			break;
		if (!strcmp(operatorList[i].string, operatorName))
			return operatorList[i].operator;
		i++;
	}
	return EA_OPERATOR_INVALID;
}

/**
 * @brief Allocate and initialize an expression according to a string
 * @param[in] token String describing a condition
 * @param[out] condition Condition to initialize
 * @return The condition if everything is ok, NULL otherwise
 */
menuAction_t *MN_AllocStringCondition (const char *description)
{
	const char *errhead = "MN_AllocStringCondition: unexpected end of string (object";
	const char *text, *base;
	menuAction_t *expression;

	base = va("( %s )", description);
	text = base;
	expression = MN_ParseExpression(&text, errhead);
	if (!expression) {
		Com_Printf("MN_AllocStringCondition: Parse error on expression \"%s\"\n", base);
		return NULL;
	}

	return expression;
}

/**
 * @brief Read a value from the stream and init an action with it
 * @return An initialized action else NULL
 */
static menuAction_t *MN_ParseValueExpression (const char **text, const char *errhead)
{
	const char* token;
	menuAction_t *expression = MN_AllocAction();

	token = Com_Parse(text);
	if (*text == NULL) {
		Com_Printf("MN_ParseTerminalExpression: Token expected\n");
		return NULL;
	}

	/* it is a cvarname */
	if (!strncmp(token, "*cvar:", 6)) {
		const char* cvarName = token + 6;
		expression->d.terminal.d1.string = MN_AllocString(cvarName, 0);
		expression->type.op = EA_VALUE_CVARNAME;
		return expression;
	}

	/* it is a node property */
	if (!strncmp(token, "*node:", 6)) {
		const char* path = token + 6;
		expression->d.terminal.d1.string = MN_AllocString(path, 0);
		expression->type.op = EA_VALUE_NODEPROPERTY;
		return expression;
	}

#if 0 /* we can't know the operation before, and token is not typed. then we must copy the string */
/** @todo we can check the string type vs INT/FLOAT but it can create new problems
 * (convert the string "1" -> into a float, then then restitute the string "1.0") */
	/* it is a const float */
	if (MN_IsFloatOperator(opCode)) {
		float f = atof(string);
		expression->d.terminal.d1.number = f;
		expression->type.op = EA_VALUE_FLOAT;
		return expression;
	}
#endif

	/* it is a const string */
	expression->d.terminal.d1.string = MN_AllocString(token, 0);
	expression->type.op = EA_VALUE_STRING;
	return expression;
}

menuAction_t *MN_ParseExpression (const char **text, const char *errhead)
{
	const char* token;

	token = Com_Parse(text);
	if (*text == NULL)
		return NULL;

	if (!strcmp(token, "(")) {
		menuAction_t *expression;
		menuAction_t *e;

		e = MN_ParseExpression(text, errhead);

		token = Com_Parse(text);
		if (*text == NULL)
			return NULL;

		/* unary operator or unneed "( ... )" */
		if (!strcmp(token, ")")) {
			if (e->type.op == EA_VALUE_CVARNAME) {
				expression = MN_AllocAction();
				expression->type.op = EA_OPERATOR_EXISTS;
				expression->d.nonTerminal.left = e;
				return expression;
			} else
				return e;
		}

		/* then its an operator */
		expression = MN_AllocAction();
		expression->d.nonTerminal.left = e;
		expression->type.op = MN_GetOperatorByName(token);
		if (expression->type.op == EA_OPERATOR_INVALID) {
			Com_Printf("Invalid 'expression' statement. Unknown '%s' operator\n", token);
			return NULL;
		}

		e = MN_ParseExpression(text, errhead);
		expression->d.nonTerminal.right = e;

		token = Com_Parse(text);
		if (*text == NULL)
			return qfalse;
		if (strcmp(token, ")") != 0) {
			Com_Printf("MN_InitCondition: Token ')' expected\n");
			return qfalse;
		}

		return expression;
	} else {
		Com_UnParseLastToken();
		return MN_ParseValueExpression(text, errhead);
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
