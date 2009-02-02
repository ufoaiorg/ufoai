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

#include "parse.h"
#include "entitiesdef.h"

#define ED_MAX_KEYS_PER_ENT 32
#define ED_MAX_TOKEN_LEN 512
#define ED_MAX_ERR_LEN 512

static char lastErr[ED_MAX_ERR_LEN];

/**
 * counts the number of entity types defined in entities.ufo
 * @return The number of entities. -1 if there is an error. call ED_GetLastError to get the message
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

		if (*parsedToken == '{') {
			tokensOnLevel0 = 0;
			braceLevel++;
			if (braceLevel > 2) {
				snprintf(lastErr, sizeof(lastErr), "Too many open braces");
				return -1;
			}
			continue;
		}

		if (*parsedToken == '}') {
			braceLevel--;
			if (braceLevel < 0) {
				snprintf(lastErr, sizeof(lastErr), "Too many close braces");
				return -1;
			}
			continue;
		}

		/* ignore everything inside the entity defs, as we are only counting them now */
		if (braceLevel != 0)
			continue;

		if (tokensOnLevel0 == 0 && *parsedToken != '\0') {
			if (!strcmp(parsedToken, "entity")) {
				tokensOnLevel0++;
				continue;
			}
			snprintf(lastErr, sizeof(lastErr), "Next entity expected, found \"%s\" token", parsedToken);
			return -1;
		}

		if (tokensOnLevel0 == 1) { /* classname of entity */
			tokensOnLevel0++;
			numEntities++;
			continue;
		}

		if (tokensOnLevel0 > 1) {
			snprintf(lastErr, sizeof(lastErr), "'{' to start entity definition expected");
			return -1;
		}
	}

	if (numEntities == 0)
		snprintf(lastErr, sizeof(lastErr), "No entities found");

	return numEntities;
}

/**
 * @return a new string, or null if out of memory. sets lastErr, if out of memory
 */
static char *ED_AllocString (const char * string) {
	const int len = strlen(string) + 1;
	char *newString = (char *) malloc(len * sizeof(char));

	if (newString)
		strncpy(newString, string, len);
	else
		snprintf(lastErr, sizeof(lastErr), "ED_AllocString: out of memory");

	return newString;
}

/**
 * @return -1 in case of an error (out of memory), else 0.
 */
static int ED_AllocKeyDef (entityKeyDef_t *keyDef, const char *newName)
{
	const int nameLen = strlen(newName) + 1;

	keyDef->name = (char *) malloc(nameLen * sizeof(char));
	if (!keyDef->name) {
		snprintf(lastErr, sizeof(lastErr), "ED_AllocKeyDef: out of memory");
		return -1;
	}
	strncpy(keyDef->name, newName, nameLen);

	return 0;
}

/**
 * @return -1 in case of an error (out of memory), else 0.
 */
static int ED_AllocEntityDef (entityKeyDef_t *newKeyDefs, int numKeyDefs, int entityIndex)
{
	entityDef_t *eDef = &entityDefs[entityIndex];

	/* now we know how many there are in this entity, malloc */
	eDef->keyDefs = (entityKeyDef_t *) malloc((numKeyDefs + 1) * sizeof(entityKeyDef_t));
	eDef->numKeyDefs = numKeyDefs;

	if (!eDef->keyDefs) {
		snprintf(lastErr, sizeof(lastErr), "ED_AllocEntityDef: out of memory");
		return -1;
	}

	/* and copy from temp buffer */
	memcpy(eDef->keyDefs, newKeyDefs, numKeyDefs * sizeof(entityKeyDef_t));

	/* set NULLs at the end, to enable looping through using a pointer */
	memset(&eDef->keyDefs[numKeyDefs], 0, sizeof(entityKeyDef_t));

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
		entityKeyDef_t *keyDef = &keyDefs[i];
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
 * @return the constant, or -1 if strType is not recognised
 */
static int ED_Type2Constant (const char *strType)
{
	if (!strcmp(strType, "V_FLOAT"))
		return ED_TYPE_FLOAT;
	else if (!strcmp(strType, "V_INT"))
		return ED_TYPE_INT;
	else if (!strcmp(strType, "V_STRING"))
		return ED_TYPE_STRING;

	snprintf(lastErr, sizeof(lastErr), "ED_Type2Constant: type string not recognised: %s", strType);
	return -1;
}

#ifdef DEBUG_ED
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
#endif

/**
 * @brief takes a type string (eg "V_FLOAT 6") and configures entity def
 * @return -1 for an error, otherwise 0
 */
static int ED_ParseType (entityKeyDef_t *kd, const char *parsedToken)
{
	const char *copyToken = parsedToken;
	int type = ED_Type2Constant(COM_Parse(&copyToken));
	int vectorLen;
	const char *vectorLenStr;
	if (-1 == type)
		return -1;

	/** @todo parsing of vector length, and defualt could probably be tidied and optimised */
	kd->flags |= type;
	vectorLenStr = COM_Parse(&copyToken);
	vectorLen = atoi(vectorLenStr);
	kd->vLen = strlen(vectorLenStr) ? 1 : vectorLen;
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
		default:
			snprintf(lastErr, sizeof(lastErr), "ED_Constant2Block: constant not recognised");
			return NULL;
	}
}

