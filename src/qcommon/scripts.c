/**
 * @file qcommon/scripts.c
 * @brief Ufo scripts used in client and server
 * @note interpreters for: object, inventory, equipment, name and team, damage
 */

/*
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


#include "qcommon.h"
#include "../game/inv_shared.h"
#include "../client/cl_research.h"

/**
 * @brief possible values for parsing functions
 * @sa valueTypes_t
 */
const char *vt_names[V_NUM_TYPES] = {
	"",
	"bool",
	"char",
	"int",
	"int2",
	"float",
	"pos",
	"vector",
	"color",
	"rgba",
	"string",
	"translation_string",
	"translation2_string",
	"longstring",
	"align",
	"blend",
	"style",
	"fade",
	"shapes",
	"shapeb",
	"dmgtype",
	"date",
	"if",
	"relabs",
	"client_hunk",
	"client_hunk_string"
};

const char *align_names[ALIGN_LAST] = {
	"ul", "uc", "ur", "cl", "cc", "cr", "ll", "lc", "lr"
};

const char *blend_names[BLEND_LAST] = {
	"replace", "blend", "add", "filter", "invfilter"
};

const char *style_names[STYLE_LAST] = {
	"facing", "rotated", "beam", "line", "axis"
};

const char *fade_names[FADE_LAST] = {
	"none", "in", "out", "sin", "saw", "blend"
};

const static char *if_strings[IF_SIZE] = {
	""
	"==",
	"<=",
	">=",
	">",
	"<",
	"!=",
	""
};

/**
 * @brief Translate the condition string to menuIfCondition_t enum value
 * @param[in] conditionString The string from scriptfiles (see if_strings)
 * @return menuIfCondition_t value
 * @return enum value for condition string
 * @note Produces a Sys_Error if conditionString was not found in if_strings array
 */
static int Com_ParseConditionType (const char* conditionString, const char *token)
{
	int i = IF_SIZE;
	for (; i--;) {
		if (!Q_strncmp(if_strings[i], conditionString, 2)) {
			return i;
		}
	}
	Sys_Error("Invalid if statement with condition '%s' token: '%s'\n", conditionString, token);
	return -1;
}

/** @brief target sizes for buffer */
static const size_t vt_sizes[V_NUM_TYPES] = {
	0,	/* V_NULL */
	sizeof(qboolean),	/* V_BOOL */
	sizeof(char),	/* V_CHAR */
	sizeof(int),	/* V_INT */
	2 * sizeof(int),	/* V_INT2 */
	sizeof(float),	/* V_FLOAT */
	sizeof(vec2_t),	/* V_POS */
	sizeof(vec3_t),	/* V_VECTOR */
	sizeof(vec4_t),	/* V_COLOR */
	sizeof(vec4_t),	/* V_RGBA */
	0,	/* V_STRING */
	0,	/* V_TRANSLATION_STRING */
	0,	/* V_TRANSLATION2_STRING */
	0,	/* V_LONGSTRING */
	sizeof(byte),	/* V_ALIGN */
	sizeof(byte),	/* V_BLEND */
	sizeof(byte),	/* V_STYLE */
	sizeof(byte),	/* V_FADE */
	sizeof(int),	/* V_SHAPE_SMALL */
	0,	/* V_SHAPE_BIG */
	sizeof(byte),	/* V_DMGTYPE */
	0,	/* V_DATE */
	0,	/* V_IF */
	sizeof(float),	/* V_RELABS */
	0,	/* V_CLIENT_HUNK */
	0	/* V_CLIENT_HUNK_STRING */
};

/**
 * @brief
 * @note translateable string are marked with _ at the beginning
 * @code menu exampleName
 * {
 *   string "_this is translatable"
 * }
 * @endcode
 */
#ifdef DEBUG
int Com_ParseValueDebug (void *base, const char *token, valueTypes_t type, int ofs, size_t size, const char *file, int line)
#else
int Com_ParseValue (void *base, const char *token, valueTypes_t type, int ofs, size_t size)
#endif
{
	byte *b;
	char string[MAX_VAR];
	char string2[MAX_VAR];
	char condition[MAX_VAR];
	int x, y, w, h;

	b = (byte *) base + ofs;

	if (size) {
#ifdef DEBUG
		if (size > vt_sizes[type]) {
			Com_Printf("Warning: Size mismatch: given size: "UFO_SIZE_T", should be: "UFO_SIZE_T". File: '%s', line: %i (type: %i)\n", size, vt_sizes[type], file, line, type);
		}
		if (size < vt_sizes[type]) {
			/* disable this sys error to return -1 and print the value_t->string that is wrong in the parsing function */
			Sys_Error("Size mismatch: given size: "UFO_SIZE_T", should be: "UFO_SIZE_T". File: '%s', line: %i (type: %i)\n", size, vt_sizes[type], file, line, type);
			return -1;	/* don't delete this please */
		}
#else
		if (size < vt_sizes[type]) {
			/* disable this sys error to return -1 and print the value_t->string that is wrong in the parsing function */
			Sys_Error("Size mismatch: given size: "UFO_SIZE_T", should be: "UFO_SIZE_T". (type: %i)\n", size, vt_sizes[type], type);
			return -1;	/* don't delete this please */
		}
#endif
	}

	switch (type) {
	case V_CLIENT_HUNK_STRING:
	case V_CLIENT_HUNK:
		Sys_Error("Com_ParseValue: V_CLIENT_HUNK and V_CLIENT_HUNK_STRING are not parsed here\n");

	case V_NULL:
		return ALIGN(0);

	case V_BOOL:
		if (!Q_strncmp(token, "true", 4) || *token == '1')
			*b = qtrue;
		else
			*b = qfalse;
		return ALIGN(sizeof(qboolean));

	case V_CHAR:
		*(char *) b = *token;
		return ALIGN(sizeof(char));

	case V_INT:
		*(int *) b = atoi(token);
		return ALIGN(sizeof(int));

	case V_INT2:
		if (strstr(token, " ") == NULL)
			Sys_Error("Com_ParseValue: Illegal int2 statement '%s'\n", token);
		sscanf(token, "%i %i", &((int *) b)[0], &((int *) b)[1]);
		return ALIGN(2 * sizeof(int));

	case V_FLOAT:
		*(float *) b = atof(token);
		return ALIGN(sizeof(float));

	case V_POS:
		if (strstr(token, " ") == NULL)
			Sys_Error("Com_ParseValue: Illegal pos statement '%s'\n", token);
		sscanf(token, "%f %f", &((float *) b)[0], &((float *) b)[1]);
		return ALIGN(2 * sizeof(float));

	case V_VECTOR:
		if (strstr(token, " ") == NULL || strstr(strstr(token, " "), " ") == NULL)
			Sys_Error("Com_ParseValue: Illegal vector statement '%s'\n", token);
		sscanf(token, "%f %f %f", &((float *) b)[0], &((float *) b)[1], &((float *) b)[2]);
		return ALIGN(3 * sizeof(float));

	case V_COLOR:
		if (strstr(strstr(strstr(token, " "), " "), " ") == NULL)
			Sys_Error("Com_ParseValue: Illegal color statement '%s'\n", token);
		sscanf(token, "%f %f %f %f", &((float *) b)[0], &((float *) b)[1], &((float *) b)[2], &((float *) b)[3]);
		return ALIGN(4 * sizeof(float));

	case V_RGBA:
		if (strstr(strstr(strstr(token, " "), " "), " ") == NULL)
			Sys_Error("Com_ParseValue: Illegal rgba statement '%s'\n", token);
		sscanf(token, "%i %i %i %i", &((int *) b)[0], &((int *) b)[1], &((int *) b)[2], &((int *) b)[3]);
		return ALIGN(4 * sizeof(int));

	case V_STRING:
		Q_strncpyz((char *) b, token, MAX_VAR);
		w = (int)strlen(token) + 1;
		if (w > MAX_VAR)
			w = MAX_VAR;
		return ALIGN(w);

	case V_TRANSLATION_STRING:
		if (*token == '_')
			token++;

		Q_strncpyz((char *) b, _(token), MAX_VAR);
		return ALIGN((int)strlen((char *) b) + 1);

	/* just remove the _ but don't translate */
	case V_TRANSLATION2_STRING:
		if (*token == '_')
			token++;

		Q_strncpyz((char *) b, token, MAX_VAR);
		w = (int)strlen((char *) b) + 1;
		return ALIGN(w);

	case V_LONGSTRING:
		strcpy((char *) b, token);
		w = (int)strlen(token) + 1;
		return ALIGN(w);

	case V_ALIGN:
		for (w = 0; w < ALIGN_LAST; w++)
			if (!Q_strcmp(token, align_names[w]))
				break;
		if (w == ALIGN_LAST)
			*b = (byte)0;
		else
			*b = (byte)w;
		return ALIGN(1);

	case V_BLEND:
		for (w = 0; w < BLEND_LAST; w++)
			if (!Q_strcmp(token, blend_names[w]))
				break;
		if (w == BLEND_LAST)
			*b = (byte)0;
		else
			*b = (byte)w;
		return ALIGN(1);

	case V_STYLE:
		for (w = 0; w < STYLE_LAST; w++)
			if (!Q_strcmp(token, style_names[w]))
				break;
		if (w == STYLE_LAST)
			*b = (byte)0;
		else
			*b = (byte)w;
		return ALIGN(1);

	case V_FADE:
		for (w = 0; w < FADE_LAST; w++)
			if (!Q_strcmp(token, fade_names[w]))
				break;
		if (w == FADE_LAST)
			*b = (byte)0;
		else
			*b = (byte)w;
		return ALIGN(1);

	case V_SHAPE_SMALL:
		if (strstr(strstr(strstr(token, " "), " "), " ") == NULL)
			Sys_Error("Com_ParseValue: Illegal shape small statement '%s'\n", token);
		sscanf(token, "%i %i %i %i", &x, &y, &w, &h);
		if (y + h > SHAPE_SMALL_MAX_HEIGHT || y >= SHAPE_SMALL_MAX_HEIGHT || h > SHAPE_SMALL_MAX_HEIGHT)
			Sys_Error("Com_ParseValue: illegal shape small statement - max h value is %i (y: %i, h: %i)\n", SHAPE_SMALL_MAX_HEIGHT, y, h);
		if (x + w > SHAPE_SMALL_MAX_WIDTH || x >= SHAPE_SMALL_MAX_WIDTH || w > SHAPE_SMALL_MAX_WIDTH)
			Sys_Error("Com_ParseValue: illegal shape small statement - max x and w values are %i\n", SHAPE_SMALL_MAX_WIDTH);
		for (h += y; y < h; y++)
			*(uint32_t *) b |= ((1 << w) - 1) << x << (y * SHAPE_SMALL_MAX_WIDTH);
		return ALIGN(SHAPE_SMALL_MAX_HEIGHT);

	case V_SHAPE_BIG:
		if (strstr(strstr(strstr(token, " "), " "), " ") == NULL)
			Sys_Error("Com_ParseValue: Illegal shape big statement '%s'\n", token);
		sscanf(token, "%i %i %i %i", &x, &y, &w, &h);
		if (y + h > SHAPE_BIG_MAX_HEIGHT || y >= SHAPE_BIG_MAX_HEIGHT || h > SHAPE_BIG_MAX_HEIGHT)
			Sys_Error("Com_ParseValue: Illegal shape big statement, max height is %i\n", SHAPE_BIG_MAX_HEIGHT);
		if (x + w > SHAPE_BIG_MAX_WIDTH || x >= SHAPE_BIG_MAX_WIDTH || w > SHAPE_BIG_MAX_WIDTH)
			Sys_Error("Com_ParseValue: illegal shape big statement - max x and w values are %i ('%s')\n", SHAPE_BIG_MAX_WIDTH, token);
		w = ((1 << w) - 1) << x;
		for (h += y; y < h; y++)
			((uint32_t *) b)[y] |= w;
		return ALIGN(SHAPE_BIG_MAX_HEIGHT * SHAPE_SMALL_MAX_HEIGHT);

	case V_DMGTYPE:
		for (w = 0; w < csi.numDTs; w++)
			if (!Q_strcmp(token, csi.dts[w]))
				break;
		if (w == csi.numDTs)
			*b = (byte)0;
		else
			*b = (byte)w;
		return ALIGN(1);

	case V_DATE:
		if (strstr(strstr(token, " "), " ") == NULL)
			Sys_Error("Com_ParseValue: Illegal if statement '%s'\n", token);
		sscanf(token, "%i %i %i", &x, &y, &w);
		((date_t *) b)->day = 365 * x + y;
		((date_t *) b)->sec = 3600 * w;
		return ALIGN(sizeof(date_t));

	case V_IF:
		if (!strstr(token, " ")) {
			/* cvar exists? (not null) */
			Q_strncpyz(((menuDepends_t *) b)->var, token, MAX_VAR);
			((menuDepends_t *) b)->cond = IF_EXISTS;
		} else if (strstr(strstr(token, " "), " ")) {
			sscanf(token, "%s %s %s", string, condition, string2);

			Q_strncpyz(((menuDepends_t *) b)->var, string, MAX_VAR);
			Q_strncpyz(((menuDepends_t *) b)->value, string2, MAX_VAR);
			((menuDepends_t *) b)->cond = Com_ParseConditionType(condition, token);
		} else
			Sys_Error("Com_ParseValue: Illegal if statement '%s'\n", token);

		return ALIGN(sizeof(menuDepends_t));

	case V_RELABS:
		if (token[0] == '-' || token[0] == '+') {
			if (fabs(atof(token + 1)) <= 2.0f)
				Com_Printf("Com_ParseValue: a V_RELABS (absolute) value should always be bigger than +/-2.0\n");
			if (token[0] == '-')
				*(float *) b = atof(token+1) * (-1);
			else
				*(float *) b = atof(token+1);
		} else {
			if (fabs(atof(token)) > 2.0f)
				Com_Printf("Com_ParseValue: a V_RELABS (relative) value should only be between 0.00..1 and 2.0\n");
			*(float *) b = atof(token);
		}
		return ALIGN(sizeof(float));

	default:
		Sys_Error("Com_ParseValue: unknown value type '%s'\n", token);
		return -1;
	}
}


