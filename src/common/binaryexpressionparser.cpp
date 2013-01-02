/**
 * @file
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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

class BinaryExpressionParser {
private:
	binaryExpressionParserError_t binaryExpressionParserError;
	BEPEvaluteCallback_t varFunc;
	char varName[MAX_VAR];
	bool result;
	const void* userdata;

	inline void SkipWhiteSpaces (const char **s) const
	{
		while (**s == ' ')
			(*s)++;
	}

	inline void NextChar (const char **s) const
	{
		(*s)++;
		/* skip white-spaces too */
		SkipWhiteSpaces(s);
	}

	const char *GetSwitchName (const char **s)
	{
		int pos = 0;

		while (**s > 32 && **s != '^' && **s != '|' && **s != '&' && **s != '!' && **s != '(' && **s != ')') {
			varName[pos++] = **s;
			(*s)++;
		}
		varName[pos] = 0;

		return varName;
	}

	bool CheckOR (const char **s)
	{
		bool result = false;
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

	bool CheckAND (const char **s)
	{
		bool result = true;
		bool negate = false;
		bool goon = false;

		do {
			while (**s == '!') {
				negate ^= true;
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
				int value = varFunc(GetSwitchName(s), userdata);
				if (value == -1)
					binaryExpressionParserError = BEPERR_NOTFOUND;
				else
					result &= value ^ negate;
				SkipWhiteSpaces(s);
			}

			if (**s == '&') {
				goon = true;
				NextChar(s);
			} else {
				goon = false;
			}
			negate = false;
		} while (goon && !binaryExpressionParserError);

		return result;
	}

public:
	BinaryExpressionParser (const char *expr, BEPEvaluteCallback_t varFuncParam, const void* userdataPtr) :
		binaryExpressionParserError(BEPERR_NONE), varFunc(varFuncParam), userdata(userdataPtr)
	{
		const char *str = expr;
		result = CheckOR(&str);
		/* check for no end error */
		if (*str && !binaryExpressionParserError)
			binaryExpressionParserError = BEPERR_NOEND;
	}

	inline bool getResult() const
	{
		return result;
	}

	inline binaryExpressionParserError_t getError() const
	{
		return binaryExpressionParserError;
	}
};

bool BEP_Evaluate (const char *expr, BEPEvaluteCallback_t varFuncParam, const void* userdata)
{
	if (!Q_strvalid(expr))
		return true;

	BinaryExpressionParser bep(expr, varFuncParam, userdata);
	const bool result = bep.getResult();
	const binaryExpressionParserError_t error = bep.getError();

	switch (error) {
	case BEPERR_NONE:
		/* do nothing */
		return result;
	case BEPERR_BRACE:
		Com_Printf("')' expected in binary expression (%s).\n", expr);
		return true;
	case BEPERR_NOEND:
		Com_Printf("Unexpected end of condition in binary expression (%s).\n", expr);
		return result;
	case BEPERR_NOTFOUND:
		Com_Printf("Variable not found in binary expression (%s).\n", expr);
		return false;
	}
	Com_Error(ERR_FATAL, "Unknown CheckBEP error in binary expression (%s)", expr);
}
