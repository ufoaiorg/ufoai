/**
 * @file entitiesdef.c
 * @brief Handles definition of entities, parsing them from entities.ufo
 */

/*
All original materal Copyright (C) 2002-2009 UFO: Alien Invasion team.

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

#include "parse.h"
#include "entitiesdef.h"

#define ED_MAX_KEYS_PER_ENT 32
#define ED_MAX_TOKEN_LEN 512
#define ED_MAX_ERR_LEN 512

static char lastErr[ED_MAX_ERR_LEN]; /**< for storing last error message */
static char lastErrExtra[ED_MAX_ERR_LEN]; /**< temporary storage for extra information to be added to lastErr */
static int lastCheckedInt; /**< @sa ED_CheckNumber */
static float lastCheckedFloat; /**< @sa ED_CheckNumber */

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
 * @brief a macro to test a condition write an error
 * message and exit the current function returning ED_ERROR
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
		strncat(lastErr, lastErrExtra, sizeof(lastErr)); \
		lastErr[ED_MAX_ERR_LEN-1] = '\0'; \
		return ED_ERROR; \
	}

/**
 * counts the number of entity types defined in entities.ufo
 * @return The number of entities. or ED_ERROR (which is -1) if there is an error. call ED_GetLastError to get the message
 */
static int ED_CountEntities (const char **data_p)
{
	int braceLevel = 0;
	int tokensOnLevel0 = 0;
	const char *parsedToken;
	int numEntities = 0;

	while (data_p) {
		parsedToken = COM_Parse(data_p);

		/* zero length tokens are returned at the end of data
		 * they are also returned by valid values "" in the file,
		 * hence the brace level check */
		if (strlen(parsedToken) == 0 && braceLevel == 0)
			break;

		if (parsedToken[0] == '{') {
			tokensOnLevel0 = 0;
			braceLevel++;
			ED_TEST_RETURN_ERROR(braceLevel > 2, "Too many open braces, nested %i deep", braceLevel);
			continue;
		}

		if (parsedToken[0] == '}') {
			braceLevel--;
			ED_TEST_RETURN_ERROR(braceLevel < 0, "Too many close braces. after %i entities", numEntities);
			continue;
		}

		/* ignore everything inside the entity defs, as we are only counting them now */
		if (braceLevel != 0)
			continue;

		if (tokensOnLevel0 == 0 && parsedToken[0] != '\0') {
			if (!strcmp(parsedToken, "entity")) {
				tokensOnLevel0++;
				continue;
			}
			ED_RETURN_ERROR( "Next entity expected, found \"%s\" token", parsedToken);
		}

		if (tokensOnLevel0 == 1) { /* classname of entity */
			tokensOnLevel0++;
			numEntities++;
			continue;
		}

		ED_TEST_RETURN_ERROR(tokensOnLevel0 > 1, "'{' to start entity definition expected. \"%s\" found.", parsedToken)
	}

	if (numEntities == 0)
		snprintf(lastErr, sizeof(lastErr), "No entities found");

	return numEntities;
}

/**
 * @return ED_ERROR in case of an error (out of memory), else 0.
 */
static int ED_AllocKeyDef (entityKeyDef_t *keyDef, const char *newName)
{
	const int nameLen = strlen(newName) + 1;

	keyDef->name = (char *) malloc(nameLen * sizeof(char));
	if (!keyDef->name) {
		snprintf(lastErr, sizeof(lastErr), "ED_AllocKeyDef: out of memory");
		return ED_ERROR;
	}
	strncpy(keyDef->name, newName, nameLen);

	return 0;
}

/**
 * @return ED_ERROR in case of an error (out of memory), else 0.
 */
