/**
 * @file entitiesdef.c
 * @brief Handles definition of entities, parsing them from entities.ufo
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Copyright (C) 1997-2001 Id Software, Inc.

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

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>

#include "shared.h"		/* needed for strdup() */
#include "parse.h"
#include "entitiesdef.h"

#define ED_MAX_KEYS_PER_ENT 32
#define ED_MAX_TOKEN_LEN 512
#define ED_MAX_ERR_LEN 512

static char lastErr[ED_MAX_ERR_LEN]; /**< for storing last error message */
static char lastErrExtra[ED_MAX_ERR_LEN]; /**< temporary storage for extra information to be added to lastErr */

int numEntityDefs;
entityDef_t entityDefs[ED_MAX_DEFS + 1];

/**
 * @brief write an error message and exit the current function returning ED_ERROR
 * @sa ED_TEST_RETURN_ERROR
 */
#define ED_RETURN_ERROR(...) \
	{ \
		snprintf(lastErr, sizeof(lastErr), __VA_ARGS__); \
		return ED_ERROR; \
	}

/**
 * @brief test a condition, write an error
 * message and exit the current function with ED_ERROR
 */
#define ED_TEST_RETURN_ERROR(condition,...) \
	if (condition) { \
		snprintf(lastErr, sizeof(lastErr), __VA_ARGS__); \
		return ED_ERROR; \
	}

/**
 * @brief if the function returns ED_ERROR, then the function that the
 * macro is in also returns ED_ERROR. Note that the called function is expected
 * to set lastErr, probably by using the ED_TEST_RETURN_ERROR macro
 */
#define ED_PASS_ERROR(function_call) \
	if ((function_call) == ED_ERROR) { \
		return ED_ERROR; \
	}

/**
 * @brief if the function returns ED_ERROR, then the function that the
 * macro is in also returns ED_ERROR. Note that the called function is expected
 * to set lastErr, probably by using the ED_TEST_RETURN_ERROR macro. this macro then
 * appends extra detail to the message to give context
 */
#define ED_PASS_ERROR_EXTRAMSG(function_call,...) \
	if ((function_call) == ED_ERROR) { \
		snprintf(lastErrExtra, sizeof(lastErr), __VA_ARGS__); \
		strncat(lastErr, lastErrExtra, sizeof(lastErr) - strlen(lastErr) -1); \
		return ED_ERROR; \
	}

/**
 * @brief allocate space for key defs etc, pointers for which are stored in the entityDef_t
 * @return ED_ERROR or ED_OK.
 */
static int ED_AllocEntityDef (entityKeyDef_t *newKeyDefs, int numKeyDefs, int entityIndex)
{
	entityDef_t *eDef = &entityDefs[entityIndex];

	/* now we know how many there are in this entity, malloc */
	eDef->keyDefs = (entityKeyDef_t *) malloc((numKeyDefs + 1) * sizeof(entityKeyDef_t));
	ED_TEST_RETURN_ERROR(!eDef->keyDefs, "ED_AllocEntityDef: out of memory");
	eDef->numKeyDefs = numKeyDefs;

	/* and copy from temp buffer */
	memcpy(eDef->keyDefs, newKeyDefs, numKeyDefs * sizeof(entityKeyDef_t));

	/* set NULLs at the end, to enable looping through using a pointer */
	OBJZERO(eDef->keyDefs[numKeyDefs]);

	/* classname is commonly used, put it in its own field, copied from keyDefs[0] */
	eDef->classname = strdup(eDef->keyDefs->desc);
	ED_TEST_RETURN_ERROR(!eDef->classname, "ED_AllocEntityDef: out of memory");

	return ED_OK;
}

/**
 * @brief search for an existing keyDef to add a new parsed pair info to.
 * @return a pointer to the entity def or NULL if it is not found
 */
static entityKeyDef_t *ED_FindKeyDefInArray (entityKeyDef_t keyDefs[], int numDefs, const char *name, int parseMode)
{
	int i;
	for (i = 0; i < numDefs; i++) {
		const entityKeyDef_t *keyDef = &keyDefs[i];
		/* names equal. both abstract or both not abstract */
		if (Q_streq(keyDef->name, name) && !((keyDef->flags ^ parseMode) & ED_ABSTRACT)) {
			return &keyDefs[i];
		}
	}
	return NULL;
}

/**
 * @brief converts a string representation of a type (eg V_FLOAT) to
 * the appropriate internal constant integer
 * @return the constant, or ED_ERROR if strType is not recognised
 */
static int ED_Type2Constant (const char *strType)
{
	if (Q_streq(strType, "V_FLOAT"))
		return ED_TYPE_FLOAT;
	else if (Q_streq(strType, "V_INT"))
		return ED_TYPE_INT;
	else if (Q_streq(strType, "V_STRING"))
		return ED_TYPE_STRING;

	ED_RETURN_ERROR("ED_Type2Constant: type string not recognised: \"%s\"", strType);
}

