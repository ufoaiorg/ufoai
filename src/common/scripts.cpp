/**
 * @file
 * @brief UFO scripts used in client and server
 * @note interpreters for: object, inventory, equipment, name and team, damage
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include "scripts.h"
#include "../shared/parse.h"
#include "../shared/keyvaluepair.h"
#include "../game/inventory.h"
#include "../client/cl_screen.h"

#define CONSTNAMEINT_HASH_SIZE	32

#define	MAX_CONSTNAMEINT_NAME	32

/**
 * @brief Structure to map (script) strings and integer (enum) values
 */
typedef struct com_constNameInt_s {
	char name[MAX_CONSTNAMEINT_NAME];		/**< script id */
	char* fullname;							/**< only set in case there was a namespace given */
	int value;								/**< integer value */
	struct com_constNameInt_s* hash_next;	/**< hash next pointer */
	struct com_constNameInt_s* next;		/**< linked list next pointer */
} com_constNameInt_t;

/** @brief Linked list of all the registeres mappings */
static com_constNameInt_t* com_constNameInt;
/** @brief Hash of all the registeres mappings */
static com_constNameInt_t* com_constNameInt_hash[CONSTNAMEINT_HASH_SIZE];

/**
 * @brief Will extract the variable from a string<=>int mapping string which contain a namespace
 * @param name The name of the script entry to map to an integer
 * @return The namespace in case one was found, @c nullptr otherwise
 */
static const char* Com_ConstIntGetVariable (const char* name)
{
	const char* space = strstr(name, "::");
	if (space)
		return space + 2;
	return name;
}

/**
 * @brief Searches whether a given value was registered as a string to int mapping
 * @param[in] name The name of the string mapping (maybe including a namespace)
 * @param[out] value The mapped integer if found, not touched if the given string
 * was found in the registered values.
 * @return True if the value is found.
 * @sa Com_RegisterConstInt
 * @sa Com_ParseValue
 */
bool Com_GetConstInt (const char* name, int* value)
{
	com_constNameInt_t* a;
	unsigned int hash;
	const char* variable;

	variable = Com_ConstIntGetVariable(name);

	/* if the alias already exists */
	hash = Com_HashKey(variable, CONSTNAMEINT_HASH_SIZE);
	for (a = com_constNameInt_hash[hash]; a; a = a->hash_next) {
		if (Q_streq(variable, a->name)) {
			if (!a->fullname || variable == name || Q_streq(a->fullname, name)) {
				*value = a->value;
				return true;
			}
		}
	}

	return false;
}

/**
 * @brief Searches whether a given value was registered as a string to int mapping
 * @param[in] space The namespace of the mapping variable
 * @param[in] variable The name of the string mapping
 * @param[out] value The mapped integer if found, not touched if the given string
 * was found in the registered values.
 * @return True if the value is found.
 * @sa Com_RegisterConstInt
 * @sa Com_ParseValue
 * @sa Com_GetConstInt
 */
bool Com_GetConstIntFromNamespace (const char* space, const char* variable, int* value)
{
	if (Q_strnull(variable))
		return false;

	if (Q_strnull(space))
		return Com_GetConstInt(variable, value);

	return Com_GetConstInt(va("%s::%s", space, variable), value);
}

/**
 * @brief Searches the mapping variable for a given integer value and a namespace
 * @param[in] space The namespace to search in - might not be @c nullptr or empty.
 * @param[in] value The mapped integer
 * @note only variables with a namespace given are found here
 * @sa Com_RegisterConstInt
 * @sa Com_ParseValue
 */
const char* Com_GetConstVariable (const char* space, int value)
{
	com_constNameInt_t* a;
	const size_t namespaceLength = strlen(space);

	a = com_constNameInt;
	while (a) {
		if (a->value == value && a->fullname) {
			if (!strncmp(a->fullname, space, namespaceLength))
				return a->name;
		}
		a = a->next;
	}

	return nullptr;
}

/**
 * @brief Removes a registered constant from the script mapping hash table.
 * @param name The name of the script entry to remove out of the const int hash. In case this string
 * is equipped with a namespace, the string is in the form "namespace::variable". If you try to
 * unregister a variable that was registered with a namespace, this namespace must be included in the
 * given name, too.
 * @sa Com_RegisterConstInt
 * @sa Com_GetConstVariable
 */
bool Com_UnregisterConstVariable (const char* name)
{
	com_constNameInt_t* prev = nullptr;

	com_constNameInt_t* a = com_constNameInt;
	while (a) {
		if (!a->fullname || !Q_streq(a->fullname, name)) {
			prev = a;
			a = a->next;
			continue;
		}

		if (prev)
			prev->next = a->next;
		else
			com_constNameInt = a->next;

		prev = nullptr;

		const char* variable = Com_ConstIntGetVariable(name);
		const unsigned int hash = Com_HashKey(variable, CONSTNAMEINT_HASH_SIZE);
		for (com_constNameInt_t* b = com_constNameInt_hash[hash]; b; prev = b, b = b->hash_next) {
			if (!b->fullname)
				continue;
			if (!Q_streq(name, b->fullname))
				continue;

			if (prev)
				prev->hash_next = b->hash_next;
			else
				com_constNameInt_hash[hash] = com_constNameInt_hash[hash]->hash_next;
			break;
		}
		Mem_Free(a->fullname);
		Mem_Free(a);
		return true;
	}
	return false;
}

/**
 * @brief Register mappings between script strings and enum values for values of the type @c V_INT
 * @param name The name of the script entry to map to an integer. This can also include a namespace prefix
 * for the case we want to map back an integer to a string from a specific namespace. In case this string
 * is equipped with a namespace, the string is in the form "namespace::variable"
 * @param value The value to map the given name to
 * @note You still can't register the same name twice even if you put it into different namespaces (yet). The
 * namespaces are only for converting an integer back into a string.
 * @sa Com_GetConstInt
 * @sa Com_UnregisterConstVariable
 */
void Com_RegisterConstInt (const char* name, int value)
{
	com_constNameInt_t* a;
	unsigned int hash;
	const char* variable;

	variable = Com_ConstIntGetVariable(name);

	/* if the alias already exists, reuse it */
	hash = Com_HashKey(variable, CONSTNAMEINT_HASH_SIZE);
	for (a = com_constNameInt_hash[hash]; a; a = a->hash_next) {
		if (a->fullname) {
			if (Q_streq(a->fullname, name))
				break;
		} else if (!strncmp(variable, a->name, sizeof(a->name))) {
			break;
		}
	}

	if (a) {
		Com_Printf("Com_RegisterConstInt: Const string already defined. '%s = %d' is not set.\n", name, value);
		return;
	}

	a = Mem_PoolAllocType(com_constNameInt_t, com_aliasSysPool);
	Q_strncpyz(a->name, variable, sizeof(a->name));
	if (!Q_streq(variable, name))
		a->fullname = Mem_StrDup(name);
	a->next = com_constNameInt;
	/* com_constNameInt_hash should be null on the first run */
	a->hash_next = com_constNameInt_hash[hash];
	com_constNameInt_hash[hash] = a;
	com_constNameInt = a;
	a->value = value;
}

/**
 * @brief Unregisters a list of string aliases
 * @param[in] constList Array of string => int mappings. Must be terminated
 * with a nullptr string ({nullptr, -1}) line
 * @sa constListEntry_t
 */
bool Com_UnregisterConstList (const constListEntry_t constList[])
{
	bool state = true;

	for (int i = 0; constList[i].name != nullptr; i++)
		state &= Com_UnregisterConstVariable(constList[i].name);

	return state;
}

/**
 * @brief Registers a list of string aliases
 * @param[in] constList Array of string => int mappings. Must be terminated
 * with a nullptr string ({nullptr, -1}) line
 * @sa constListEntry_t
 */
void Com_RegisterConstList (const constListEntry_t constList[])
{
	for (int i = 0; constList[i].name != nullptr; i++)
		Com_RegisterConstInt(constList[i].name, constList[i].value);
}

/**
 * Find name type id by is name
 * @return id of the name type, else -1 if not found
 */
static int Com_FindNameType (const char* nameType)
{
	for (int i = 0; i < NAME_NUM_TYPES; i++) {
		if (Q_streq(nameType, name_strings[i])) {
			return i;
		}
	}
	return -1;
}

/**
 * @brief Parsing function that prints an error message when there is no text in the buffer
 * @sa Com_Parse
 */
const char* Com_EParse (const char** text, const char* errhead, const char* errinfo, char* target, size_t size)
{
	const char* token = Com_Parse(text, target, size);
	if (!*text) {
		if (errinfo)
			Com_Printf("%s \"%s\")\n", errhead, errinfo);
		else
			Com_Printf("%s\n", errhead);

		return nullptr;
	}

	return token;
}

static bool versionParsed;

static void Com_ParseVersion (const char* version)
{
	if (!versionParsed) {
		if (!Q_streq(version, UFO_VERSION))
			Sys_Error("You are mixing versions of the binary (" UFO_VERSION ") and the script (%s) files.", version);
	} else {
		Sys_Error("More than one version string found in the script files.");
	}

	versionParsed = true;
}

/**
 * @brief possible values for parsing functions
 * @sa valueTypes_t
 */
const char* const vt_names[] = {
	"",
	"bool",
	"char",
	"int",
	"int2",
	"float",
	"pos",
	"vector",
	"color",
	"string",
	"translation_string",
	"longstring",
	"align",
	"blend",
	"style",
	"fade",
	"shapes",
	"shapeb",
	"damage",
	"date",
	"relabs",
	"hunk_string",
	"team",
	"ufo",
	"ufocrashed",
	"aircrafttype",
	"list"
};
CASSERT(lengthof(vt_names) == V_NUM_TYPES);

const char* const align_names[] = {
	"ul", "uc", "ur", "cl", "cc", "cr", "ll", "lc", "lr", "ul_rsl", "uc_rsl", "ur_rsl", "cl_rsl", "cc_rsl", "cr_rsl", "ll_rsl", "lc_rsl", "lr_rsl"
};
CASSERT(lengthof(align_names) == ALIGN_LAST);

const char* const blend_names[] = {
	"replace", "one", "blend", "add", "filter", "invfilter"
};
CASSERT(lengthof(blend_names) == BLEND_LAST);

const char* const style_names[] = {
	"facing", "rotated", "beam", "line", "axis", "circle"
};
CASSERT(lengthof(style_names) == STYLE_LAST);

const char* const fade_names[] = {
	"none", "in", "out", "sin", "saw"
};
CASSERT(lengthof(fade_names) == FADE_LAST);

/** @brief target sizes for buffer */
static const size_t vt_sizes[] = {
	0,					/* V_NULL */
	sizeof(bool),		/* V_BOOL */
	sizeof(char),		/* V_CHAR */
	sizeof(int),		/* V_INT */
	2 * sizeof(int),	/* V_INT2 */
	sizeof(float),		/* V_FLOAT */
	sizeof(vec2_t),		/* V_POS */
	sizeof(vec3_t),		/* V_VECTOR */
	sizeof(vec4_t),		/* V_COLOR */
	0,					/* V_STRING */
	0,					/* V_TRANSLATION_STRING */
	0,					/* V_LONGSTRING */
	sizeof(align_t),	/* V_ALIGN */
	sizeof(blend_t),	/* V_BLEND */
	sizeof(style_t),	/* V_STYLE */
	sizeof(fade_t),		/* V_FADE */
	sizeof(int),		/* V_SHAPE_SMALL */
	0,					/* V_SHAPE_BIG */
	sizeof(byte),		/* V_DAMAGE */
	0,					/* V_DATE */
	sizeof(float),		/* V_RELABS */
	0,					/* V_HUNK_STRING */
	sizeof(int),		/* V_TEAM */
	sizeof(ufoType_t),	/* V_UFO */
	sizeof(ufoType_t),	/* V_UFOCRASHED */
	sizeof(humanAircraftType_t),		/* V_AIRCRAFTTYPE */
	0					/* V_LIST */
};
CASSERT(lengthof(vt_sizes) == V_NUM_TYPES);

/** @brief natural align for each targets */
static const size_t vt_aligns[] = {
	0,					/* V_NULL */
	sizeof(bool),		/* V_BOOL */
	sizeof(char),		/* V_CHAR */
	sizeof(int),		/* V_INT */
	sizeof(int),		/* V_INT2 */
	sizeof(float),		/* V_FLOAT */
	sizeof(vec_t),		/* V_POS */
	sizeof(vec_t),		/* V_VECTOR */
	sizeof(vec_t),		/* V_COLOR */
	sizeof(char),		/* V_STRING */
	sizeof(char),		/* V_TRANSLATION_STRING */
	sizeof(char),		/* V_LONGSTRING */
	sizeof(align_t),	/* V_ALIGN */
	sizeof(blend_t),	/* V_BLEND */
	sizeof(style_t),	/* V_STYLE */
	sizeof(fade_t),		/* V_FADE */
	sizeof(int),		/* V_SHAPE_SMALL */
	sizeof(uint32_t),	/* V_SHAPE_BIG */
	sizeof(byte),		/* V_DAMAGE */
	sizeof(date_t),		/* V_DATE */
	sizeof(float),		/* V_RELABS */
	sizeof(char),		/* V_HUNK_STRING */
	sizeof(int),		/* V_TEAM */
	sizeof(ufoType_t),	/* V_UFO */
	sizeof(ufoType_t),	/* V_UFOCRASHED */
	sizeof(humanAircraftType_t),	/* V_AIRCRAFTTYPE */
	sizeof(void*)
};
CASSERT(lengthof(vt_aligns) == V_NUM_TYPES);

static char parseErrorMessage[256];

/**
 * Returns the last error message
 * @return string that contains the last error message
 */
const char* Com_GetLastParseError (void)
{
	return parseErrorMessage;
}

/**
 * @brief Align a memory to use a natural address for the data type we will write
 * @note it speed up data read, and fix crash on PPC processors
 */
void* Com_AlignPtr (const void* memory, valueTypes_t type)
{
	const size_t align = vt_aligns[type];
	assert(memory != nullptr);
	if (align == V_NULL)
		Sys_Error("Com_AlignPtr: can't align V_NULL");
	if (type >= V_NUM_TYPES)
		Sys_Error("Com_AlignPtr: type hit V_NUM_TYPES");
	return ALIGN_PTR(memory, align);
}

/**
 * @brief Ufoai uses two types of ids for ufos: the string is used for references in the scripts,
 * the numeric/enum type in the code. This table and the following functions convert these ids
 */
static const char* ufoIdsTable[UFO_MAX];
static short ufoIdsNum;

/**
 * @brief Pretend to parse the aircraft names from the scripts until we have gotten rid of the enum ufoType_t
 */
static void Com_ParseAircraftNames (const char* name, const char** text)
{
	ufoIdsTable[0]	= "scout";
	ufoIdsTable[1]	= "fighter";
	ufoIdsTable[2]	= "harvester";
	ufoIdsTable[3]	= "corrupter";
	ufoIdsTable[4]	= "bomber";
	ufoIdsTable[5]	= "carrier";
	ufoIdsTable[6]	= "supply";
	ufoIdsTable[7]	= "gunboat";
	ufoIdsTable[8]	= "ripper";
	ufoIdsTable[9]	= "mothership";
	ufoIdsNum = 10;
}

static const char* Com_GetUfoDef (ufoType_t idNum)
{
	if (idNum >= 0 && idNum < ufoIdsNum) {
		return ufoIdsTable[idNum];
	}
	return nullptr;
}

static ufoType_t Com_GetUfoIdNum (const char* idString)
{
	if (!strncmp(idString, "craft_ufo_", 10)) {
		for (int i = 0; i < ufoIdsNum; i++)
			if (Q_streq(idString + 10, ufoIdsTable[i]))
				return i;
	}

	return UFO_NONE;
}

static void Com_GetUfoIdStr (ufoType_t idNum, char* outStr)
{
	const char* uDef = Com_GetUfoDef(idNum);
	if (uDef)
		sprintf(outStr, "craft_ufo_%s", uDef);
	else
		outStr[0] = 0;
}

static ufoType_t Com_GetCrashedUfoIdNum (const char* idString)
{
	if (!strncmp(idString, "craft_crash_", 12)) {
		for (int i = 0; i < ufoIdsNum; i++)
			if (Q_streq(idString + 12, ufoIdsTable[i]))
				return i;
	}

	return UFO_NONE;
}

static void Com_GetCrashedUfoIdStr (ufoType_t idNum, char* outStr)
{
	const char* uDef = Com_GetUfoDef(idNum);
	if (uDef)
		sprintf(outStr, "craft_crash_%s", uDef);
	else
		outStr[0] = 0;
}

/**
 * @brief Parse a value from a string
 * @param[in] base The start pointer to a given data type (typedef, struct) where the parsed data is stored
 * @param[in] token The data which should be parsed
 * @param[in] type The data type that should be parsed
 * @param[in] ofs The offset for the value
 * @param[in] size The expected size of the data type. If 0, no checks are done
 * @param[out] writtenBytes
 * @return A resultStatus_t value
 * @note instead of , this function separate error message and write byte result
 * @todo This function has much in common with Com_SetValue. Refactor them !
 */