/**
 * @brief
 * @param[in] base The start pointer to a given data type (typedef, struct)
 * @param[in] set The data which should be parsed
 * @param[in] type The data type that should be parsed
 * @param[in] ofs The offset for the value
 * @sa Com_ValueToStr
 * @note The offset is most likely given by the offsetof macro
 */
#ifdef DEBUG
int Com_SetValueDebug (void *base, const void *set, valueTypes_t type, int ofs, size_t size, const char *file, int line)
#else
int Com_SetValue (void *base, const void *set, valueTypes_t type, int ofs, size_t size)
#endif
{
	byte *b;
	int len;

	b = (byte *) base + ofs;

	if (size) {
#ifdef DEBUG
		if (size > vt_sizes[type]) {
			Com_Printf("Warning: Size mismatch: given size: "UFO_SIZE_T", should be: "UFO_SIZE_T". File: '%s', line: %i (type: %i)\n", size, vt_sizes[type], file, line, type);
		}
		if (size < vt_sizes[type]) {
			/* disable this sys error to return -1 and print the value_t->string that is wrong in the parsing function */
			Sys_Error("Size mismatch: given size: "UFO_SIZE_T", should be: "UFO_SIZE_T". File: '%s', line: %i (type: %i)\n", size, vt_sizes[type], file, line, type);
			return -1;	/* don't delete this please */
		}
#else
		if (size < vt_sizes[type]) {
			/* disable this sys error to return -1 and print the value_t->string that is wrong in the parsing function */
			Sys_Error("Size mismatch: given size: "UFO_SIZE_T", should be: "UFO_SIZE_T". (type: %i)\n", size, vt_sizes[type], type);
			return -1;	/* don't delete this please */
		}
#endif
	}

	switch (type) {
	case V_NULL:
		return ALIGN(0);

	case V_BOOL:
		if (*(const qboolean *) set)
			*(qboolean *)b = qtrue;
		else
			*(qboolean *)b = qfalse;
		return ALIGN(sizeof(qboolean));

	case V_CHAR:
		*(char *) b = *(const char *) set;
		return ALIGN(sizeof(char));

	case V_INT:
		*(int *) b = *(const int *) set;
		return ALIGN(sizeof(int));

	case V_INT2:
		((int *) b)[0] = ((const int *) set)[0];
		((int *) b)[1] = ((const int *) set)[1];
		return ALIGN(2 * sizeof(int));

	case V_FLOAT:
		*(float *) b = *(const float *) set;
		return ALIGN(sizeof(float));

	case V_POS:
		((float *) b)[0] = ((const float *) set)[0];
		((float *) b)[1] = ((const float *) set)[1];
		return ALIGN(2 * sizeof(float));

	case V_VECTOR:
		((float *) b)[0] = ((const float *) set)[0];
		((float *) b)[1] = ((const float *) set)[1];
		((float *) b)[2] = ((const float *) set)[2];
		return ALIGN(3 * sizeof(float));

	case V_COLOR:
		((float *) b)[0] = ((const float *) set)[0];
		((float *) b)[1] = ((const float *) set)[1];
		((float *) b)[2] = ((const float *) set)[2];
		((float *) b)[3] = ((const float *) set)[3];
		return ALIGN(4 * sizeof(float));

	case V_RGBA:
		((byte *) b)[0] = ((const byte *) set)[0];
		((byte *) b)[1] = ((const byte *) set)[1];
		((byte *) b)[2] = ((const byte *) set)[2];
		((byte *) b)[3] = ((const byte *) set)[3];
		return ALIGN(4 * sizeof(int));

	case V_STRING:
		Q_strncpyz((char *) b, (const char *) set, MAX_VAR);
		len = (int)strlen((const char *) set) + 1;
		if (len > MAX_VAR)
			len = MAX_VAR;
		return len;

	case V_LONGSTRING:
		strcpy((char *) b, (const char *) set);
		len = (int)strlen((const char *) set) + 1;
		return len;

	case V_ALIGN:
	case V_BLEND:
	case V_STYLE:
	case V_FADE:
		*b = *(const byte *) set;
		return ALIGN(1);

	case V_SHAPE_SMALL:
		*(int *) b = *(const int *) set;
		return ALIGN(SHAPE_SMALL_MAX_HEIGHT);

	case V_SHAPE_BIG:
		memcpy(b, set, 64);
		return ALIGN(SHAPE_BIG_MAX_HEIGHT * 4);

	case V_DMGTYPE:
		*b = *(const byte *) set;
		return ALIGN(1);

	case V_DATE:
		memcpy(b, set, sizeof(date_t));
		return sizeof(date_t);

	default:
		Sys_Error("Com_SetValue: unknown value type\n");
		return -1;
	}
}