/**
 * @brief converts an internal constant integer to a string
 * representation of a type (eg V_FLOAT)
 * @return the string, or NULL if the integer is not recognised.
 */
static const char *ED_Constant2Type (int constInt)
{
	switch (constInt) {
	case ED_TYPE_FLOAT:
		return "V_FLOAT";
	case ED_TYPE_INT:
		return "V_INT";
	case ED_TYPE_STRING:
		return "V_STRING";
	default:
		snprintf(lastErr, sizeof(lastErr), "ED_Constant2Type: constant not recognised");
		return NULL;
	}
}

/**
 * @brief parses an int array from a string
 * @param str the string to parse
 * @param[out] v the array of int to put the answer in
 * it must have enough space for n elements.
 * @param n the number of elements expected in the vector
 * @return ED_ERROR or ED_OK
 * @sa ED_GetLastError.
 */
static int ED_GetIntVectorFromString (const char *str, int v[], const int n)
{
	int i;
	const char *buf_p = str;

	for (i = 0; buf_p; i++) {
		const char *tok = Com_Parse(&buf_p);
		if (tok[0] == '\0')
			break; /* previous tok was the last real one, don't waste time */
		ED_TEST_RETURN_ERROR(i >= n, "ED_GetIntVectorFromString: v[%i] too small for ints from string \"%s\"", n, str);
		v[i] = atoi(tok);
	}
	ED_TEST_RETURN_ERROR(i != n, "ED_GetIntVectorFromString: v[%i] wrong size for ints from string \"%s\"", n, str);
	return ED_OK;
}

/**
 * @brief parses an float array from a string
 * @param str the string to parse
 * @param[out] v the array of float to put the answer in
 * it must have enough space for n elements.
 * @param n the number of elements expected in the vector
 * @return ED_ERROR or ED_OK
 * @sa ED_GetLastError.
 */
static int ED_GetFloatVectorFromString (const char *str, float v[], const int n)
{
	int i;
	const char *buf_p = str;

	for (i = 0; buf_p; i++) {
		const char *tok = Com_Parse(&buf_p);
		if (tok[0] == '\0')
			break; /* previous tok was the last real one, don't waste time */
		ED_TEST_RETURN_ERROR(i >= n, "ED_GetFloatVectorFromString: v[%i] too small for floats from string \"%s\"", n, str);
		v[i] = atof(tok);
	}
	ED_TEST_RETURN_ERROR(i != n, "ED_GetFloatVectorFromString: v[%i] wrong size for floats from string \"%s\"", n, str);
	return ED_OK;
}

/**
 * @brief gets the default value for the key
 * @param[in] defaultBuf the calling function is responsible for ensuring this buffer
 * has enough space
 * @param n the number of floats the defaultBuf can hold
 * @param kd the key definition to get the default for.
 * @sa ED_GetKeyDef can be used to find kd.
 * @return ED_OK or ED_ERROR (no default, or n is too small).
 * @note the defualt int array is tested on parsing
 */
int ED_GetDefaultFloat (float *defaultBuf, const int n, const entityKeyDef_t *kd)
{
	ED_TEST_RETURN_ERROR(!kd->defaultVal, "ED_GetDefaultFloat: no default available");
	ED_TEST_RETURN_ERROR(!(kd->flags & ED_TYPE_FLOAT), "ED_GetDefaultFloat: key does not define V_FLOAT");
	ED_PASS_ERROR(ED_GetFloatVectorFromString(kd->defaultVal, defaultBuf, n));
	return ED_OK;
}

/**
 * @brief gets the default value for the key
 * @param[in] defaultBuf the calling function is responsible for ensuring this buffer
 * has enough space
 * @param n the number of ints the defaultBuf can hold
 * @param kd the key definition to get the default for.
 * @sa ED_GetKeyDef can be used to find kd.
 * @return ED_OK or ED_ERROR (no default, or n is too small).
 * @note the defualt int array is tested on parsing
 */
int ED_GetDefaultInt (int *defaultBuf, const int n, const entityKeyDef_t *kd)
{
	ED_TEST_RETURN_ERROR(!kd->defaultVal, "ED_GetDefaultInt: no default available");
	ED_TEST_RETURN_ERROR(!(kd->flags & ED_TYPE_INT), "ED_GetDefaultInt: key does not define V_INT");
	ED_PASS_ERROR(ED_GetIntVectorFromString(kd->defaultVal, defaultBuf, n));
	return ED_OK;
}

/**
 * @brief parses a value from the definition
 * @param kd the key definition to parse from
 * @param[out] v the array of int to put the answer in
 * it must have enough space for n elements.
 * @param n the number of elements expected in the vector
 * @return ED_ERROR or ED_OK
 * @sa ED_GetLastError.
 */
int ED_GetIntVector (const entityKeyDef_t *kd, int v[], const int n)
{
	ED_PASS_ERROR(ED_GetIntVectorFromString(kd->desc, v, n));
	return ED_OK;
}