resultStatus_t Com_ParseValue (void* base, const char* token, valueTypes_t type, int ofs, size_t size, size_t* writtenBytes)
{
	byte* b;
	int x, y, w, h;
	byte num;
	resultStatus_t status = RESULT_OK;
	b = (byte*) base + ofs;
	*writtenBytes = 0;
	ufoType_t ufoType = UFO_NONE;

#ifdef DEBUG
	if (b != Com_AlignPtr(b, type))
		Com_Printf("Wrong alignment: %p %p type:%d size:" UFO_SIZE_T "\n", b, Com_AlignPtr(b, type), type, vt_aligns[type]);
#endif

	if (size) {
#ifdef DEBUG
		if (size > vt_sizes[type]) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Size mismatch: given size: " UFO_SIZE_T ", should be: " UFO_SIZE_T " (type: %i). ", size, vt_sizes[type], type);
			status = RESULT_WARNING;
		}
#endif
		if (size < vt_sizes[type]) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Size mismatch: given size: " UFO_SIZE_T ", should be: " UFO_SIZE_T ". (type: %i)", size, vt_sizes[type], type);
			return RESULT_ERROR;
		}
	}

	switch (type) {
	case V_HUNK_STRING:
		snprintf(parseErrorMessage, sizeof(parseErrorMessage), "V_HUNK_STRING is not parsed here");
		return RESULT_ERROR;

	case V_NULL:
		*writtenBytes = 0;
		break;

	case V_BOOL:
		if (Q_streq(token, "true") || *token == '1')
			*(bool*)b = true;
		else if (Q_streq(token, "false") || *token == '0')
			*(bool*)b = false;
		else {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal bool statement '%s'", token);
			return RESULT_ERROR;
		}
		*writtenBytes = sizeof(bool);
		break;

	case V_CHAR:
		if (token[0] == '\0') {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Char expected, but end of string found");
			return RESULT_ERROR;
		}
		if (token[1] != '\0') {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal end of string. '\\0' explected but 0x%x found", token[1]);
			return RESULT_ERROR;
		}
		*(char*) b = token[0];
		*writtenBytes = sizeof(char);
		break;

	case V_TEAM:
		if (Q_streq(token, "civilian"))
			*(int*) b = TEAM_CIVILIAN;
		else if (Q_streq(token, "phalanx"))
			*(int*) b = TEAM_PHALANX;
		else if (Q_streq(token, "alien"))
			*(int*) b = TEAM_ALIEN;
		else
			Sys_Error("Unknown team string: '%s' found in script files", token);
		*writtenBytes = sizeof(int);
		break;

	case V_AIRCRAFTTYPE:
		if (Q_streq(token, "craft_drop_firebird"))
			*(humanAircraftType_t*) b = DROPSHIP_FIREBIRD;
		else if (Q_streq(token, "craft_drop_herakles"))
			*(humanAircraftType_t*) b = DROPSHIP_HERAKLES;
		else if (Q_streq(token, "craft_drop_raptor"))
			*(humanAircraftType_t*) b = DROPSHIP_RAPTOR;
		else if (Q_streq(token, "craft_inter_stiletto"))
			*(humanAircraftType_t*) b = INTERCEPTOR_STILETTO;
		else if (Q_streq(token, "craft_inter_saracen"))
			*(humanAircraftType_t*) b = INTERCEPTOR_SARACEN;
		else if (Q_streq(token, "craft_inter_dragon"))
			*(humanAircraftType_t*) b = INTERCEPTOR_DRAGON;
		else if (Q_streq(token, "craft_inter_starchaser"))
			*(humanAircraftType_t*) b = INTERCEPTOR_STARCHASER;
		else if (Q_streq(token, "craft_inter_stingray"))
			*(humanAircraftType_t*) b = INTERCEPTOR_STINGRAY;
		else
			Sys_Error("Unknown aircrafttype type: '%s'", token);
		*writtenBytes = sizeof(humanAircraftType_t);
		break;

	case V_UFO:
		ufoType = Com_GetUfoIdNum(token);
		if (ufoType != UFO_NONE)
			*(ufoType_t*) b = ufoType;
		else
			Sys_Error("Unknown ufo type: '%s'", token);
		*writtenBytes = sizeof(ufoType_t);
		break;

	case V_UFOCRASHED:
		ufoType = Com_GetCrashedUfoIdNum(token);
		if (ufoType != UFO_NONE)
			*(ufoType_t*) b = ufoType;
		else
			Sys_Error("Unknown ufo type: '%s'", token);
		*writtenBytes = sizeof(ufoType_t);
		break;

	case V_INT:
		if (sscanf(token, "%i", &((int*) b)[0]) != 1) {
			if (!Com_GetConstInt(token, &((int*) b)[0])) {
				snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal int statement '%s'", token);
				return RESULT_ERROR;
			}
		}
		*writtenBytes = sizeof(int);
		break;

	case V_INT2:
		if (sscanf(token, "%i %i", &((int*) b)[0], &((int*) b)[1]) != 2) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal int2 statement '%s'", token);
			return RESULT_ERROR;
		}
		*writtenBytes = 2 * sizeof(int);
		break;

	case V_FLOAT:
		if (sscanf(token, "%f", &((float*) b)[0]) != 1) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal float statement '%s'", token);
			return RESULT_ERROR;
		}
		*writtenBytes = sizeof(float);
		break;

	case V_POS:
		if (sscanf(token, "%f %f", &((float*) b)[0], &((float*) b)[1]) != 2) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal pos statement '%s'", token);
			return RESULT_ERROR;
		}
		*writtenBytes = 2 * sizeof(float);
		break;

	case V_VECTOR:
		if (sscanf(token, "%f %f %f", &((float*) b)[0], &((float*) b)[1], &((float*) b)[2]) != 3) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal vector statement '%s'", token);
			return RESULT_ERROR;
		}
		*writtenBytes = 3 * sizeof(float);
		break;

	case V_COLOR:
		{
			float* f = (float*) b;
			if (sscanf(token, "%f %f %f %f", &f[0], &f[1], &f[2], &f[3]) != 4) {
				snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal color statement '%s'", token);
				return RESULT_ERROR;
			}
			*writtenBytes = 4 * sizeof(float);
		}
		break;

	case V_STRING:
		Q_strncpyz((char*) b, token, MAX_VAR);
		w = (int)strlen(token) + 1;
		*writtenBytes = w;
		break;

	/* just remove the _ but don't translate */
	case V_TRANSLATION_STRING:
		if (*token == '_')
			token++;

		Q_strncpyz((char*) b, token, MAX_VAR);
		w = (int)strlen((char*) b) + 1;
		*writtenBytes = w;
		break;

	case V_LONGSTRING:
		strcpy((char*) b, token);
		w = (int)strlen(token) + 1;
		*writtenBytes = w;
		break;

	case V_ALIGN:
		for (num = 0; num < ALIGN_LAST; num++)
			if (Q_streq(token, align_names[num]))
				break;
		if (num == ALIGN_LAST) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal align token '%s'", token);
			return RESULT_ERROR;
		}
		*(align_t*)b = (align_t)num;
		*writtenBytes = sizeof(align_t);
		break;

	case V_BLEND:
		for (num = 0; num < BLEND_LAST; num++)
			if (Q_streq(token, blend_names[num]))
				break;
		if (num == BLEND_LAST) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal blend token '%s'", token);
			return RESULT_ERROR;
		}
		*(blend_t*)b = (blend_t)num;
		*writtenBytes = sizeof(blend_t);
		break;

	case V_STYLE:
		for (num = 0; num < STYLE_LAST; num++)
			if (Q_streq(token, style_names[num]))
				break;
		if (num == STYLE_LAST) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal style token '%s'", token);
			return RESULT_ERROR;
		}
		*(style_t*)b = (style_t)num;
		*writtenBytes = sizeof(style_t);
		break;

	case V_FADE:
		for (num = 0; num < FADE_LAST; num++)
			if (Q_streq(token, fade_names[num]))
				break;
		if (num == FADE_LAST) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal fade token '%s'", token);
			return RESULT_ERROR;
		}
		*(fade_t*)b = (fade_t)num;
		*writtenBytes = sizeof(fade_t);
		break;

	case V_SHAPE_SMALL:
		if (sscanf(token, "%i %i %i %i", &x, &y, &w, &h) != 4) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal shape small statement '%s'", token);
			return RESULT_ERROR;
		}

		if (y + h > SHAPE_SMALL_MAX_HEIGHT || y >= SHAPE_SMALL_MAX_HEIGHT || h > SHAPE_SMALL_MAX_HEIGHT) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "illegal shape small statement - max h value is %i (y: %i, h: %i)", SHAPE_SMALL_MAX_HEIGHT, y, h);
			return RESULT_ERROR;
		}
		if (x + w > SHAPE_SMALL_MAX_WIDTH || x >= SHAPE_SMALL_MAX_WIDTH || w > SHAPE_SMALL_MAX_WIDTH) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "illegal shape small statement - max x and w values are %i", SHAPE_SMALL_MAX_WIDTH);
			return RESULT_ERROR;
		}
		for (h += y; y < h; y++)
			*(uint32_t*) b |= ((1 << w) - 1) << x << (y * SHAPE_SMALL_MAX_WIDTH);
		*writtenBytes = SHAPE_SMALL_MAX_HEIGHT;
		break;

	case V_SHAPE_BIG:
		if (sscanf(token, "%i %i %i %i", &x, &y, &w, &h) != 4) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal shape big statement '%s'", token);
			return RESULT_ERROR;
		}
		if (y + h > SHAPE_BIG_MAX_HEIGHT || y >= SHAPE_BIG_MAX_HEIGHT || h > SHAPE_BIG_MAX_HEIGHT) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal shape big statement, max height is %i", SHAPE_BIG_MAX_HEIGHT);
			return RESULT_ERROR;
		}
		if (x + w > SHAPE_BIG_MAX_WIDTH || x >= SHAPE_BIG_MAX_WIDTH || w > SHAPE_BIG_MAX_WIDTH) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "illegal shape big statement - max x and w values are %i ('%s')", SHAPE_BIG_MAX_WIDTH, token);
			return RESULT_ERROR;
		}
		w = ((1 << w) - 1) << x;
		for (h += y; y < h; y++)
			((uint32_t*) b)[y] |= w;
		*writtenBytes = SHAPE_BIG_MAX_HEIGHT * SHAPE_SMALL_MAX_HEIGHT;
		break;

	case V_DAMAGE:
		for (num = 0; num < csi.numDTs; num++)
			if (Q_streq(token, csi.dts[num].id))
				break;
		if (num == csi.numDTs)
			*b = 0;
		else
			*b = num;
		*writtenBytes = sizeof(byte);
		break;

	case V_DATE:
		if (sscanf(token, "%i %i %i", &x, &y, &w) != 3) {
			snprintf(parseErrorMessage, sizeof(parseErrorMessage), "Illegal if statement '%s'", token);
			return RESULT_ERROR;
		}

		((date_t*) b)->day = DAYS_PER_YEAR * x + y;
		((date_t*) b)->sec = SECONDS_PER_HOUR * w;
		*writtenBytes = sizeof(date_t);
		break;

	case V_RELABS:
		if (token[0] == '-' || token[0] == '+') {
			if (fabs(atof(token + 1)) <= 2.0f) {
				snprintf(parseErrorMessage, sizeof(parseErrorMessage), "a V_RELABS (absolute) value should always be bigger than +/-2.0");
				status = RESULT_WARNING;
			}
			if (token[0] == '-')
				*(float*) b = atof(token + 1) * (-1);
			else
				*(float*) b = atof(token + 1);
		} else {
			if (fabs(atof(token)) > 2.0f) {
				snprintf(parseErrorMessage, sizeof(parseErrorMessage), "a V_RELABS (relative) value should only be between 0.00..1 and 2.0");
				status = RESULT_WARNING;
			}
			*(float*) b = atof(token);
		}
		*writtenBytes = sizeof(float);
		break;

	default:
		snprintf(parseErrorMessage, sizeof(parseErrorMessage), "unknown value type '%s'", token);
		return RESULT_ERROR;
	}
	return status;
}

/**
 * @note translatable string are marked with _ at the beginning
 * @code menu exampleName
 * {
 *   string "_this is translatable"
 * }
 * @endcode
 */
int Com_EParseValue (void* base, const char* token, valueTypes_t type, int ofs, size_t size)
{
	size_t writtenBytes;
	const resultStatus_t result = Com_ParseValue(base, token, type, ofs, size, &writtenBytes);
	switch (result) {
	case RESULT_ERROR:
		Sys_Error("Com_EParseValue: %s\n", parseErrorMessage);
		break;
	case RESULT_WARNING:
		Com_Printf("Com_EParseValue: %s\n", parseErrorMessage);
		break;
	case RESULT_OK:
		break;
	}
	return writtenBytes;
}

/**
 * @brief Parses a boolean from a string
 * @param token The token to convert into a boolean
 * @return @c false if the string could not get parsed
 */
bool Com_ParseBoolean (const char* token)
{
	bool b;
	size_t writtenBytes;
	if (Com_ParseValue(&b, token, V_BOOL, 0, sizeof(b), &writtenBytes) != RESULT_ERROR) {
		assert(writtenBytes == sizeof(b));
		return b;
	}
	return false;
}

/**
 * @param[in] base The start pointer to a given data type (typedef, struct)
 * @param[in] set The data which should be parsed
 * @param[in] type The data type that should be parsed
 * @param[in] ofs The offset for the value
 * @param[in] size The expected size of the data type. If 0, no checks are done
 * @sa Com_ValueToStr
 * @note The offset is most likely given by the offsetof macro
 */