static int ED_AllocEntityDef (entityKeyDef_t *newKeyDefs, int numKeyDefs, int entityIndex)
{
	entityDef_t *eDef = &entityDefs[entityIndex];
	int classnameLen;

	/* now we know how many there are in this entity, malloc */
	eDef->keyDefs = (entityKeyDef_t *) malloc((numKeyDefs + 1) * sizeof(entityKeyDef_t));
	eDef->numKeyDefs = numKeyDefs;

	if (!eDef->keyDefs) {
		snprintf(lastErr, sizeof(lastErr), "ED_AllocEntityDef: out of memory");
		return ED_ERROR;
	}

	/* and copy from temp buffer */
	memcpy(eDef->keyDefs, newKeyDefs, numKeyDefs * sizeof(entityKeyDef_t));

	/* set NULLs at the end, to enable looping through using a pointer */
	memset(&eDef->keyDefs[numKeyDefs], 0, sizeof(entityKeyDef_t));

	/* classname is commonly used, put it in its own field, copied from keyDefs[0] */
	classnameLen = strlen(eDef->keyDefs->desc) + 1;
	eDef->classname = malloc(classnameLen * sizeof(char));
	strncpy(eDef->classname, eDef->keyDefs->desc, classnameLen);

	return 0;
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
		if (!strcmp(keyDef->name, name) && !((keyDef->flags ^ parseMode) & ED_ABSTRACT)) {
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
	if (!strcmp(strType, "V_FLOAT"))
		return ED_TYPE_FLOAT;
	else if (!strcmp(strType, "V_INT"))
		return ED_TYPE_INT;
	else if (!strcmp(strType, "V_STRING"))
		return ED_TYPE_STRING;

	snprintf(lastErr, sizeof(lastErr), "ED_Type2Constant: type string not recognised: \"%s\"", strType);
	return ED_ERROR;
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
 * @brief parses a value from the definition
 * @param kd the key definition to parse from
 * @param[out] v the array of int to put the answer in
 * it must have enough space for n elements.
 * @param n the number of elements expected in the vector
 * @return ED_ERROR if n does not agree with the key def. call ED_GetLastError.
 */
int ED_GetIntVector (const entityKeyDef_t *kd, int v[], const int n)
{
	int i;
	const char *buf_p = kd->desc;

	for (i = 0; buf_p; i++) {
		const char *tok = COM_Parse (&buf_p);
		if (tok[0] == '\0')
			break; /* previous tok was the last real one, don't waste time */
		if (i >= n) {
			snprintf(lastErr, sizeof(lastErr), "ED_GetIntVector: supplied buffer v[%i] too small for the number of elements in key def \"%s\"", n, kd->name);
			return ED_ERROR;
		}
		v[i] = atoi(tok);
	}
	if (i != n) {
		snprintf(lastErr, sizeof(lastErr), "ED_GetIntVector: supplied buffer v[%i] too large for the number of elements in key def \"%s\"", n, kd->name);
		return ED_ERROR;
	}
	return 0;
}

/**
 * @brief checks that a string represents a single number
 * @param insistPositive if 1, then tests for the number being greater than or equal to zero.
 * @sa ED_CheckNumericType
 * @note disallows hex, inf, NaN, numbers with junk on the end (eg -0123junk)
 * @return ED_OK if the value string matches the type, ED_ERROR otherwise.
 * (call ED_GetLastError)
 * @note the parsed numbers are stored for later use in lastCheckedInt and lastCheckedFloat
 */
static int ED_CheckNumber (const char *value, const int floatOrInt, const int insistPositive)
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
		lastCheckedFloat = strtof(value, &end_p);
		ED_TEST_RETURN_ERROR(insistPositive && lastCheckedFloat < 0.0f, "ED_CheckNumber: not positive %s", value);
		break;
	case ED_TYPE_INT:
		lastCheckedInt = (int)strtol(value, &end_p, 10);
		ED_TEST_RETURN_ERROR(insistPositive && lastCheckedInt < 0, "ED_CheckNumber: not positive %s", value);
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
 * @param index the index of the number being checked in the value. eg angles "90 180", 90 is at 0, 180 is at 1.
 * @note checks lastCheckedInt or lastCheckedFloat against the range in the supplied keyDef.
 * @return ED_ERROR or ED_OK
 */
static int ED_CheckRange (const entityKeyDef_t *keyDef, const int floatOrInt, const int index)
{
	/* there may be a range for each element of the value, or there may only be one */
	int useRangeIndex = (keyDef->numRanges == 1) ? 0 : index;
	entityKeyRange_t *kr;
	const float discreteFloatEpsilon = 0.0001f;
	if (0 == keyDef->numRanges)
		return ED_OK; /* if no range defined, that is OK */
	assert(useRangeIndex < keyDef->numRanges);
	kr = keyDef->ranges[useRangeIndex];
	switch (floatOrInt) {
	case ED_TYPE_FLOAT:
		if (kr->continuous) {
			ED_TEST_RETURN_ERROR(lastCheckedFloat < kr->fArr[0] || lastCheckedFloat > kr->fArr[1],
				"ED_CheckRange: %.1f out of range, \"%s\" in %s",
				lastCheckedFloat, kr->str, keyDef->name);
			return ED_OK;
		} else {
			int j;
			for (j = 0; j < kr->numElements; j++)
				if (fabs(lastCheckedFloat - kr->fArr[j]) < discreteFloatEpsilon)
					return ED_OK;
		}
		break;
	case ED_TYPE_INT:
		if (kr->continuous) {
			ED_TEST_RETURN_ERROR(lastCheckedInt < kr->iArr[0] || lastCheckedInt > kr->iArr[1],
				"ED_CheckRange: %i out of range, \"%s\" in %s",
				lastCheckedInt, kr->str, keyDef->name);
			return ED_OK;
		} else {
			int j;
			for (j = 0; j < kr->numElements; j++)
				if (kr->iArr[j] == lastCheckedInt)
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
	assert(floatOrInt & (ED_TYPE_INT | ED_TYPE_FLOAT));
	while (buf_p) {
		const char *tok = COM_Parse(&buf_p);
		if (tok[0] == '\0')
			break; /* previous tok was the last real one, don't waste time */

		ED_PASS_ERROR_EXTRAMSG(ED_CheckNumber(tok, floatOrInt, keyDef->flags & ED_INSIST_POSITIVE),
			" in key \"%s\"", keyDef->name);

		if (ED_ERROR == ED_CheckRange(keyDef, floatOrInt, i))
			return ED_ERROR;

		i++;
	}

	ED_TEST_RETURN_ERROR(i != keyDef->vLen, "ED_CheckNumericType: %i elements in vector that should have %i for \"%s\" key",
			i, keyDef->vLen, keyDef->name);

	return 1;
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
 * @return ED_ERROR if there is a problem (call ED_GetLastError). return 1 if OK.
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
		snprintf(lastErr, sizeof(lastErr), "ED_CheckTypeEntityKey: type not recognised in key def");
		return ED_ERROR;
	}
}

/**
 * @brief takes a type string (eg "V_FLOAT 6") and configures entity def
 * @return ED_ERROR for an error, otherwise 0
 */
static int ED_ParseType (entityKeyDef_t *kd, const char *parsedToken)
{
	static char tokBuf[64];
	const char *buf_p;
	int type;
	int vectorLen;
	const char *partToken;
	/* need a copy, as parsedToken is held in a static buffer in the
	 * Com_Parse function */
	if ((strlen(parsedToken) + 1) > sizeof(tokBuf)) {
		snprintf(lastErr, sizeof(lastErr), "ED_ParseType: type string too long for buffer for key %s",kd->name);
		return ED_ERROR;
	}
	strncpy(tokBuf, parsedToken, sizeof(tokBuf));
	buf_p = tokBuf;

	partToken = COM_Parse(&buf_p);

	if (!strcmp("SIGNED", partToken)) {
		partToken = COM_Parse(&buf_p);/* get next token */
	} else if (!strcmp("UNSIGNED", partToken)) {
		kd->flags |= ED_INSIST_POSITIVE;
		partToken = COM_Parse(&buf_p);
	}

	if (strlen(partToken)) {
		type = ED_Type2Constant(partToken);
		if (ED_ERROR == type)
			return ED_ERROR;
	} else {/* default is string */
		type = ED_TYPE_STRING;
	}

	kd->flags |= type;
	partToken = COM_Parse(&buf_p);
	vectorLen = atoi(partToken);
	if (vectorLen && (ED_ERROR == ED_CheckNumber(partToken, ED_TYPE_INT, 1))) {
		snprintf(lastErr, sizeof(lastErr), "ED_ParseType: problem with vector length \"%s\" in key %s",
			partToken, kd->name);
		return ED_ERROR;
	}
	kd->vLen = strlen(partToken) ? (vectorLen ? vectorLen : 1) : 1; /* default is 1 */
	return 0;
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

static int ED_AllocRange(entityKeyDef_t *kd, const char *rangeStr)
{
	entityKeyRange_t **newRanges;
	/* start a new range */
	char *newStr = strdup(rangeStr);
	entityKeyRange_t *newRange = (entityKeyRange_t *)malloc(sizeof(entityKeyRange_t));
	memset(newRange, 0, sizeof(entityKeyRange_t));
	/* resize array of pointers */
	newRanges = (entityKeyRange_t **)malloc((kd->numRanges + 1) * sizeof(entityKeyRange_t *));
	if (!newRanges || !newStr || !newRange) {
		snprintf(lastErr, sizeof(lastErr) - 1, "ED_AllocRange: out of memory");
		return ED_ERROR;
	}
	newRange->str = newStr;
	newRanges[kd->numRanges] = newRange;
	if (kd->ranges) {
		memcpy(newRanges, kd->ranges, kd->numRanges * sizeof(entityKeyRange_t *));
		free(kd->ranges);
	}
	(kd->numRanges)++;
	kd->ranges = newRanges;
	return 1;
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
		if (*numKeyDefsSoFar_p >= ED_MAX_KEYS_PER_ENT) {
			snprintf(lastErr, sizeof(lastErr), "ED_PairParsed: too many keys for buffer");
			return ED_ERROR;
		}
		if (ED_AllocKeyDef(keyDef, newName) == ED_ERROR)
			return ED_ERROR; /* lastErr already set */
	}

	if (keyDef->flags & mode & ~ED_RANGE) {/* multiple range defs are permitted, different elements can have different ranges */
		snprintf(lastErr, sizeof(lastErr), "Duplicate %s for %s key. second value: %s", ED_Constant2Block(mode), newName, newVal);
		return ED_ERROR;
	}

	keyDef->flags |= mode;

	/* store information */
	switch (mode) {
	case ED_MANDATORY:
	case ED_OPTIONAL:
	case ED_ABSTRACT:
		keyDef->desc = strdup(newVal);
		ED_TEST_RETURN_ERROR(!keyDef->desc, "ED_PairParsed: out of memory while storing string.");
		return ED_OK;
	case ED_DEFAULT:
		keyDef->defaultVal = strdup(newVal);
		ED_TEST_RETURN_ERROR(!keyDef->defaultVal, "ED_PairParsed: out of memory while storing string.");
		return ED_OK;
	case ED_MODE_TYPE:
		/* only optional or abstract keys may have types, not possible to test for this here,
			* as the type block may come before the optional or mandatory block */
		if (ED_ParseType(keyDef, newVal) == ED_ERROR)
			return ED_ERROR; /* lastErr set in function call */
		return 0;
	case ED_RANGE:
		/** @todo test only typed keys may have ranges, but this
			* may only be tested after the whole ent has been parsed, as the
			* blocks may come in any order. test that only */
		if (ED_AllocRange(keyDef, newVal) == ED_ERROR)
			return ED_ERROR;
		return 0;
	default:
		ED_RETURN_ERROR("ED_PairParsed: parse mode not recognised");
	}
}

/**
 * Parses the defs, once the number of entity defs has been found
 * @return 0. ED_ERROR if there is an error. call ED_GetLastError to get the message
 */
static int ED_ParseEntities (const char **data_p)
{
	int braceLevel = 0;
	int tokensOnLevel0 = 0;
	int mode = ED_OPTIONAL;
	int entityIndex = 0;
	entityKeyDef_t keyDefBuf[ED_MAX_KEYS_PER_ENT];
	char lastTokenBuf[ED_MAX_TOKEN_LEN];
	int keyIndex = 0;
	int toggle = 0; /* many lines should have a pair of tokens on, this toggles 0, 1 to indicate progress */

	/* less checking here, as a lot is done whilst counting the entities */
	while (data_p) {
		const char *parsedToken = COM_Parse(data_p);
		toggle ^= 1;

		if (parsedToken[0] == '{') {
			braceLevel++;
			if (!toggle) {
				snprintf(lastErr, sizeof(lastErr), "ED_ParseEntities: Incorrect number of tokens before '{'");
				return ED_ERROR;
			}
			toggle ^= 1; /* reset, as toggle is only for counting proper text tokens, not braces */
			tokensOnLevel0 = 0;
			continue;
		}

		if (parsedToken[0] == '}') {
			braceLevel--;
			toggle ^= 1; /* reset, as toggle is only for counting proper text tokens, not braces */
			if (braceLevel == 0) { /* finished parsing entity def and prepare for the next one */
				ED_AllocEntityDef(keyDefBuf, keyIndex, entityIndex);
				entityIndex++;
				if (entityIndex == numEntityDefs)
					break;
			}
			if (braceLevel == 1) { /* ending a default, mandatory, etc block, go back to default parse mode */
				mode = ED_ABSTRACT;
			}
			continue;
		}

		if (braceLevel == 0) {
			if (tokensOnLevel0 == 1) {/* classname of entity, start parsing new entity */
				const entityDef_t *prevED = ED_GetEntityDef(parsedToken);
				if (prevED) {
					snprintf(lastErr, sizeof(lastErr), "ED_ParseEntities: duplicate entity definition name:%s", parsedToken);
					return ED_ERROR;
				}
				memset(keyDefBuf, 0, sizeof(keyDefBuf)); /* ensure pointers are not carried over from previous entity */
				keyIndex = 0;
				if (ED_PairParsed(keyDefBuf, &keyIndex, "classname", parsedToken, ED_MANDATORY) == ED_ERROR)
					return ED_ERROR;
				mode = ED_ABSTRACT;
			}
			tokensOnLevel0++;
		} else { /* braceLevel > 0 */
			if (!strcmp("mandatory", parsedToken)) {
				mode = ED_MANDATORY;
				toggle ^= 1; /* start of a mode changing block is the only time that only one non-brace token is on a line */
			} else if (!strcmp("optional", parsedToken)) {
				mode = ED_OPTIONAL;
				toggle ^= 1;
			} else if (!strcmp("default", parsedToken)) {
				mode = ED_DEFAULT;
				toggle ^= 1;
			} else if (!strcmp("type", parsedToken)) {
				mode = ED_MODE_TYPE;
				toggle ^= 1;
			} else if (!strcmp("range", parsedToken)) {
				mode = ED_RANGE;
				toggle ^= 1;
			} else {/* must be a keyname or value */
				if (toggle) { /* store key name til after next token is parsed */
					strncpy(lastTokenBuf, parsedToken, ED_MAX_TOKEN_LEN);
				} else { /* store key-value pair in buffer until whole entity is parsed */
					if (ED_PairParsed(keyDefBuf, &keyIndex, lastTokenBuf, parsedToken, mode) == ED_ERROR)
						return ED_ERROR;
				}
			}
		}
	}

	return 0;
}

/**
 * @return ED_ERROR if the default for a key does not meet the type definition, otherwise 0
 * as we have the pointers available here
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
					const char *tok = COM_Parse(&tmpRange_p);
					if (tok[0] == '\0')
						break;
					if (!strcmp("-", tok)) {
						kr->continuous = 1;
						if (numElements != 1) {
							snprintf(lastErr, sizeof(lastErr),
								"ED_ProcessRanges: problem with continuous range, \"%s\" in %s in %s",
								kr->str, kd->name, ed->classname);
							return ED_ERROR;
						}
						continue;
					}
					if (ED_CheckNumber(tok, keyType, kd->flags & ED_INSIST_POSITIVE) == ED_ERROR)
						return ED_ERROR;
					switch (keyType) {
					case ED_TYPE_INT:
						ibuf[numElements++] = atoi(tok);
						break;
					case ED_TYPE_FLOAT:
						fbuf[numElements++] = atof(tok);
						break;
					default:
						snprintf(lastErr, sizeof(lastErr),
							"ED_ProcessRanges: unexpected type");
						return ED_ERROR;
					}
				}
				kr->numElements = numElements;
				if (kr->continuous && numElements != 2) {
					snprintf(lastErr, sizeof(lastErr),
						"ED_ProcessRanges: continuous range should only have 2 elements, upper and lower bounds, \"%s\" in %s in %s",
						kr->str, kd->name, ed->classname);
					return ED_ERROR;
				}
				if (ED_TYPE_INT == keyType) {
					const size_t size = numElements * sizeof(int);
					kr->iArr = (int *)malloc(size);
					if (!kr->iArr) {
						snprintf(lastErr, sizeof(lastErr), "ED_ProcessRanges: out of memory");
						return ED_ERROR;
					}
					memcpy(kr->iArr, ibuf, size);
				} else { /* ED_TYPE_FLOAT */
					const size_t size = numElements * sizeof(float);
					kr->fArr = (float *)malloc(size);
					if (!kr->fArr) {
						snprintf(lastErr, sizeof(lastErr), "ED_ProcessRanges: out of memory");
						return ED_ERROR;
					}
					memcpy(kr->fArr, fbuf, size);
				}
			}
			if (kd->numRanges && kd->numRanges != 1 && kd->vLen != kd->numRanges) {
				snprintf(lastErr, sizeof(lastErr), "ED_ProcessRanges: if range definitions are supplied, "
					"there must be one (which is applied to each element of a vector), "
					"or one for each element of the vector. "
					"%s in %s has %i elements in vector and %i range definitions",
					ed->classname, kd->name, kd->vLen, kd->numRanges);
				return ED_ERROR;
			}
		}
	}
	/** @todo test defaults against ranges */
	return 0;
}