/**
 * @brief checks that a string represents a single number
 * @param floatOrInt one of ED_TYPE_FLOAT or ED_TYPE_INT
 * @param insistPositive if 1, then tests for the number being greater than or equal to zero.
 * @sa ED_CheckNumericType
 * @note disallows hex, inf, NaN, numbers with junk on the end (eg -0123junk)
 * @return ED_OK or ED_ERROR
 * @sa ED_GetLastError
 * @note the parsed numbers are stored for later use in lastCheckedInt and lastCheckedFloat
 */
static int ED_CheckNumber (const char *value, const int floatOrInt, const int insistPositive, int_float_tu *parsedNumber)
{
	char *end_p;
	/* V_INTs are protected from octal and hex as strtol with base 10 is used.
	 * this test is useful for V_INT, as it gives a specific error message.
	 * V_FLOATs are not protected from hex, inf, nan, so this check is here.
	 * strstr is used for hex, as 0x may not be the start of the string.
	 * eg -0x0.2 is a negative hex float  */
	ED_TEST_RETURN_ERROR(value[0] == 'i' || value[0] == 'I' || value[0] == 'n' || value[0] == 'N'
		|| strstr(value, "0x") || strstr(value, "0X"),
		"infinity, NaN, hex (0x...) not allowed. found \"%s\"", value);
	switch (floatOrInt) {
	case ED_TYPE_FLOAT:
		parsedNumber->f = strtof(value, &end_p);
		ED_TEST_RETURN_ERROR(insistPositive && parsedNumber->f < 0.0f, "ED_CheckNumber: not positive %s", value);
		break;
	case ED_TYPE_INT:
		parsedNumber->i = (int)strtol(value, &end_p, 10);
		ED_TEST_RETURN_ERROR(insistPositive && parsedNumber->i < 0, "ED_CheckNumber: not positive %s", value);
		break;
	default:
		ED_RETURN_ERROR("ED_CheckNumber: type to test against not recognised");
	}
	/* if strto* did not use the whole token, then there is some non-number part to it */
	ED_TEST_RETURN_ERROR(strlen(value) != (unsigned int)(end_p-value),
		"problem with numeric value: \"%s\" declared as %s. (might be relevant: only use whitespace to delimit values)",
		value, ED_Constant2Type(floatOrInt));
	return ED_OK;
}

/**
 * @brief check a value against the range for the key
 * @param floatOrInt either ED_TYPE_FLOAT or ED_TYPE_INT
 * @param index the index of the number being checked in the value. eg angles "90 180", 90 is at 0, 180 is at 1.
 * @note checks lastCheckedInt or lastCheckedFloat against the range in the supplied keyDef.
 * @return ED_ERROR or ED_OK
 */
static int ED_CheckRange (const entityKeyDef_t *keyDef, const int floatOrInt, const int index, int_float_tu parsedNumber)
{
	/* there may be a range for each element of the value, or there may only be one */
	const int useRangeIndex = (keyDef->numRanges == 1) ? 0 : index;
	entityKeyRange_t *kr;
	const float discreteFloatEpsilon = 0.0001f;
	if (0 == keyDef->numRanges)
		return ED_OK; /* if no range defined, that is OK */
	assert(useRangeIndex < keyDef->numRanges);
	kr = keyDef->ranges[useRangeIndex];
	switch (floatOrInt) {
	case ED_TYPE_FLOAT:
		if (kr->continuous) {
			ED_TEST_RETURN_ERROR(parsedNumber.f < kr->fArr[0] || parsedNumber.f > kr->fArr[1],
				"ED_CheckRange: %.1f out of range, \"%s\" in %s",
				parsedNumber.f, kr->str, keyDef->name);
			return ED_OK;
		} else {
			int j;
			for (j = 0; j < kr->numElements; j++)
				if (fabs(parsedNumber.f - kr->fArr[j]) < discreteFloatEpsilon)
					return ED_OK;
		}
		break;
	case ED_TYPE_INT:
		if (kr->continuous) {
			ED_TEST_RETURN_ERROR(parsedNumber.i < kr->iArr[0] || parsedNumber.i > kr->iArr[1],
				"ED_CheckRange: %i out of range, \"%s\" in %s",
				parsedNumber.i, kr->str, keyDef->name);
			return ED_OK;
		} else {
			int j;
			for (j = 0; j < kr->numElements; j++)
				if (kr->iArr[j] == parsedNumber.i)
					return ED_OK;
		}
		break;
	default:
		ED_RETURN_ERROR( "ED_CheckRange: type to test against not recognised in %s", keyDef->name);
	}
	ED_RETURN_ERROR("ED_CheckRange: value not specified in range definition, \"%s\" in %s",
		kr->str, keyDef->name);
}

/**
 * @brief tests if a value string matches the type for this key. this includes
 * each element of a numeric array. Also checks value against range def, if one exists.
 * @param floatOrInt one of ED_TYPE_FLOAT or ED_TYPE_INT
 * @return ED_OK or ED_ERROR (call ED_GetLastError)
 */