#ifdef DEBUG
int Com_SetValueDebug (void* base, const void* set, valueTypes_t type, int ofs, size_t size, const char* file, int line)
#else
int Com_SetValue (void* base, const void* set, valueTypes_t type, int ofs, size_t size)
#endif
{
	byte* b;
	int len;
	ufoType_t ufoType = UFO_NONE;

	b = (byte*) base + ofs;

	if (size) {
#ifdef DEBUG
		if (size > vt_sizes[type])
			Com_Printf("Warning: Size mismatch: given size: " UFO_SIZE_T ", should be: " UFO_SIZE_T ". File: '%s', line: %i (type: %i)\n", size, vt_sizes[type], file, line, type);

		if (size < vt_sizes[type])
			Sys_Error("Size mismatch: given size: " UFO_SIZE_T ", should be: " UFO_SIZE_T ". File: '%s', line: %i (type: %i)", size, vt_sizes[type], file, line, type);
#else
		if (size < vt_sizes[type])
			Sys_Error("Size mismatch: given size: " UFO_SIZE_T ", should be: " UFO_SIZE_T ". (type: %i)", size, vt_sizes[type], type);
#endif
	}

#ifdef DEBUG
	if (b != Com_AlignPtr(b, type)) {
		Com_Printf("Wrong alignment: %p %p type:%d size:" UFO_SIZE_T " - this code will CRASH on ARM CPU\n", b, Com_AlignPtr(b, type), type, vt_aligns[type]);
		Sys_Backtrace();
	}
#endif

	switch (type) {
	case V_NULL:
		return 0;

	case V_BOOL:
		if (*(const bool*) set)
			*(bool*)b = true;
		else
			*(bool*)b = false;
		return sizeof(bool);

	case V_CHAR:
		*(char*) b = *(const char*) set;
		return sizeof(char);

	case V_TEAM:
		if (Q_streq((const char*)set, "civilian"))
			*(int*) b = TEAM_CIVILIAN;
		else if (Q_streq((const char*)set, "phalanx"))
			*(int*) b = TEAM_PHALANX;
		else if (Q_streq((const char*)set, "alien"))
			*(int*) b = TEAM_ALIEN;
		else
			Sys_Error("Unknown team given: '%s'", (const char*)set);
		return sizeof(int);

	case V_AIRCRAFTTYPE:
		if (Q_streq((const char*)set, "craft_drop_firebird"))
			*(humanAircraftType_t*) b = DROPSHIP_FIREBIRD;
		else if (Q_streq((const char*)set, "craft_drop_herakles"))
			*(humanAircraftType_t*) b = DROPSHIP_HERAKLES;
		else if (Q_streq((const char*)set, "craft_drop_raptor"))
			*(humanAircraftType_t*) b = DROPSHIP_RAPTOR;
		else if (Q_streq((const char*)set, "craft_inter_stiletto"))
			*(humanAircraftType_t*) b = INTERCEPTOR_STILETTO;
		else if (Q_streq((const char*)set, "craft_inter_saracen"))
			*(humanAircraftType_t*) b = INTERCEPTOR_SARACEN;
		else if (Q_streq((const char*)set, "craft_inter_dragon"))
			*(humanAircraftType_t*) b = INTERCEPTOR_DRAGON;
		else if (Q_streq((const char*)set, "craft_inter_starchaser"))
			*(humanAircraftType_t*) b = INTERCEPTOR_STARCHASER;
		else if (Q_streq((const char*)set, "craft_inter_stingray"))
			*(humanAircraftType_t*) b = INTERCEPTOR_STINGRAY;
		else
			Sys_Error("Unknown aircrafttype type: '%s'", (const char*)set);
		return sizeof(humanAircraftType_t);

	case V_UFO:
		ufoType = Com_GetUfoIdNum((const char*)set);
		if (ufoType != UFO_NONE)
			*(ufoType_t*) b = ufoType;
		else
			Sys_Error("Unknown ufo type: '%s'", (const char*)set);
		return sizeof(ufoType_t);

	case V_UFOCRASHED:
		ufoType = Com_GetCrashedUfoIdNum((const char*)set);
		if (ufoType != UFO_NONE)
			*(ufoType_t*) b = ufoType;
		else
			Sys_Error("Unknown ufo type: '%s'", (const char*)set);
		return sizeof(ufoType_t);

	case V_INT:
		*(int*) b = *(const int*) set;
		return sizeof(int);

	case V_INT2:
		((int*) b)[0] = ((const int*) set)[0];
		((int*) b)[1] = ((const int*) set)[1];
		return 2 * sizeof(int);

	case V_FLOAT:
		*(float*) b = *(const float*) set;
		return sizeof(float);

	case V_POS:
		((float*) b)[0] = ((const float*) set)[0];
		((float*) b)[1] = ((const float*) set)[1];
		return 2 * sizeof(float);

	case V_VECTOR:
		((float*) b)[0] = ((const float*) set)[0];
		((float*) b)[1] = ((const float*) set)[1];
		((float*) b)[2] = ((const float*) set)[2];
		return 3 * sizeof(float);

	case V_COLOR:
		((float*) b)[0] = ((const float*) set)[0];
		((float*) b)[1] = ((const float*) set)[1];
		((float*) b)[2] = ((const float*) set)[2];
		((float*) b)[3] = ((const float*) set)[3];
		return 4 * sizeof(float);

	case V_STRING:
		Q_strncpyz((char*) b, (const char*) set, MAX_VAR);
		len = (int)strlen((const char*) set) + 1;
		if (len > MAX_VAR)
			len = MAX_VAR;
		return len;

	case V_LONGSTRING:
		strcpy((char*) b, (const char*) set);
		len = (int)strlen((const char*) set) + 1;
		return len;

	case V_ALIGN:
		*(align_t*)b = *(const align_t*) set;
		return sizeof(align_t);

	case V_BLEND:
		*(blend_t*)b = *(const blend_t*) set;
		return sizeof(blend_t);

	case V_STYLE:
		*(style_t*)b = *(const style_t*) set;
		return sizeof(style_t);

	case V_FADE:
		*(fade_t*)b = *(const fade_t*) set;
		return sizeof(fade_t);

	case V_SHAPE_SMALL:
		*(int*) b = *(const int*) set;
		return SHAPE_SMALL_MAX_HEIGHT;

	case V_SHAPE_BIG:
		memcpy(b, set, 64);
		return SHAPE_BIG_MAX_HEIGHT * 4;

	case V_DAMAGE:
		*b = *(const byte*) set;
		return 1;

	case V_DATE:
		memcpy(b, set, sizeof(date_t));
		return sizeof(date_t);

	default:
		Sys_Error("Com_SetValue: unknown value type\n");
	}
}

/**
 * @param[in] base The start pointer to a given data type (typedef, struct)
 * @param[in] type The data type that should be parsed
 * @param[in] ofs The offset for the value
 * @sa Com_SetValue
 * @return char pointer with translated data type value
 */
const char* Com_ValueToStr (const void* base, const valueTypes_t type, const int ofs)
{
	static char valuestr[MAX_VAR];
	const byte* b;

	b = (const byte*) base + ofs;

#ifdef DEBUG
	if (b != Com_AlignPtr(b, type)) {
		Com_Printf("Wrong alignment: %p %p type:%d size:" UFO_SIZE_T " - this code will CRASH on ARM CPU\n", b, Com_AlignPtr(b, type), type, vt_aligns[type]);
		Sys_Backtrace();
	}
#endif

	switch (type) {
	case V_NULL:
		return 0;

	case V_HUNK_STRING:
		if (b == nullptr)
			return "(null)";
		else
			return (const char*)b;

	case V_BOOL:
		if (*(const bool*)b)
			return "true";
		else
			return "false";

	case V_CHAR:
		return (const char*) b;
		break;

	case V_TEAM:
		switch (*(const int*) b) {
		case TEAM_CIVILIAN:
			return "civilian";
		case TEAM_PHALANX:
			return "phalanx";
		case TEAM_ALIEN:
			return "alien";
		default:
			Sys_Error("Unknown team id '%i'", *(const int*) b);
		}

	case V_AIRCRAFTTYPE:
		switch (*(const humanAircraftType_t*) b) {
		case DROPSHIP_FIREBIRD:
			return "craft_drop_firebird";
		case DROPSHIP_HERAKLES:
			return "craft_drop_herakles";
		case DROPSHIP_RAPTOR:
			return "craft_drop_raptor";
		case INTERCEPTOR_STILETTO:
			return "craft_inter_stiletto";
		case INTERCEPTOR_SARACEN:
			return "craft_inter_saracen";
		case INTERCEPTOR_DRAGON:
			return "craft_inter_dragon";
		case INTERCEPTOR_STARCHASER:
			return "craft_inter_starchaser";
		case INTERCEPTOR_STINGRAY:
			return "craft_inter_stingray";
		default:
			Sys_Error("Unknown aircrafttype type: '%i'", *(const humanAircraftType_t*) b);
		}

	case V_UFO:
		Com_GetUfoIdStr(*(const ufoType_t*) b, valuestr);
		if (valuestr[0])
			return valuestr;
		else
			Sys_Error("Unknown ufo type: '%i'", *(const ufoType_t*) b);

	case V_UFOCRASHED:
		Com_GetCrashedUfoIdStr(*(const ufoType_t*) b, valuestr);
		if (valuestr[0])
			return valuestr;
		else
			Sys_Error("Unknown crashed ufo type: '%i'", *(const ufoType_t*) b);

	case V_INT:
		Com_sprintf(valuestr, sizeof(valuestr), "%i", *(const int*) b);
		return valuestr;

	case V_INT2:
		Com_sprintf(valuestr, sizeof(valuestr), "%i %i", ((const int*) b)[0], ((const int*) b)[1]);
		return valuestr;

	case V_FLOAT:
		Com_sprintf(valuestr, sizeof(valuestr), "%.2f", *(const float*) b);
		return valuestr;

	case V_POS:
		Com_sprintf(valuestr, sizeof(valuestr), "%.2f %.2f", ((const float*) b)[0], ((const float*) b)[1]);
		return valuestr;

	case V_VECTOR:
		Com_sprintf(valuestr, sizeof(valuestr), "%.2f %.2f %.2f", ((const float*) b)[0], ((const float*) b)[1], ((const float*) b)[2]);
		return valuestr;

	case V_COLOR:
		Com_sprintf(valuestr, sizeof(valuestr), "%.2f %.2f %.2f %.2f", ((const float*) b)[0], ((const float*) b)[1], ((const float*) b)[2], ((const float*) b)[3]);
		return valuestr;

	case V_TRANSLATION_STRING:
	case V_STRING:
	case V_LONGSTRING:
		assert(b);	/* this should never happen. let's see */
		return (const char*) b;

	case V_ALIGN:
		assert(*(const align_t*)b < ALIGN_LAST);
		Q_strncpyz(valuestr, align_names[*(const align_t*)b], sizeof(valuestr));
		return valuestr;

	case V_BLEND:
		assert(*(const blend_t*)b < BLEND_LAST);
		Q_strncpyz(valuestr, blend_names[*(const blend_t*)b], sizeof(valuestr));
		return valuestr;

	case V_STYLE:
		assert(*(const style_t*)b < STYLE_LAST);
		Q_strncpyz(valuestr, style_names[*(const style_t*)b], sizeof(valuestr));
		return valuestr;

	case V_FADE:
		assert(*(const fade_t*)b < FADE_LAST);
		Q_strncpyz(valuestr, fade_names[*(const fade_t*)b], sizeof(valuestr));
		return valuestr;

	case V_SHAPE_SMALL:
	case V_SHAPE_BIG:
		return "";

	case V_DAMAGE:
		assert(*(const byte*)b < MAX_DAMAGETYPES);
		return csi.dts[*(const byte*)b].id;

	case V_DATE:
		Com_sprintf(valuestr, sizeof(valuestr), "%i %i %i", ((const date_t*) b)->day / DAYS_PER_YEAR, ((const date_t*) b)->day % DAYS_PER_YEAR, ((const date_t*) b)->sec);
		return valuestr;

	case V_RELABS:
		/* absolute value */
		if (*(const float*) b > 2.0)
			Com_sprintf(valuestr, sizeof(valuestr), "+%.2f", *(const float*) b);
		/* absolute value */
		else if (*(const float*) b < 2.0)
			Com_sprintf(valuestr, sizeof(valuestr), "-%.2f", *(const float*) b);
		/* relative value */
		else
			Com_sprintf(valuestr, sizeof(valuestr), "%.2f", *(const float*) b);
		return valuestr;

	default:
		Sys_Error("Com_ValueToStr: unknown value type %i\n", type);
	}
}

bool Com_ParseBlockToken (const char* name, const char** text, void* base, const value_t* values, memPool_t* mempool, const char* token)
{
	const value_t* v;
	const char* errhead = "Com_ParseBlockToken: unexpected end of file (";

	for (v = values; v->string; v++)
		if (Q_streq(token, v->string)) {
			/* found a definition */
			token = Com_EParse(text, errhead, name);
			if (!*text)
				return false;

			switch (v->type) {
			case V_TRANSLATION_STRING:
				if (mempool == nullptr) {
					if (Com_EParseValue(base, token, v->type, v->ofs, v->size) == -1)
						Com_Printf("Com_ParseBlockToken: Wrong size for value %s\n", v->string);
					break;
				} else {
					if (*token == '_')
						token++;
					/* fall through */
				}
			case V_HUNK_STRING:
				Mem_PoolStrDupTo(token, &Com_GetValue<char*>(base, v), mempool, 0);
				break;
			case V_LIST: {
				linkedList_t*& list = Com_GetValue<linkedList_t*>(base, v);
				assert(!list);
				Com_UnParseLastToken();
				if (!Com_ParseList(text, &list)) {
					return false;
				}
				break;
			}
			default:
				if (Com_EParseValue(base, token, v->type, v->ofs, v->size) == -1)
					Com_Printf("Com_ParseBlockToken: Wrong size for value %s\n", v->string);
				break;
			}
			break;
		}

	return v->string != nullptr;
}

/**
 * Parse tokens between '(' and ')' and return them into a linked list.
 * It the list is not well formed, the returned list is null.
 * @param[in] text Pointer to a token stream
 * @param[out] list list to return
 * @return True if the list is well formed, else false.
 */
bool Com_ParseList (const char** text, linkedList_t** list)
{
	*list = nullptr;

	if (Com_NextToken(text) != TT_BEGIN_LIST) {
		Com_Printf("Com_ParseList: expected '(' but \"%s\" found\n", Com_GetToken(text));
		return false;
	}

	while (true) {
		Com_TokenType_t type = Com_NextToken(text);
		if (type == TT_END_LIST)
			break;
		if (type == TT_EOF) {
			Com_Printf("Com_ParseList: expected list content but end of file found\n");
			LIST_Delete(list);
			return false;
		}
		if (type < TT_CONTENT) {
			Com_Printf("Com_ParseList: expected list content but \"%s\" found\n", Com_GetToken(text));
			LIST_Delete(list);
			return false;
		}
		// read content
		LIST_AddString(list, Com_GetToken(text));
	}

	return true;
}

bool Com_ParseBlock (const char* name, const char** text, void* base, const value_t* values, memPool_t* mempool)
{
	const char* errhead = "Com_ParseBlock: unexpected end of file (";
	const char* token;

	/* get name/id */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseBlock: block \"%s\" without body ignored\n", name);
		return false;
	}

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;
		if (!Com_ParseBlockToken(name, text, base, values, mempool, token))
			Com_Printf("Com_ParseBlock: unknown token '%s' ignored (%s)\n", token, name);
	} while (*text);

	return true;
}

/*
==============================================================================
OBJECT DEFINITION INTERPRETER
==============================================================================
*/

static const char* const skillNames[SKILL_NUM_TYPES + 1] = {
	"strength",
	"speed",
	"accuracy",
	"mind",
	"close",
	"heavy",
	"assault",
	"sniper",
	"explosive",
	"piloting",
	"targeting",
	"evading",
	"health"
};

/** @brief The order here must be the same as in od_vals */
enum {
	OD_WEAPON,			/**< parse a weapon */
	OD_PROTECTION,		/**< parse armour protection values */
	OD_RATINGS			/**< parse rating values for displaying in the menus */
};

static const value_t od_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(objDef_t, name), 0},
	{"armourpath", V_HUNK_STRING, offsetof(objDef_t, armourPath), 0},
	{"model", V_HUNK_STRING, offsetof(objDef_t, model), 0},
	{"image", V_HUNK_STRING, offsetof(objDef_t, image), 0},
	{"type", V_HUNK_STRING, offsetof(objDef_t, type), 0},
	{"reloadsound", V_HUNK_STRING, offsetof(objDef_t, reloadSound), 0},
	{"animationindex", V_CHAR, offsetof(objDef_t, animationIndex), MEMBER_SIZEOF(objDef_t, animationIndex)},
	{"shape", V_SHAPE_SMALL, offsetof(objDef_t, shape), MEMBER_SIZEOF(objDef_t, shape)},
	{"scale", V_FLOAT, offsetof(objDef_t, scale), MEMBER_SIZEOF(objDef_t, scale)},
	{"center", V_VECTOR, offsetof(objDef_t, center), MEMBER_SIZEOF(objDef_t, center)},
	{"weapon", V_BOOL, offsetof(objDef_t, weapon), MEMBER_SIZEOF(objDef_t, weapon)},
	{"holdtwohanded", V_BOOL, offsetof(objDef_t, holdTwoHanded), MEMBER_SIZEOF(objDef_t, holdTwoHanded)},
	{"firetwohanded", V_BOOL, offsetof(objDef_t, fireTwoHanded), MEMBER_SIZEOF(objDef_t, fireTwoHanded)},
	{"implant", V_BOOL, offsetof(objDef_t, implant), MEMBER_SIZEOF(objDef_t, implant)},
	{"headgear", V_BOOL, offsetof(objDef_t, headgear), MEMBER_SIZEOF(objDef_t, headgear)},
	{"thrown", V_BOOL, offsetof(objDef_t, thrown), MEMBER_SIZEOF(objDef_t, thrown)},
	{"ammo", V_INT, offsetof(objDef_t, ammo), MEMBER_SIZEOF(objDef_t, ammo)},
	{"oneshot", V_BOOL, offsetof(objDef_t, oneshot), MEMBER_SIZEOF(objDef_t, oneshot)},
	{"deplete", V_BOOL, offsetof(objDef_t, deplete), MEMBER_SIZEOF(objDef_t, deplete)},
	{"reload", V_INT, offsetof(objDef_t, _reload), MEMBER_SIZEOF(objDef_t, _reload)},
	{"reloadattenuation", V_FLOAT, offsetof(objDef_t, reloadAttenuation), MEMBER_SIZEOF(objDef_t, reloadAttenuation)},
	{"size", V_INT, offsetof(objDef_t, size), MEMBER_SIZEOF(objDef_t, size)},
	{"weight", V_INT, offsetof(objDef_t, weight), MEMBER_SIZEOF(objDef_t, weight)},
	{"price", V_INT, offsetof(objDef_t, price), MEMBER_SIZEOF(objDef_t, price)},
	{"productioncost", V_INT, offsetof(objDef_t, productionCost), MEMBER_SIZEOF(objDef_t, productionCost)},
	{"useable", V_TEAM, offsetof(objDef_t, useable), MEMBER_SIZEOF(objDef_t, useable)},
	{"notonmarket", V_BOOL, offsetof(objDef_t, notOnMarket), MEMBER_SIZEOF(objDef_t, notOnMarket)},

	{"installationTime", V_INT, offsetof(objDef_t, craftitem.installationTime), MEMBER_SIZEOF(objDef_t, craftitem.installationTime)},
	{"bullets", V_BOOL, offsetof(objDef_t, craftitem.bullets), MEMBER_SIZEOF(objDef_t, craftitem.bullets)},
	{"beam", V_BOOL, offsetof(objDef_t, craftitem.beam), MEMBER_SIZEOF(objDef_t, craftitem.beam)},
	{"beamcolor", V_COLOR, offsetof(objDef_t, craftitem.beamColor), MEMBER_SIZEOF(objDef_t, craftitem.beamColor)},
	{"wdamage", V_FLOAT, offsetof(objDef_t, craftitem.weaponDamage), MEMBER_SIZEOF(objDef_t, craftitem.weaponDamage)},
	{"wspeed", V_FLOAT, offsetof(objDef_t, craftitem.weaponSpeed), MEMBER_SIZEOF(objDef_t, craftitem.weaponSpeed)},
	{"delay", V_FLOAT, offsetof(objDef_t, craftitem.weaponDelay), MEMBER_SIZEOF(objDef_t, craftitem.weaponDelay)},
	{"shield", V_FLOAT, offsetof(objDef_t, craftitem.stats[AIR_STATS_SHIELD]), MEMBER_SIZEOF(objDef_t, craftitem.stats[AIR_STATS_SHIELD])},
	{"wrange", V_FLOAT, offsetof(objDef_t, craftitem.stats[AIR_STATS_WRANGE]), MEMBER_SIZEOF(objDef_t, craftitem.stats[AIR_STATS_WRANGE])},
	{"damage", V_RELABS, offsetof(objDef_t, craftitem.stats[AIR_STATS_DAMAGE]), MEMBER_SIZEOF(objDef_t, craftitem.stats[AIR_STATS_DAMAGE])},
	{"accuracy", V_RELABS, offsetof(objDef_t, craftitem.stats[AIR_STATS_ACCURACY]), MEMBER_SIZEOF(objDef_t, craftitem.stats[AIR_STATS_ACCURACY])},
	{"ecm", V_RELABS, offsetof(objDef_t, craftitem.stats[AIR_STATS_ECM]), MEMBER_SIZEOF(objDef_t, craftitem.stats[AIR_STATS_ECM])},
	{"speed", V_RELABS, offsetof(objDef_t, craftitem.stats[AIR_STATS_SPEED]), MEMBER_SIZEOF(objDef_t, craftitem.stats[AIR_STATS_SPEED])},
	{"maxspeed", V_RELABS, offsetof(objDef_t, craftitem.stats[AIR_STATS_MAXSPEED]), MEMBER_SIZEOF(objDef_t, craftitem.stats[AIR_STATS_SPEED])},
	{"fuelsize", V_RELABS, offsetof(objDef_t, craftitem.stats[AIR_STATS_FUELSIZE]), MEMBER_SIZEOF(objDef_t, craftitem.stats[AIR_STATS_FUELSIZE])},
	{"dmgtype", V_DAMAGE, offsetof(objDef_t, dmgtype), MEMBER_SIZEOF(objDef_t, dmgtype)},

	{"is_primary", V_BOOL, offsetof(objDef_t, isPrimary), MEMBER_SIZEOF(objDef_t, isPrimary)},
	{"is_secondary", V_BOOL, offsetof(objDef_t, isSecondary), MEMBER_SIZEOF(objDef_t, isSecondary)},
	{"is_heavy", V_BOOL, offsetof(objDef_t, isHeavy), MEMBER_SIZEOF(objDef_t, isHeavy)},
	{"is_misc", V_BOOL, offsetof(objDef_t, isMisc), MEMBER_SIZEOF(objDef_t, isMisc)},
	{"is_ugvitem", V_BOOL, offsetof(objDef_t, isUGVitem), MEMBER_SIZEOF(objDef_t, isUGVitem)},
	{"is_dummy", V_BOOL, offsetof(objDef_t, isDummy), MEMBER_SIZEOF(objDef_t, isDummy)},
	{"virtual", V_BOOL, offsetof(objDef_t, isVirtual), MEMBER_SIZEOF(objDef_t, isVirtual)},

	{nullptr, V_NULL, 0, 0}
};