/**
 * @return -1 in case of an error, else 0.
 */
static int ED_PairParsed (entityKeyDef_t keyDefsBuf[], int *numKeyDefsSoFar_p,
							const char *newName, const char *newVal, const int mode)
{
	/* check if there is already a key def */
	entityKeyDef_t *keyDef = ED_FindKeyDefInArray(keyDefsBuf, *numKeyDefsSoFar_p, newName, mode);

	/* create one if required */
	if (!keyDef) {
		keyDef = &keyDefsBuf[(*numKeyDefsSoFar_p)++];
		if ((*numKeyDefsSoFar_p) >= ED_MAX_KEYS_PER_ENT) {
			snprintf(lastErr, sizeof(lastErr), "ED_PairParsed: too many keys for buffer");
			return -1;
		}
		if (-1 == ED_AllocKeyDef(keyDef, newName))
			return -1; /* lastErr already set */
	}

	if (keyDef->flags & mode) {
		snprintf(lastErr, sizeof(lastErr), "Duplicate %s for %s key. second value: %s", ED_Constant2Block(mode), newName, newVal);
		return -1;
	}

	keyDef->flags |= mode;

	/* store information */
	switch (mode) {
		case ED_MANDATORY:
		case ED_OPTIONAL:
		case ED_ABSTRACT:
			keyDef->desc = ED_AllocString(newVal);
			if (!keyDef->desc)
				return -1;
			return 0;
		case ED_DEFAULT:
			keyDef->defaultVal = ED_AllocString(newVal);
			if (!keyDef->defaultVal)
				return -1;
			return 0;
		case ED_MODE_TYPE:
			/* only optional or abstract keys may have types, not possible to test for this here,
			 * as the type block may come before the optional or mandatory block */
			if (-1 == ED_ParseType(keyDef, newVal))
				return -1; /* lastErr set in function call */
			return 0;
		default:
			snprintf(lastErr, sizeof(lastErr), "ED_PairParsed: parse mode not recognised");
			return -1;
	}
}

/**
 * Parses the defs, once the number of entity defs has been found
 * @return 0. -1 if there is an error. call ED_GetLastError to get the message
 */