static int ED_CheckNumericType (const entityKeyDef_t *keyDef, const char *value, const int floatOrInt)
{
	int i = 0;
	static char tokBuf[64];
	const char *buf_p = tokBuf;

	strncpy(tokBuf, value, sizeof(tokBuf));
	assert(!(floatOrInt & ED_TYPE_INT) != !(floatOrInt & ED_TYPE_FLOAT));/* logical exclusive or */
	while (buf_p) {
		const char *tok = Com_Parse(&buf_p);
		int_float_tu parsedNumber;
		if (tok[0] == '\0')
			break; /* previous tok was the last real one, don't waste time */

		ED_PASS_ERROR_EXTRAMSG(ED_CheckNumber(tok, floatOrInt, keyDef->flags & ED_INSIST_POSITIVE, &parsedNumber),
			" in key \"%s\"", keyDef->name);

		ED_PASS_ERROR(ED_CheckRange(keyDef, floatOrInt, i, parsedNumber));

		i++;
	}

	ED_TEST_RETURN_ERROR(i != keyDef->vLen, "ED_CheckNumericType: %i elements in vector that should have %i for \"%s\" key",
			i, keyDef->vLen, keyDef->name);

	return ED_OK;
}

/**
 * @brief tests if a value string matches the type for this key. Also checks the value against the
 * range, if one was defined.
 * @return ED_OK or ED_ERROR
 * @sa ED_GetLastError
 * @note abstract (radiant) keys may not have types. keys used here must be declared in entities.ufo in
 * an optional or mandatory block.
 */
int ED_Check (const char *classname, const char *key, const char *value)
{
	const entityKeyDef_t *kd = ED_GetKeyDef(classname, key, 0);
	if (!kd)
		return ED_ERROR;

	return ED_CheckKey(kd, value);
}

/**
 * @brief as ED_Check, but where the entity and key are known, so takes
 * different arguments.
 * @return ED_ERROR or ED_OK
 * @sa ED_GetLastError
 */
int ED_CheckKey (const entityKeyDef_t *kd, const char *value)
{
	ED_TEST_RETURN_ERROR(!kd, "ED_CheckTypeEntityKey: null key def");
	switch (kd->flags & ED_KEY_TYPE) {
	case ED_TYPE_FLOAT:
		return ED_CheckNumericType(kd, value, ED_TYPE_FLOAT);
	case ED_TYPE_INT:
		return ED_CheckNumericType(kd, value, ED_TYPE_INT);
	case ED_TYPE_STRING:
	case 0: /* string is the default */
		return ED_OK; /* all strings are good */
	default:
		ED_RETURN_ERROR("ED_CheckTypeEntityKey: type not recognised in key def");
	}
}

/**
 * @brief gets the default value for the key
 * @param[in] defaultBuf the calling function is responsible for ensuring this buffer
 * has enough space
 * @param n the number of chars the defualtBuf can hold
 * @param kd the key definition to get the default for. E
 * @sa D_GetKeyDef can be used to find kd.
 * @return ED_OK or ED_ERROR (no default, or n is too small).
 */
int ED_GetDefaultString (char *defaultBuf, const size_t n, const entityKeyDef_t *kd)
{
	ED_TEST_RETURN_ERROR(n < 1, "ED_GetDefaultString: n is %lu", (unsigned long)n);
	defaultBuf[0] = '\0'; /* be safe, in case an error is found, and we return early */
	ED_TEST_RETURN_ERROR(!kd->defaultVal, "ED_GetDefaultString: no default available");
	strncpy(defaultBuf, kd->defaultVal, n - 1);
	ED_TEST_RETURN_ERROR(strlen(defaultBuf) < strlen(kd->defaultVal),
		"ED_GetDefaultString: target buffer supplied is too small");
	return ED_OK;
}

/**
 * @brief takes a type string (eg "V_FLOAT 6") and configures entity def
 * @return ED_ERROR or ED_OK
 */
static int ED_ParseType (entityKeyDef_t *kd, const char *parsedToken)
{
	static char tokBuf[64];
	const char *buf_p;
	int type;
	int vectorLen;
	const char *partToken;
	int_float_tu parsedNumber;

	/* need a copy, as parsedToken is held in a static buffer in the
	 * Com_Parse function */
	ED_TEST_RETURN_ERROR((strlen(parsedToken) + 1) > sizeof(tokBuf),
		"ED_ParseType: type string too long for buffer for key %s", kd->name);
	strncpy(tokBuf, parsedToken, sizeof(tokBuf));
	tokBuf[sizeof(tokBuf) - 1] = '\0';
	buf_p = tokBuf;

	partToken = Com_Parse(&buf_p);

	if (Q_streq("SIGNED", partToken)) {
		partToken = Com_Parse(&buf_p);/* get next token */
	} else if (Q_streq("UNSIGNED", partToken)) {
		kd->flags |= ED_INSIST_POSITIVE;
		partToken = Com_Parse(&buf_p);
	}

	if (partToken[0] != '\0') {
		type = ED_Type2Constant(partToken);
		ED_PASS_ERROR(type);
	} else {/* default is string */
		type = ED_TYPE_STRING;
	}

	kd->flags |= type;
	partToken = Com_Parse(&buf_p);
	vectorLen = atoi(partToken);
	if (vectorLen)
		ED_TEST_RETURN_ERROR(ED_ERROR == ED_CheckNumber(partToken, ED_TYPE_INT, 1, &parsedNumber),
			"ED_ParseType: problem with vector length \"%s\" in key %s",
			partToken, kd->name);
	kd->vLen = strlen(partToken) ? (vectorLen ? vectorLen : 1) : 1; /* default is 1 */
	return ED_OK;
}