static const value_t effect_vals[] = {
	{"period", V_INT, offsetof(itemEffect_t, period), MEMBER_SIZEOF(itemEffect_t, period)},
	{"duration", V_INT, offsetof(itemEffect_t, duration), MEMBER_SIZEOF(itemEffect_t, duration)},
	{"permanent", V_BOOL, offsetof(itemEffect_t, isPermanent), MEMBER_SIZEOF(itemEffect_t, isPermanent)},

	{"accuracy", V_FLOAT, offsetof(itemEffect_t, accuracy), MEMBER_SIZEOF(itemEffect_t, accuracy)},
	{"tu", V_FLOAT, offsetof(itemEffect_t, TUs), MEMBER_SIZEOF(itemEffect_t, TUs)},
	{"power", V_FLOAT, offsetof(itemEffect_t, power), MEMBER_SIZEOF(itemEffect_t, power)},
	{"mind", V_FLOAT, offsetof(itemEffect_t, mind), MEMBER_SIZEOF(itemEffect_t, mind)},
	{"morale", V_FLOAT, offsetof(itemEffect_t, morale), MEMBER_SIZEOF(itemEffect_t, morale)},

	{nullptr, V_NULL, 0, 0}
};

/* =========================================================== */

static const value_t fdps[] = {
	{"name", V_TRANSLATION_STRING, offsetof(fireDef_t, name), 0},
	{"shotorg", V_POS, offsetof(fireDef_t, shotOrg), MEMBER_SIZEOF(fireDef_t, shotOrg)},
	{"projtl", V_HUNK_STRING, offsetof(fireDef_t, projectile), 0},
	{"impact", V_HUNK_STRING, offsetof(fireDef_t, impact), 0},
	{"hitbody", V_HUNK_STRING, offsetof(fireDef_t, hitBody), 0},
	{"firesnd", V_HUNK_STRING, offsetof(fireDef_t, fireSound), 0},
	{"impsnd", V_HUNK_STRING, offsetof(fireDef_t, impactSound), 0},
	{"bodysnd", V_HUNK_STRING, offsetof(fireDef_t, hitBodySound), 0},
	{"bncsnd", V_HUNK_STRING, offsetof(fireDef_t, bounceSound), 0},
	{"fireattenuation", V_FLOAT, offsetof(fireDef_t, fireAttenuation), MEMBER_SIZEOF(fireDef_t, fireAttenuation)},
	{"impactattenuation", V_FLOAT, offsetof(fireDef_t, impactAttenuation), MEMBER_SIZEOF(fireDef_t, impactAttenuation)},
	{"throughwall", V_INT, offsetof(fireDef_t, throughWall), MEMBER_SIZEOF(fireDef_t, throughWall)},
	{"sndonce", V_BOOL, offsetof(fireDef_t, soundOnce), MEMBER_SIZEOF(fireDef_t, soundOnce)},
	{"gravity", V_BOOL, offsetof(fireDef_t, gravity), MEMBER_SIZEOF(fireDef_t, gravity)},
	{"launched", V_BOOL, offsetof(fireDef_t, launched), MEMBER_SIZEOF(fireDef_t, launched)},
	{"rolled", V_BOOL, offsetof(fireDef_t, rolled), MEMBER_SIZEOF(fireDef_t, rolled)},
	{"reaction", V_BOOL, offsetof(fireDef_t, reaction), MEMBER_SIZEOF(fireDef_t, reaction)},
	{"delay", V_INT, offsetof(fireDef_t, delay), MEMBER_SIZEOF(fireDef_t, delay)},
	{"bounce", V_INT, offsetof(fireDef_t, bounce), MEMBER_SIZEOF(fireDef_t, bounce)},
	{"bncfac", V_FLOAT, offsetof(fireDef_t, bounceFac), MEMBER_SIZEOF(fireDef_t, bounceFac)},
	{"speed", V_FLOAT, offsetof(fireDef_t, speed), MEMBER_SIZEOF(fireDef_t, speed)},
	{"spread", V_POS, offsetof(fireDef_t, spread), MEMBER_SIZEOF(fireDef_t, spread)},
	{"crouch", V_FLOAT, offsetof(fireDef_t, crouch), MEMBER_SIZEOF(fireDef_t, crouch)},
	{"shots", V_INT, offsetof(fireDef_t, shots), MEMBER_SIZEOF(fireDef_t, shots)},
	{"ammo", V_INT, offsetof(fireDef_t, ammo), MEMBER_SIZEOF(fireDef_t, ammo)},
	{"delaybetweenshots", V_FLOAT, offsetof(fireDef_t, delayBetweenShots), MEMBER_SIZEOF(fireDef_t, delayBetweenShots)},
	{"time", V_INT, offsetof(fireDef_t, time), MEMBER_SIZEOF(fireDef_t, time)},
	{"damage", V_POS, offsetof(fireDef_t, damage), MEMBER_SIZEOF(fireDef_t, damage)},
	{"spldmg", V_POS, offsetof(fireDef_t, spldmg), MEMBER_SIZEOF(fireDef_t, spldmg)},
	{"dmgweight", V_DAMAGE, offsetof(fireDef_t, dmgweight), MEMBER_SIZEOF(fireDef_t, dmgweight)},
	{"irgoggles", V_BOOL, offsetof(fireDef_t, irgoggles), MEMBER_SIZEOF(fireDef_t, irgoggles)},
	{"rounds", V_INT, offsetof(fireDef_t, rounds), MEMBER_SIZEOF(fireDef_t, rounds)},
	{nullptr, V_NULL, 0, 0}
};

/**
 * @brief Parses the item effect.
 * @param[in,out] e The item effect that is filled in here.
 * @param[in] name Already parsed name of the weapon, used for error messages
 * @param[in,out] text The string which is to be parsed
 */
static effectStages_t Com_ParseItemEffect (itemEffect_t* e, const char* name, const char** text)
{
	effectStages_t stage = EFFECT_MAX;

	const char* token = Com_Parse(text);
	if (!*text) {
		Com_Printf("Com_ParseItemEffect: syntax error for item '%s'\n", name);
		return stage;
	}

	if (Q_streq(token, "active")) {
		stage = EFFECT_ACTIVE;
	} else if (Q_streq(token, "inactive")) {
		stage = EFFECT_INACTIVE;
	} else if (Q_streq(token, "overdose")) {
		stage = EFFECT_OVERDOSE;
	} else if (Q_streq(token, "strengthen")) {
		stage = EFFECT_STRENGTHEN;
	} else {
		token = Com_Parse(text);
		if (!*text || *token != '{') {
			Com_Printf("Com_ParseItemEffect: syntax error for item '%s'\n", name);
			return stage;
		}
		Com_SkipBlock(text);
		Com_Printf("Com_ParseItemEffect: item effect of \"%s\" has invalid effect stage: '%s'\n", name, token);
		return stage;
	}

	token = Com_Parse(text);
	if (token[0] != '{') {
		Com_Printf("Com_ParseItemEffect: syntax error for item '%s'\n", name);
		return stage;
	}

	do {
		token = Com_Parse(text);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (!Com_ParseBlockToken(name, text, e, effect_vals, com_genericPool, token)) {
			Com_Printf("Com_ParseItemEffect: item effect of \"%s\" contains invalid values\n", name);
			return stage;
		}
	} while (*text); /* dummy condition */

	return stage;
}

/**
 * @brief Parses the effect that is bound to a fire definitions.
 * @param[in,out] fd The fire definition to add the effect to
 * @param[in] name Already parsed name of the weapon, used for error messages
 * @param[in,out] text The string which is to be parsed
 */
static void Com_ParseFireEffect (fireDef_t* fd, const char* name, const char** text)
{
	itemEffect_t* e = Mem_AllocType(itemEffect_t);
	const effectStages_t stage = Com_ParseItemEffect(e, name, text);
	if (stage == EFFECT_MAX) {
		Mem_Free(e);
		return;
	}

	itemEffect_t** stagePtr = nullptr;
	switch (stage) {
	case EFFECT_ACTIVE:
		stagePtr = &fd->activeEffect;
		break;
	case EFFECT_INACTIVE:
		stagePtr = &fd->deactiveEffect;
		break;
	case EFFECT_OVERDOSE:
		stagePtr = &fd->overdoseEffect;
		break;
	case EFFECT_STRENGTHEN:
		/* strengthen effect isn't available here */
	case EFFECT_MAX:
		stagePtr = nullptr;
		break;
	}

	if (stagePtr == nullptr) {
		Mem_Free(e);
		Com_Printf("Com_ParseFireEffect: invalid effect stage for '%s'\n", name);
		return;
	}

	if (*stagePtr != nullptr) {
		Mem_Free(e);
		Com_Printf("Com_ParseFireEffect: item effect of \"%s\" already has an effect assigned\n", name);
		return;
	}

	*stagePtr = e;
}

/**
 * @brief Parses the firemode
 * @param[in] name Already parsed name of the weapon, used for error messages
 * @param[in,out] text The string which is to be parsed
 * @param[in,out] fd The fire definition to fill
 */
static bool Com_ParseFire (const char* name, const char** text, fireDef_t* fd)
{
	const char* errhead = "Com_ParseFire: unexpected end of file";
	const char* token;

	/* get its body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseFire: fire definition \"%s\" without body ignored\n", name);
		return false;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			return true;
		if (*token == '}')
			return true;

		if (!Com_ParseBlockToken(name, text, fd, fdps, com_genericPool, token)) {
			if (Q_streq(token, "skill")) {
				int skill;

				token = Com_EParse(text, errhead, name);
				if (!*text)
					return false;

				for (skill = ABILITY_NUM_TYPES; skill < SKILL_NUM_TYPES; skill++)
					if (Q_streq(skillNames[skill], token)) {
						fd->weaponSkill = skill;
						break;
					}
				if (skill >= SKILL_NUM_TYPES)
					Com_Printf("Com_ParseFire: unknown weapon skill \"%s\" ignored (weapon %s)\n", token, name);
			} else if (Q_streq(token, "effect")) {
				Com_ParseFireEffect(fd, name, text);
			} else if (Q_streq(token, "range")) {
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return false;
				fd->range = atof(token) * UNIT_SIZE;
			} else if (Q_streq(token, "splrad")) {
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return false;
				fd->splrad = atof(token) * UNIT_SIZE;
			} else
				Com_Printf("Com_ParseFire: unknown token \"%s\" ignored (weapon %s)\n", token, name);
		}
	} while (*text);

	if (fd->impactAttenuation < SOUND_ATTN_NONE || fd->impactAttenuation > SOUND_ATTN_MAX)
		Com_Printf("Com_ParseFire: firedef for weapon \"%s\" has an invalid impact sound attenuation value set\n", name);

	if (fd->fireAttenuation < SOUND_ATTN_NONE || fd->fireAttenuation > SOUND_ATTN_MAX)
		Com_Printf("Com_ParseFire: firedef for weapon \"%s\" has an invalid fire sound attenuation value set\n", name);

	if (fd->weaponSkill < ABILITY_NUM_TYPES)
		Com_Printf("Com_ParseFire: firedef for weapon \"%s\" doesn't have a skill set\n", name);

	if (fd->shots == 1 && fd->delayBetweenShots > 0.0f) {
		Com_Printf("Com_ParseFire: firedef for weapon \"%s\" has delayBetweenShots set but is only a one-shot-firedef\n", name);
		fd->delayBetweenShots = 0.0f;
	}

	if (fd->name == nullptr) {
		Com_Printf("firedef without name\n");
		return false;
	}

	return true;
}

/**
 * @brief Parses the armour definitions or the team resistance values from script files. The
 * protection and rating values.
 * @note The rating values are just for menu displaying
 * @sa Com_ParseItem
 */
static void Com_ParseArmourOrResistance (const char* name, const char** text, short* ad, bool rating)
{
	const char* errhead = "Com_ParseArmourOrResistance: unexpected end of file";

	/* get its body */
	const char* token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseArmourOrResistance: armour definition \"%s\" without body ignored\n", name);
		return;
	}

	do {
		int i;
		token = Com_EParse(text, errhead, name);
		if (!*text)
			return;
		if (*token == '}')
			return;

		for (i = 0; i < csi.numDTs; i++) {
			const damageType_t& dt = csi.dts[i];
			if (Q_streq(token, dt.id)) {
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;
				if (rating && !dt.showInMenu)
					Sys_Error("Com_ParseArmourOrResistance: You try to set a rating value for a none menu displayed damage type '%s'",
							dt.id);
				/* protection or rating values */
				ad[i] = atoi(token);
				break;
			}
		}

		if (i >= csi.numDTs)
			Com_Printf("Com_ParseArmourOrResistance: unknown damage type \"%s\" ignored (in %s)\n", token, name);
	} while (*text);
}

/**
 * @brief List of valid strings for slot types
 * @note slot names are the same as the item types (and must be in the same order)
 */
const char* const air_slot_type_strings[] = AIR_SLOT_TYPE_STRINGS;
CASSERT(lengthof(air_slot_type_strings) == MAX_ACITEMS);

/**
 * @brief Temporary list of weapon ids as parsed from the ufo file "weapon_mod \<id\>"
 * in Com_ParseItem and used in Com_AddObjectLinks.
 */
static linkedList_t* parseItemWeapons = nullptr;

