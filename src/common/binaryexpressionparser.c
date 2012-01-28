/**
 * @file binaryexpressionparser.c
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "common.h"
#include "binaryexpressionparser.h"

typedef enum
{
	BEPERR_NONE, BEPERR_BRACE, BEPERR_NOEND, BEPERR_NOTFOUND
} binaryExpressionParserError_t;

static binaryExpressionParserError_t binaryExpressionParserError;
static BEPEvaluteCallback_t varFunc;

static qboolean CheckAND (const char **s);

static void SkipWhiteSpaces (const char **s)
{
	while (**s == ' ')
		(*s)++;
}

static void NextChar (const char **s)
{
	(*s)++;
	/* skip white-spaces too */
	SkipWhiteSpaces(s);
}

static const char *GetSwitchName (const char **s)
{
	static char varName[MAX_VAR];
	int pos = 0;

	while (**s > 32 && **s != '^' && **s != '|' && **s != '&' && **s != '!' && **s != '(' && **s != ')') {
		varName[pos++] = **s;
		(*s)++;
	}
	varName[pos] = 0;

	return varName;
}

static qboolean CheckOR (const char **s)
{
	qboolean result = qfalse;
	int goon = 0;

	SkipWhiteSpaces(s);
	do {
		if (goon == 2)
			result ^= CheckAND(s);
		else
			result |= CheckAND(s);

		if (**s == '|') {
			goon = 1;
			NextChar(s);
		} else if (**s == '^') {
			goon = 2;
			NextChar(s);
		} else {
			goon = 0;
		}
	} while (goon && !binaryExpressionParserError);

	return result;
}

static qboolean CheckAND (const char **s)
{
	qboolean result = qtrue;
	qboolean negate = qfalse;
	qboolean goon = qfalse;
	int value;

	do {
		while (**s == '!') {
			negate ^= qtrue;
			NextChar(s);
		}
		if (**s == '(') {
			NextChar(s);
			result &= CheckOR(s) ^ negate;
			if (**s != ')')
				binaryExpressionParserError = BEPERR_BRACE;
			NextChar(s);
		} else {
			/* get the variable state */
			value = varFunc(GetSwitchName(s));
			if (value == -1)
				binaryExpressionParserError = BEPERR_NOTFOUND;
			else
				result &= value ^ negate;
			SkipWhiteSpaces(s);
		}

		if (**s == '&') {
			goon = qtrue;
			NextChar(s);
		} else {
			goon = qfalse;
		}
		negate = qfalse;
	} while (goon && !binaryExpressionParserError);

	return result;
}

qboolean BEP_Evaluate (const char *expr, BEPEvaluteCallback_t varFuncParam)
{
	qboolean result;
	const char *str;

	binaryExpressionParserError = BEPERR_NONE;
	varFunc = varFuncParam;
	str = expr;
	result = CheckOR(&str);

	// check for no end error
	if (*str && !binaryExpressionParserError)
		binaryExpressionParserError = BEPERR_NOEND;

	switch (binaryExpressionParserError) {
	case BEPERR_NONE:
		/* do nothing */
		return result;
	case BEPERR_BRACE:
		Com_Printf("')' expected in binary expression (%s).\n", expr);
		return qtrue;
	case BEPERR_NOEND:
		Com_Printf("Unexpected end of condition in binary expression (%s).\n", expr);
		return result;
	case BEPERR_NOTFOUND:
		Com_Printf("Variable not found in binary expression (%s).\n", expr);
		return qfalse;
	}
	Com_Error(ERR_FATAL, "Unknown CheckBEP error in binary expression (%s)", expr);
}