/**
 * @brief converts a block name (eg "optional") to an constant (eg ED_OPTIONAL).
 * @return the parse mode or ED_ERROR
 */
static int ED_Block2Constant (const char *blockName)
{
	if (Q_streq("optional", blockName))
		return ED_OPTIONAL;
	else if (Q_streq("mandatory", blockName))
		return ED_MANDATORY;
	else if (Q_streq("abstract", blockName))
		return ED_ABSTRACT;
	else if (Q_streq("default", blockName))
		return ED_DEFAULT;
	else if (Q_streq("type", blockName))
		return ED_MODE_TYPE;
	else if (Q_streq("range", blockName))
		return ED_RANGE;
	else
		ED_RETURN_ERROR("parse mode not recognised");
}

/**
 * @brief converts an internal constant integer to a string
 * representation of a type (eg V_FLOAT)
 * @return the string, or NULL if the integer is not recognised.
 */
static const char *ED_Constant2Block (int constInt)
{
	switch (constInt) {
	case ED_OPTIONAL:
		return "optional";
	case ED_MANDATORY:
		return "mandatory";
	case ED_ABSTRACT:
		return "abstract";
	case ED_DEFAULT:
		return "default";
	case ED_MODE_TYPE:
		return "type";
	case ED_RANGE:
		return "range";
	default:
		snprintf(lastErr, sizeof(lastErr), "ED_Constant2Block: constant not recognised");
		return NULL;
	}
}

static int ED_AllocRange (entityKeyDef_t *kd, const char *rangeStr)
{
	entityKeyRange_t **newRanges;
	/* start a new range */
	char *newStr = strdup(rangeStr);
	entityKeyRange_t *newRange = (entityKeyRange_t *)malloc(sizeof(entityKeyRange_t));
	OBJZERO(*newRange);
	/* resize array of pointers */
	newRanges = (entityKeyRange_t **)malloc((kd->numRanges + 1) * sizeof(entityKeyRange_t *));
	ED_TEST_RETURN_ERROR(!newRanges || !newStr || !newRange, "ED_AllocRange: out of memory");
	newRange->str = newStr;
	newRanges[kd->numRanges] = newRange;
	if (kd->ranges) {
		memcpy(newRanges, kd->ranges, kd->numRanges * sizeof(entityKeyRange_t *));
		free(kd->ranges);
	}
	kd->numRanges++;
	kd->ranges = newRanges;
	return ED_OK;
}

/**
 * @return ED_ERROR or ED_OK.
 */
static int ED_PairParsed (entityKeyDef_t keyDefsBuf[], int *numKeyDefsSoFar_p,
		const char *newName, const char *newVal, const int mode)
{
	/* check if there is already a key def */
	entityKeyDef_t *keyDef = ED_FindKeyDefInArray(keyDefsBuf, *numKeyDefsSoFar_p, newName, mode);

	/* create one if required */
	if (!keyDef) {
		keyDef = &keyDefsBuf[(*numKeyDefsSoFar_p)++];
		ED_TEST_RETURN_ERROR(*numKeyDefsSoFar_p >= ED_MAX_KEYS_PER_ENT, "ED_PairParsed: too many keys for buffer");
		keyDef->name = strdup(newName);
		ED_TEST_RETURN_ERROR(!keyDef->name, "ED_PairParsed: out of memory");
	}

	/* multiple range defs are permitted, different elements can have different ranges */
	ED_TEST_RETURN_ERROR(keyDef->flags & mode & ~ED_RANGE,
		"Duplicate %s for %s key. second value: %s", ED_Constant2Block(mode), newName, newVal);

	keyDef->flags |= mode;

	/* store information */
	switch (mode) {
	case ED_MANDATORY:
	case ED_OPTIONAL:
	case ED_ABSTRACT: /* intentional fall-through */
		keyDef->desc = strdup(newVal);
		ED_TEST_RETURN_ERROR(!keyDef->desc, "ED_PairParsed: out of memory while storing string.");
		return ED_OK;
	case ED_DEFAULT:
		keyDef->defaultVal = strdup(newVal);
		ED_TEST_RETURN_ERROR(!keyDef->defaultVal, "ED_PairParsed: out of memory while storing string.");
		return ED_OK;
	case ED_MODE_TYPE:
		/* only optional or mandatory keys may have types, not possible to test for this here,
		 * as the type block may come before the optional or mandatory block */
		ED_PASS_ERROR(ED_ParseType(keyDef, newVal));
		return ED_OK;
	case ED_RANGE:
		/* only typed keys may have ranges, this may only be tested after
		 * the whole ent has been parsed: the blocks may come in any order */
		ED_PASS_ERROR(ED_AllocRange(keyDef, newVal));
		return ED_OK;
	default:
		ED_RETURN_ERROR("ED_PairParsed: parse mode not recognised");
	}
}