struct parseItemWeapon_t {
	objDef_t* od;
	int numWeapons;
	char* token;
};

static void Com_ParseFireDefinition (objDef_t* od, const char* name, const char** text)
{
	if (od->numWeapons >= MAX_WEAPONS_PER_OBJDEF)
		Sys_Error("max weapons per objdef exceeded");

	const char* token;
	/* get it's body */
	token = Com_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("Com_ParseFireDefinition: weapon_mod \"%s\" without body ignored\n", name);
		return;
	}

	/* get weapon property */
	token = Com_Parse(text);
	if (!*text || !Q_streq(token, "weapon")) {
		Com_Printf("Com_ParseFireDefinition: weapon_mod \"%s\" weapon as first element expected.\n", name);
		return;
	}

	/* Save the weapon id. */
	token = Com_Parse(text);
	/* Store the current item-pointer and the weapon id for later linking of the "weapon" pointers */
	parseItemWeapon_t parse;
	parse.od = od;
	parse.numWeapons = od->numWeapons;
	parse.token = Mem_StrDup(token);
	LIST_Add(&parseItemWeapons, parse);

	/* For each firedef entry for this weapon.  */
	do {
		const char* errhead = "Com_ParseFireDefinition: unexpected end of file (weapon_mod ";
		token = Com_EParse(text, errhead, name);
		if (!*text)
			return;
		if (*token == '}')
			break;

		if (Q_streq(token, "firedef")) {
			const weaponFireDefIndex_t weapFdsIdx = od->numWeapons;
			if (od->numFiredefs[weapFdsIdx] < MAX_FIREDEFS_PER_WEAPON) {
				const fireDefIndex_t fdIdx = od->numFiredefs[weapFdsIdx];
				fireDef_t* fd = &od->fd[weapFdsIdx][fdIdx];
				fd->fireAttenuation = SOUND_ATTN_NORM;
				fd->impactAttenuation = SOUND_ATTN_NORM;
				/* Parse firemode into fd[IDXweapon][IDXfiremode] */
				Com_ParseFire(name, text, fd);
				/* Self-link fd */
				fd->fdIdx = fdIdx;
				/* Self-link weapon_mod */
				fd->weapFdsIdx = weapFdsIdx;
				od->numFiredefs[od->numWeapons]++;
			} else {
				Com_Printf("Com_ParseFireDefinition: Too many firedefs at \"%s\". Max is %i\n", name, MAX_FIREDEFS_PER_WEAPON);
			}
		} else {
			Com_Printf("Unknown token '%s' - expected firedef\n", token);
		}
	} while (*text);
	od->numWeapons++;
}

static void Com_ParseObjDefEffect (objDef_t* od, const char* name, const char** text)
{
	itemEffect_t* e = Mem_AllocType(itemEffect_t);
	const effectStages_t stage = Com_ParseItemEffect(e, name, text);
	if (stage != EFFECT_STRENGTHEN) {
		Com_Printf("Com_ParseObjDefEffect: ignore invalid item effect stage for item: '%s'\n", name);
		Mem_Free(e);
		return;
	}
	if (od->strengthenEffect != nullptr) {
		Com_Printf("Com_ParseObjDefEffect: there is already a strengthen effect assigned to: '%s'\n", name);
		Mem_Free(e);
		return;
	}
	od->strengthenEffect = e;
}

/**
 * @brief Parses weapon, equipment, craft items and armour
 * @sa Com_ParseArmour
 */
static void Com_ParseItem (const char* name, const char** text)
{
	/* search for items with same name */
	if (INVSH_GetItemByIDSilent(name) != nullptr) {
		Com_Printf("Com_ParseItem: weapon def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (csi.numODs >= MAX_OBJDEFS)
		Sys_Error("Com_ParseItem: MAX_OBJDEFS exceeded\n");

	Com_DPrintf(DEBUG_SHARED, "...found item: '%s' (%i)\n", name, csi.numODs);

	/* initialize the object definition */
	objDef_t* od = &csi.ods[csi.numODs++];
	OBJZERO(*od);

	/* default is no craftitem */
	od->craftitem.type = MAX_ACITEMS;
	od->reloadAttenuation = SOUND_ATTN_IDLE;
	od->reloadSound = "weapons/reload-pistol";
	od->armourPath = od->image = od->type = od->model = od->name = "";

	od->id = Mem_StrDup(name);
	if (Q_strnull(od->id))
		Sys_Error("Com_ParseItem: no id given\n");

	od->idx = csi.numODs - 1;

	/* get it's body */
	const char* token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseItem: weapon def \"%s\" without body ignored\n", name);
		csi.numODs--;
		return;
	}

	const char* errhead = "Com_ParseItem: unexpected end of file (weapon ";
	int i;

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (!Com_ParseBlockToken(name, text, od, od_vals, com_genericPool, token)) {
			if (Q_streq(token, "craftweapon")) {
				/* parse a value */
				token = Com_EParse(text, errhead, name);
				if (od->numWeapons < MAX_WEAPONS_PER_OBJDEF) {
					parseItemWeapon_t parse;
					parse.od = od;
					parse.numWeapons = od->numWeapons;
					parse.token = Mem_StrDup(token);
					/* Store the current item-pointer and the weapon id for later linking of the "weapon" pointers */
					LIST_Add(&parseItemWeapons, parse);
					od->numWeapons++;
				} else {
					Com_Printf("Com_ParseItem: Too many weapon_mod definitions at \"%s\". Max is %i\n", name, MAX_WEAPONS_PER_OBJDEF);
				}
			} else if (Q_streq(token, "effect")) {
				Com_ParseObjDefEffect(od, name, text);
			} else if (Q_streq(token, "crafttype")) {
				/* Craftitem type definition. */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				/* Check which type it is and store the correct one.*/
				for (i = 0; i < MAX_ACITEMS; i++) {
					if (Q_streq(token, air_slot_type_strings[i])) {
						od->craftitem.type = (aircraftItemType_t)i;
						break;
					}
				}
				if (i == MAX_ACITEMS)
					Com_Printf("AII_ParseAircraftItem: \"%s\" unknown craftitem type: \"%s\" - ignored.\n", name, token);
			} else if (Q_streq(token, "protection")) {
				Com_ParseArmourOrResistance(name, text, od->protection, false);
			} else if (Q_streq(token, "rating")) {
				Com_ParseArmourOrResistance(name, text, od->ratings, true);
			} else if (Q_streq(token, "weapon_mod")) {
				Com_ParseFireDefinition(od, name, text);
			} else {
				Com_Printf("Com_ParseItem: unknown token \"%s\" ignored (weapon %s)\n", token, name);
			}
		}
	} while (*text);
	if (od->productionCost == 0)
		od->productionCost = od->price;

	/* get size */
	for (i = SHAPE_SMALL_MAX_WIDTH - 1; i >= 0; i--)
		if (od->shape & (0x01010101 << i))
			break;
	od->sx = i + 1;

	for (i = SHAPE_SMALL_MAX_HEIGHT - 1; i >= 0; i--)
		if (od->shape & (0xFF << (i * SHAPE_SMALL_MAX_WIDTH)))
			break;
	od->sy = i + 1;

	if ((od->weapon || od->isAmmo() || od->isArmour() || od->implant) && !od->isVirtual && od->shape == 0) {
		Sys_Error("Item %s has no shape\n", od->id);
	}

	if (od->thrown && od->deplete && od->oneshot && od->ammo) {
		Sys_Error("Item %s has invalid parameters\n", od->id);
	}

	if (od->reloadAttenuation < SOUND_ATTN_NONE || od->reloadAttenuation > SOUND_ATTN_MAX)
		Com_Printf("Com_ParseItem: weapon \"%s\" has an invalid reload sound attenuation value set\n", od->id);
}

/*
==============================================================================
IMPLANT DEFINITION INTERPRETER
==============================================================================
*/

static const value_t implant_vals[] = {
	{"installationtime", V_INT, offsetof(implantDef_t, installationTime), 0},
	{"removetime", V_INT, offsetof(implantDef_t, removeTime), 0},

	{nullptr, V_NULL, 0, 0}
};

static void Com_ParseImplant (const char* name, const char** text)
{
	/* search for implants with same name */
	if (INVSH_GetItemByIDSilent(name) != nullptr) {
		Com_Printf("Com_ParseImplant: implant def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (csi.numImplants >= MAX_IMPLANTS)
		Sys_Error("Com_ParseImplant: MAX_IMPLANTS exceeded\n");

	Com_DPrintf(DEBUG_SHARED, "...found implant: '%s' (%i)\n", name, csi.numImplants);

	/* initialize the implant definition */
	implantDef_t* implant = &csi.implants[csi.numImplants++];
	OBJZERO(*implant);
	implant->id = Mem_StrDup(name);
	if (Q_strnull(implant->id))
		Sys_Error("Com_ParseImplant: no id given\n");

	implant->idx = csi.numImplants - 1;

	/* get it's body */
	const char* token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseImplant: implant def \"%s\" without body ignored\n", name);
		csi.numImplants--;
		return;
	}

	const char* errhead = "Com_ParseImplant: unexpected end of file (implant ";
	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (!Com_ParseBlockToken(name, text, implant, implant_vals, com_genericPool, token)) {
			if (Q_streq(token, "item")) {
				token = Com_EParse(text, errhead, name);
				if (!*text) {
					Com_Printf("Com_ParseImplant: syntax error (implant %s)\n", name);
					break;
				}
				implant->item = INVSH_GetItemByID(token);
			} else {
				Com_Printf("Com_ParseImplant: unknown token \"%s\" ignored (implant %s)\n", token, name);
			}
		}
	} while (*text);

	if (implant->item == nullptr) {
		Sys_Error("implant %s without item found", name);
	}
}

/*
==============================================================================
INVENTORY DEFINITION INTERPRETER
==============================================================================
*/

static const value_t idps[] = {
	{"shape", V_SHAPE_BIG, offsetof(invDef_t, shape), 0},
	/* only a single item */
	{"single", V_BOOL, offsetof(invDef_t, single), MEMBER_SIZEOF(invDef_t, single)},
	/* Scrollable container */
	{"scroll", V_BOOL, offsetof(invDef_t, scroll), MEMBER_SIZEOF(invDef_t, scroll)},
	/* this is the implant container */
	{"implant", V_BOOL, offsetof(invDef_t, implant), MEMBER_SIZEOF(invDef_t, implant)},
	/* this is the armour container */
	{"armour", V_BOOL, offsetof(invDef_t, armour), MEMBER_SIZEOF(invDef_t, armour)},
	/* this is the headgear container */
	{"headgear", V_BOOL, offsetof(invDef_t, headgear), MEMBER_SIZEOF(invDef_t, headgear)},
	/* allow everything to be stored in this container (e.g armour and weapons) */
	{"all", V_BOOL, offsetof(invDef_t, all), MEMBER_SIZEOF(invDef_t, all)},
	/* Does not allow to put the same item more than once into the container */
	{"unique", V_BOOL, offsetof(invDef_t, unique), MEMBER_SIZEOF(invDef_t, unique)},
	{"temp", V_BOOL, offsetof(invDef_t, temp), MEMBER_SIZEOF(invDef_t, temp)},
	/* time units for moving something in */
	{"in", V_INT, offsetof(invDef_t, in), MEMBER_SIZEOF(invDef_t, in)},
	/* time units for moving something out */
	{"out", V_INT, offsetof(invDef_t, out), MEMBER_SIZEOF(invDef_t, out)},

	{nullptr, V_NULL, 0, 0}
};

