/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "binaryexpressionparser.h"
#include "common.h"

typedef enum
{
	BEPERR_NONE, BEPERR_BRACE, BEPERR_NOEND, BEPERR_NOTFOUND
} binaryExpressionParserError_t;

/**
 * @brief Evaluates stuff like this expression <pre>(x & !y) ^ z</pre>
 */
class BinaryExpressionParser {
private:
	binaryExpressionParserError_t binaryExpressionParserError;
	BEPEvaluteCallback_t varFunc;
	char varName[MAX_VAR];
	bool result;
	const void* userdata;

	inline void SkipWhiteSpaces (const char** s) const
	{
		while (**s == ' ' || **s == '\t')
			(*s)++;
	}

	/**
	 * @brief Advance to the next char that is no whitespace
	 */
	inline void NextChar (const char** s) const
	{
		(*s)++;
		/* skip white-spaces too */
		SkipWhiteSpaces(s);
	}

	const char* GetSwitchName (const char** s)
	{
		int pos = 0;

		/* skip non printable chars and special chars that are used to define the binary expression */
		while (**s > ' ' && **s != '^' && **s != '|' && **s != '&' && **s != '!' && **s != '(' && **s != ')') {
			varName[pos++] = **s;
			(*s)++;
		}
		varName[pos] = '\0';

		return varName;
	}

	/**
	 * @brief Evaluates or and xor in the given string. This is the entry point, it delegates to the and checks
	 * that are done in @c CheckAnd
	 * @return The result of the evaluation
	 */
	bool CheckOR (const char** s)
	{
		bool result = false;
		enum {
			BEP_NONE, BEP_OR, BEP_EXCLUSIVE_OR
		};
		int goOn = BEP_NONE;

		SkipWhiteSpaces(s);
		do {
			if (goOn == BEP_EXCLUSIVE_OR)
				result ^= CheckAND(s);
			else
				result |= CheckAND(s);

			if (**s == '|') {
				goOn = BEP_OR;
				NextChar(s);
			} else if (**s == '^') {
				goOn = BEP_EXCLUSIVE_OR;
				NextChar(s);
			} else {
				goOn = BEP_NONE;
			}
		} while (goOn != BEP_NONE && !binaryExpressionParserError);

		return result;
	}

	bool CheckAND (const char** s)
	{
		bool result = true;
		bool negate = false;
		bool goOn = false;

		do {
			/* parse all negate chars and swap the flag accordingly */
			while (**s == '!') {
				negate ^= true;
				NextChar(s);
			}
			/* handling braces */
			if (**s == '(') {
				NextChar(s);
				/* and the result of the inner clause by calling the entry
				 * point again (and apply the negate flag) */
				result &= CheckOR(s) ^ negate;
				if (**s != ')')
					binaryExpressionParserError = BEPERR_BRACE;
				NextChar(s);
			} else {
				/* get the variable state by calling the evaluate callback */
				const int value = varFunc(GetSwitchName(s), userdata);
				if (value == -1)
					binaryExpressionParserError = BEPERR_NOTFOUND;
				else
					result &= value ^ negate;
				SkipWhiteSpaces(s);
			}

			/* check whether there is another and clause */
			if (**s == '&') {
				goOn = true;
				NextChar(s);
			} else {
				goOn = false;
			}
			negate = false;
		} while (goOn && !binaryExpressionParserError);

		return result;
	}

public:
	BinaryExpressionParser (const char* expr, BEPEvaluteCallback_t varFuncParam, const void* userdataPtr) :
		binaryExpressionParserError(BEPERR_NONE), varFunc(varFuncParam), userdata(userdataPtr)
	{
		varName[0] = 0;
		const char* str = expr;
		result = CheckOR(&str);
		/* check for no end error */
		if (Q_strvalid(str) && !binaryExpressionParserError)
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

bool BEP_Evaluate (const char* expr, BEPEvaluteCallback_t varFuncParam, const void* userdata)
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