/**
 * @return ED_ERROR or ED_OK
 */
static int ED_ParseEntities (const char **data_p)
{
	int braceLevel = 0;
	int tokensOnLevel0 = 0;
	int mode = ED_ABSTRACT;
	entityKeyDef_t keyDefBuf[ED_MAX_KEYS_PER_ENT];
	char lastTokenBuf[ED_MAX_TOKEN_LEN];
	int keyIndex = 0;
	int toggle = 0; /* many lines should have a pair of tokens on, this toggles 0, 1 to indicate progress */

	while (data_p) {
		const char *parsedToken = Com_Parse(data_p);
		toggle ^= 1;

		if (parsedToken[0] == '\0' && braceLevel == 0)
			break;

		if (parsedToken[0] == '{') {
			braceLevel++;
			ED_TEST_RETURN_ERROR(braceLevel > 2, "Too many open braces, nested %i deep", braceLevel);
			ED_TEST_RETURN_ERROR(!toggle, "ED_ParseEntities: Incorrect number of tokens before '{'");
			toggle ^= 1; /* reset, as toggle is only for counting proper text tokens, not braces */
			tokensOnLevel0 = 0;
			continue;
		}

		if (parsedToken[0] == '}') {
			braceLevel--;
			ED_TEST_RETURN_ERROR(braceLevel < 0, "Too many close braces. after %i entities", numEntityDefs);
			toggle ^= 1; /* reset, as toggle is only for counting proper text tokens, not braces */
			if (braceLevel == 0) { /* finished parsing entity def and prepare for the next one */
				ED_PASS_ERROR(ED_AllocEntityDef(keyDefBuf, keyIndex, numEntityDefs));
				numEntityDefs++;
				ED_TEST_RETURN_ERROR(numEntityDefs >= ED_MAX_DEFS, "ED_ParseEntities: too many entity defs for buffer");
			}
			if (braceLevel == 1) /* ending a default, mandatory, etc block, go back to default parse mode */
				mode = ED_ABSTRACT;

			continue;
		}

		if (braceLevel == 0) {
			if (tokensOnLevel0 == 0 && !Q_streq(parsedToken, "entity"))
				ED_RETURN_ERROR( "Next entity expected, found \"%s\"", parsedToken);

			if (tokensOnLevel0 == 1) {/* classname of entity, start parsing new entity */
				const entityDef_t *prevED = ED_GetEntityDef(parsedToken);
				ED_TEST_RETURN_ERROR(prevED, "ED_ParseEntities: duplicate entity definition \"%s\"", parsedToken);
				OBJZERO(keyDefBuf); /* ensure pointers are not carried over from previous entity */
				keyIndex = 0;
				ED_PASS_ERROR(ED_PairParsed(keyDefBuf, &keyIndex, "classname", parsedToken, ED_MANDATORY));
				mode = ED_ABSTRACT;
			}

			if (tokensOnLevel0 > 1)
				ED_RETURN_ERROR( "Start of entity block expected found \"%s\"", parsedToken);

			tokensOnLevel0++;
		} else { /* braceLevel > 0 */
			const int newMode = ED_Block2Constant(parsedToken);
			if (ED_ERROR == newMode) { /* must be a key name or value */
				if (toggle) { /* store key name til after next token is parsed */
					if ('\0' == parsedToken[0])
						ED_RETURN_ERROR("key name null string, \"\", or missing closing brace");
					strncpy(lastTokenBuf, parsedToken, sizeof(lastTokenBuf));
					lastTokenBuf[sizeof(lastTokenBuf) - 1] = '\0';
				} else { /* store key-value pair in buffer until whole entity is parsed */
					ED_PASS_ERROR(ED_PairParsed(keyDefBuf, &keyIndex, lastTokenBuf, parsedToken, mode));
				}
			} else {
				mode = newMode;
				toggle ^= 1; /* start of a mode changing block is the only time that only one non-brace token is on a line */
			}
		}
	}

	return ED_OK;
}

/**
 * @brief checks if the default block entries meet the type and range definitions.
 * @return ED_ERROR or ED_OK
 * @sa CheckLastError
 */
static int ED_CheckDefaultTypes (void)
{
	const entityDef_t *ed;
	const entityKeyDef_t *kd;
	for (ed = entityDefs; ed->numKeyDefs; ed++)
		for (kd = ed->keyDefs; kd->name; kd++)
			if (kd->defaultVal)
				ED_PASS_ERROR_EXTRAMSG(ED_CheckKey(kd, kd->defaultVal),
					" while checking default block entry agrees with type")

	return ED_OK;
}