static void Com_ParseInventory (const char* name, const char** text)
{
	containerIndex_t cid;

	/* Special IDs for container. These are also used elsewhere, so be careful. */
	if (Q_streq(name, "right")) {
		cid = CID_RIGHT;
	} else if (Q_streq(name, "left")) {
		cid = CID_LEFT;
	} else if (Q_streq(name, "implant")) {
		cid = CID_IMPLANT;
	} else if (Q_streq(name, "belt")) {
		cid = CID_BELT;
	} else if (Q_streq(name, "holster")) {
		cid = CID_HOLSTER;
	} else if (Q_streq(name, "backpack")) {
		cid = CID_BACKPACK;
	} else if (Q_streq(name, "armour")) {
		cid = CID_ARMOUR;
	} else if (Q_streq(name, "floor")) {
		cid = CID_FLOOR;
	} else if (Q_streq(name, "equip")) {
		cid = CID_EQUIP;
	} else if (Q_streq(name, "headgear")) {
		cid = CID_HEADGEAR;
	} else {
		Sys_Error("Unknown inventory definition \"%s\". Aborting.\n", name);
		return; /* never reached */
	}

	/* search for containers with same name */
	if (!strncmp(name, csi.ids[cid].name, sizeof(csi.ids[cid].name))) {
		Com_Printf("Com_ParseInventory: inventory def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the inventory definition */
	invDef_t* id = &csi.ids[cid];
	OBJZERO(*id);

	if (!Com_ParseBlock(name, text, id, idps, nullptr))
		return;

	Q_strncpyz(id->name, name, sizeof(id->name));
	id->id = cid;

	csi.numIDs++;
}

/*
==============================================================================
EQUIPMENT DEFINITION INTERPRETER
==============================================================================
*/

const char* const name_strings[NAME_NUM_TYPES] = {
	"neutral",
	"female",
	"male",
	"lastname",
	"female_lastname",
	"male_lastname"
};

/** @brief Valid equipment definition values from script files. */
static const value_t equipment_definition_vals[] = {
	{"mininterest", V_INT, offsetof(equipDef_t, minInterest), MEMBER_SIZEOF(equipDef_t, minInterest)},
	{"maxinterest", V_INT, offsetof(equipDef_t, maxInterest), MEMBER_SIZEOF(equipDef_t, maxInterest)},
	{"name", V_TRANSLATION_STRING, offsetof(equipDef_t, name), 0},

	{nullptr, V_NULL, 0, 0}
};

static void Com_ParseEquipment (const char* name, const char** text)
{
	const char* errhead = "Com_ParseEquipment: unexpected end of file (equipment ";
	equipDef_t* ed;
	const char* token;
	int i, n;

	/* search for equipments with same name */
	for (i = 0; i < csi.numEDs; i++)
		if (Q_streq(name, csi.eds[i].id))
			break;

	if (i < csi.numEDs) {
		Com_Printf("Com_ParseEquipment: equipment def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (i >= MAX_EQUIPDEFS)
		Sys_Error("Com_ParseEquipment: MAX_EQUIPDEFS exceeded\n");

	/* initialize the equipment definition */
	ed = &csi.eds[csi.numEDs++];
	OBJZERO(*ed);

	Q_strncpyz(ed->id, name, sizeof(ed->id));
	ed->name = ed->id;

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseEquipment: equipment def \"%s\" without body ignored\n", name);
		csi.numEDs--;
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text || *token == '}')
			return;

		if (!Com_ParseBlockToken(name, text, ed, equipment_definition_vals, com_genericPool, token)) {
			if (Q_streq(token, "item")) {
				linkedList_t* list;
				if (!Com_ParseList(text, &list)) {
					Com_Error(ERR_DROP, "Com_ParseEquipment: error while reading equipment item tuple");
				}
				if (LIST_Count(list) != 2) {
					Com_Error(ERR_DROP, "Com_ParseEquipment: equipment item tuple must contains 2 elements (id amount)");
				}
				const char* itemToken = (char*)list->data;
				const char* amountToken = (char*)list->next->data;

				const objDef_t* od;
				od = INVSH_GetItemByID(itemToken);
				if (od) {
					n = atoi(amountToken);
					if (ed->numItems[od->idx])
						Com_Printf("Com_ParseEquipment: item '%s' is used several times in def '%s'. Only last entry will be taken into account.\n",
							od->id, name);
					if (n)
						ed->numItems[od->idx] = n;
				} else {
					Com_Printf("Com_ParseEquipment: unknown token \"%s\" ignored (equipment %s)\n", itemToken, name);
				}
				LIST_Delete(&list);
			} else if (Q_streq(token, "aircraft")) {
				linkedList_t* list;
				if (!Com_ParseList(text, &list)) {
					Com_Error(ERR_DROP, "Com_ParseEquipment: error while reading equipment aircraft tuple");
				}
				if (LIST_Count(list) != 2) {
					Com_Error(ERR_DROP, "Com_ParseEquipment: equipment aircraft tuple must contains 2 elements (id amount)");
				}
				const char* aircraftToken = (char*)list->data;
				const char* amountToken = (char*)list->next->data;

				humanAircraftType_t type;
				type = Com_DropShipShortNameToID(aircraftToken);
				n = atoi(amountToken);
				if (ed->numAircraft[type])
					Com_Printf("Com_ParseEquipment: aircraft type '%i' is used several times in def '%s'. Only last entry will be taken into account.\n",
						type, name);
				if (n)
					ed->numAircraft[type] = n;
				LIST_Delete(&list);
			} else {
				Sys_Error("unknown token in equipment in definition %s: '%s'", ed->id, token);
			}
		}
	} while (*text);
}


/*
==============================================================================
NAME AND TEAM DEFINITION INTERPRETER
==============================================================================
*/

/**
 * @param[in] gender 1 (female) or 2 (male)
 * @param[in] td The team definition to get the name from
 * @sa Com_GetCharacterValues
 */
static const char* Com_GiveName (int gender, const teamDef_t* td)
{
	int j, name = 0;

#ifdef DEBUG
	for (j = 0; j < NAME_NUM_TYPES; j++)
		name += td->numNames[j];
	if (!name)
		Sys_Error("Could not find any valid name definitions for category '%s'\n", td->id);
#endif
	/* found category */
	if (!td->numNames[gender]) {
#ifdef DEBUG
		Com_DPrintf(DEBUG_ENGINE, "No valid name definitions for gender %i in category '%s'\n", gender, td->id);
#endif
		return nullptr;
	}
	name = rand() % td->numNames[gender];

	/* skip names */
	linkedList_t* list = td->names[gender];
	for (j = 0; j < name; j++) {
		assert(list);
		list = list->next;
	}

	/* store the name */
	return (const char*)list->data;
}

/**
 * @param[in] gender 1 (female) or 2 (male)
 * @param[in] td The team definition
 * @sa Com_GetCharacterValues
 */
static teamDef_t::model_t const* Com_GiveModel (int gender, const teamDef_t* td)
{
	const linkedList_t* list;

	/* found category */
	if (!td->numModels[gender]) {
		Com_Printf("Com_GiveModel: no models defined for gender %i and category '%s'\n", gender, td->id);
		return nullptr;
	}

	/* search one of the model definitions */
	size_t n = rand() % td->numModels[gender];

	/* skip models and unwanted info */
	list = td->models[gender];
	while (n-- != 0) {
		assert(list);
		list = list->next;
	}

	/* return the value */
	return static_cast<teamDef_t::model_t const*>(list->data);
}

/**
 * @brief Returns the teamDef pointer for the searched team id - or nullptr if not
 * found in the teamDef array
 * @param[in] team The team id (given in ufo-script files)
 */
const teamDef_t* Com_GetTeamDefinitionByID (const char* team)
{
	/* get team definition */
	for (int i = 0; i < csi.numTeamDefs; i++) {
		const teamDef_t* t = &csi.teamDef[i];
		if (Q_streq(team, t->id))
			return t;
	}

	Com_Printf("Com_GetTeamDefinitionByID: could not find team definition for '%s' in team definitions\n", team);
	return nullptr;
}

bool Com_GetCharacterModel (character_t* chr)
{
	if (!chr->teamDef)
		return false;

	/* get model */
	teamDef_t::model_t const* const model = Com_GiveModel(chr->gender, chr->teamDef);
	if (!model)
		return false;

	Q_strncpyz(chr->path, model->path, sizeof(chr->path));
	Q_strncpyz(chr->body, model->body, sizeof(chr->body));
	Q_strncpyz(chr->head, model->head, sizeof(chr->head));
	chr->bodySkin = model->bodySkin;
	chr->headSkin = model->headSkin;

	return true;
}

/**
 * @brief Return a random (weighted by number of models) gender for this teamDef.
 * @param[in] teamDef pointer to the teamDef to get the gender for.
 * @return A valid gender for the teamDef.
 */
static int Com_GetGender (const teamDef_t* teamDef)
{
	int gender;
	int numModels = 0;
	for (gender = 0; gender < NAME_LAST; ++gender)
		if (teamDef->numNames[gender] > 0 && teamDef->numNames[gender + NAME_LAST] > 0)
			numModels += teamDef->numModels[gender];
	if (numModels == 0)
		Com_Error(ERR_DROP, "Could not set character values for team '%s'", teamDef->name);
	int roll = rand() % numModels;
	for (gender = 0; gender < NAME_LAST; ++gender)
		if (teamDef->numNames[gender] > 0 && teamDef->numNames[gender + NAME_LAST] > 0) {
			if (roll < teamDef->numModels[gender])
				break;
			roll -= teamDef->numModels[gender];
		}
	return gender;
}

/**
 * @brief Assign character values, 3D models and names to a character.
 * @param[in] teamDefition The team definition id to use to generate the character values.
 * @param[in,out] chr The character that should get the paths to the different models/skins.
 * @sa Com_GiveName
 * @sa Com_GiveModel
 */
void Com_GetCharacterValues (const char* teamDefition, character_t* chr)
{
	int retry = 1000;

	assert(chr);

	chr->teamDef = Com_GetTeamDefinitionByID(teamDefition);
	if (chr->teamDef == nullptr)
		Com_Error(ERR_DROP, "Com_GetCharacterValues: could not find team '%s' in team definitions", teamDefition);

	if (chr->teamDef->size != ACTOR_SIZE_INVALID)
		chr->fieldSize = chr->teamDef->size;
	else
		chr->fieldSize = ACTOR_SIZE_NORMAL;

	/* get the models */
	while (retry--) {
		const char* str;
		const int gender = Com_GetGender(chr->teamDef);

		chr->gender = gender;

		/* get name */
		str = Com_GiveName(gender, chr->teamDef);
		if (!str)
			continue;
		Q_strncpyz(chr->name, str, sizeof(chr->name));
		Q_strcat(chr->name, sizeof(chr->name), " ");
		str = Com_GiveName(gender + NAME_LAST, chr->teamDef);
		if (!str)
			continue;
		Q_strcat(chr->name, sizeof(chr->name), "%s", str);

		if (!Com_GetCharacterModel(chr))
			continue;
		return;
	}
	Com_Error(ERR_DROP, "Could not set character values for team '%s'\n", teamDefition);
}

/**
 * @brief Parses "name" definition from team_* ufo script files
 * @sa Com_ParseActors
 * @sa Com_ParseScripts
 */
static void Com_ParseActorNames (const char* name, const char** text)
{
	const char* errhead = "Com_ParseNames: unexpected end of file (names ";
	const char* token;
	teamNames_t nameList;

	LIST_Foreach(csi.actorNames, teamNames_t, names) {
		if (Q_streq(name, names->id)) {
			Com_Printf("Com_ParseActorNames: Name list with same name found, second ignored '%s'\n", name);
			return;
		}
	}

	OBJZERO(nameList);
	Q_strncpyz(nameList.id, name, sizeof(nameList.id));

	/* get name list body */
	token = Com_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("Com_ParseActorNames: names def \"%s\" without body ignored\n", name);
		return;
	}

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		int nameType = Com_FindNameType(token);

		linkedList_t* list;
		if (!Com_ParseList(text, &list)) {
			Com_Error(ERR_DROP, "Com_ParseActorNames: error while reading names (%s)", name);
		}

		for (linkedList_t* element = list; element != nullptr; element = element->next) {
			/* some names can be translatable */
			const char* n = (char*)element->data;
			if (*n == '_')
				token++;
			LIST_AddString(&nameList.names[nameType], n);
			nameList.numNames[nameType]++;
		}
		LIST_Delete(&list);

		/* lastname is different */
		/* fill female and male lastnames from neutral lastnames */
		if (nameType == NAME_LAST) {
			for (int i = NAME_NUM_TYPES - 1; i > NAME_LAST; i--) {
				nameList.names[i] = nameList.names[NAME_LAST];
				nameList.numNames[i] = nameList.numNames[NAME_LAST];
			}
		}

	} while (*text);

	if (nameList.numNames[NAME_FEMALE] && !nameList.numNames[NAME_FEMALE_LAST])
		Sys_Error("Com_ParseNames: '%s' has no female lastname category\n", nameList.id);
	if (nameList.numNames[NAME_MALE] && !nameList.numNames[NAME_MALE_LAST])
		Sys_Error("Com_ParseNames: '%s' has no male lastname category\n", nameList.id);
	if (nameList.numNames[NAME_NEUTRAL] && !nameList.numNames[NAME_LAST])
		Sys_Error("Com_ParseNames: '%s' has no neutral lastname category\n", nameList.id);

	LIST_Add(&csi.actorNames, nameList);
}

/**
 * @brief Parses "actors" definition from team_* ufo script files
 * @sa Com_ParseNames
 * @sa Com_ParseScripts
 */
static void Com_ParseActorModels (const char* name, const char** text, teamDef_t* td)
{
	const char* errhead = "Com_ParseActorModels: unexpected end of file (actors ";
	const char* token;

	/* get name list body body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseActorModels: actor def \"%s\" without body ignored\n", td->id);
		return;
	}

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		const int nameType = Com_FindNameType(token);
		if (nameType == -1) {
			Com_Error(ERR_DROP, "Com_ParseActorModels: name type \"%s\" unknown", token);
		}

		linkedList_t* list;
		if (!Com_ParseList(text, &list)) {
			Com_Error(ERR_DROP, "Com_ParseActorModels: error while reading model tuple (%s)", name);
		}
		if (LIST_Count(list) != 5) {
			LIST_Delete(&list);
			Com_Error(ERR_DROP, "Com_ParseActorModels: model tuple must contains 5 elements");
		}

		linkedList_t* element = list;
		const char* pathToken = (const char*)element->data;
		element = element->next;
		const char* bodyToken = (const char*)element->data;
		element = element->next;
		const char* headToken = (const char*)element->data;
		element = element->next;
		const char* bodySkinToken = (const char*)element->data;
		element = element->next;
		const char* headSkinToken = (const char*)element->data;

		teamDef_t::model_t model;
		model.path = Mem_StrDup(pathToken);
		model.body = Mem_StrDup(bodyToken);
		model.head = Mem_StrDup(headToken);
		model.bodySkin = atoi(bodySkinToken);
		model.headSkin = atoi(headSkinToken);

		LIST_Add(&td->models[nameType], model);
		td->numModels[nameType]++;
		LIST_Delete(&list);

	} while (*text);
}

/**
 * @brief Parses "actorsounds" definition from team_* ufo script files
 * @sa Com_ParseNames
 * @sa Com_ParseScripts
 */
static void Com_ParseActorSounds (const char* name, const char** text, teamDef_t* td)
{
	const char* const errhead = "Com_ParseActorSounds: unexpected end of file (actorsounds ";
	int i;

	/* get name list body body */
	const char* token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseActorSounds: actorsounds def \"%s\" without body ignored\n", name);
		return;
	}

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (i = 0; i < NAME_LAST; i++)
			if (Q_streq(token, name_strings[i])) {
				token = Com_EParse(text, errhead, name);
				if (!*text)
					break;
				if (*token != '{')
					break;

				do {
					/* get the sounds */
					token = Com_EParse(text, errhead, name);
					if (!*text)
						break;
					if (*token == '}')
						break;
					if (Q_streq(token, "hurtsound")) {
						token = Com_EParse(text, errhead, name);
						if (!*text)
							break;
						LIST_AddString(&td->sounds[SND_HURT][i], token);
						td->numSounds[SND_HURT][i]++;
					} else if (Q_streq(token, "deathsound")) {
						token = Com_EParse(text, errhead, name);
						if (!*text)
							break;
						LIST_AddString(&td->sounds[SND_DEATH][i], token);
						td->numSounds[SND_DEATH][i]++;
					} else {
						Com_Printf("Com_ParseActorSounds: unknown token \"%s\" ignored (actorsounds %s)\n", token, name);
					}
				} while (*text);
				break; /* next gender sound definition */
			}

		if (i == NAME_NUM_TYPES)
			Com_Printf("Com_ParseActorSounds: unknown token \"%s\" ignored (actorsounds %s)\n", token, name);

	} while (*text);
}

static const BodyData* Com_GetBodyTemplateByID (const char* id)
{
	LIST_Foreach(csi.bodyTemplates, BodyData, bd)
		if (Q_streq(id, bd->id()))
			return bd;
	Com_Printf("Com_GetBodyTemplateByID: could not find template: '%s'\n", id);
	return nullptr;
}

static const teamNames_t* Com_GetNameListByID (const char* id)
{
	LIST_Foreach(csi.actorNames, teamNames_t, names)
		if (Q_streq(id, names->id))
			return names;
	Com_Printf("Com_GetNameListByID: could not find name list: '%s'\n", id);
	return nullptr;
}

/** @brief possible teamdesc values (ufo-scriptfiles) */
static const value_t teamDefValues[] = {
	{"tech", V_STRING, offsetof(teamDef_t, tech), 0}, /**< tech id from research.ufo */
	{"footstepsound", V_STRING, offsetof(teamDef_t, footstepSound), 0}, /**< play this sound for footsteps - overrides the terrain definitions */
	{"name", V_TRANSLATION_STRING, offsetof(teamDef_t, name), 0}, /**< internal team name */
	{"armour", V_BOOL, offsetof(teamDef_t, armour), MEMBER_SIZEOF(teamDef_t, armour)}, /**< are these team members able to wear armour? */
	{"weapons", V_BOOL, offsetof(teamDef_t, weapons), MEMBER_SIZEOF(teamDef_t, weapons)}, /**< are these team members able to use weapons? */
	{"size", V_INT, offsetof(teamDef_t, size), MEMBER_SIZEOF(teamDef_t, size)}, /**< What size is this unit on the field (1=1x1 or 2=2x2)? */
	{"hit_particle", V_STRING, offsetof(teamDef_t, hitParticle), 0}, /**< What particle effect should be spawned if a unit of this type is hit? */
	{"death_texture", V_STRING, offsetof(teamDef_t, deathTextureName), 0},
	{"team", V_TEAM, offsetof(teamDef_t, team), MEMBER_SIZEOF(teamDef_t, team)},
	{"robot", V_BOOL, offsetof(teamDef_t, robot), MEMBER_SIZEOF(teamDef_t, robot)},

	{nullptr, V_NULL, 0, 0}
};