/**
 * @brief
 * @param[in] base The start pointer to a given data type (typedef, struct)
 * @param[in] type The data type that should be parsed
 * @param[in] ofs The offset for the value
 * @sa Com_SetValue
 * @return char pointer with translated data type value
 */
const char *Com_ValueToStr (void *base, valueTypes_t type, int ofs)
{
	static char valuestr[MAX_VAR];
	byte *b;

	b = (byte *) base + ofs;

	switch (type) {
	case V_NULL:
		return 0;

	case V_CLIENT_HUNK:
	case V_CLIENT_HUNK_STRING:
		if (b == NULL)
			return "(null)";
		else
			return (char*)b;

	case V_BOOL:
		if (*b)
			return "true";
		else
			return "false";

	case V_CHAR:
		return (char *) b;
		break;

	case V_INT:
		Com_sprintf(valuestr, sizeof(valuestr), "%i", *(int *) b);
		return valuestr;

	case V_INT2:
		Com_sprintf(valuestr, sizeof(valuestr), "%i %i", ((int *) b)[0], ((int *) b)[1]);
		return valuestr;

	case V_FLOAT:
		Com_sprintf(valuestr, sizeof(valuestr), "%.2f", *(float *) b);
		return valuestr;

	case V_POS:
		Com_sprintf(valuestr, sizeof(valuestr), "%.2f %.2f", ((float *) b)[0], ((float *) b)[1]);
		return valuestr;

	case V_VECTOR:
		Com_sprintf(valuestr, sizeof(valuestr), "%.2f %.2f %.2f", ((float *) b)[0], ((float *) b)[1], ((float *) b)[2]);
		return valuestr;

	case V_COLOR:
		Com_sprintf(valuestr, sizeof(valuestr), "%.2f %.2f %.2f %.2f", ((float *) b)[0], ((float *) b)[1], ((float *) b)[2], ((float *) b)[3]);
		return valuestr;

	case V_RGBA:
		Com_sprintf(valuestr, sizeof(valuestr), "%3i %3i %3i %3i", ((byte *) b)[0], ((byte *) b)[1], ((byte *) b)[2], ((byte *) b)[3]);
		return valuestr;

	case V_TRANSLATION_STRING:
	case V_TRANSLATION2_STRING:
	case V_STRING:
	case V_LONGSTRING:
		if (b == NULL)
			return "(null)";
		else
			return (char *) b;

	case V_ALIGN:
		assert(*(int *)b < ALIGN_LAST);
		Q_strncpyz(valuestr, align_names[*(align_t *)b], sizeof(valuestr));
		return valuestr;

	case V_BLEND:
		assert(*(blend_t *)b < BLEND_LAST);
		Q_strncpyz(valuestr, blend_names[*(blend_t *)b], sizeof(valuestr));
		return valuestr;

	case V_STYLE:
		assert(*(style_t *)b < STYLE_LAST);
		Q_strncpyz(valuestr, style_names[*(style_t *)b], sizeof(valuestr));
		return valuestr;

	case V_FADE:
		assert(*(fade_t *)b < FADE_LAST);
		Q_strncpyz(valuestr, fade_names[*(fade_t *)b], sizeof(valuestr));
		return valuestr;

	case V_SHAPE_SMALL:
	case V_SHAPE_BIG:
		return "";

	case V_DMGTYPE:
		assert(*(int *)b < MAX_DAMAGETYPES);
		return csi.dts[*(int *)b];

	case V_DATE:
		Com_sprintf(valuestr, sizeof(valuestr), "%i %i %i", ((date_t *) b)->day / 365, ((date_t *) b)->day % 365, ((date_t *) b)->sec);
		return valuestr;

	case V_IF:
		return "";

	case V_RELABS:
		/* absolute value */
		if (*(float *) b > 2.0)
			Com_sprintf(valuestr, sizeof(valuestr), "+%.2f", *(float *) b);
		/* absolute value */
		else if (*(float *) b < 2.0)
			Com_sprintf(valuestr, sizeof(valuestr), "-%.2f", *(float *) b);
		/* relative value */
		else
			Com_sprintf(valuestr, sizeof(valuestr), "%.2f", *(float *) b);
		return valuestr;

	default:
		Sys_Error("Com_ParseValue: unknown value type %i\n", type);
		return NULL;
	}
}


/*
==============================================================================
OBJECT DEFINITION INTERPRETER
==============================================================================
*/

static const char *skillNames[SKILL_NUM_TYPES - ABILITY_NUM_TYPES] = {
	"close",
	"heavy",
	"assault",
	"sniper",
	"explosive"
};

typedef enum objdefs {
	OD_WEAPON,
	OD_PROTECTION,
	OD_HARDNESS
} objdef_t;


/** @sa q_shared.h:equipment_buytypes_t */
static const char *buytypeNames[MAX_BUYTYPES] = {
	"weap_pri",
	"weap_sec",
	"misc",
	"armour",
	"multi_ammo",
	"aircraft",
	"dummy"
};

static const value_t od_vals[] = {
	{"weapon_mod", V_NULL, 0, 0},
	{"protection", V_NULL, 0, 0},
	{"hardness", V_NULL, 0, 0},

	{"name", V_TRANSLATION_STRING, offsetof(objDef_t, name), 0},
	{"model", V_STRING, offsetof(objDef_t, model), 0},
	{"image", V_STRING, offsetof(objDef_t, image), 0},
	{"type", V_STRING, offsetof(objDef_t, type), 0},
	{"category", V_CHAR, offsetof(objDef_t, category), MEMBER_SIZEOF(objDef_t, category)},
	{"shape", V_SHAPE_SMALL, offsetof(objDef_t, shape), MEMBER_SIZEOF(objDef_t, shape)},
	{"scale", V_FLOAT, offsetof(objDef_t, scale), MEMBER_SIZEOF(objDef_t, scale)},
	{"center", V_VECTOR, offsetof(objDef_t, center), MEMBER_SIZEOF(objDef_t, center)},
	{"weapon", V_BOOL, offsetof(objDef_t, weapon), MEMBER_SIZEOF(objDef_t, weapon)},
	{"holdtwohanded", V_BOOL, offsetof(objDef_t, holdtwohanded), MEMBER_SIZEOF(objDef_t, holdtwohanded)},
	{"firetwohanded", V_BOOL, offsetof(objDef_t, firetwohanded), MEMBER_SIZEOF(objDef_t, firetwohanded)},
	{"extends_item", V_STRING, offsetof(objDef_t, extends_item), 0},
	{"extension", V_BOOL, offsetof(objDef_t, extension), MEMBER_SIZEOF(objDef_t, extension)},
	{"headgear", V_BOOL, offsetof(objDef_t, headgear), MEMBER_SIZEOF(objDef_t, headgear)},
	{"thrown", V_BOOL, offsetof(objDef_t, thrown), MEMBER_SIZEOF(objDef_t, thrown)},
	{"ammo", V_INT, offsetof(objDef_t, ammo), MEMBER_SIZEOF(objDef_t, ammo)},
	{"oneshot", V_BOOL, offsetof(objDef_t, oneshot), MEMBER_SIZEOF(objDef_t, oneshot)},
	{"deplete", V_BOOL, offsetof(objDef_t, deplete), MEMBER_SIZEOF(objDef_t, deplete)},
	{"reload", V_INT, offsetof(objDef_t, reload), MEMBER_SIZEOF(objDef_t, reload)},
	{"size", V_INT, offsetof(objDef_t, size), MEMBER_SIZEOF(objDef_t, size)},
	{"price", V_INT, offsetof(objDef_t, price), MEMBER_SIZEOF(objDef_t, price)},
	{"buytype", V_INT, offsetof(objDef_t, buytype), MEMBER_SIZEOF(objDef_t, buytype)},	/** Not parsed automatically anymore, this enrty is just there for overview. */
	{"useable", V_INT, offsetof(objDef_t, useable), MEMBER_SIZEOF(objDef_t, useable)},
	{"notonmarket", V_BOOL, offsetof(objDef_t, notonmarket), MEMBER_SIZEOF(objDef_t, notonmarket)},
	{NULL, V_NULL, 0, 0}
};

/* =========================================================== */

