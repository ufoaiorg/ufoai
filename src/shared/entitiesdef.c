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

#include "parse.h"
#include "entitiesdef.h"

#define ED_MAX_KEYS_PER_ENT 32
#define ED_MAX_TOKEN_LEN 512

static char* lastErr = "No error found";

/** counts the number of entity types defined in entities.ufo
 *  @return The number of entities. -1 if there is an error. call ED_LastErr to get the message */
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
				lastErr = "Too many open braces";
				return -1;
			}
			continue;
		}

		if (*parsedToken == '}') {
			braceLevel--;
			if (braceLevel < 0) {
				lastErr = "Too many close braces";
				return -1;
			}
			continue;
		}

		/* ignore everything inside the entity defs, as we are only counting them now */
		if (braceLevel != 0)
			continue;

		if (tokensOnLevel0 == 0) {
			if (!strcmp(parsedToken, "entity")) {
				tokensOnLevel0++;
				continue;
			}
			lastErr = "Next entity expected";
			return -1;
		}

		if (tokensOnLevel0 == 1) { /* classname of entity */
			tokensOnLevel0++;
			numEntities++;
			continue;
		}

		if (tokensOnLevel0 > 1) {
			lastErr = "'{' to start entity definition expected";
			return -1;
		}
	}

	if (numEntities == 0)
		lastErr = "No entities found";

	return numEntities;
}

static void ED_AllocKeyDef (entityKeyDef_t *keyDef, const char *newName, const char *newDesc, const int newFlags)
{
	const int nameLen = strlen(newName) + 1;
	const int descLen = strlen(newDesc) + 1;

	keyDef->name = (char *) malloc(nameLen * sizeof(char));
	strncpy(keyDef->name, newName, nameLen);

	keyDef->desc = (char *) malloc(descLen * sizeof(char));
	strncpy(keyDef->desc, newDesc, descLen);

	keyDef->defaultVal = NULL; /* parsed later, if there is a default */

	keyDef->flags = newFlags;
}

static void ED_AllocEntityDef (entityKeyDef_t *newKeyDefs, int numKeyDefs, int entityIndex) {
	entityDef_t *eDef = &entityDefs[entityIndex];

	/* now we know how many there are in this entity, malloc */
	eDef->keyDefs = (entityKeyDef_t *) malloc((numKeyDefs + 1) * sizeof(entityKeyDef_t));
	eDef->numKeyDefs = numKeyDefs;

	/* and copy from temp buffer */
	memcpy(eDef->keyDefs, newKeyDefs, numKeyDefs * sizeof(entityKeyDef_t));

	memset(&eDef->keyDefs[numKeyDefs], 0, sizeof(entityKeyDef_t));/* set NULLs at the end, to enable looping through using a pointer */
}

/** Parses the defs, once the number of entity defs has been found
 *  @return 0. -1 if there is an error. call ED_LastErr to get the message */
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
				lastErr="Incorrect number of tokens before '{'";
				return -1;
			}
			toggle ^= 1; /* reset, as toggle is only for counting proper text tokens, not braces */
			tokensOnLevel0 = 0;
			if (braceLevel == 1) { /* started parsing entity def*/
				mode = ED_OPTIONAL; /* set default mode */
			}
			continue;
		}

		if (*parsedToken == '}') {
			braceLevel--;
			toggle ^= 1; /* reset, as toggle is only for counting proper text tokens, not braces */
			if (braceLevel == 0) { /* finished parsing entity def and prepare for the next one */
				ED_AllocEntityDef(keyDefBuf, keyIndex, entityIndex);
				keyIndex = 0;
				entityIndex++;
				if (entityIndex == numEntityDefs)
					break;
			}
			continue;
		}

		if (braceLevel == 0) {
			if (tokensOnLevel0 == 1) { /* classname of entity */
				ED_AllocKeyDef(&keyDefBuf[keyIndex++], "classname", parsedToken, ED_MANDATORY);
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
				if (toggle) { /* store key name til after next token is parsed */ /** @todo toggle or !toggle, that is the question */
					strncpy(lastTokenBuf, parsedToken, ED_MAX_TOKEN_LEN);
					/** @todo fix default parsing - should search for key already parsed */
				} else {
					ED_AllocKeyDef(&keyDefBuf[keyIndex++], lastTokenBuf, parsedToken, mode);
				}
			}
		}



	}

	return 0;
}

/** parses entity types defined in entities.ufo
 *  @return 0 if everything is OK. nonzero otherwise. then call ED_LastErr to get the message */
int ED_Parse (const char **data_p)
{
	const char *copy_data = *data_p;

	numEntityDefs = ED_CountEntities(data_p);
	if (numEntityDefs < 1)
		return -1;

	entityDefs = (entityDef_t *)malloc((numEntityDefs + 1) * sizeof(entityDef_t));

	if (ED_ParseEntities(&copy_data) == -1)
		return -1;

	memset(&entityDefs[numEntityDefs], 0, sizeof(entityDef_t)); /* NULLs at the end to allow pointer looping */

	return 0;
}


char *ED_LastErr (void)
{
	return lastErr;
}

void ED_Free (void)
{
	if (numEntityDefs > 0) {
		entityDef_t *ed;
		for (ed = entityDefs; ed->numKeyDefs; ed++) {
			entityKeyDef_t *kd;
			for (kd = &ed->keyDefs[1]; kd->name; kd++) {
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
/** @brief print all the parsed entity definitions */
void ED_Dump (void)
{
	entityDef_t *ed;
	for (ed = entityDefs; ed->numKeyDefs; ed++) {
		entityKeyDef_t *kd;
		Com_Printf("*** >%s< >%s<\n", ed->keyDefs[0].name, ed->keyDefs[0].desc);
		for (kd = &ed->keyDefs[1]; kd->name; kd++)
			Com_Printf(">%s< mandatory:%i\n", kd->name, kd->flags & ED_MANDATORY);
	}
}
#endif