static void Com_ParseTeam (const char* name, const char** text)
{
	teamDef_t* td;
	const char* errhead = "Com_ParseTeam: unexpected end of file (team ";
	const char* token;
	int i;

	/* check for additions to existing name categories */
	for (i = 0, td = csi.teamDef; i < csi.numTeamDefs; i++, td++)
		if (Q_streq(td->id, name))
			break;

	/* reset new category */
	if (i == csi.numTeamDefs) {
		if (csi.numTeamDefs < MAX_TEAMDEFS) {
			OBJZERO(*td);
			/* index backlink */
			td->idx = csi.numTeamDefs;
			csi.numTeamDefs++;
		} else {
			Com_Printf("CL_ParseTeam: Too many team definitions, '%s' ignored.\n", name);
			return;
		}
	} else {
		Com_Printf("CL_ParseTeam: Team with same name found, second ignored '%s'\n", name);
		Com_SkipBlock(text);
		return;
	}

	Q_strncpyz(td->id, name, sizeof(td->id));
	td->armour = td->weapons = true; /* default values */
	td->onlyWeapon = nullptr;

	/* get name list body body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseTeam: team def \"%s\" without body ignored\n", name);
		if (csi.numTeamDefs - 1 == td - csi.teamDef)
			csi.numTeamDefs--;
		return;
	}

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (!Com_ParseBlockToken(name, text, td, teamDefValues, nullptr, token)) {
			if (Q_streq(token, "onlyWeapon")) {
				const objDef_t* od;
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;
				od = INVSH_GetItemByID(token);

				if (od)
					td->onlyWeapon = od;
				else
					Sys_Error("Com_ParseTeam: Could not get item definition for '%s'", token);
			} else if (Q_streq(token, "templates")) {
				linkedList_t* list;
				if (!Com_ParseList(text, &list)) {
					Com_Error(ERR_DROP, "Com_ParseTeam: error while reading templates (team \"%s\")", name);
				}

				for (linkedList_t* element = list; element != nullptr; element = element->next) {
					for (i = 0; i < td->numTemplates; i++) {
						if (Q_streq(token, td->characterTemplates[i]->id)) {
							Com_Printf("Com_ParseTeam: template %s used more than once in team def %s second ignored", (char*)element->data, name);
							break;
						}
					}
					if (i >= td->numTemplates) {
						const chrTemplate_t* ct = Com_GetCharacterTemplateByID((char*)element->data);
						if (ct)
							td->characterTemplates[td->numTemplates++] = ct;
						else
							Sys_Error("Com_ParseTeam: Could not get character template for '%s' in %s", (char*)element->data, name);
					} else
						break;
				}
				LIST_Delete(&list);
			} else if (Q_streq(token, "bodytype")) {
				const BodyData* bd;
				token = Com_EParse(text, errhead, name);
				bd = Com_GetBodyTemplateByID(token);
				if (bd == nullptr)
					Sys_Error("Com_ParseTeam: Could not find body type %s in team def %s\n", token, name);
				td->bodyTemplate = bd;
			} else if (Q_streq(token, "names")) {
				const teamNames_t* nameList;
				token = Com_EParse(text, errhead, name);
				nameList = Com_GetNameListByID(token);
				if (nameList == nullptr)
					Sys_Error("Com_ParseTeam: Could not find name list %s in team def %s\n", token, name);
				td->names = nameList->names;
				td->numNames = nameList->numNames;
			} else if (Q_streq(token, "models"))
				Com_ParseActorModels(name, text, td);
			else if (Q_streq(token, "actorsounds"))
				Com_ParseActorSounds(name, text, td);
			else if (Q_streq(token, "resistance"))
				Com_ParseArmourOrResistance(name, text, td->resistance, false);
			else
				Com_Printf("Com_ParseTeam: unknown token \"%s\" ignored (team %s)\n", token, name);
		}
	} while (*text);

	if (td->deathTextureName[0] == '\0') {
		const int i = rand() % MAX_DEATH;
		Q_strncpyz(td->deathTextureName, va("pics/sfx/blood_%i", i), sizeof(td->deathTextureName));
		Com_DPrintf(DEBUG_CLIENT, "Using random blood for teamdef: '%s' (%i)\n", td->id, i);
	}
	if (td->bodyTemplate == nullptr)
		Sys_Error("Teamdef without body data: %s\n", td->id);
}

/**
 * @brief Returns the chrTemplate pointer for the given id - or nullptr if not found in the chrTemplates
 * array
 * @param[in] chrTemplate The character template id (given in ufo-script files)
 */
const chrTemplate_t* Com_GetCharacterTemplateByID (const char* chrTemplate)
{
	if (Q_strnull(chrTemplate))
		return nullptr;

	/* get character template */
	for (int i = 0; i < csi.numChrTemplates; i++)
		if (Q_streq(chrTemplate, csi.chrTemplates[i].id))
			return &csi.chrTemplates[i];

	Com_Printf("Com_GetCharacterTemplateByID: could not find character template: '%s'\n", chrTemplate);
	return nullptr;
}

static const value_t ugvValues[] = {
	{"tu", V_INT, offsetof(ugv_t, tu), MEMBER_SIZEOF(ugv_t, tu)},
	{"weapon", V_STRING, offsetof(ugv_t, weapon), 0},
	{"armour", V_STRING, offsetof(ugv_t, armour), 0},
	{"actors", V_STRING, offsetof(ugv_t, actors), 0},
	{"price", V_INT, offsetof(ugv_t, price), 0},

	{nullptr, V_NULL, 0, 0}
};

/**
 * @brief Parse 2x2 units (e.g. UGVs)
 * @sa CL_ParseClientData
 */
static void Com_ParseUGVs (const char* name, const char** text)
{
	for (int i = 0; i < csi.numUGV; i++) {
		if (Q_streq(name, csi.ugvs[i].id)) {
			Com_Printf("Com_ParseUGVs: ugv \"%s\" with same name already loaded\n", name);
			return;
		}
	}

	if (csi.numUGV >= MAX_UGV) {
		Com_Printf("Com_ParseUGVs: Too many UGV descriptions, '%s' ignored.\n", name);
		return;
	}

	/* parse ugv */
	ugv_t* ugv = &csi.ugvs[csi.numUGV];
	OBJZERO(*ugv);

	if (Com_ParseBlock(name, text, ugv, ugvValues, nullptr)) {
		ugv->id = Mem_PoolStrDup(name, com_genericPool, 0);
		ugv->idx = csi.numUGV;
		csi.numUGV++;
	}
}

/**
 * @brief Parses character templates from scripts
 */
static void Com_ParseCharacterTemplate (const char* name, const char** text)
{
	const char* errhead = "Com_ParseCharacterTemplate: unexpected end of file";
	const char* token;
	chrTemplate_t* ct;
	int i;

	for (i = 0; i < csi.numChrTemplates; i++)
		if (Q_streq(name, csi.chrTemplates[i].id)) {
			Com_Printf("Com_ParseCharacterTemplate: Template with same name found, second ignored '%s'\n", name);
			return;
		}

	if (i >= MAX_CHARACTER_TEMPLATES)
		Sys_Error("Com_ParseCharacterTemplate: too many character templates");

	/* initialize the character template */
	ct = &csi.chrTemplates[csi.numChrTemplates++];
	OBJZERO(*ct);

	Q_strncpyz(ct->id, name, sizeof(ct->id));

	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseCharacterTemplate: character template \"%s\" without body ignored\n", name);
		csi.numChrTemplates--;
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text || *token == '}')
			return;

		for (i = 0; i < SKILL_NUM_TYPES + 1; i++)
			if (Q_streq(token, skillNames[i])) {
				/* found a definition */
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_EParseValue(ct->skills[i], token, V_INT2, 0, sizeof(ct->skills[i]));
				break;
			}
		if (i >= SKILL_NUM_TYPES + 1) {
			if (Q_streq(token, "rate")) {
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;
				ct->rate = atof(token);
			} else
				Com_Printf("Com_ParseCharacterTemplate: unknown token \"%s\" ignored (template %s)\n", token, name);
		}
	} while (*text);
}

static const value_t bodyPartValues[] = {
		{"name", V_TRANSLATION_STRING, offsetof(BodyPartData, name), 0},
		{"hit_area", V_COLOR, offsetof(BodyPartData, shape), MEMBER_SIZEOF(BodyPartData, shape)},
		{"bleeding_rate", V_INT, offsetof(BodyPartData, bleedingFactor), MEMBER_SIZEOF(BodyPartData, bleedingFactor)},
		{"wound_threshold", V_INT, offsetof(BodyPartData, woundThreshold), MEMBER_SIZEOF(BodyPartData, woundThreshold)},

		{nullptr, V_NULL, 0, 0}
};

static const char* const penaltyNames[MODIFIER_MAX] = {
		"accuracy", "shooting_tu", "movement_tu", "detection", "reaction_time", "max_tu"
};

static void Com_ParseBodyPart (const char* name, const char** text, BodyData* bd)
{
	const char* errhead = "Com_ParseBodyPart: unexpected end of file";
	const char* token;
	BodyPartData bp;
	int i;

	for (i = 0; i < bd->numBodyParts(); i++) {
		if (Q_streq(name, bd->id(i))) {
			Com_Printf("Com_ParseBodyPart: BodyPart with same name found, second ignored '%s'\n", name);
			return;
		}
	}

	if (i > BODYPART_MAXTYPE) {
		Com_Printf("Com_ParseBodyPart: too many BodyParts '%s' ignored ('%s')\n", name, bd->id());
	}

	OBJZERO(bp);
	Q_strncpyz(bp.id, name, sizeof(bp.id));

	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseBodyPart: BodyPart '%s' without body ignored\n", name);
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text || *token == '}')
			break;

		if (!Com_ParseBlockToken(name, text, &bp, bodyPartValues, nullptr, token)) {
			if (Q_streq(token, "penalty")) {
				linkedList_t* list;
				if (!Com_ParseList(text, &list)) {
					Com_Error(ERR_DROP, "Com_ParseBodyPart: error while reading penalties ('%s')", name);
				}

				if (LIST_Count(list) != 2) {
					LIST_Delete(&list);
					Com_Error(ERR_DROP, "Com_ParseBodyPart: penalty tuple must contain 2 elements ('%s')", name);
				}

				linkedList_t* element = list;
				for (i = 0; i < MODIFIER_MAX; i++) {
					if (Q_streq(static_cast<const char*>(element->data), penaltyNames[i])) {
						/* Found a definition */
						element = element->next;
						Com_EParseValue(&bp.penalties[i], static_cast<const char*>(element->data), V_INT, 0, sizeof(bp.penalties[i]));
						break;
					}
				}
				if (i >= MODIFIER_MAX)
					Com_Printf("Com_ParseBodyPart: Unknown penalty '%s' ignored ('%s')\n", static_cast<const char*>(element->data), name);

				LIST_Delete(&list);
			} else {
				Com_Printf("Com_ParseBodyPart: Unknown token '%s' ignored ('%s')\n", token, name);
			}
		}
	} while (*text);

	bd->addBodyPart(bp);
}

static void Com_ParseBodyTemplate (const char* name, const char** text)
{
	const char* errhead = "Com_ParseBodyTemplate: unexpected end of file";
	const char* token;
	BodyData bd;

	LIST_Foreach(csi.bodyTemplates, BodyData, bt) {
		if (Q_streq(name, bt->id())) {
			Com_Printf("Com_ParseBodyTemplate: BodyTemplate with same name found, second ignored '%s'\n", name);
			return;
		}
	}

	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseBodyTemplate: body template '%s' without body ignored\n", name);
		return;
	}

	bd.setId(name);

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text || *token == '}')
			break;

		if (Q_streq(token, "bodypart")) {
			token = Com_EParse(text, errhead, name);
			if (!*text)
				break;

			Com_ParseBodyPart (token, text, &bd);
		} else {
			Com_Printf("Com_ParseBodyTemplate: unknown token '%s' ignored ('%s')\n", token, name);
		}
	} while (*text);

	if (bd.numBodyParts() < 1) {
		Com_Printf("Body template without bodyparts %s ignored!\n", name);
		return;
	}

	LIST_Add(&csi.bodyTemplates, bd);
}

/*
==============================================================================
TERRAIN PARSERS
==============================================================================
*/

#define TERRAIN_HASH_SIZE 64
static terrainType_t* terrainTypesHash[TERRAIN_HASH_SIZE];

static const value_t terrainTypeValues[] = {
	{"footstepsound", V_HUNK_STRING, offsetof(terrainType_t, footstepSound), 0},
	{"particle", V_HUNK_STRING, offsetof(terrainType_t, particle), 0},
	{"footstepvolume", V_FLOAT, offsetof(terrainType_t, footstepVolume), 0},
	{"bouncefraction", V_FLOAT, offsetof(terrainType_t, bounceFraction), 0},

	{nullptr, V_NULL, 0, 0}
};

/**
 * @brief Searches the terrain definition if given
 * @param[in] textureName The terrain definition id from script files
 * which is the texture name relative to base/textures
 */
const terrainType_t* Com_GetTerrainType (const char* textureName)
{
	unsigned hash;
	const terrainType_t* t;

	assert(textureName);
	hash = Com_HashKey(textureName, TERRAIN_HASH_SIZE);
	for (t = terrainTypesHash[hash]; t; t = t->hash_next) {
		if (Q_streq(textureName, t->texture))
			return t;
	}

	return nullptr;
}

/**
 * @brief Parses "terrain" definition from script files
 * @note Terrain definitions are used for footstep sounds and terrain particles
 * @sa Com_ParseScripts
 */
static void Com_ParseTerrain (const char* name, const char** text)
{

	/* check for additions to existing name categories */
	if (Com_GetTerrainType(name) != nullptr) {
		Com_Printf("Terrain definition with same name already parsed: '%s'\n", name);
		return;
	}

	terrainType_t* const t = Mem_PoolAllocType(terrainType_t, com_genericPool);
	t->footstepVolume = SND_VOLUME_FOOTSTEPS;
	t->bounceFraction = 1.0f;

	if (Com_ParseBlock(name, text, t, terrainTypeValues, com_genericPool)) {
		const unsigned hash = Com_HashKey(name, TERRAIN_HASH_SIZE);
		t->texture = Mem_PoolStrDup(name, com_genericPool, 0);
		/* link in terrainTypesHash[hash] should be nullptr on the first run */
		t->hash_next = terrainTypesHash[hash];
		terrainTypesHash[hash] = t;
	} else {
		Mem_Free(t);
	}
}

/*
==============================================================================
GAMETYPE INTERPRETER
==============================================================================
*/

/** @brief possible gametype values for the gameserver (ufo-scriptfiles) */
static const value_t gameTypeValues[] = {
	{"name", V_TRANSLATION_STRING, offsetof(gametype_t, name), 0}, /**< translated game-type name for menu displaying */
	{nullptr, V_NULL, 0, 0}
};

static void Com_ParseGameTypes (const char* name, const char** text)
{
	const char* errhead = "Com_ParseGameTypes: unexpected end of file (gametype ";
	const char* token;
	int i;
	gametype_t* gt;
	cvarlist_t* cvarlist;

	/* get it's body */
	token = Com_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("Com_ParseGameTypes: gametype \"%s\" without body ignored\n", name);
		return;
	}

	/* search for game types with same name */
	for (i = 0; i < csi.numGTs; i++)
		if (!strncmp(token, csi.gts[i].id, MAX_VAR))
			break;

	if (i == csi.numGTs) {
		if (i >= MAX_GAMETYPES)
			Sys_Error("Com_ParseGameTypes: MAX_GAMETYPES exceeded");
		gt = &csi.gts[csi.numGTs++];
		OBJZERO(*gt);
		Q_strncpyz(gt->id, name, sizeof(gt->id));
		if (csi.numGTs >= MAX_GAMETYPES)
			Sys_Error("Com_ParseGameTypes: Too many gametypes.");

		do {
			token = Com_EParse(text, errhead, name);
			if (!*text)
				break;
			if (*token == '}')
				break;

			if (!Com_ParseBlockToken(name, text, gt, gameTypeValues, nullptr, token)) {
				if (!Q_streq(token, "cvarlist"))
					Sys_Error("Com_ParseGameTypes: gametype \"%s\" without cvarlist", name);

				token = Com_EParse(text, errhead, name);
				if (!*text)
					break;
				if (*token != '{')
					Sys_Error("Com_ParseGameTypes: gametype \"%s\" without cvarlist", name);

				do {
					token = Com_EParse(text, errhead, name);
					if (!*text || *token == '}') {
						if (!gt->num_cvars)
							Sys_Error("Com_ParseGameTypes: gametype \"%s\" with empty cvarlist", name);
						else
							break;
					}
					/* initial pointer */
					cvarlist = &gt->cvars[gt->num_cvars++];
					if (gt->num_cvars >= MAX_CVARLISTINGAMETYPE)
						Sys_Error("Com_ParseGameTypes: gametype \"%s\" max cvarlist hit", name);
					Q_strncpyz(cvarlist->name, token, sizeof(cvarlist->name));
					token = Com_EParse(text, errhead, name);
					if (!*text || *token == '}')
						Sys_Error("Com_ParseGameTypes: gametype \"%s\" cvar \"%s\" with no value", name, cvarlist->name);
					Q_strncpyz(cvarlist->value, token, sizeof(cvarlist->value));
				} while (*text && *token != '}');
			}
		} while (*text);
	} else {
		Com_Printf("Com_ParseGameTypes: gametype \"%s\" with same already exists - ignore the second one\n", name);
		Com_SkipBlock(text);
	}
}

/*
==============================================================================
DAMAGE TYPES INTERPRETER
==============================================================================
*/

static void Com_ParseDamageTypes (const char* name, const char** text)
{
	const char* errhead = "Com_ParseDamageTypes: unexpected end of file (damagetype ";
	const char* token;
	int i;

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseDamageTypes: damage type list \"%s\" without body ignored\n", name);
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* Gettext marker (also indicates that it is a dmgtype value - additional to being a dmgweight value) */
		if (*token == '_') {
			token++;
			csi.dts[csi.numDTs].showInMenu = true;
		}

		/* search for damage types with same name */
		for (i = 0; i < csi.numDTs; i++)
			if (Q_streq(token, csi.dts[i].id))
				break;

		/* Not found in the for loop. */
		if (i == csi.numDTs) {
			Q_strncpyz(csi.dts[csi.numDTs].id, token, sizeof(csi.dts[csi.numDTs].id));

			/* Special IDs */
			if (Q_streq(token, "normal"))
				csi.damNormal = csi.numDTs;
			else if (Q_streq(token, "blast"))
				csi.damBlast = csi.numDTs;
			else if (Q_streq(token, "fire"))
				csi.damFire = csi.numDTs;
			else if (Q_streq(token, "shock"))
				csi.damShock = csi.numDTs;
			else if (Q_streq(token, "laser"))
				csi.damLaser = csi.numDTs;
			else if (Q_streq(token, "plasma"))
				csi.damPlasma = csi.numDTs;
			else if (Q_streq(token, "particlebeam"))
				csi.damParticle = csi.numDTs;
			else if (Q_streq(token, "stun_electro"))
				csi.damStunElectro = csi.numDTs;
			else if (Q_streq(token, "stun_gas"))
				csi.damStunGas = csi.numDTs;
			else if (Q_streq(token, "smoke"))
				csi.damSmoke = csi.numDTs;
			else if (Q_streq(token, "incendiary"))
				csi.damIncendiary = csi.numDTs;

			csi.numDTs++;
			if (csi.numDTs >= MAX_DAMAGETYPES)
				Sys_Error("Com_ParseDamageTypes: Too many damage types.");
		} else {
			Com_Printf("Com_ParseDamageTypes: damage type \"%s\" in list \"%s\" with same already exists - ignore the second one (#%i)\n", token, name, csi.numDTs);
		}
	} while (*text);
}


/*
==============================================================================
MAIN SCRIPT PARSING FUNCTION
==============================================================================
*/