static const value_t fdps[] = {
	{"name", V_TRANSLATION_STRING, offsetof(fireDef_t, name), 0},
	{"shotorg", V_POS, offsetof(fireDef_t, shotOrg), MEMBER_SIZEOF(fireDef_t, shotOrg)},
	{"projtl", V_STRING, offsetof(fireDef_t, projectile), 0},
	{"impact", V_STRING, offsetof(fireDef_t, impact), 0},
	{"hitbody", V_STRING, offsetof(fireDef_t, hitBody), 0},
	{"firesnd", V_STRING, offsetof(fireDef_t, fireSound), 0},
	{"impsnd", V_STRING, offsetof(fireDef_t, impactSound), 0},
	{"bodysnd", V_STRING, offsetof(fireDef_t, hitBodySound), 0},
	{"bncsnd", V_STRING, offsetof(fireDef_t, bounceSound), 0},
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
	{"modif", V_FLOAT, offsetof(fireDef_t, modif), MEMBER_SIZEOF(fireDef_t, modif)},
	{"crouch", V_FLOAT, offsetof(fireDef_t, crouch), MEMBER_SIZEOF(fireDef_t, crouch)},
/*	{"range", V_FLOAT, offsetof(fireDef_t, range), MEMBER_SIZEOF(fireDef_t, range)},*/
	{"shots", V_INT, offsetof(fireDef_t, shots), MEMBER_SIZEOF(fireDef_t, shots)},
	{"ammo", V_INT, offsetof(fireDef_t, ammo), MEMBER_SIZEOF(fireDef_t, ammo)},
	{"rof", V_FLOAT, offsetof(fireDef_t, rof), MEMBER_SIZEOF(fireDef_t, rof)},
	{"time", V_INT, offsetof(fireDef_t, time), MEMBER_SIZEOF(fireDef_t, time)},
	{"damage", V_POS, offsetof(fireDef_t, damage), MEMBER_SIZEOF(fireDef_t, damage)},
	{"spldmg", V_POS, offsetof(fireDef_t, spldmg), MEMBER_SIZEOF(fireDef_t, spldmg)},
/*	{"splrad", V_FLOAT, offsetof(fireDef_t, splrad), MEMBER_SIZEOF(fireDef_t, splrad)},*/
	{"dmgtype", V_DMGTYPE, offsetof(fireDef_t, dmgtype), MEMBER_SIZEOF(fireDef_t, dmgtype)},
	{"irgoggles", V_BOOL, offsetof(fireDef_t, irgoggles), MEMBER_SIZEOF(fireDef_t, irgoggles)},
	{NULL, 0, 0, 0}
};


/**
 * @brief
 */
static void Com_ParseFire (const char *name, const char **text, fireDef_t * fd)
{
	const value_t *fdp;
	const char *errhead = "Com_ParseFire: unexpected end of file";
	const char *token;

	/* get its body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseFire: fire definition \"%s\" without body ignored\n", name);
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			return;
		if (*token == '}')
			return;

		for (fdp = fdps; fdp->string; fdp++)
			if (!Q_stricmp(token, fdp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_ParseValue(fd, token, fdp->type, fdp->ofs, fdp->size);
				break;
			}

		if (!fdp->string) {
			if (!Q_strncmp(token, "skill", 5)) {
				int skill;

				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				for (skill = ABILITY_NUM_TYPES; skill < SKILL_NUM_TYPES; skill++)
					if (!Q_stricmp(skillNames[skill - ABILITY_NUM_TYPES], token)) {
						fd->weaponSkill = skill;
						break;
					}
				if (skill >= SKILL_NUM_TYPES)
					Com_Printf("Com_ParseFire: unknown weapon skill \"%s\" ignored (weapon %s)\n", token, name);
			} else if (!Q_strncmp(token, "range", 5)) {
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				fd->range = atof(token) * UNIT_SIZE;
			} else if (!Q_strncmp(token, "splrad", 6)) {
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				fd->splrad = atof(token) * UNIT_SIZE;
			} else
				Com_Printf("Com_ParseFire: unknown token \"%s\" ignored (weapon %s)\n", token, name);
		}
	} while (*text);
}


/**
 * @brief
 * @sa Com_ParseItem
 */
static void Com_ParseArmor (const char *name, const char **text, short *ad)
{
	const char *errhead = "Com_ParseArmor: unexpected end of file";
	const char *token;
	int i;

	/* get its body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseArmor: armor definition \"%s\" without body ignored\n", name);
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			return;
		if (*token == '}')
			return;

		for (i = 0; i < csi.numDTs; i++)
			if (!Q_strncmp(token, csi.dts[i], MAX_VAR)) {
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				/* protection or damage type values */
				ad[i] = atoi(token);
				break;
			}

		if (i >= csi.numDTs)
			Com_Printf("Com_ParseArmor: unknown damage type \"%s\" ignored (in %s)\n", token, name);
	} while (*text);
}

/**
 * @brief
 */
static void Com_ParseItem (const char *name, const char **text)
{
	const char *errhead = "Com_ParseItem: unexpected end of file (weapon ";
	const value_t *val;
	objDef_t *od;
	const char *token;
	int i,j;
	int weap_fds_idx, fd_idx;

	/* search for menus with same name */
	for (i = 0; i < csi.numODs; i++)
		if (!Q_strncmp(name, csi.ods[i].name, sizeof(csi.ods[i].name)))
			break;

	if (i < csi.numODs) {
		Com_Printf("Com_ParseItem: weapon def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (i >= MAX_OBJDEFS)
		Sys_Error("Com_ParseItem: MAX_OBJDEFS exceeded\n");

	/* initialize the object definition */
	od = &csi.ods[csi.numODs++];
	memset(od, 0, sizeof(objDef_t));

	/* default value is no ammo */
	memset(od->weap_idx, -1, sizeof(od->weap_idx));

	Q_strncpyz(od->id, name, sizeof(od->id));

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseItem: weapon def \"%s\" without body ignored\n", name);
		csi.numODs--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (val = od_vals, i = 0; val->string; val++, i++) {
			if (!Q_stricmp(token, "buytype")) {
				/* Found a buytype option ... checking for the correct string */

				/* parse a value */
				token = COM_EParse(text, errhead, name);
				/* @todo: maybe we couidl add a check for hte old numbers as well here */
				for (j = 0; j < MAX_BUYTYPES; j++) {
					if (!Q_stricmp(token, buytypeNames[j])) {
						od->buytype = j;
						break;
					}
				}
				break;
			} else if (!Q_stricmp(token, val->string)) {
				/* found a definition */
				if (val->type != V_NULL) {
					/* parse a value */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						break;

					if (Com_ParseValue(od, token, val->type, val->ofs, val->size) == -1)
						Com_Printf("Com_ParseItem: Wrong size for value %s\n", val->string);
				} else {
					/* parse fire definitions */
					switch (i) {
					case OD_WEAPON:
						/* Save the weapon id. */
						token = COM_Parse(text);
						Q_strncpyz(od->weap_id[od->numWeapons], token, sizeof(od->weap_id[od->numWeapons]));

						/* get it's body */
						token = COM_Parse(text);

						if (!*text || *token != '{') {
							Com_Printf("Com_ParseItem: weapon_mod \"%s\" without body ignored\n", name);
							break;
						}

						if (od->numWeapons < MAX_WEAPONS_PER_OBJDEF) {
							weap_fds_idx = od->numWeapons;
							/* For parse each firedef entry for this weapon.  */
							do {
								token = COM_EParse(text, errhead, name);
								if (!*text)
									return;
								if (*token == '}')
									break;

								if (!Q_strncmp(token, "firedef", MAX_VAR)) {
									if (od->numFiredefs[weap_fds_idx] < MAX_FIREDEFS_PER_WEAPON) {
										fd_idx = od->numFiredefs[weap_fds_idx];
										/* Parse firemode into fd[IDXweapon][IDXfiremode] */
										Com_ParseFire(name, text, &od->fd[weap_fds_idx][fd_idx]);
										/* Self-link fd */
										od->fd[weap_fds_idx][fd_idx].fd_idx = fd_idx;
										/* Self-link weapon_mod */
										od->fd[weap_fds_idx][fd_idx].weap_fds_idx = weap_fds_idx;
										od->numFiredefs[od->numWeapons]++;
									} else {
										Com_Printf("Com_ParseItem: Too many firedefs at \"%s\". Max is %i\n", name, MAX_FIREDEFS_PER_WEAPON);
									}
								}
								/*
								else {
									Bad syntax.
								}
								*/
							} while (*text);
							od->numWeapons++;
						} else {
							Com_Printf("Com_ParseItem: Too many weapon_mod definitions at \"%s\". Max is %i\n", name, MAX_WEAPONS_PER_OBJDEF);
						}
						break;
					case OD_PROTECTION:
						Com_ParseArmor(name, text, od->protection);
						break;
					case OD_HARDNESS:
						Com_ParseArmor(name, text, od->hardness);
						break;
					default:
						break;
					}
				}
				break;
			}
		}
		if (!val->string)
			Com_Printf("Com_ParseItem: unknown token \"%s\" ignored (weapon %s)\n", token, name);

	} while (*text);

	/* get size */
	for (i = SHAPE_SMALL_MAX_WIDTH - 1; i >= 0; i--)
		if (od->shape & (0x01010101 << i))
			break;
	od->sx = i + 1;

	for (i = SHAPE_SMALL_MAX_HEIGHT - 1; i >= 0; i--)
		if (od->shape & (0xFF << (i * SHAPE_SMALL_MAX_WIDTH)))
			break;
	od->sy = i + 1;
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
	/* only a single item as weapon extension - single should be set, too */
	{"extension", V_BOOL, offsetof(invDef_t, extension), MEMBER_SIZEOF(invDef_t, extension)},
	/* this is the armor container */
	{"armor", V_BOOL, offsetof(invDef_t, armor), MEMBER_SIZEOF(invDef_t, armor)},
	/* this is the headgear container */
	{"headgear", V_BOOL, offsetof(invDef_t, headgear), MEMBER_SIZEOF(invDef_t, headgear)},
	/* allow everything to be stored in this container (e.g armor and weapons) */
	{"all", V_BOOL, offsetof(invDef_t, all), MEMBER_SIZEOF(invDef_t, all)},
	{"temp", V_BOOL, offsetof(invDef_t, temp), MEMBER_SIZEOF(invDef_t, temp)},
	/* time units for moving something in */
	{"in", V_INT, offsetof(invDef_t, in), MEMBER_SIZEOF(invDef_t, in)},
	/* time units for moving something out */
	{"out", V_INT, offsetof(invDef_t, out), MEMBER_SIZEOF(invDef_t, out)},

	{NULL, 0, 0, 0}
};