/**
 * @brief finish parsing ranges. Could not be done earlier as would not have necessarily known
 * types and defaults. parses values in ranges into ints or floats and tests ranges against types
 * and defaults against ranges.
 * @return ED_ERROR or ED_OK
 */
static int ED_ProcessRanges (void)
{
	static int ibuf[32];
	static float fbuf[32];

	entityDef_t *ed;
	for (ed = entityDefs; ed->numKeyDefs; ed++) {
		entityKeyDef_t *kd;
		for (kd = ed->keyDefs; kd->name; kd++) {
			const int keyType = kd->flags & ED_KEY_TYPE;
			int i;
			for (i = 0; i < kd->numRanges ;i++) {
				int numElements = 0;
				entityKeyRange_t *kr = kd->ranges[i];
				const char *tmpRange_p = kr->str;
				ED_TEST_RETURN_ERROR(!keyType || (keyType & ED_TYPE_STRING), "ED_ProcessRanges: ranges may not be specified for strings. note that V_STRING is the default type. %s in %s",
					kd->name, ed->classname);
				while (tmpRange_p) {
					int_float_tu parsedNumber;
					const char *tok = Com_Parse(&tmpRange_p);
					if (tok[0] == '\0')
						break;
					if (Q_streq("-", tok)) {
						kr->continuous = 1;
						ED_TEST_RETURN_ERROR(numElements != 1, "ED_ProcessRanges: problem with continuous range, \"%s\" in %s in %s",
							kr->str, kd->name, ed->classname);
						continue;
					}
					ED_PASS_ERROR(ED_CheckNumber(tok, keyType, kd->flags & ED_INSIST_POSITIVE, &parsedNumber));
					switch (keyType) {
					case ED_TYPE_INT:
						ibuf[numElements++] = atoi(tok);
						break;
					case ED_TYPE_FLOAT:
						fbuf[numElements++] = atof(tok);
						break;
					default:
						ED_RETURN_ERROR("ED_ProcessRanges: unexpected type");
					}
				}
				kr->numElements = numElements;
				ED_TEST_RETURN_ERROR(kr->continuous && numElements != 2,
					"ED_ProcessRanges: continuous range should only have 2 elements, upper and lower bounds, \"%s\" in %s in %s",
					kr->str, kd->name, ed->classname);
				if (ED_TYPE_INT == keyType) {
					const size_t size = numElements * sizeof(int);
					kr->iArr = (int *)malloc(size);
					ED_TEST_RETURN_ERROR(!kr->iArr, "ED_ProcessRanges: out of memory");
					memcpy(kr->iArr, ibuf, size);
				} else { /* ED_TYPE_FLOAT */
					const size_t size = numElements * sizeof(float);
					kr->fArr = (float *)malloc(size);
					ED_TEST_RETURN_ERROR(!kr->fArr, "ED_ProcessRanges: out of memory");
					memcpy(kr->fArr, fbuf, size);
				}
			}
			ED_TEST_RETURN_ERROR(kd->numRanges && kd->numRanges != 1 && kd->vLen != kd->numRanges,
				"ED_ProcessRanges: if range definitions are supplied, "
				"there must be one (which is applied to each element of a vector), "
				"or one for each element of the vector. "
				"%s in %s has %i elements in vector and %i range definitions",
				ed->classname, kd->name, kd->vLen, kd->numRanges);
		}
	}
	return ED_OK;
}

/**
 * parses entity definitions from entities.ufo
 * @return ED_OK or ED_ERR
 * @sa ED_GetLastErr
 */
int ED_Parse (const char *data_p)
{
	/* only do this once, repeat calls are OK */
	static int done = 0;
	if (done)
		return ED_OK;

	snprintf(lastErr, sizeof(lastErr), "no error");
	/* Zero out so that looping through the ones that have already
	 * been parsed is possible while the rest are parsed */
	OBJZERO(entityDefs);
	numEntityDefs = 0;

	ED_PASS_ERROR(ED_ParseEntities(&data_p));
	ED_TEST_RETURN_ERROR(numEntityDefs == 0, "ED_Parse: Zero entity definitions found");

	ED_PASS_ERROR(ED_ProcessRanges());

	ED_PASS_ERROR(ED_CheckDefaultTypes());

	done = 1; /* do not do it again */

#ifdef DEBUG_ED
	ED_Dump();
#endif

	return ED_OK;
}

const char *ED_GetLastError (void)
{
	return lastErr;
}

/**
 * @brief searches for the parsed key def
 * @param abstract send abstract to find an abstract key with this name
 * @return NULL if the entity def or key def is not found. call ED_GetLastError to get a relevant message.
 */
const entityKeyDef_t *ED_GetKeyDef (const char *classname, const char *keyname, const int abstract)
{
	const entityDef_t *ed = ED_GetEntityDef(classname);
	return ED_GetKeyDefEntity(ed, keyname, abstract);
}

/**
 * @brief searches for the parsed key def, when the entity def is known
 * @param abstract send a nonzero value if the abstract (radiant - not in any block) version of the
 * key is required
 * @return NULL if the entity def or key def is not found. call ED_GetLastError to get a relevant message.
 */