/**
 * parses entity types defined in entities.ufo
 * @return 0 if everything is OK. nonzero otherwise. then call ED_GetLastError to get the message
 */
int ED_Parse (const char **data_p)
{
	/* this function should not change *data_p, as the calling function uses
	 * it to free(). also, it is parsed twice, and Com_Parse changes *data_p.
	 * hence the use of copies of the pointer*/
	const char *copy_data_p = *data_p;
	size_t ed_block_size;

	/* only do this once, repeat calls are OK */
	if (numEntityDefs)
		return 0;

	snprintf(lastErr, sizeof(lastErr), "no error");

	numEntityDefs = ED_CountEntities(&copy_data_p);
	if (numEntityDefs < 1) {
		if (numEntityDefs == 0)
			snprintf(lastErr, sizeof(lastErr), "ED_Parse: no entity definitions found");
		return ED_ERROR;
	}

	/* make the block one larger than required, so when finished there are NULLs
	 * at the end to allow looping through with pointers. */
	ed_block_size = (numEntityDefs + 1) * sizeof(entityDef_t);
	entityDefs = (entityDef_t *)malloc(ed_block_size);

	if (!entityDefs) {
		snprintf(lastErr, sizeof(lastErr), "ED_Parse: out of memory");
		return ED_ERROR;
	}

	/* memset to NULL now so that looping through the ones that have already
	 * been parsed is possible while the rest are parsed */
	memset(entityDefs, 0, ed_block_size);

	copy_data_p = *data_p;
	if (ED_ParseEntities(&copy_data_p) == ED_ERROR)
		return ED_ERROR;

	if (ED_ProcessRanges() == ED_ERROR)
		return ED_ERROR;

	if (ED_CheckDefaultTypes() == ED_ERROR)
		return ED_ERROR;

	return 0;
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
		if (!strcmp(keyname, kd->name)) {
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
	entityDef_t *ed;

	for (ed = entityDefs; ed->numKeyDefs; ed++)
		if (!strcmp(classname, ed->classname))
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
		free(entityDefs);
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