/**
 * @brief
 */
static void Com_ParseInventory (const char *name, const char **text)
{
	const char *errhead = "Com_ParseInventory: unexpected end of file (inventory ";
	invDef_t *id;
	const value_t *idp;
	const char *token;
	int i;

	/* search for containers with same name */
	for (i = 0; i < csi.numIDs; i++)
		if (!Q_strncmp(name, csi.ids[i].name, sizeof(csi.ids[i].name)))
			break;

	if (i < csi.numIDs) {
		Com_Printf("Com_ParseInventory: inventory def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (i >= MAX_INVDEFS) {
		Sys_Error("Too many inventory definitions - max allowed: %i\n", MAX_INVDEFS);
		return; /* never reached */
	}

	/* initialize the inventory definition */
	id = &csi.ids[csi.numIDs++];
	memset(id, 0, sizeof(invDef_t));

	Q_strncpyz(id->name, name, sizeof(id->name));

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseInventory: inventory def \"%s\" without body ignored\n", name);
		csi.numIDs--;
		return;
	}

	/* special IDs */
	if (!Q_strncmp(name, "right", 5))
		csi.idRight = id - csi.ids;
	else if (!Q_strncmp(name, "left", 4))
		csi.idLeft = id - csi.ids;
	else if (!Q_strncmp(name, "extension", 4))
		csi.idExtension = id - csi.ids;
	else if (!Q_strncmp(name, "belt", 4))
		csi.idBelt = id - csi.ids;
	else if (!Q_strncmp(name, "holster", 7))
		csi.idHolster = id - csi.ids;
	else if (!Q_strncmp(name, "backpack", 8))
		csi.idBackpack = id - csi.ids;
	else if (!Q_strncmp(name, "armor", 5))
		csi.idArmor = id - csi.ids;
	else if (!Q_strncmp(name, "floor", 5))
		csi.idFloor = id - csi.ids;
	else if (!Q_strncmp(name, "equip", 5))
		csi.idEquip = id - csi.ids;
	else if (!Q_strncmp(name, "headgear", 8))
		csi.idHeadgear = id - csi.ids;

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			return;
		if (*token == '}')
			return;

		for (idp = idps; idp->string; idp++)
			if (!Q_stricmp(token, idp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_ParseValue(id, token, idp->type, idp->ofs, idp->size);
				break;
			}

		if (!idp->string)
			Com_Printf("Com_ParseInventory: unknown token \"%s\" ignored (inventory %s)\n", token, name);

	} while (*text);
}


/*
==============================================================================
EQUIPMENT DEFINITION INTERPRETER
==============================================================================
*/

#define MAX_NAMECATS	64
#define MAX_INFOSTRING	65536

typedef enum model_script_s {
	MODEL_PATH,
	MODEL_BODY,
	MODEL_HEAD,
	MODEL_SKIN,

	MODEL_NUM_TYPES
} model_script_t;

/* different sounds for each sex and actor definition */
#define MAX_ACTOR_SOUNDS 16

typedef struct nameCategory_s {
	char title[MAX_VAR];
	char *names[NAME_NUM_TYPES];
	int numNames[NAME_NUM_TYPES];
	char *models[NAME_LAST];
	int numModels[NAME_LAST];
	char sounds[MAX_SOUND_TYPES][NAME_LAST][MAX_ACTOR_SOUNDS][MAX_QPATH];
	int numSounds[MAX_SOUND_TYPES][NAME_LAST];
} nameCategory_t;

typedef struct teamDef_s {
	char title[MAX_VAR];
	char *cats;
	int num;
} teamDef_t;

teamDesc_t teamDesc[MAX_TEAMDEFS];

static nameCategory_t nameCat[MAX_NAMECATS];
static teamDef_t teamDef[MAX_TEAMDEFS];
static char infoStr[MAX_INFOSTRING];
static char *infoPos; /* size checked - see infoSize */
static int numNameCats = 0;
static int numTeamDefs = 0;
int numTeamDesc = 0;

const char *name_strings[NAME_NUM_TYPES] = {
	"neutral",
	"female",
	"male",
	"lastname",
	"female_lastname",
	"male_lastname"
};


/**
 * @brief
 */
static void Com_ParseEquipment (const char *name, const char **text)
{
	const char *errhead = "Com_ParseEquipment: unexpected end of file (equipment ";
	equipDef_t *ed;
	const char *token;
	int i, n;

	/* search for equipments with same name */
	for (i = 0; i < csi.numEDs; i++)
		if (!Q_strncmp(name, csi.eds[i].name, MAX_VAR))
			break;

	if (i < csi.numEDs) {
		Com_Printf("Com_ParseEquipment: equipment def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (i >= MAX_EQUIPDEFS)
		Sys_Error("Com_ParseEquipment: MAX_EQUIPDEFS exceeded\n");

	/* initialize the equipment definition */
	ed = &csi.eds[csi.numEDs++];
	memset(ed, 0, sizeof(equipDef_t));

	Q_strncpyz(ed->name, name, sizeof(ed->name));

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseEquipment: equipment def \"%s\" without body ignored\n", name);
		csi.numEDs--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text || *token == '}')
			return;

		for (i = 0; i < csi.numODs; i++)
			if (!Q_strncmp(token, csi.ods[i].id, MAX_VAR)) {
				token = COM_EParse(text, errhead, name);
				if (!*text || *token == '}') {
					Com_Printf("Com_ParseEquipment: unexpected end of equipment def \"%s\"\n", name);
					return;
				}
				n = atoi(token);
				if (n)
					ed->num[i] = n;
				break;
			}

		if (i == csi.numODs)
			Com_Printf("Com_ParseEquipment: unknown token \"%s\" ignored (equipment %s)\n", token, name);

	} while (*text);
}


/*
==============================================================================
NAME AND TEAM DEFINITION INTERPRETER
==============================================================================
*/

/**
 * @brief
 * @param[in] gender 1 (female) or 2 (male)
 * @param[in] category country strings like: spanish_italian, german, russian and so on
 * @sa Com_GetModelAndName
 */
char *Com_GiveName (int gender, const char *category)
{
	static char returnName[MAX_VAR];
	nameCategory_t *nc;
	char *pos;
	int i, j, name = 0;

	/* search the name */
	for (i = 0, nc = nameCat; i < numNameCats; i++, nc++)
		if (!Q_strncmp(category, nc->title, MAX_VAR)) {
#ifdef DEBUG
			for (j = 0; j < NAME_NUM_TYPES; j++)
				name += nc->numNames[j];
			if (!name)
				Sys_Error("Could not find any valid name definitions for category '%s'\n", category);
#endif
			/* found category */
			if (!nc->numNames[gender]) {
#ifdef DEBUG
				Com_DPrintf("No valid name definitions for gender %i in category '%s'\n", gender, category);
#endif
				return NULL;
			}
			name = rand() % nc->numNames[gender];

			/* skip names */
			pos = nc->names[gender];
			for (j = 0; j < name; j++)
				pos += strlen(pos) + 1;

			/* store the name */
			Q_strncpyz(returnName, pos, sizeof(returnName));
			return returnName;
		}

	/* nothing found */
	return NULL;
}

/**
 * @brief
 * @param[in] type MODEL_PATH, MODEL_BODY, MODEL_HEAD, MODEL_SKIN (path, body, head, skin - see team_*.ufo)
 * @param[in] gender 1 (female) or 2 (male)
 * @param[in] category country strings like: spanish_italian, german, russian and so on
 * @sa Com_GetModelAndName
 */
char *Com_GiveModel (int type, int gender, const char *category)
{
	nameCategory_t *nc;
	char *str;
	int i, j, num;

	/* search the name */
	for (i = 0, nc = nameCat; i < numNameCats; i++, nc++)
		if (!Q_strncmp(category, nc->title, MAX_VAR)) {
			/* found category */
			if (!nc->numModels[gender]) {
				Com_Printf("Com_GiveModel: no models defined for gender %i and category '%s'\n", gender, category);
				return NULL;
			}
			/* search one of the model definitions */
			num = (rand() % nc->numModels[gender]) * MODEL_NUM_TYPES;
			/* now go to the type entry from team_*.ufo */
			num += type;

			/* skip models and unwanted info */
			str = nc->models[gender];
			for (j = 0; j < num; j++)
				str += strlen(str) + 1;

			/* return the value */
			return str;
		}

	Com_Printf("Com_GiveModel: no models for gender %i and category '%s'\n", gender, category);
	/* nothing found */
	return NULL;
}