const entityKeyDef_t *ED_GetKeyDefEntity (const entityDef_t *ed, const char *keyname, const int abstract)
{
	const entityKeyDef_t *kd;

	if (!ed)
		return NULL;

	for (kd = ed->keyDefs; kd->name; kd++)
		if (Q_streq(keyname, kd->name)) {
			if (abstract) {
				if (kd->flags & ED_ABSTRACT)
					return kd;
			} else {
				if (!(kd->flags & ED_ABSTRACT))
					return kd;
			}
		}

	snprintf(lastErr, sizeof(lastErr), "ED_GetKeyDefEntity: no key definition for %s found in entity %s entities.ufo", keyname, ed->classname);
	return NULL;
}

/**
 * @brief searches for the parsed entity def by classname
 * @return NULL if the entity def is not found. call ED_GetLastError to get a relevant message.
 */
const entityDef_t *ED_GetEntityDef (const char *classname)
{
	const entityDef_t *ed;

	for (ed = entityDefs; ed->numKeyDefs; ed++)
		if (Q_streq(classname, ed->classname))
			return ed;

	snprintf(lastErr, sizeof(lastErr), "ED_GetEntityDef: no entity definition for %s found in entities.ufo", classname);
	return NULL;
}

void ED_Free (void)
{
	if (numEntityDefs) {
		entityDef_t *ed;
		for (ed = entityDefs; ed->numKeyDefs; ed++) {
			entityKeyDef_t *kd;
			free(ed->classname);
			for (kd = ed->keyDefs; kd->name; kd++) {
				if (kd->name)
					free(kd->name);
				if (kd->desc)
					free(kd->desc);
				if (kd->defaultVal)
					free(kd->defaultVal);
				if (kd->numRanges) {
					int i;
					for (i = 0; i < kd->numRanges ;i++) {
						entityKeyRange_t *kr = kd->ranges[i];
						if (kr->iArr)
							free(kr->iArr);
						if (kr->fArr)
							free(kr->fArr);
						free(kr->str);
						free(kr);
					}
					free(kd->ranges);
				}
			}
			free(kd = ed->keyDefs);
		}
	}
}

#ifdef DEBUG_ED

void ED_PrintRange (const entityKeyRange_t *kr)
{
	int i;
	printf(" rge:%c:%c>", kr->continuous ? 'c' : 'd', kr->iArr ? 'i' : 'f');
	for (i = 0; i < kr->numElements; i++) {
		if (kr->iArr)
			printf("%i ", kr->iArr[i]);
		else
			printf("%.1f ",kr->fArr[i]);
	}
	printf("\b<");
}

void ED_PrintParsedDefault (const entityKeyDef_t *kd)
{
	float fa[32];
	int ia[32];
	char ca[128];
	int i;

	if (!kd->defaultVal)
		return;

	printf(" parsed default>");

	switch (kd->flags & ED_KEY_TYPE) {
	case ED_TYPE_INT:
		ED_GetDefaultInt(ia, kd->vLen, kd);
		for (i = 0; i < kd->vLen; i++)
			printf("%i ", ia[i]);
		break;
	case ED_TYPE_FLOAT:
		ED_GetDefaultFloat(fa, kd->vLen, kd);
		for (i = 0; i < kd->vLen; i++)
			printf("%f ", fa[i]);
		break;
	case ED_TYPE_STRING:
		ED_GetDefaultString(ca, sizeof(ca) - 1, kd);
		printf("%s", ca);
		break;
	}

	printf("<");
}

void ED_PrintKeyDef (const entityKeyDef_t *kd)
{
	printf("  >%s< mandtry:%i opt:%i abst:%i type:%i", kd->name,
		kd->flags & ED_MANDATORY ? 1 : 0,
		kd->flags & ED_OPTIONAL ? 1 : 0,
		kd->flags & ED_ABSTRACT ? 1 : 0,
		kd->flags & ED_MODE_TYPE ? 1 : 0);
	if (kd->flags & ED_MODE_TYPE)
		printf(" type:%s[%i]", ED_Constant2Type(kd->flags & ED_KEY_TYPE), kd->vLen);

	if (kd->defaultVal)
		printf(" default>%s<",kd->defaultVal);

	if (kd->numRanges) {
		int i;
		for (i = 0; i < kd->numRanges; i++) {
			ED_PrintRange(kd->ranges[i]);
		}
	}

	ED_PrintParsedDefault(kd);

	printf("\n");
}

/**
 * @brief print all the parsed entity definitions
 */
void ED_Dump (void)
{
	const entityDef_t *ed;
	printf("ED_Dump:\n");
	for (ed = entityDefs; ed->numKeyDefs; ed++) {
		const entityKeyDef_t *kd;
		printf("entity def >%s<\n", ed->classname);
		for (kd = &ed->keyDefs[0]; kd->name; kd++) {
			ED_PrintKeyDef(kd);
		}
	}
}
#endif