/**
 * @brief Returns the name of an aircraft or an ufo that is used in the ump files for
 * the random map assembly
 * @see cvar rm_drop, rm_ufo, rm_crashed
 * @note Uses a static buffer - so after you got the name you should ensure that you
 * put it into a proper location. Otherwise it will get overwritten with the next call
 * of this function.
 */
const char* Com_GetRandomMapAssemblyNameForCraft (const char* craftID)
{
	return va("+%s", craftID);
}

/**
 * @todo implement this in a better way
 */
const char* Com_GetRandomMapAssemblyNameForCrashedCraft (const char* craftID)
{
	if (Q_streq(craftID, "craft_drop_firebird"))
		return "+craft_crash_drop_firebird";
	else if (Q_streq(craftID, "craft_drop_raptor"))
		return "+craft_crash_drop_raptor";
	else if (Q_streq(craftID, "craft_inter_dragon"))
		return "+craft_crash_inter_dragon";
	else if (Q_streq(craftID, "craft_inter_saracen"))
		return "+craft_crash_inter_saracen";
	else if (Q_streq(craftID, "craft_inter_starchaser"))
		return "+craft_crash_inter_starchaser";

	return "";
}

/**
 * @brief Translate DropShip type to short name.
 * @return Will always return a valid human aircraft type or errors out if the given token can't
 * be mapped to an human aircraft type
 * @sa Com_DropShipTypeToShortName
 */
humanAircraftType_t Com_DropShipShortNameToID (const char* token)
{
	humanAircraftType_t aircraftType;
	size_t dummy;
	Com_ParseValue(&aircraftType, token, V_AIRCRAFTTYPE, 0, sizeof(aircraftType), &dummy);
	return aircraftType;
}

/**
 * @brief Translate DropShip type to short name.
 * @sa Com_DropShipShortNameToID
 */
const char* Com_DropShipTypeToShortName (humanAircraftType_t type)
{
	return Com_ValueToStr(&type, V_AIRCRAFTTYPE, 0);
}

/**
 * @brief Translate short name to UFO type.
 * @sa UFO_TypeToName
 * @sa Com_UFOTypeToShortName
 */
ufoType_t Com_UFOShortNameToID (const char* token)
{
	ufoType_t ufoType;
	size_t dummy;
	Com_ParseValue(&ufoType, token, V_UFO, 0, sizeof(ufoType), &dummy);
	return ufoType;
}

/**
 * @brief Translate UFO type to short name.
 * @sa UFO_TypeToName
 * @sa Com_UFOShortNameToID
 */
const char* Com_UFOTypeToShortName (ufoType_t type)
{
	return Com_ValueToStr(&type, V_UFO, 0);
}

/**
 * @brief Translate UFO type to short name when UFO is crashed.
 * @sa Com_UFOTypeToShortName
 */
const char* Com_UFOCrashedTypeToShortName (ufoType_t type)
{
	return Com_ValueToStr(&type, V_UFOCRASHED, 0);
}

/**
 * @brief Searches an UGV definition by a given script id and returns the pointer to the global data
 * @param[in] ugvID The script id of the UGV definition you are looking for
 * @return ugv_t pointer or nullptr if not found.
 * @note This function gives no warning on null name or if no ugv found
 */
const ugv_t* Com_GetUGVByIDSilent (const char* ugvID)
{
	if (!ugvID)
		return nullptr;

	for (int i = 0; i < csi.numUGV; i++) {
		const ugv_t* ugv = &csi.ugvs[i];
		if (Q_streq(ugv->id, ugvID)) {
			return ugv;
		}
	}
	return nullptr;
}

/**
 * @brief Searches an UGV definition by a given script id and returns the pointer to the global data
 * @param[in] ugvID The script id of the UGV definition you are looking for
 * @return ugv_t pointer or nullptr if not found.
 */
const ugv_t* Com_GetUGVByID (const char* ugvID)
{
	const ugv_t* ugv = Com_GetUGVByIDSilent(ugvID);

	if (!ugvID)
		Com_Printf("Com_GetUGVByID Called with nullptr ugvID!\n");
	else if (!ugv)
		Com_Printf("Com_GetUGVByID: No ugv_t entry found for id '%s' in %i entries.\n", ugvID, csi.numUGV);
	return ugv;
}

/**
 * @brief Creates links to other items (i.e. ammo<->weapons)
 */
static void Com_AddObjectLinks (void)
{
	/* Add links to weapons. */
	LIST_Foreach (parseItemWeapons, parseItemWeapon_t, parse) {
		const int weaponsIdx = parse->numWeapons;
		const char* id = parse->token;

		/* Link the weapon pointers for this item. */
		parse->od->weapons[weaponsIdx] = INVSH_GetItemByID(id);
		if (!parse->od->weapons[weaponsIdx]) {
			Sys_Error("Com_AddObjectLinks: Could not get item '%s' for linking into item '%s'\n",
				id , parse->od->id);
		}

		/* Back-link the obj-idx inside the fds */
		for (int k = 0; k < parse->od->numFiredefs[weaponsIdx]; k++) {
			parse->od->fd[weaponsIdx][k].obj = parse->od;
		}

		Mem_Free(parse->token);
	}

	/* Clear the temporary list. */
	LIST_Delete(&parseItemWeapons);

	/* Add links to ammos */
	objDef_t* od;
	int i;
	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		od->numAmmos = 0;	/* Default value */
		if (od->weapon || od->craftitem.type <= AC_ITEM_WEAPON) {
			/* this is a weapon, an aircraft weapon, or a base defence system */
			for (int n = 0; n < csi.numODs; n++) {
				const objDef_t* weapon = INVSH_GetItemByIDX(n);
				for (int m = 0; m < weapon->numWeapons; m++) {
					if (weapon->weapons[m] == od) {
						assert(od->numAmmos < MAX_AMMOS_PER_OBJDEF);
						od->ammos[od->numAmmos++] = weapon;
						Com_DPrintf(DEBUG_SHARED, "link ammo %s to weapon: %s\n", weapon->id, od->id);
					}
				}
			}
		}
	}
}

/** @brief valid mapdef descriptors */
static const value_t mapdef_vals[] = {
	{"description", V_TRANSLATION_STRING, offsetof(mapDef_t, description), 0},
	{"victorycondition", V_TRANSLATION_STRING, offsetof(mapDef_t, victoryCondition), 0},
	{"missionbriefing", V_TRANSLATION_STRING, offsetof(mapDef_t, missionBriefing), 0},
	{"map", V_HUNK_STRING, offsetof(mapDef_t, mapTheme), 0},
	{"size", V_HUNK_STRING, offsetof(mapDef_t, size), 0},
	{"civilianteam", V_HUNK_STRING, offsetof(mapDef_t, civTeam), 0},

	{"maxaliens", V_INT, offsetof(mapDef_t, maxAliens), MEMBER_SIZEOF(mapDef_t, maxAliens)},
	{"hwclass", V_INT, offsetof(mapDef_t, hwclass), MEMBER_SIZEOF(mapDef_t, hwclass)},
	{"storyrelated", V_BOOL, offsetof(mapDef_t, storyRelated), MEMBER_SIZEOF(mapDef_t, storyRelated)},

	{"teams", V_INT, offsetof(mapDef_t, teams), MEMBER_SIZEOF(mapDef_t, teams)},
	{"multiplayer", V_BOOL, offsetof(mapDef_t, multiplayer), MEMBER_SIZEOF(mapDef_t, multiplayer)},
	{"singleplayer", V_BOOL, offsetof(mapDef_t, singleplayer), MEMBER_SIZEOF(mapDef_t, singleplayer)},
	{"campaign", V_BOOL, offsetof(mapDef_t, campaign), MEMBER_SIZEOF(mapDef_t, campaign)},

	{"onwin", V_HUNK_STRING, offsetof(mapDef_t, onwin), 0},
	{"onlose", V_HUNK_STRING, offsetof(mapDef_t, onlose), 0},

	{"ufos", V_LIST, offsetof(mapDef_t, ufos), 0},
	{"aircraft", V_LIST, offsetof(mapDef_t, aircraft), 0},
	{"terrains", V_LIST, offsetof(mapDef_t, terrains), 0},
	{"populations", V_LIST, offsetof(mapDef_t, populations), 0},
	{"cultures", V_LIST, offsetof(mapDef_t, cultures), 0},
	{"gametypes", V_LIST, offsetof(mapDef_t, gameTypes), 0},

	{nullptr, V_NULL, 0, 0}
};

static void Com_ParseMapDefinition (const char* name, const char** text)
{
	const char* errhead = "Com_ParseMapDefinition: unexpected end of file (mapdef ";
	mapDef_t* md;
	const char* token;

	/* get it's body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseMapDefinition: mapdef \"%s\" without body ignored\n", name);
		return;
	}

	md = Com_GetMapDefByIDX(csi.numMDs);
	csi.numMDs++;
	if (csi.numMDs >= lengthof(csi.mds))
		Sys_Error("Com_ParseMapDefinition: Max mapdef hit");

	OBJZERO(*md);
	md->id = Mem_PoolStrDup(name, com_genericPool, 0);
	md->singleplayer = true;
	md->campaign = true;
	md->multiplayer = false;

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (!Com_ParseBlockToken(name, text, md, mapdef_vals, com_genericPool, token)) {
			if (Q_streq(token, "params")) {
				Com_ParseList(text, &md->params);
			} else {
				Com_Printf("Com_ParseMapDefinition: unknown token \"%s\" ignored (mapdef %s)\n", token, name);
				continue;
			}
		}
	} while (*text);

	if (!md->mapTheme) {
		Com_Printf("Com_ParseMapDefinition: mapdef \"%s\" with no map\n", name);
		csi.numMDs--;
	}

	if (!md->description) {
		Com_Printf("Com_ParseMapDefinition: mapdef \"%s\" with no description\n", name);
		csi.numMDs--;
	}

	if (md->maxAliens <= 0) {
		Com_Printf("Com_ParseMapDefinition: mapdef \"%s\" with invalid maxAlien value\n", name);
		csi.numMDs--;
	}

	/* Skip if the hardware can't handle this map. */
	if (hwclass->integer < md->hwclass) {
		Com_DPrintf(DEBUG_SHARED, "Com_ParseMapDefinition: mapdef \"%s\" is skipped because hwclass doesn't match\n", name);
		csi.numMDs--;
	}
}

static void Com_ParseTerrainDefinition (const char* name, const char** text)
{
	const char* errhead = "Com_ParseTerrainDefinition: unexpected end of file (terraindef ";
	TerrainDef* tDef = new TerrainDef;

	strcpy(tDef->terrainName, name);

	/* get it's body */
	const char* token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseTerrainDefinition: mapdef \"%s\" without body ignored\n", name);
		return;
	}

	do {
		token = Com_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		char key[MAX_VAR];
		Q_strncpyz(key, token, sizeof(key));
		token = Com_EParse(text, errhead, name);
		KeyValuePair kvp(key, token);

		if (kvp.isKey("rgbred"))
			tDef->rgbRed = kvp.asInt();
		else if (kvp.isKey("rgbgreen"))
			tDef->rgbGreen = kvp.asInt();
		else if (kvp.isKey("rgbblue"))
			tDef->rgbBlue = kvp.asInt();
		else if (kvp.isKey("survivalchance"))
			tDef->survivalChance = kvp.asFloat();
		else if (kvp.isKey("rainchance"))
			tDef->rainChance = kvp.asFloat();
		else if (kvp.isKey("snowchance"))
			tDef->snowChance = kvp.asFloat();
		else {
			Com_Printf("Com_ParseTerrainDefinition: unknown token \"%s\" ignored (terraindef %s)\n", token, name);
		}
	} while (*text);

	if (!tDef->terrainName[0]) {
		Com_Printf("Com_ParseTerrainDefinition: terraindef \"%s\" with no name\n", name);
	}
	/* Now add the stuff we just parsed to the table. */
	if (!csi.terrainDefs.add(tDef))
		Com_Printf("Com_ParseTerrainDefinition: could not add terraindef \"%s\". Duplicate colors or name.\n", name);
}

int Com_GetMapDefNumber (void)
{
	return csi.numMDs;
}

mapDef_t* Com_GetMapDefByIDX (int index)
{
	return &csi.mds[index];
}

mapDef_t* Com_GetMapDefinitionByID (const char* mapDefID)
{
	mapDef_t* md;

	assert(mapDefID);

	MapDef_Foreach(md) {
		if (Q_streq(md->id, mapDefID))
			return md;
	}

	Com_DPrintf(DEBUG_SHARED, "Com_GetMapDefinition: Could not find mapdef with id: '%s'\n", mapDefID);
	return nullptr;
}

/**
 * @sa CL_ParseClientData
 * @sa CL_ParseScriptFirst
 * @sa CL_ParseScriptSecond
 * @sa Qcommon_Init
 */
void Com_ParseScripts (bool onlyServer)
{
	const char* type, *name, *text;

	Com_Printf("\n----------- parse scripts ----------\n");

	/* reset csi basic info */
	INVSH_InitCSI(&csi);
	csi.damNormal = csi.damBlast = csi.damFire = csi.damShock = csi.damLaser = csi.damPlasma = csi.damParticle = csi.damStunElectro = csi.damStunGas = NONE;
	csi.damSmoke = csi.damIncendiary = NONE;

	/* pre-stage parsing */
	Com_Printf("%i script files\n", FS_BuildFileList("ufos/*.ufo"));
	text = nullptr;

	FS_NextScriptHeader(nullptr, nullptr, nullptr);

	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != nullptr)
		if (Q_streq(type, "damagetypes"))
			Com_ParseDamageTypes(name, &text);
		else if (Q_streq(type, "gametype"))
			Com_ParseGameTypes(name, &text);
		else if (Q_streq(type, "version"))
			Com_ParseVersion(name);

	/* stage one parsing */
	FS_NextScriptHeader(nullptr, nullptr, nullptr);
	text = nullptr;

	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != nullptr) {
		/* server/client scripts */
		if (Q_streq(type, "item") || Q_streq(type, "craftitem"))
			Com_ParseItem(name, &text);
		else if (Q_streq(type, "inventory"))
			Com_ParseInventory(name, &text);
		else if (Q_streq(type, "terrain"))
			Com_ParseTerrain(name, &text);
		else if (Q_streq(type, "ugv"))
			Com_ParseUGVs(name, &text);
		else if (Q_streq(type, "chrtemplate"))
			Com_ParseCharacterTemplate(name, &text);
		else if (Q_streq(type, "terraindef"))
			Com_ParseTerrainDefinition(name, &text);
		else if (Q_streq(type, "mapdef"))
			Com_ParseMapDefinition(name, &text);
		else if (Q_streq(type, "bodydef"))
			Com_ParseBodyTemplate(name, &text);
		else if (Q_streq(type, "names"))
			Com_ParseActorNames(name, &text);
		else if (Q_streq(type, "aircraftnames"))
			Com_ParseAircraftNames(name, &text);
		else if (!onlyServer)
			CL_ParseClientData(type, name, &text);
	}

	if (!versionParsed)
		Sys_Error("Could not find version string for script files");

	/* Stage two parsing (weapon/inventory dependant stuff). */
	FS_NextScriptHeader(nullptr, nullptr, nullptr);
	text = nullptr;

	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != nullptr) {
		/* server/client scripts */
		if (Q_streq(type, "equipment"))
			Com_ParseEquipment(name, &text);
		else if (Q_streq(type, "team"))
			Com_ParseTeam(name, &text);
		else if (Q_streq(type, "implant"))
			Com_ParseImplant(name, &text);
	}

	Com_AddObjectLinks();	/* Add ammo<->weapon links to items.*/

	/* parse ui node script */
	if (!onlyServer) {
		Com_Printf("%i ui script files\n", FS_BuildFileList("ufos/ui/*.ufo"));
		FS_NextScriptHeader(nullptr, nullptr, nullptr);
		text = nullptr;
		while ((type = FS_NextScriptHeader("ufos/ui/*.ufo", &name, &text)) != nullptr)
			CL_ParseClientData(type, name, &text);
	}

	Com_Printf("Shared Client/Server Info loaded\n");
	Com_Printf("...%3i items parsed\n", csi.numODs);
	Com_Printf("...%3i damage types parsed\n", csi.numDTs);
	Com_Printf("...%3i equipment definitions parsed\n", csi.numEDs);
	Com_Printf("...%3i inventory definitions parsed\n", csi.numIDs);
	Com_Printf("...%3i team definitions parsed\n", csi.numTeamDefs);
}

int Com_GetScriptChecksum (void)
{
	static int checksum = 0;
	const char* buf;

	if (checksum != 0)
		return checksum;

	while ((buf = FS_GetFileData("ufos/*.ufo")) != nullptr)
		checksum += LittleLong(Com_BlockChecksum(buf, strlen(buf)));
	FS_GetFileData(nullptr);

	return checksum;
}

void Com_Shutdown (void)
{
	OBJZERO(terrainTypesHash);
	OBJZERO(com_constNameInt_hash);
	com_constNameInt = nullptr;
	versionParsed = false;
}