/**
 * @brief Returns the actor sounds for a given category
 * @param[in] category Index in nameCat array (from le_t after actor appear event)
 * @param[in] gender The gender of the actor
 * @param[in] sound Which sound category (actorSound_t)
 */
const char* Com_GetActorSound (int category, int gender, actorSound_t soundType)
{
	int random;

	if (category < 0 || category >= numNameCats) {
		Com_Printf("Com_GetActorSound: invalid category: %i\n", category);
		return NULL;
	}
	if (gender < 0 || gender >= 3) {
		Com_Printf("Com_GetActorSound: invalid gender: %i\n", gender);
		return NULL;
	}
	if (nameCat[category].numSounds[soundType][gender] <= 0) {
		Com_Printf("Com_GetActorSound: no sound defined for soundtype: %i, category: %i, gender: %i\n", soundType, category, gender);
		return NULL;
	}

	random = rand() % nameCat[category].numSounds[soundType][gender];

	return nameCat[category].sounds[soundType][gender][random];
}

/**
 * @brief Assign 3D models and names to a character.
 * @param[in] team What team the character is on.
 * @param[in,out] chr The character that should get the paths to the differenbt models/skins.
 * @sa Com_GiveName
 * @sa Com_GiveModel
 */
int Com_GetModelAndName (const char *team, character_t * chr)
{
	teamDef_t *td;
	char *str;
	int i, gender, category = 0;
	int retry = 1000;

	/* get team definition */
	for (i = 0; i < numTeamDefs; i++)
		if (!Q_strncmp(team, teamDef[i].title, MAX_VAR))
			break;

	if (i < numTeamDefs)
		td = &teamDef[i];
	else {
		Com_DPrintf("Com_GetModelAndName: could not find team '%s' in team definitions - searching name definitions now\n", team);
		/* search in name categories, if it isn't a team definition */
		td = NULL;
		for (i = 0; i < numNameCats; i++)
			if (!Q_strncmp(team, nameCat[i].title, MAX_VAR))
				break;
		if (i == numNameCats) {
			/* use default team */
			td = &teamDef[0];
			assert(td);
			Com_DPrintf("Com_GetModelAndName: could not find team '%s' in name definitions - using the first team definition now: '%s'\n", team, td->title);
		} else
			category = i;
	}

#ifdef DEBUG
	if (!td)
		Com_DPrintf("Com_GetModelAndName: Warning - this team (%s) could not be handled via aliencont code - no tech pointer will be available\n", team);
#endif

	/* get the models */
	while (retry--) {
		gender = rand() % NAME_LAST;
		if (td)
			category = (int) td->cats[rand() % td->num];

		chr->category = category;
		chr->gender = gender;
		for (i = 0; i < numTeamDesc; i++)
			if (!Q_strcmp(teamDesc[i].id, nameCat[category].title)) {
				/* transfered as byte - 0 means, not found - no -1 possible */
				chr->teamDesc = i + 1;
				/* set some team definition values to character, too */
				/* make these values available to the game lib */
				chr->weapons = teamDesc[i].weapons;
				chr->armor = teamDesc[i].armor;
			}

#ifdef DEBUG
		if (i == numTeamDesc)
			Com_DPrintf("Com_GetModelAndName: Warning - could not find a valid teamdesc for team '%s'\n", nameCat[category].title);
#endif

		/* get name */
		str = Com_GiveName(gender, nameCat[category].title);
		if (!str)
			continue;
		Q_strncpyz(chr->name, str, sizeof(chr->name));
		Q_strcat(chr->name, " ", sizeof(chr->name));
		str = Com_GiveName(gender + LASTNAME, nameCat[category].title);
		if (!str)
			continue;
		Q_strcat(chr->name, str, sizeof(chr->name));

		/* get model */
		str = Com_GiveModel(MODEL_PATH, gender, nameCat[category].title);
		if (!str)
			continue;
		Q_strncpyz(chr->path, str, sizeof(chr->path));

		str = Com_GiveModel(MODEL_BODY, gender, nameCat[category].title);
		if (!str)
			continue;
		Q_strncpyz(chr->body, str, sizeof(chr->body));

		str = Com_GiveModel(MODEL_HEAD, gender, nameCat[category].title);
		if (!str)
			continue;
		Q_strncpyz(chr->head, str, sizeof(chr->head));

		str = Com_GiveModel(MODEL_SKIN, gender, nameCat[category].title);
		if (!str)
			continue;
		return atoi(str);
	}
	return 0;
}

/**
 * @brief Parses "name" definition from team_* ufo script files
 * @sa Com_ParseActors
 * @sa Com_ParseScripts
 * @note fills the nameCat array
 * @note searches whether the actor id is already somewhere in the nameCat array
 * if not, it generates a new nameCat entry
 */
static void Com_ParseNames (const char *name, const char **text)
{
	nameCategory_t *nc;
	const char *errhead = "Com_ParseNames: unexpected end of file (names ";
	const char *token;
	int i;

	/* check for additions to existing name categories */
	for (i = 0, nc = nameCat; i < numNameCats; i++, nc++)
		if (!Q_strncmp(nc->title, name, sizeof(nc->title)))
			break;

	/* reset new category */
	if (i == numNameCats) {
		memset(nc, 0, sizeof(nameCategory_t));
		numNameCats++;
	}
	Q_strncpyz(nc->title, name, sizeof(nc->title));

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseNames: name def \"%s\" without body ignored\n", name);
		if (numNameCats - 1 == nc - nameCat)
			numNameCats--;
		return;
	}

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (i = 0; i < NAME_NUM_TYPES; i++)
			if (!Q_strcmp(token, name_strings[i])) {
				/* initialize list */
				nc->names[i] = infoPos;
				nc->numNames[i] = 0;

				token = COM_EParse(text, errhead, name);
				if (!*text)
					break;
				if (*token != '{')
					break;

				do {
					/* get a name */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						break;
					if (*token == '}')
						break;

					/* some names can be translateable */
					if (*token == '_')
						token++;
					strcpy(infoPos, token);
					infoPos += strlen(token) + 1;
					nc->numNames[i]++;
				} while (*text);

				/* lastname is different */
				/* fill female and male lastnames from neutral lastnames */
				if (i == NAME_LAST)
					for (i = NAME_NUM_TYPES - 1; i > NAME_LAST; i--) {
						nc->names[i] = nc->names[NAME_LAST];
						nc->numNames[i] = nc->numNames[NAME_LAST];
					}
				break;
			}

		if (i == NAME_NUM_TYPES)
			Com_Printf("Com_ParseNames: unknown token \"%s\" ignored (names %s)\n", token, name);

	} while (*text);

	if (nc->numNames[NAME_FEMALE] && !nc->numNames[NAME_FEMALE_LAST])
		Sys_Error("Com_ParseNames: '%s' has no female lastname category\n", nc->title);
	if (nc->numNames[NAME_MALE] && !nc->numNames[NAME_MALE_LAST])
		Sys_Error("Com_ParseNames: '%s' has no male lastname category\n", nc->title);
	if (nc->numNames[NAME_NEUTRAL] && !nc->numNames[NAME_LAST])
		Sys_Error("Com_ParseNames: '%s' has no neutral lastname category\n", nc->title);
}


/**
 * @brief Parses "actors" definition from team_* ufo script files
 * @sa Com_ParseNames
 * @sa Com_ParseScripts
 * @note fills the nameCat array
 * @note searches whether the actor id is already somewhere in the nameCat array
 * if not, it generates a new nameCat entry
 */
static void Com_ParseActors (const char *name, const char **text)
{
	nameCategory_t *nc;
	const char *errhead = "Com_ParseActors: unexpected end of file (actors ";
	const char *token;
	int i, j;

	/* check for additions to existing name categories */
	for (i = 0, nc = nameCat; i < numNameCats; i++, nc++)
		if (!Q_strncmp(nc->title, name, sizeof(nc->title)))
			break;

	/* reset new category */
	if (i == numNameCats) {
		if (numNameCats < MAX_NAMECATS) {
			memset(nc, 0, sizeof(nameCategory_t));
			numNameCats++;
		} else {
			Com_Printf("Too many name categories, '%s' ignored.\n", name);
			return;
		}
	}
	Q_strncpyz(nc->title, name, sizeof(nc->title));

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseActors: actor def \"%s\" without body ignored\n", name);
		if (numNameCats - 1 == nc - nameCat)
			numNameCats--;
		return;
	}

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (i = 0; i < NAME_NUM_TYPES; i++)
			if (!Q_strcmp(token, name_strings[i])) {
				/* initialize list */
				nc->models[i] = infoPos;
				nc->numModels[i] = 0;
				token = COM_EParse(text, errhead, name);
				if (!*text)
					break;
				if (*token != '{')
					break;

				do {
					/* get the path, body, head and skin */
					for (j = 0; j < 4; j++) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							break;
						if (*token == '}')
							break;

						if (j == 3 && *token == '*')
							*infoPos++ = 0;
						else {
							strcpy(infoPos, token);
							infoPos += strlen(token) + 1;
						}
					}

					/* only add complete actor info */
					if (j == 4)
						nc->numModels[i]++;
					else
						break;
				}
				while (*text);
				break;
			}

		if (i == NAME_NUM_TYPES)
			Com_Printf("Com_ParseActors: unknown token \"%s\" ignored (actors %s)\n", token, name);

	} while (*text);
}