static int ED_ParseEntities (const char **data_p)
{
	int braceLevel = 0;
	int tokensOnLevel0 = 0;
	const char *parsedToken;
	int mode = ED_OPTIONAL;
	int entityIndex = 0;
	entityKeyDef_t keyDefBuf[ED_MAX_KEYS_PER_ENT];
	char lastTokenBuf[ED_MAX_TOKEN_LEN];
	int keyIndex = 0;
	int toggle = 0; /* many lines should have a pair of tokens on, this toggles 0, 1 to indicate progress */


	/* less checking here, as a lot is done whilst counting the entities */
	while (data_p) {
		parsedToken = COM_Parse(data_p);
		toggle ^= 1;

		if (*parsedToken == '{') {
			braceLevel++;
			if (!toggle) {
				snprintf(lastErr, sizeof(lastErr), "ED_ParseEntities: Incorrect number of tokens before '{'");
				return -1;
			}
			toggle ^= 1; /* reset, as toggle is only for counting proper text tokens, not braces */
			tokensOnLevel0 = 0;
			continue;
		}

		if (*parsedToken == '}') {
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
					return -1;
				}
				memset(keyDefBuf, 0, sizeof(keyDefBuf)); /* ensure pointers are not carried over from previous entity */
				keyIndex = 0;
				if (-1 == ED_PairParsed(keyDefBuf, &keyIndex, "classname", parsedToken, ED_MANDATORY))
					return -1;
				mode = ED_ABSTRACT;
			}
			tokensOnLevel0++;
			continue;
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
			} else {/* must be a keyname or value */
				if (toggle) { /* store key name til after next token is parsed */
					strncpy(lastTokenBuf, parsedToken, ED_MAX_TOKEN_LEN);
				} else { /* store key-value pair in buffer until whole entity is parsed */
					if (-1 == ED_PairParsed(keyDefBuf, &keyIndex, lastTokenBuf, parsedToken, mode))
						return -1;
				}
			}
			continue;
		}
	}

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

	snprintf(lastErr, sizeof(lastErr), "no error");

	numEntityDefs = ED_CountEntities(&copy_data_p);
	if (numEntityDefs < 1) {
		if (numEntityDefs == 0)
			snprintf(lastErr, sizeof(lastErr), "ED_Parse: no entity definitions found");
		return -1;
	}

	/* make the block one larger than required, so when finished there are NULLs
	 * at the end to allow looping through with pointers. */
	ed_block_size = (numEntityDefs + 1) * sizeof(entityDef_t);
	entityDefs = (entityDef_t *)malloc(ed_block_size);

	if (!entityDefs) {
		snprintf(lastErr, sizeof(lastErr), "ED_Parse: out of memory");
		return -1;
	}

	/* memset to NULL now so that looping through the ones that have already
	 * been parsed is possible while the rest are parsed */
	memset(entityDefs, 0, ed_block_size);

	copy_data_p = *data_p;
	if (-1 == ED_ParseEntities(&copy_data_p)) {
		return -1;
	}

	return 0;
}

const char *ED_GetLastError (void)
{
	return lastErr;
}

/**
 * @brief searches for the parsed entity def by classname
 * @return NULL if the entity def is not found
 */
const entityDef_t *ED_GetEntityDef (const char *classname)
{
	entityDef_t *ed;

	for (ed = entityDefs; ed->numKeyDefs; ed++)
		if(!strcmp(classname, ed->keyDefs->desc)) /* the classname is always the zeroth keyDef */
			return ed;

	return NULL;
}

void ED_Free (void)
{
	if (numEntityDefs > 0) {
		entityDef_t *ed;
		for (ed = entityDefs; ed->numKeyDefs; ed++) {
			entityKeyDef_t *kd;
			for (kd = ed->keyDefs; kd->name; kd++) {
				if (kd->name)
					free(kd->name);
				if (kd->desc)
					free(kd->desc);
				if (kd->defaultVal)
					free(kd->defaultVal);
			}
			free(kd = ed->keyDefs);
		}
		free(entityDefs);
	}
}

#ifdef DEBUG_ED
/**
 * @brief print all the parsed entity definitions
 */
void ED_Dump (void)
{
	Com_Printf("ED_Dump:\n");
	const entityDef_t *ed;
	for (ed = entityDefs; ed->numKeyDefs; ed++) {
		const entityKeyDef_t *kd;
		Com_Printf(">%s< >%s<\n", ed->keyDefs[0].name, ed->keyDefs[0].desc);
		for (kd = &ed->keyDefs[1]; kd->name; kd++) {
			Com_Printf("  >%s< mandatory:%i optional:%i abstract:%i\n", kd->name,
				kd->flags & ED_MANDATORY ? 1 : 0,
				kd->flags & ED_OPTIONAL ? 1 : 0,
				kd->flags & ED_ABSTRACT ? 1 : 0);
			if (kd->defaultVal)
				Com_Printf("    >%s<",kd->defaultVal);
		}
	}
}
#endif