/**
 * @brief Parses "actorsounds" definition from team_* ufo script files
 * @sa Com_ParseNames
 * @sa Com_ParseScripts
 */
static void Com_ParseActorSounds (const char *name, const char **text)
{
	nameCategory_t *nc;
	const char *errhead = "Com_ParseActorSounds: unexpected end of file (actorsounds ";
	const char *token;
	int i;

	/* check for additions to existing name categories */
	for (i = 0, nc = nameCat; i < numNameCats; i++, nc++)
		if (!Q_strncmp(nc->title, name, sizeof(nc->title)))
			break;

	/* reset new category */
	if (i == numNameCats) {
		if (numNameCats < MAX_NAMECATS) {
			memset(nc, 0, sizeof(nameCategory_t));
			numNameCats++;
		} else {
			Com_Printf("Too many name categories, '%s' ignored.\n", name);
			return;
		}
	}
	Q_strncpyz(nc->title, name, sizeof(nc->title));

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseActorSounds: actorsounds def \"%s\" without body ignored\n", name);
		if (numNameCats - 1 == nc - nameCat)
			numNameCats--;
		return;
	}

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (i = 0; i < NAME_LAST; i++)
			if (!Q_strcmp(token, name_strings[i])) {
				token = COM_EParse(text, errhead, name);
				if (!*text)
					break;
				if (*token != '{')
					break;

				do {
					/* get the sounds */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						break;
					if (*token == '}')
						break;
					if (!Q_strcmp(token, "hurtsound")) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							break;
						Q_strncpyz(nc->sounds[SOUND_HURT][i][nc->numSounds[SOUND_HURT][i]++], token, MAX_QPATH);
					} else if (!Q_strcmp(token, "deathsound")) {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							break;
						Q_strncpyz(nc->sounds[SOUND_DEATH][i][nc->numSounds[SOUND_DEATH][i]++], token, MAX_QPATH);
					} else {
						Com_Printf("Com_ParseActorSounds: unknown token \"%s\" ignored (actorsounds %s)\n", token, name);
					}
				}
				while (*text);
				break;
			}

		if (i == NAME_NUM_TYPES)
			Com_Printf("Com_ParseActorSounds: unknown token \"%s\" ignored (actorsounds %s)\n", token, name);

	} while (*text);
}

/** @brief possible teamdesc values (ufo-scriptfiles) */
static const value_t teamDescValues[] = {
	{"tech", V_STRING, offsetof(teamDesc_t, tech), 0}, /**< tech id from research.ufo */
	{"name", V_TRANSLATION2_STRING, offsetof(teamDesc_t, name), 0}, /**< internal team name */
	{"alien", V_BOOL, offsetof(teamDesc_t, alien), MEMBER_SIZEOF(teamDesc_t, alien)}, /**< is this an alien? */
	{"armor", V_BOOL, offsetof(teamDesc_t, armor), MEMBER_SIZEOF(teamDesc_t, armor)}, /**< are these team members able to wear armor? */
	{"weapons", V_BOOL, offsetof(teamDesc_t, weapons), MEMBER_SIZEOF(teamDesc_t, weapons)}, /**< are these team members able to use weapons? */
	{NULL, 0, 0, 0}
};


/**
 * @brief Parse the team descriptions (teamdesc) in the teams*.ufo files.
 */
static void Com_ParseTeamDesc (const char *name, const char **text)
{
	teamDesc_t *td;
	const char *errhead = "Com_ParseTeamDesc: unexpected end of file (teamdesc ";
	const char *token;
	int i;
	const value_t *v;

	/* check whether team description already exists */
	for (i = 0, td = teamDesc; i < numTeamDesc; i++, td++)
		if (!Q_strncmp(td->id, name, MAX_VAR))
			break;

	/* reset new category */
	if (i >= MAX_TEAMDEFS) {
		Com_Printf("Too many team descriptions, '%s' ignored.\n", name);
		return;
	}

	if (i < numTeamDesc) {
		Com_Printf("Com_ParseTeamDesc: found defition with same name - second ignored\n");
		return;
	}

	td = &teamDesc[numTeamDesc++];
	memset(td, 0, sizeof(teamDesc_t));
	Q_strncpyz(td->id, name, sizeof(td->id));
	td->armor = td->weapons = qtrue; /* default values */

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseTeamDesc: team desc \"%s\" without body ignored\n", name);
		if (numTeamDesc - 1 == td - teamDesc)
			numTeamDesc--;
		return;
	}

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (v = teamDescValues; v->string; v++)
			if (!Q_strncmp(token, v->string, sizeof(v->string))) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_ParseValue(td, token, v->type, v->ofs, v->size);
				break;
			}

		if (!v->string)
			Com_Printf("Com_ParseTeamDesc: unknown token \"%s\" ignored (team %s)\n", token, name);
	} while (*text);
}

/**
 * @brief
 */
static void Com_ParseTeam (const char *name, const char **text)
{
	nameCategory_t *nc;
	teamDef_t *td;
	const char *errhead = "Com_ParseTeam: unexpected end of file (team ";
	const char *token;
	int i;

	/* check for additions to existing name categories */
	for (i = 0, td = teamDef; i < numTeamDefs; i++, td++)
		if (!Q_strncmp(td->title, name, sizeof(td->title)))
			break;

	/* reset new category */
	if (i == numTeamDefs) {
		if (numTeamDefs < MAX_TEAMDEFS) {
			memset(td, 0, sizeof(teamDef_t));
			numTeamDefs++;
		} else {
			Com_Printf("Too many team definitions, '%s' ignored.\n", name);
			return;
		}
	}
	Q_strncpyz(td->title, name, sizeof(td->title));

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseTeam: team def \"%s\" without body ignored\n", name);
		if (numTeamDefs - 1 == td - teamDef)
			numTeamDefs--;
		return;
	}

	/* initialize list */
	td->cats = infoPos;
	td->num = 0;

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		if (*token == '{') {
			for (;;) {
				token = COM_EParse(text, errhead, name);
				if (*token == '}') {
					token = COM_EParse(text, errhead, name);
					break;
				}
				for (i = 0, nc = nameCat; i < numNameCats; i++, nc++)
					if (!Q_strncmp(token, nc->title, MAX_VAR)) {
						*infoPos++ = (char) i;
						td->num++;
						break;
					}
				if (i == numNameCats)
					Com_Printf("Com_ParseTeam: unknown token \"%s\" ignored (team %s)\n", token, name);
			}
			if (*token == '}')
				break;
		}
	} while (*text);
}

/*
==============================================================================
GAMETYPE INTERPRETER
==============================================================================
*/

gametype_t gts[MAX_GAMETYPES];
int numGTs = 0;

/** @brief possible gametype values for the gameserver (ufo-scriptfiles) */
static const value_t gameTypeValues[] = {
	{"name", V_TRANSLATION2_STRING, offsetof(gametype_t, name), 0}, /**< translated game-type name for menu displaying */
	{NULL, 0, 0, 0}
};

static void Com_ParseGameTypes (const char *name, const char **text)
{
	const char *errhead = "Com_ParseGameTypes: unexpected end of file (gametype ";
	const char *token;
	int i;
	const value_t *v;
	gametype_t* gt;
	cvarlist_t* cvarlist;

	/* get it's body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("Com_ParseGameTypes: gametype \"%s\" without body ignored\n", name);
		return;
	}

	/* search for game types with same name */
	for (i = 0; i < numGTs; i++)
		if (!Q_strncmp(token, gts[i].id, MAX_VAR))
			break;

	if (i == numGTs) {
		if (i >= MAX_DAMAGETYPES)
			Sys_Error("Com_ParseGameTypes: MAX_DAMAGETYPES exceeded\n");
		gt = &gts[numGTs++];
		memset(gt, 0, sizeof(gametype_t));
		Q_strncpyz(gt->id, name, sizeof(gt->id));
		if (numGTs >= MAX_GAMETYPES)
			Sys_Error("Com_ParseGameTypes: Too many damage types.\n");

		do {
			token = COM_EParse(text, errhead, name);
			if (!*text)
				break;
			if (*token == '}')
				break;

			for (v = gameTypeValues; v->string; v++)
				if (!Q_strncmp(token, v->string, sizeof(v->string))) {
					/* found a definition */
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;

					Com_ParseValue(gt, token, v->type, v->ofs, v->size);
					break;
				}

			if (!v->string) {
				if (*token != '{')
					Sys_Error("Com_ParseGameTypes: gametype \"%s\" without cvarlist\n", name);

				do {
					token = COM_EParse(text, errhead, name);
					if (!*text || *token == '}') {
						if (!gt->num_cvars)
							Sys_Error("Com_ParseGameTypes: gametype \"%s\" with empty cvarlist\n", name);
						else
							break;
					}
					/* initial pointer */
					cvarlist = &gt->cvars[gt->num_cvars++];
					if (gt->num_cvars >= MAX_CVARLISTINGAMETYPE)
						Sys_Error("Com_ParseGameTypes: gametype \"%s\" max cvarlist hit\n", name);
					Q_strncpyz(cvarlist->name, token, sizeof(cvarlist->name));
					token = COM_EParse(text, errhead, name);
					if (!*text || *token == '}')
						Sys_Error("Com_ParseGameTypes: gametype \"%s\" cvar \"%s\" with no value\n", name, cvarlist->name);
					Q_strncpyz(cvarlist->value, token, sizeof(cvarlist->value));
				} while (*text && *token != '}');
			}
		} while (*text);
	} else {
		Com_Printf("Com_ParseGameTypes: gametype \"%s\" with same already exists - ignore the second one\n", name);
		FS_SkipBlock(text);
	}
}

/*
==============================================================================
DAMAGE TYPES INTERPRETER
==============================================================================
*/

/**
 * @brief
 */
static void Com_ParseDamageTypes (const char *name, const char **text)
{
	const char *errhead = "Com_ParseDamageTypes: unexpected end of file (damagetype ";
	const char *token;
	int i;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseDamageTypes: damage type list \"%s\" without body ignored\n", name);
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		/* search for damage types with same name */
		for (i = 0; i < csi.numDTs; i++)
			if (!Q_strncmp(token, csi.dts[i], MAX_VAR))
				break;

		/* not found in the for loop */
		if (i == csi.numDTs) {
			/* gettext marker */
			if (*token == '_')
				token++;
			Q_strncpyz(csi.dts[csi.numDTs], token, sizeof(csi.dts[csi.numDTs]));

			/* special IDs */
			if (!Q_strncmp(token, "normal", 6))
				csi.damNormal = csi.numDTs;
			else if (!Q_strncmp(token, "blast", 5))
				csi.damBlast = csi.numDTs;
			else if (!Q_strncmp(token, "fire", 4))
				csi.damFire = csi.numDTs;
			else if (!Q_strncmp(token, "shock", 5))
				csi.damShock = csi.numDTs;
			else if (!Q_strncmp(token, "laser", 5))
				csi.damLaser = csi.numDTs;
			else if (!Q_strncmp(token, "plasma", 6))
				csi.damPlasma = csi.numDTs;
			else if (!Q_strncmp(token, "particle", 7))
				csi.damParticle = csi.numDTs;
			else if (!Q_strncmp(token, "stun", 4))
				csi.damStun = csi.numDTs;

			csi.numDTs++;
			if (csi.numDTs >= MAX_DAMAGETYPES)
				Sys_Error("Com_ParseDamageTypes: Too many damage types.\n");
		} else {
			Com_Printf("Com_ParseDamageTypes: damage type list \"%s\" with same already exists - ignore the second one\n", name);
			FS_SkipBlock(text);
			return;
		}
	} while (*text);
}


/*
==============================================================================
MAIN SCRIPT PARSING FUNCTION
==============================================================================
*/

/**
 * @brief Creates links to the technology entries in the pedia and to other items (i.e. ammo<->weapons)
 */
void Com_AddObjectLinks (void)
{
	objDef_t *od = NULL;
	int i, n, m;
	byte j, k;
#ifndef DEDICATED_ONLY
	technology_t *tech = NULL;
#endif

	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {

#ifndef DEDICATED_ONLY
		/* Add links to technologies. */
		tech = RS_GetTechByProvided(od->id);
		od->tech = tech;
		if (!od->tech) {
			Com_Printf("Com_AddObjectLinks: Could not find a valid tech for item %s\n", od->id);
		}
#endif

		/* Add links to weapons. */
		for (j = 0; j < od->numWeapons; j++ ) {
			od->weap_idx[j] = INVSH_GetItemByID(od->weap_id[j]);
			assert(od->weap_idx[j] != -1);
			/* Back-link the obj-idx inside the fds */
			for (k = 0; k < od->numFiredefs[j]; k++ ) {
				od->fd[j][k].obj_idx = i;
			}
		}
	}

	/* Add links to ammos */
	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		od->numAmmos = 0;	/* Default value */
		if (od->weapon && od->numWeapons == 0) {
			for (n = 0; n < csi.numODs; n++) {
				for (m = 0; m < csi.ods[n].numWeapons; m++) {
					if (csi.ods[n].weap_idx[m] == i) {
						assert(od->numAmmos <= MAX_AMMOS_PER_OBJDEF);
						od->ammo_idx[od->numAmmos++] = n;
					}
				}
			}
		}
	}
}

/**
 * @brief
 * @sa CL_ParseClientData
 * @sa CL_ParseScriptFirst
 * @sa CL_ParseScriptSecond
 * @sa Qcommon_Init
 */
void Com_ParseScripts (void)
{
	const char *type, *name, *text;
	ptrdiff_t infoSize = 0;

	/* reset csi basic info */
	Com_InitCSI(&csi);
	csi.idRight = csi.idLeft = csi.idExtension = csi.idBackpack = csi.idBelt = csi.idHolster = csi.idArmor = csi.idFloor = csi.idEquip = csi.idHeadgear = NONE;
	csi.damNormal = csi.damBlast = csi.damFire = csi.damShock = csi.damLaser = csi.damPlasma = csi.damParticle = csi.damStun = NONE;

	/* I guess this is needed, too, if not please remove */
	csi.numODs = 0;
	csi.numIDs = 0;
	csi.numEDs = 0;
	csi.numDTs = 0;

	/* reset name and team def counters */
	numNameCats = 0;
	numTeamDefs = 0;
	infoPos = infoStr;

	/* pre-stage parsing */
	FS_BuildFileList("ufos/*.ufo");
	text = NULL;

	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != 0)
		if (!Q_strncmp(type, "damagetypes", 11))
			Com_ParseDamageTypes(name, &text);
		else if (!Q_strncmp(type, "gametype", 8))
			Com_ParseGameTypes(name, &text);

	/* stage one parsing */
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != 0) {
		/* server/client scripts */
		if (!Q_strncmp(type, "item", 4))
			Com_ParseItem(name, &text);
		else if (!Q_strncmp(type, "inventory", 9))
			Com_ParseInventory(name, &text);
		else if (!Q_strncmp(type, "names", 5))
			Com_ParseNames(name, &text);
		else if (!Q_strncmp(type, "actorsounds", 11))
			Com_ParseActorSounds(name, &text);
		else if (!Q_strncmp(type, "actors", 6))
			Com_ParseActors(name, &text);
		else if (!dedicated->integer)
			CL_ParseClientData(type, name, &text);
	}

	/* Stage two parsing (weapon/inventory dependant stuff). */
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != 0) {
		/* server/client scripts */
		if (!Q_strncmp(type, "equipment", 9))
			Com_ParseEquipment(name, &text);
		else if (!Q_strncmp(type, "teamdesc", 8))
			Com_ParseTeamDesc(name, &text);
		else if (!Q_strncmp(type, "team", 4))
			Com_ParseTeam(name, &text);
	}

	infoSize = infoPos - infoStr;
	if (infoSize > sizeof(infoStr))
		Sys_Error("Com_ParseScripts: infoStr overflow\n");
#ifdef DEBUG
	else
		Com_DPrintf("Free info memory: %ib\n", (int)infoSize);
#endif

	Com_Printf("Shared Client/Server Info loaded\n");
}

/* FIXME: a mess - but i don't want to make the variables non static */
#ifndef DEDICATED_ONLY
#include "../client/client.h"
/**
 * @brief Precache all menu models for faster access
 * @sa CL_PrecacheModels
 */
void Com_PrecacheCharacterModels (void)
{
	nameCategory_t *nc;
	int i, j, num;
	char *str;
	char model[MAX_QPATH];
	const char *path;
	float loading = cls.loadingPercent;

	/* search the name */
	for (i = 0, nc = nameCat; i < numNameCats; i++, nc++)
		for (j = NAME_NEUTRAL; j < NAME_LAST; j++) {
			/* no models for this gender */
			if (!nc->numModels[j])
				continue;
			/* search one of the model definitions */
			str = nc->models[j];
			for (num = 0; num < nc->numModels[j]; num++) {
				path = str; /* model base path */
				str += strlen(str) + 1;
				/* register body */
				Com_sprintf(model, sizeof(model), "%s/%s", path, str);
				if (!re.RegisterModel(model))
					Com_Printf("Com_PrecacheCharacterModels: Could not register model %s\n", model);
#ifdef DEBUG
				_Mem_CheckPoolIntegrity(vid_modelPool, model, __LINE__);
#endif
				str += strlen(str) + 1;
				/* register head */
				Com_sprintf(model, sizeof(model), "%s/%s", path, str);
				if (!re.RegisterModel(model))
					Com_Printf("Com_PrecacheCharacterModels: Could not register model %s\n", model);
#ifdef DEBUG
				_Mem_CheckPoolIntegrity(vid_modelPool, model, __LINE__);
#endif
				/* skip skin */
				str += strlen(str) + 1;
				/* new path */
				str += strlen(str) + 1;
				cls.loadingPercent += 20.0f / (nc->numModels[j] * numNameCats * NAME_LAST);
				SCR_DrawPrecacheScreen(qtrue);
			}
		}
	/* some genders may not have models - ensure that we do a 20 percent step */
	cls.loadingPercent = loading + 20.0f;
}
#endif
