/**
 * @file scripts.c
 * @brief UFO scripts used in client and server
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


#include "common.h"
#include "../game/inv_shared.h"
#ifndef DEDICATED_ONLY
#include "../client/client.h"
#endif

/**
 * @brief possible values for parsing functions
 * @sa valueTypes_t
 */
const char *vt_names[] = {
	"",
	"bool",
	"char",
	"int",
	"int2",
	"float", /* 5 */
	"pos",
	"vector",
	"color",
	"rgba",
	"string", /* 10 */
	"translation_string",
	"translation2_string",
	"longstring",
	"align",
	"blend", /* 15 */
	"style",
	"fade",
	"shapes",
	"shapeb",
	"dmgtype", /* 20 */
	"dmgweight",
	"date",
	"if",
	"relabs",
	"client_hunk", /* 25 */
	"client_hunk_string",
	"num",
	"baseid"
};
CASSERT(lengthof(vt_names) == V_NUM_TYPES);

const char *align_names[] = {
	"ul", "uc", "ur", "cl", "cc", "cr", "ll", "lc", "lr", "ul_rsl", "uc_rsl", "ur_rsl", "cl_rsl", "cc_rsl", "cr_rsl", "ll_rsl", "lc_rsl", "lr_rsl"
};
CASSERT(lengthof(align_names) == ALIGN_LAST);

const char *blend_names[] = {
	"replace", "blend", "add", "filter", "invfilter"
};
CASSERT(lengthof(blend_names) == BLEND_LAST);

const char *menutextid_names[] = {
	"TEXT_STANDARD", "TEXT_LIST", "TEXT_UFOPEDIA", "TEXT_BUILDINGS", "TEXT_BUILDING_INFO",
	"TEXT_RESEARCH", "TEXT_POPUP", "TEXT_POPUP_INFO", "TEXT_AIRCRAFT_LIST",
	"TEXT_AIRCRAFT_INFO", "TEXT_MESSAGESYSTEM", "TEXT_CAMPAIGN_LIST",
	"TEXT_MULTISELECTION", "TEXT_PRODUCTION_LIST", "TEXT_PRODUCTION_AMOUNT", "TEXT_PRODUCTION_INFO",
	"TEXT_EMPLOYEE", "TEXT_MOUSECURSOR_RIGHT", "TEXT_PRODUCTION_QUEUED", "TEXT_STATS_BASESUMMARY",
	"TEXT_STATS_MISSION", "TEXT_STATS_BASES", "TEXT_STATS_NATIONS", "TEXT_STATS_EMPLOYEES", "TEXT_STATS_COSTS",
	"TEXT_STATS_INSTALLATIONS", "TEXT_STATS_7", "TEXT_BASE_LIST", "TEXT_BASE_INFO",
	"TEXT_TRANSFER_LIST", "TEXT_MOUSECURSOR_PLAYERNAMES",
	"TEXT_CARGO_LIST", "TEXT_UFOPEDIA_MAILHEADER", "TEXT_UFOPEDIA_MAIL", "TEXT_MARKET_NAMES",
	"TEXT_MARKET_STORAGE", "TEXT_MARKET_MARKET", "TEXT_MARKET_PRICES", "TEXT_CHAT_WINDOW",
	"TEXT_AIREQUIP_1", "TEXT_AIREQUIP_2", "TEXT_AIREQUIP_3", "TEXT_BASEDEFENCE_LIST", "TEXT_TIPOFTHEDAY",
	"TEXT_GENERIC", "TEXT_XVI"
};
CASSERT(lengthof(menutextid_names) == MAX_MENUTEXTS);

const char *style_names[] = {
	"facing", "rotated", "beam", "line", "axis", "circle"
};
CASSERT(lengthof(style_names) == STYLE_LAST);

const char *fade_names[] = {
	"none", "in", "out", "sin", "saw", "blend"
};
CASSERT(lengthof(fade_names) == FADE_LAST);

#ifndef DEDICATED_ONLY
static const char *if_strings[] = {
	"==",
	"<=",
	">=",
	">",
	"<",
	"!=",
	"",
	"eq",
	"ne"
};
CASSERT(lengthof(if_strings) == IF_SIZE);

/**
 * @brief Translate the condition string to menuIfCondition_t enum value
 * @param[in] conditionString The string from scriptfiles (see if_strings)
 * @return menuIfCondition_t value
 * @return enum value for condition string
 * @note Produces a Sys_Error if conditionString was not found in if_strings array
 */
static int Com_ParseConditionType (const char* conditionString, const char *token)
{
	int i;
	for (i = 0; i < IF_SIZE; i++) {
		if (!Q_strncmp(if_strings[i], conditionString, 2)) {
			return i;
		}
	}
	Sys_Error("Invalid if statement with condition '%s' token: '%s'\n", conditionString, token);
	return -1;
}
#endif

/** @brief target sizes for buffer */
static const size_t vt_sizes[] = {
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
	0,	/* V_TRANSLATION_MANUAL_STRING */
	0,	/* V_LONGSTRING */
	sizeof(byte),	/* V_ALIGN */
	sizeof(byte),	/* V_BLEND */
	sizeof(byte),	/* V_STYLE */
	sizeof(byte),	/* V_FADE */
	sizeof(int),	/* V_SHAPE_SMALL */
	0,	/* V_SHAPE_BIG */
	sizeof(byte),	/* V_DMGTYPE */
	sizeof(byte),	/* V_DMGWEIGHT */
	0,	/* V_DATE */
	0,	/* V_IF */
	sizeof(float),	/* V_RELABS */
	0,	/* V_CLIENT_HUNK */
	0,	/* V_CLIENT_HUNK_STRING */
	sizeof(int),	/* V_MENUTEXTID */
	sizeof(int)		/* V_BASEID */
};
CASSERT(lengthof(vt_sizes) == V_NUM_TYPES);

/**
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
	int x, y, w, h;
	byte num;

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

	case V_MENUTEXTID:
		for (num = 0; num < MAX_MENUTEXTS; num++)
			if (!Q_strcmp(token, menutextid_names[num]))
				break;
		if (num == MAX_MENUTEXTS)
			Sys_Error("Could not find menutext id '%s'", token);
		*(int *) b = num;
		return ALIGN(sizeof(int));

	case V_BASEID:
		*(int *) b = atoi(token);
		if (*(int *) b < 0 || *(int *) b >= MAX_BASES)
			Sys_Error("Invalid baseid given %i", *(int *) b);
		return ALIGN(sizeof(int));

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
	case V_TRANSLATION_MANUAL_STRING:
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
		for (num = 0; num < ALIGN_LAST; num++)
			if (!Q_strcmp(token, align_names[num]))
				break;
		if (num == ALIGN_LAST)
			*b = 0;
		else
			*b = num;
		return ALIGN(1);

	case V_BLEND:
		for (num = 0; num < BLEND_LAST; num++)
			if (!Q_strcmp(token, blend_names[num]))
				break;
		if (num == BLEND_LAST)
			*b = 0;
		else
			*b = num;
		return ALIGN(1);

	case V_STYLE:
		for (num = 0; num < STYLE_LAST; num++)
			if (!Q_strcmp(token, style_names[num]))
				break;
		if (num == STYLE_LAST)
			*b = 0;
		else
			*b = num;
		return ALIGN(1);

	case V_FADE:
		for (num = 0; num < FADE_LAST; num++)
			if (!Q_strcmp(token, fade_names[num]))
				break;
		if (num == FADE_LAST)
			*b = 0;
		else
			*b = num;
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

	case V_DMGWEIGHT:
	case V_DMGTYPE:
		for (num = 0; num < csi.numDTs; num++)
			if (!Q_strcmp(token, csi.dts[num].id))
				break;
		if (num == csi.numDTs)
			*b = 0;
		else
			*b = num;
		return ALIGN(1);

	case V_DATE:
		if (strstr(strstr(token, " "), " ") == NULL)
			Sys_Error("Com_ParseValue: Illegal if statement '%s'\n", token);
		sscanf(token, "%i %i %i", &x, &y, &w);
		((date_t *) b)->day = DAYS_PER_YEAR * x + y;
		((date_t *) b)->sec = SECONDS_PER_HOUR * w;
		return ALIGN(sizeof(date_t));

	case V_IF:
#ifndef DEDICATED_ONLY
		if (!strstr(token, " ")) {
			/* cvar exists? (not null) */
			Mem_PoolStrDupTo(token, (char**) &((menuDepends_t *) b)->var, cl_menuSysPool, CL_TAG_MENU);
			((menuDepends_t *) b)->cond = IF_EXISTS;
		} else if (strstr(strstr(token, " "), " ")) {
			char string[MAX_VAR];
			char string2[MAX_VAR];
			char condition[MAX_VAR];
			sscanf(token, "%s %s %s", string, condition, string2);

			Mem_PoolStrDupTo(string, (char**) &((menuDepends_t *) b)->var, cl_menuSysPool, CL_TAG_MENU);
			Mem_PoolStrDupTo(string2, (char**) &((menuDepends_t *) b)->value, cl_menuSysPool, CL_TAG_MENU);
			((menuDepends_t *) b)->cond = Com_ParseConditionType(condition, token);
		} else
			Sys_Error("Com_ParseValue: Illegal if statement '%s'\n", token);
#endif

		return ALIGN(sizeof(menuDepends_t));

	case V_RELABS:
		if (token[0] == '-' || token[0] == '+') {
			if (fabs(atof(token + 1)) <= 2.0f)
				Com_Printf("Com_ParseValue: a V_RELABS (absolute) value should always be bigger than +/-2.0\n");
			if (token[0] == '-')
				*(float *) b = atof(token + 1) * (-1);
			else
				*(float *) b = atof(token + 1);
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

	case V_MENUTEXTID:
	case V_BASEID:
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

	case V_DMGWEIGHT:
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

	case V_MENUTEXTID:
		assert(*(int *)b < MAX_MENUTEXTS);
		Q_strncpyz(valuestr, menutextid_names[*(int *)b], sizeof(valuestr));
		return valuestr;

	case V_BASEID:
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
	case V_TRANSLATION_MANUAL_STRING:
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

	case V_DMGWEIGHT:
	case V_DMGTYPE:
		assert(*(int *)b < MAX_DAMAGETYPES);
		return csi.dts[*(int *)b].id;

	case V_DATE:
		Com_sprintf(valuestr, sizeof(valuestr), "%i %i %i", ((date_t *) b)->day / DAYS_PER_YEAR, ((date_t *) b)->day % DAYS_PER_YEAR, ((date_t *) b)->sec);
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

/** @brief The order here must be the same as in od_vals */
enum {
	OD_WEAPON,			/**< parse a weapon */
	OD_PROTECTION,		/**< parse armour protection values */
	OD_RATINGS			/**< parse rating values for displaying in the menus */
};

/**
 * @note Make sure, that you don't change the order of the first entries
 * or you have the change the enum values OD_*, too
 */
static const value_t od_vals[] = {
	{"weapon_mod", V_NULL, 0, 0},
	{"protection", V_NULL, 0, 0},
	{"rating", V_NULL, 0, 0},

	{"name", V_TRANSLATION_STRING, offsetof(objDef_t, name), 0},
	{"model", V_STRING, offsetof(objDef_t, model), 0},
	{"image", V_STRING, offsetof(objDef_t, image), 0},
	{"type", V_STRING, offsetof(objDef_t, type), 0},
	{"animationindex", V_CHAR, offsetof(objDef_t, animationIndex), MEMBER_SIZEOF(objDef_t, animationIndex)},
	{"shape", V_SHAPE_SMALL, offsetof(objDef_t, shape), MEMBER_SIZEOF(objDef_t, shape)},
	{"scale", V_FLOAT, offsetof(objDef_t, scale), MEMBER_SIZEOF(objDef_t, scale)},
	{"center", V_VECTOR, offsetof(objDef_t, center), MEMBER_SIZEOF(objDef_t, center)},
	{"weapon", V_BOOL, offsetof(objDef_t, weapon), MEMBER_SIZEOF(objDef_t, weapon)},
	{"holdtwohanded", V_BOOL, offsetof(objDef_t, holdTwoHanded), MEMBER_SIZEOF(objDef_t, holdTwoHanded)},
	{"firetwohanded", V_BOOL, offsetof(objDef_t, fireTwoHanded), MEMBER_SIZEOF(objDef_t, fireTwoHanded)},
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
	{"useable", V_INT, offsetof(objDef_t, useable), MEMBER_SIZEOF(objDef_t, useable)},
	{"notonmarket", V_BOOL, offsetof(objDef_t, notOnMarket), MEMBER_SIZEOF(objDef_t, notOnMarket)},

	{"installationTime", V_INT, offsetof(objDef_t, craftitem.installationTime), MEMBER_SIZEOF(objDef_t, craftitem.installationTime)},
	{"bullets", V_BOOL, offsetof(objDef_t, craftitem.bullets), MEMBER_SIZEOF(objDef_t, craftitem.bullets)},
	{"laser", V_BOOL, offsetof(objDef_t, craftitem.laser), MEMBER_SIZEOF(objDef_t, craftitem.laser)},
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
	{"dmgtype", V_DMGTYPE, offsetof(objDef_t, dmgtype), MEMBER_SIZEOF(objDef_t, dmgtype)},

	{"is_primary", V_BOOL, offsetof(objDef_t, isPrimary), MEMBER_SIZEOF(objDef_t, isPrimary)},
	{"is_secondary", V_BOOL, offsetof(objDef_t, isSecondary), MEMBER_SIZEOF(objDef_t, isSecondary)},
	{"is_heavy", V_BOOL, offsetof(objDef_t, isHeavy), MEMBER_SIZEOF(objDef_t, isHeavy)},
	{"is_misc", V_BOOL, offsetof(objDef_t, isMisc), MEMBER_SIZEOF(objDef_t, isMisc)},
	{"is_ugvitem", V_BOOL, offsetof(objDef_t, isUGVitem), MEMBER_SIZEOF(objDef_t, isUGVitem)},
	{"is_dummy", V_BOOL, offsetof(objDef_t, isDummy), MEMBER_SIZEOF(objDef_t, isDummy)},

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
	{"firevolume", V_FLOAT, offsetof(fireDef_t, relFireVolume), MEMBER_SIZEOF(fireDef_t, relFireVolume)},
	{"impactvolume", V_FLOAT, offsetof(fireDef_t, relImpactVolume), MEMBER_SIZEOF(fireDef_t, relImpactVolume)},
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
	{"dmgweight", V_DMGWEIGHT, offsetof(fireDef_t, dmgweight), MEMBER_SIZEOF(fireDef_t, dmgweight)},
	{"irgoggles", V_BOOL, offsetof(fireDef_t, irgoggles), MEMBER_SIZEOF(fireDef_t, irgoggles)},
	{NULL, 0, 0, 0}
};


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
			if (!Q_strcasecmp(token, fdp->string)) {
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
					if (!Q_strcasecmp(skillNames[skill - ABILITY_NUM_TYPES], token)) {
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
	if (fd->weaponSkill < ABILITY_NUM_TYPES)
		Com_Printf("Com_ParseFire: firedef for weapon \"%s\" doesn't have a skill set\n", name);
}


/**
 * @brief Parses the armour definitions from script files. The protection and rating values
 * @note The rating values are just for menu displaying
 * @sa Com_ParseItem
 */
static void Com_ParseArmour (const char *name, const char **text, short *ad, qboolean rating)
{
	const char *errhead = "Com_ParseArmour: unexpected end of file";
	const char *token;
	int i;

	/* get its body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseArmour: armour definition \"%s\" without body ignored\n", name);
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			return;
		if (*token == '}')
			return;

		for (i = 0; i < csi.numDTs; i++)
			if (!Q_strncmp(token, csi.dts[i].id, MAX_VAR)) {
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				if (rating && !csi.dts[i].showInMenu)
					Sys_Error("Com_ParseArmour: You try to set a rating value for a none menu displayed damage type '%s'", csi.dts[i].id);
				/* protection or rating values */
				ad[i] = atoi(token);
				break;
			}

		if (i >= csi.numDTs)
			Com_Printf("Com_ParseArmour: unknown damage type \"%s\" ignored (in %s)\n", token, name);
	} while (*text);
}


/**
 * @brief List of valid strings for slot types
 * @note slot names are the same as the item types (and must be in the same order)
 */
const char *air_slot_type_strings[] = {
	"base_missile",
	"base_laser",
	"weapon",
	"shield",
	"electronics",
	"pilot",
	"ammo",
	"base_ammo_missile",
	"base_ammo_laser"
};
CASSERT(lengthof(air_slot_type_strings) == MAX_ACITEMS);


/**
 * @brief Temporary list of weapon ids as parsed from the ufo file "weapon_mod <id>"
 * in Com_ParseItem and used in Com_AddObjectLinks.
 */
static linkedList_t *parseItemWeapons = NULL;

/**
 * @brief Parses weapon, equipment, craft items and armour
 * @sa Com_ParseArmour
 */
static void Com_ParseItem (const char *name, const char **text, qboolean craftitem)
{
	const char *errhead = "Com_ParseItem: unexpected end of file (weapon ";
	const value_t *val;
	objDef_t *od;
	const char *token;
	int i;
	int weapFdsIdx, fdIdx;

	/* search for items with same name */
	for (i = 0; i < csi.numODs; i++)
		if (!Q_strncmp(name, csi.ods[i].name, sizeof(csi.ods[i].name)))
			break;

	if (i < csi.numODs) {
		Com_Printf("Com_ParseItem: weapon def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (i >= MAX_OBJDEFS)
		Sys_Error("Com_ParseItem: MAX_OBJDEFS exceeded\n");

	Com_DPrintf(DEBUG_SHARED, "...found item: '%s' (%i)\n", name, csi.numODs);

	/* initialize the object definition */
	od = &csi.ods[csi.numODs++];
	memset(od, 0, sizeof(*od));

	od->craftitem.type = MAX_ACITEMS; /**< default is no craftitem */

	Q_strncpyz(od->id, name, sizeof(od->id));

	od->idx = csi.numODs - 1;

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
			if (!Q_strcasecmp(token, val->string)) {
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
						if (od->numWeapons < MAX_WEAPONS_PER_OBJDEF) {
							/** Store the current item-pointer and the weapon id for later linking of the "weapon" pointers */
							LIST_AddPointer(&parseItemWeapons, od);
							LIST_Add(&parseItemWeapons, (byte *)&od->numWeapons, sizeof(int));
							LIST_AddString(&parseItemWeapons, token);

							/* get it's body */
							token = COM_Parse(text);

							if (!*text || *token != '{') {
								Com_Printf("Com_ParseItem: weapon_mod \"%s\" without body ignored\n", name);
								break;
							}

							weapFdsIdx = od->numWeapons;
							/* For parse each firedef entry for this weapon.  */
							do {
								token = COM_EParse(text, errhead, name);
								if (!*text)
									return;
								if (*token == '}')
									break;

								if (!Q_strncmp(token, "firedef", MAX_VAR)) {
									if (od->numFiredefs[weapFdsIdx] < MAX_FIREDEFS_PER_WEAPON) {
										fdIdx = od->numFiredefs[weapFdsIdx];
										od->fd[weapFdsIdx][fdIdx].relFireVolume = DEFAULT_SOUND_PACKET_VOLUME;
										od->fd[weapFdsIdx][fdIdx].relImpactVolume = DEFAULT_SOUND_PACKET_VOLUME;
										/* Parse firemode into fd[IDXweapon][IDXfiremode] */
										Com_ParseFire(name, text, &od->fd[weapFdsIdx][fdIdx]);
										/* Self-link fd */
										od->fd[weapFdsIdx][fdIdx].fdIdx = fdIdx;
										/* Self-link weapon_mod */
										od->fd[weapFdsIdx][fdIdx].weapFdsIdx = weapFdsIdx;
										od->numFiredefs[od->numWeapons]++;
									} else {
										Com_Printf("Com_ParseItem: Too many firedefs at \"%s\". Max is %i\n", name, MAX_FIREDEFS_PER_WEAPON);
									}
								}
							} while (*text);
							od->numWeapons++;
						} else {
							Com_Printf("Com_ParseItem: Too many weapon_mod definitions at \"%s\". Max is %i\n", name, MAX_WEAPONS_PER_OBJDEF);
						}
						break;
					case OD_PROTECTION:
						Com_ParseArmour(name, text, od->protection, qfalse);
						break;
					case OD_RATINGS:
						Com_ParseArmour(name, text, od->ratings, qtrue);
						break;
					default:
						break;
					}
				}
				break;
			}
		}
		if (!val->string) {
			if (!Q_strcmp(token, "craftweapon")) {
				/* parse a value */
				token = COM_EParse(text, errhead, name);
				if (od->numWeapons < MAX_WEAPONS_PER_OBJDEF) {
					/** Store the current item-pointer and the weapon id for later linking of the "weapon" pointers */
					LIST_AddPointer(&parseItemWeapons, od);
					LIST_Add(&parseItemWeapons, (byte *)&od->numWeapons, sizeof(int));
					LIST_AddString(&parseItemWeapons, token);
					od->numWeapons++;
				} else {
					Com_Printf("Com_ParseItem: Too many weapon_mod definitions at \"%s\". Max is %i\n", name, MAX_WEAPONS_PER_OBJDEF);
				}
			} else if (!Q_strcmp(token, "crafttype")) {
				/* Craftitem type definition. */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				/* Check which type it is and store the correct one.*/
				for (i = 0; i < MAX_ACITEMS; i++) {
					if (!Q_strcmp(token, air_slot_type_strings[i])) {
						od->craftitem.type = i;
						break;
					}
				}
				if (i == MAX_ACITEMS)
					Com_Printf("AII_ParseAircraftItem: \"%s\" unknown craftitem type: \"%s\" - ignored.\n", name, token);
			} else
				Com_Printf("Com_ParseItem: unknown token \"%s\" ignored (weapon %s)\n", token, name);
		}

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
	/* Scrollable container */
	{"scroll", V_INT, offsetof(invDef_t, scroll), MEMBER_SIZEOF(invDef_t, scroll)},
	{"scroll_height", V_INT, offsetof(invDef_t, scrollHeight), MEMBER_SIZEOF(invDef_t, scrollHeight)},
	{"scroll_vertical", V_BOOL, offsetof(invDef_t, scrollVertical), MEMBER_SIZEOF(invDef_t, scrollVertical)},
	/* only a single item as weapon extension - single should be set, too */
	{"extension", V_BOOL, offsetof(invDef_t, extension), MEMBER_SIZEOF(invDef_t, extension)},
	/* this is the armour container */
	{"armour", V_BOOL, offsetof(invDef_t, armour), MEMBER_SIZEOF(invDef_t, armour)},
	/* this is the headgear container */
	{"headgear", V_BOOL, offsetof(invDef_t, headgear), MEMBER_SIZEOF(invDef_t, headgear)},
	/* allow everything to be stored in this container (e.g armour and weapons) */
	{"all", V_BOOL, offsetof(invDef_t, all), MEMBER_SIZEOF(invDef_t, all)},
	{"temp", V_BOOL, offsetof(invDef_t, temp), MEMBER_SIZEOF(invDef_t, temp)},
	/* time units for moving something in */
	{"in", V_INT, offsetof(invDef_t, in), MEMBER_SIZEOF(invDef_t, in)},
	/* time units for moving something out */
	{"out", V_INT, offsetof(invDef_t, out), MEMBER_SIZEOF(invDef_t, out)},

	{NULL, 0, 0, 0}
};

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
	memset(id, 0, sizeof(*id));

	Q_strncpyz(id->name, name, sizeof(id->name));

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseInventory: inventory def \"%s\" without body ignored\n", name);
		csi.numIDs--;
		return;
	}

	/* Special IDs for container. These are also used elsewhere, so be careful. */
	if (!Q_strncmp(name, "right", 5)) {
		csi.idRight = id - csi.ids;
		id->id = csi.idRight;
	} else if (!Q_strncmp(name, "left", 4)) {
		csi.idLeft = id - csi.ids;
		id->id = csi.idLeft;
	} else if (!Q_strncmp(name, "extension", 4)) {
		csi.idExtension = id - csi.ids;
		id->id = csi.idExtension;
	} else if (!Q_strncmp(name, "belt", 4)) {
		csi.idBelt = id - csi.ids;
		id->id = csi.idBelt;
	} else if (!Q_strncmp(name, "holster", 7)) {
		csi.idHolster = id - csi.ids;
		id->id = csi.idHolster;
	} else if (!Q_strncmp(name, "backpack", 8)) {
		csi.idBackpack = id - csi.ids;
		id->id = csi.idBackpack;
	} else if (!Q_strncmp(name, "armour", 5)) {
		csi.idArmour = id - csi.ids;
		id->id = csi.idArmour;
	} else if (!Q_strncmp(name, "floor", 5)) {
		csi.idFloor = id - csi.ids;
		id->id = csi.idFloor;
	} else if (!Q_strncmp(name, "equip", 5)) {
		csi.idEquip = id - csi.ids;
		id->id = csi.idEquip;
	} else if (!Q_strncmp(name, "headgear", 8)) {
		csi.idHeadgear = id - csi.ids;
		id->id = csi.idHeadgear;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			return;
		if (*token == '}')
			return;

		for (idp = idps; idp->string; idp++)
			if (!Q_strcasecmp(token, idp->string)) {
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

typedef enum model_script_s {
	MODEL_PATH,
	MODEL_BODY,
	MODEL_HEAD,
	MODEL_SKIN,

	MODEL_NUM_TYPES
} model_script_t;

const char *name_strings[NAME_NUM_TYPES] = {
	"neutral",
	"female",
	"male",
	"lastname",
	"female_lastname",
	"male_lastname"
};

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
	memset(ed, 0, sizeof(*ed));

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
 * @param[in] gender 1 (female) or 2 (male)
 * @param[in] team Country strings like: spanish_italian, german, russian and so on
 * @sa Com_GetCharacterValues
 */
const char *Com_GiveName (int gender, const char *team)
{
	static char returnName[MAX_VAR];
	teamDef_t *td;
	int i, j, name = 0;
	linkedList_t* list;

	/* search the name */
	for (i = 0, td = csi.teamDef; i < csi.numTeamDefs; i++, td++)
		if (!Q_strncmp(team, td->id, MAX_VAR)) {
#ifdef DEBUG
			for (j = 0; j < NAME_NUM_TYPES; j++)
				name += td->numNames[j];
			if (!name)
				Sys_Error("Could not find any valid name definitions for category '%s'\n", team);
#endif
			/* found category */
			if (!td->numNames[gender]) {
#ifdef DEBUG
				Com_DPrintf(DEBUG_ENGINE, "No valid name definitions for gender %i in category '%s'\n", gender, team);
#endif
				return NULL;
			}
			name = rand() % td->numNames[gender];

			/* skip names */
			list = td->names[gender];
			for (j = 0; j < name; j++) {
				assert(list);
				list = list->next;
			}

			/* store the name */
			Q_strncpyz(returnName, (const char*)list->data, sizeof(returnName));
			return returnName;
		}

	/* nothing found */
	return NULL;
}

/**
 * @param[in] type MODEL_PATH, MODEL_BODY, MODEL_HEAD, MODEL_SKIN (path, body, head, skin - see team_*.ufo)
 * @param[in] gender 1 (female) or 2 (male)
 * @param[in] teamID The team definition id
 * @sa Com_GetCharacterValues
 */
const char *Com_GiveModel (int type, int gender, const char *teamID)
{
	teamDef_t *td;
	int i, j, num;
	linkedList_t* list;

	/* search the name */
	for (i = 0, td = csi.teamDef; i < csi.numTeamDefs; i++, td++)
		if (!Q_strncmp(teamID, td->id, MAX_VAR)) {
			/* found category */
			if (!td->numModels[gender]) {
				Com_Printf("Com_GiveModel: no models defined for gender %i and category '%s'\n", gender, teamID);
				return NULL;
			}
			/* search one of the model definitions */
			num = (rand() % td->numModels[gender]) * MODEL_NUM_TYPES;
			/* now go to the type entry from team_*.ufo */
			num += type;

			/* skip models and unwanted info */
			list = td->models[gender];
			for (j = 0; j < num; j++) {
				assert(list);
				list = list->next;
			}

			/* return the value */
			return (const char*)list->data;
		}

	Com_Printf("Com_GiveModel: no models for gender %i and category '%s'\n", gender, teamID);
	/* nothing found */
	return NULL;
}

/**
 * @brief Returns the actor sounds for a given category
 * @param[in] td teamDef pointer
 * @param[in] gender The gender of the actor
 * @param[in] soundType Which sound category (actorSound_t)
 */
const char* Com_GetActorSound (teamDef_t* td, int gender, actorSound_t soundType)
{
	int random, j;
	linkedList_t* list;

	if (!td)
		return NULL;

	if (gender < 0 || gender >= 3) {
		Com_DPrintf(DEBUG_SOUND|DEBUG_CLIENT, "Com_GetActorSound: invalid gender: %i\n", gender);
		return NULL;
	}
	if (td->numSounds[soundType][gender] <= 0) {
		Com_DPrintf(DEBUG_SOUND|DEBUG_CLIENT, "Com_GetActorSound: no sound defined for soundtype: %i, teamID: '%s', gender: %i\n", soundType, td->id, gender);
		return NULL;
	}

	random = rand() % td->numSounds[soundType][gender];
	list = td->sounds[soundType][gender];
	for (j = 0; j < random; j++) {
		assert(list);
		list = list->next;
	}

	assert(list);
	assert(list->data);
	return (const char*)list->data;
}

/**
 * @brief Returns the teamDef pointer for the searched team id - or NULL if not
 * found in the teamDef array
 * @param[in] team The team id (given in ufo-script files)
 */
teamDef_t* Com_GetTeamDefinitionByID (const char *team)
{
	int i;

	/* get team definition */
	for (i = 0; i < csi.numTeamDefs; i++)
		if (!Q_strncmp(team, csi.teamDef[i].id, MAX_VAR))
			return &csi.teamDef[i];

	Com_Printf("Com_GetTeamDefinitionByID: could not find team definition for '%s' in team definitions\n", team);
	return NULL;
}

/**
 * @brief Assign character values, 3D models and names to a character.
 * @param[in] team What team the character is on.
 * @param[in,out] chr The character that should get the paths to the different models/skins.
 * @sa Com_GiveName
 * @sa Com_GiveModel
 */
int Com_GetCharacterValues (const char *team, character_t * chr)
{
	teamDef_t *td;
	const char *str;
	int i, gender;
	int retry = 1000;

	assert(chr);

	/* get team definition */
	for (i = 0; i < csi.numTeamDefs; i++)
		if (!Q_strncmp(team, csi.teamDef[i].id, MAX_VAR))
			break;

	/* If no team was found this should be NULL for error checking. */
	chr->teamDef = NULL;

	if (i < csi.numTeamDefs)
		td = &csi.teamDef[i];
	else {
		Com_Printf("Com_GetCharacterValues: could not find team '%s' in team definitions\n", team);
		return 0;
	}

	/* default values for human characters */
	switch (td->size) {
	case 2:	/* 2x2 unit*/
		chr->fieldSize = ACTOR_SIZE_2x2;
		break;
	case 1:	/* 1x1 unit*/
	default:	/* Default value is 1x1 */
		chr->fieldSize = ACTOR_SIZE_NORMAL;
		break;
	}
	chr->weapons = td->weapons;
	chr->armour = td->armour;
	chr->teamDef = &csi.teamDef[i];

	/* get the models */
	while (retry--) {
		gender = rand() % NAME_LAST;
		chr->gender = gender;

		/* get name */
		str = Com_GiveName(gender, td->id);
		if (!str)
			continue;
		Q_strncpyz(chr->name, str, sizeof(chr->name));
		Q_strcat(chr->name, " ", sizeof(chr->name));
		str = Com_GiveName(gender + LASTNAME, td->id);
		if (!str)
			continue;
		Q_strcat(chr->name, str, sizeof(chr->name));

		/* get model */
		str = Com_GiveModel(MODEL_PATH, gender, td->id);
		if (!str)
			continue;
		Q_strncpyz(chr->path, str, sizeof(chr->path));

		str = Com_GiveModel(MODEL_BODY, gender, td->id);
		if (!str)
			continue;
		Q_strncpyz(chr->body, str, sizeof(chr->body));

		str = Com_GiveModel(MODEL_HEAD, gender, td->id);
		if (!str)
			continue;
		Q_strncpyz(chr->head, str, sizeof(chr->head));

		str = Com_GiveModel(MODEL_SKIN, gender, td->id);
		if (!str)
			continue;
		return atoi(str);
	}
	Com_Printf("Could not set character values for team '%s' - team '%s'\n", team, td->id);
	return 0;
}

/**
 * @brief Parses "name" definition from team_* ufo script files
 * @sa Com_ParseActors
 * @sa Com_ParseScripts
 */
static void Com_ParseActorNames (const char *name, const char **text, teamDef_t* td)
{
	const char *errhead = "Com_ParseNames: unexpected end of file (names ";
	const char *token;
	int i;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseActorNames: names def \"%s\" without body ignored\n", name);
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
				td->numNames[i] = 0;

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
					LIST_AddString(&td->names[i], token);
					td->numNames[i]++;
				} while (*text);

				/* lastname is different */
				/* fill female and male lastnames from neutral lastnames */
				if (i == NAME_LAST)
					for (i = NAME_NUM_TYPES - 1; i > NAME_LAST; i--) {
						td->names[i] = td->names[NAME_LAST];
						td->numNames[i] = td->numNames[NAME_LAST];
					}
				break;
			}

		if (i == NAME_NUM_TYPES)
			Com_Printf("Com_ParseNames: unknown token \"%s\" ignored (names %s)\n", token, name);

	} while (*text);

	if (td->numNames[NAME_FEMALE] && !td->numNames[NAME_FEMALE_LAST])
		Sys_Error("Com_ParseNames: '%s' has no female lastname category\n", td->id);
	if (td->numNames[NAME_MALE] && !td->numNames[NAME_MALE_LAST])
		Sys_Error("Com_ParseNames: '%s' has no male lastname category\n", td->id);
	if (td->numNames[NAME_NEUTRAL] && !td->numNames[NAME_LAST])
		Sys_Error("Com_ParseNames: '%s' has no neutral lastname category\n", td->id);
}

/**
 * @brief Parses "actors" definition from team_* ufo script files
 * @sa Com_ParseNames
 * @sa Com_ParseScripts
 */
static void Com_ParseActorModels (const char *name, const char **text, teamDef_t* td)
{
	const char *errhead = "Com_ParseActorModels: unexpected end of file (actors ";
	const char *token;
	int i, j;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseActorModels: actor def \"%s\" without body ignored\n", td->id);
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
				if (td->numModels[i])
					Sys_Error("Com_ParseActorModels: Already parsed models for actor definition '%s'\n", name);
				td->numModels[i] = 0;
				token = COM_EParse(text, errhead, name);
				if (!*text)
					break;
				if (*token != '{') {
					Com_Printf("Com_ParseActorModels: Empty model definition '%s' for gender '%s'\n", name, name_strings[i]);
					break;
				}

				do {
					/* get the path, body, head and skin */
					for (j = 0; j < 4; j++) {
						token = COM_EParse(text, errhead, name);
						if (!*text) {
							Com_Printf("Com_ParseActors: Premature end of script at j=%i\n", j);
							break;
						}
						if (*token == '}')
							break;

						if (j == 3 && *token == '*')
							LIST_AddString(&td->models[i], "");
						else
							LIST_AddString(&td->models[i], token);
					}
					/* first token was } */
					if (j == 0)
						break;

					/* only add complete actor info */
					if (j == 4)
						td->numModels[i]++;
					else {
						Com_Printf("Com_ParseActors: Incomplete actor data: '%s' - j: %i\n", td->id, j);
						break;
					}
				} while (*text);
				if (!td->numModels[i])
					Com_Printf("Com_ParseActors: actor definition '%s' with no models (gender: %s)\n", name, name_strings[i]);
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
static void Com_ParseActorSounds (const char *name, const char **text, teamDef_t* td)
{
	const char *errhead = "Com_ParseActorSounds: unexpected end of file (actorsounds ";
	const char *token;
	int i;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseActorSounds: actorsounds def \"%s\" without body ignored\n", name);
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
						LIST_AddString(&td->sounds[SND_HURT][i], token);
						td->numSounds[SND_HURT][i]++;
					} else if (!Q_strcmp(token, "deathsound")) {
						token = COM_EParse(text, errhead, name);
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

/** @brief possible teamdesc values (ufo-scriptfiles) */
static const value_t teamDefValues[] = {
	{"tech", V_STRING, offsetof(teamDef_t, tech), 0}, /**< tech id from research.ufo */
	{"name", V_TRANSLATION_MANUAL_STRING, offsetof(teamDef_t, name), 0}, /**< internal team name */
	{"alien", V_BOOL, offsetof(teamDef_t, alien), MEMBER_SIZEOF(teamDef_t, alien)}, /**< is this an alien? */
	{"armour", V_BOOL, offsetof(teamDef_t, armour), MEMBER_SIZEOF(teamDef_t, armour)}, /**< are these team members able to wear armour? */
	{"weapons", V_BOOL, offsetof(teamDef_t, weapons), MEMBER_SIZEOF(teamDef_t, weapons)}, /**< are these team members able to use weapons? */
	{"civilian", V_BOOL, offsetof(teamDef_t, civilian), MEMBER_SIZEOF(teamDef_t, civilian)}, /**< is this a civilian? */
	{"size", V_INT, offsetof(teamDef_t, size), MEMBER_SIZEOF(teamDef_t, size)}, /**< What size is this unit on the field (1=1x1 or 2=2x2)? */
	{"hit_particle", V_STRING, offsetof(teamDef_t, hitParticle), 0}, /**< What particle effect should be spawned if a unit of this type is hit? */
	{"robot", V_BOOL, offsetof(teamDef_t, robot), MEMBER_SIZEOF(teamDef_t, robot)}, /**< Is this a robotic unit? */
	{NULL, 0, 0, 0}
};

static void Com_ParseTeam (const char *name, const char **text)
{
	teamDef_t *td;
	const char *errhead = "Com_ParseTeam: unexpected end of file (team ";
	const char *token;
	int i;
	const value_t *v;

	/* check for additions to existing name categories */
	for (i = 0, td = csi.teamDef; i < csi.numTeamDefs; i++, td++)
		if (!Q_strncmp(td->id, name, sizeof(td->id)))
			break;

	/* reset new category */
	if (i == csi.numTeamDefs) {
		if (csi.numTeamDefs < MAX_TEAMDEFS) {
			memset(td, 0, sizeof(*td));
			/* index backlink */
			td->idx = csi.numTeamDefs;
			csi.numTeamDefs++;
		} else {
			Com_Printf("CL_ParseTeam: Too many team definitions, '%s' ignored.\n", name);
			return;
		}
	} else {
		Com_Printf("CL_ParseTeam: Team with same name found, second ignored '%s'\n", name);
		FS_SkipBlock(text);
		return;
	}

	Q_strncpyz(td->id, name, sizeof(td->id));
	td->armour = td->weapons = qtrue; /* default values */
	td->onlyWeapon = NULL;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseTeam: team def \"%s\" without body ignored\n", name);
		if (csi.numTeamDefs - 1 == td - csi.teamDef)
			csi.numTeamDefs--;
		return;
	}

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (v = teamDefValues; v->string; v++)
			if (!Q_strncmp(token, v->string, strlen(token))) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				Com_ParseValue(td, token, v->type, v->ofs, v->size);
				break;
			}

		if (!v->string) {
			if (!Q_strcmp(token, "onlyWeapon")) {
				objDef_t *od;
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				od = INVSH_GetItemByID(token);

				if (od)
					td->onlyWeapon = od;
				else
					Sys_Error("Com_ParseTeam: Could not get item definition for '%s'", token);
			} else if (!Q_strcmp(token, "models"))
				Com_ParseActorModels(name, text, td);
			else if (!Q_strcmp(token, "names"))
				Com_ParseActorNames(name, text, td);
			else if (!Q_strcmp(token, "actorsounds"))
				Com_ParseActorSounds(name, text, td);
			else
				Com_Printf("Com_ParseTeam: unknown token \"%s\" ignored (team %s)\n", token, name);
		}
	} while (*text);
}

/*
==============================================================================
TERRAIN PARSERS
==============================================================================
*/

#define TERRAIN_HASH_SIZE 64
static terrainType_t *terrainTypesHash[TERRAIN_HASH_SIZE];

static const value_t terrainTypeValues[] = {
	{"footstepsound", V_STRING, offsetof(terrainType_t, footStepSound), 0},
	{"particle", V_STRING, offsetof(terrainType_t, particle), 0},
	{"footstepvolume", V_FLOAT, offsetof(terrainType_t, footStepVolume), 0},
	{"bouncefraction", V_FLOAT, offsetof(terrainType_t, bounceFraction), 0},

	{NULL, 0, 0, 0}
};

/**
 * @brief Searches the terrain defintion if given
 * @param[in] textureName The terrain definition id from script files
 * which is the texture name relative to base/textures
 */
const terrainType_t* Com_GetTerrainType (const char *textureName)
{
	unsigned hash;
	const terrainType_t *t;

	assert(textureName);
	hash = Com_HashKey(textureName, TERRAIN_HASH_SIZE);
	for (t = terrainTypesHash[hash]; t; t = t->hash_next) {
		if (!Q_strcmp(textureName, t->texture))
			return t;
	}

	return NULL;
}

/**
 * @brief Parses "terrain" definition from script files
 * @note Terrain definitions are used for footstep sounds and terrain particles
 * @sa Com_ParseScripts
 */
static void Com_ParseTerrain (const char *name, const char **text)
{
	const char *errhead = "Com_ParseTerrain: unexpected end of file (terrain ";
	const char *token;
	terrainType_t *t;
	const value_t *v;
	unsigned hash;

	/* check for additions to existing name categories */
	if (Com_GetTerrainType(name) != NULL) {
		Com_Printf("Terrain definition with same name already parsed: '%s'\n", name);
		return;
	}

	/* get name list body body */
	token = COM_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("Com_ParseTerrain: terrain def \"%s\" without body ignored\n", name);
		return;
	}

	t = Mem_PoolAlloc(sizeof(*t), com_genericPool, 0);
	t->texture = Mem_PoolStrDup(name, com_genericPool, 0);
	hash = Com_HashKey(name, TERRAIN_HASH_SIZE);
	/* link in terrainTypesHash[hash] should be NULL on the first run */
	t->hash_next = terrainTypesHash[hash];
	terrainTypesHash[hash] = t;
	t->footStepVolume = DEFAULT_SOUND_PACKET_VOLUME;

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (v = terrainTypeValues; v->string; v++)
			if (!Q_strncmp(token, v->string, sizeof(v->string))) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				switch (v->type) {
				case V_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)t + (int)v->ofs), com_genericPool, 0);
					break;
				default:
					Com_ParseValue(t, token, v->type, v->ofs, v->size);
					break;
				}
				break;
			}
		if (!v->string)
			Com_Printf("Unknown token '%s' in terrain parsing\n", token);
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
	{"name", V_TRANSLATION_MANUAL_STRING, offsetof(gametype_t, name), 0}, /**< translated game-type name for menu displaying */
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
		if (i >= MAX_GAMETYPES)
			Sys_Error("Com_ParseGameTypes: MAX_GAMETYPES exceeded\n");
		gt = &gts[numGTs++];
		memset(gt, 0, sizeof(*gt));
		Q_strncpyz(gt->id, name, sizeof(gt->id));
		if (numGTs >= MAX_GAMETYPES)
			Sys_Error("Com_ParseGameTypes: Too many gametypes.\n");

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

		/* Gettext marker (also indicates that it is a dmgtype value - additional to beeing a dmgweight value) */
		if (*token == '_') {
			token++;
			csi.dts[csi.numDTs].showInMenu = qtrue;
		}

		/* search for damage types with same name */
		for (i = 0; i < csi.numDTs; i++)
			if (!Q_strncmp(token, csi.dts[i].id, MAX_VAR))
				break;

		/* Not found in the for loop. */
		if (i == csi.numDTs) {
			Q_strncpyz(csi.dts[csi.numDTs].id, token, sizeof(csi.dts[csi.numDTs].id));

			/* Check dmgtype IDs and store their IDs in csi. */
			if (csi.dts[csi.numDTs].showInMenu) {
				Com_DPrintf(DEBUG_CLIENT, "Com_ParseDamageTypes: dmgtype/dmgweight %s\n", token);
				/* Special IDs */
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
				else if (!Q_strncmp(token, "stun_electro", 12))
					csi.damStunElectro = csi.numDTs;
				else if (!Q_strncmp(token, "stun_gas", 8))
					csi.damStunGas = csi.numDTs;
				else
					Com_Printf("Unknown dmgtype: '%s'\n", token);
			} else {
				Com_DPrintf(DEBUG_CLIENT, "Com_ParseDamageTypes: dmyweight %s\n", token);
			}

			csi.numDTs++;
			if (csi.numDTs >= MAX_DAMAGETYPES)
				Sys_Error("Com_ParseDamageTypes: Too many damage types.\n");
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
 * @brief Creates links to the technology entries in the pedia and to other items (i.e. ammo<->weapons)
 */
void Com_AddObjectLinks (void)
{
	linkedList_t* ll = parseItemWeapons;	/**< Use this so we do not change the original popupListData pointer. */
	objDef_t *od;
	int i, n, m;
	byte k, weaponsIdx;
	char *id;
#ifndef DEDICATED_ONLY
	technology_t *tech;
#endif

#ifndef DEDICATED_ONLY
	/* Add links to technologies. */
	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		tech = RS_GetTechByProvided(od->id);
		od->tech = tech;
		if (!od->tech) {
			Com_Printf("Com_AddObjectLinks: Could not find a valid tech for item %s\n", od->id);
		}
	}
	#endif

	/* Add links to weapons. */
	while (ll) {
		/* Get the data stored in the linked list. */
		assert(ll);
		od = (objDef_t *) ll->data;
		ll = ll->next;

		assert(ll);
		weaponsIdx = *(int*)ll->data;
		ll = ll->next;

		assert(ll);
		id = (char*)ll->data;
		ll = ll->next;

		/* Link the weapon pointers for this item. */
		od->weapons[weaponsIdx] = INVSH_GetItemByID(id);
		if (!od->weapons[weaponsIdx]) {
			Sys_Error("Com_AddObjectLinks: Could not get item '%s' for linking into item '%s'\n",
				id , od->id);
		}

		/* Back-link the obj-idx inside the fds */
		for (k = 0; k < od->numFiredefs[weaponsIdx]; k++) {
			od->fd[weaponsIdx][k].obj = od;
		}
	}

	/* Clear the temporary list. */
	LIST_Delete(&parseItemWeapons);

	/* Add links to ammos */
	for (i = 0, od = csi.ods; i < csi.numODs; i++, od++) {
		od->numAmmos = 0;	/* Default value */
		if (od->numWeapons == 0 && (od->weapon || od->craftitem.type <= AC_ITEM_WEAPON)) {
			/* this is a weapon, an aircraft weapon, or a base defence system */
			for (n = 0; n < csi.numODs; n++) {
				for (m = 0; m < csi.ods[n].numWeapons; m++) {
					if (csi.ods[n].weapons[m] == &csi.ods[i]) {
						assert(od->numAmmos <= MAX_AMMOS_PER_OBJDEF);
						od->ammos[od->numAmmos++] = &csi.ods[n];
					}
				}
			}
		}
	}
}

mapDef_t* Com_GetMapDefinitionByID (const char *mapDefID)
{
	int i;

	assert(mapDefID);

	for (i = 0; i < csi.numMDs; i++) {
		if (!Q_strcmp(csi.mds[i].id, mapDefID))
			return &csi.mds[i];
	}

	Com_DPrintf(DEBUG_CLIENT, "Com_GetMapDefinition: Could not find mapdef with id: '%s'\n", mapDefID);
	return NULL;
}

/** @brief valid mapdef descriptors */
static const value_t mapdef_vals[] = {
	{"description", V_TRANSLATION_MANUAL_STRING, offsetof(mapDef_t, description), 0},
	{"map", V_CLIENT_HUNK_STRING, offsetof(mapDef_t, map), 0},
	{"param", V_CLIENT_HUNK_STRING, offsetof(mapDef_t, param), 0},
	{"size", V_CLIENT_HUNK_STRING, offsetof(mapDef_t, size), 0},

	{"maxaliens", V_INT, offsetof(mapDef_t, maxAliens), MEMBER_SIZEOF(mapDef_t, maxAliens)},
	{"storyrelated", V_BOOL, offsetof(mapDef_t, storyRelated), MEMBER_SIZEOF(mapDef_t, storyRelated)},

	{"teams", V_INT, offsetof(mapDef_t, teams), MEMBER_SIZEOF(mapDef_t, teams)},
	{"multiplayer", V_BOOL, offsetof(mapDef_t, multiplayer), MEMBER_SIZEOF(mapDef_t, multiplayer)},

	{NULL, 0, 0, 0}
};

static void Com_ParseMapDefinition (const char *name, const char **text)
{
	const char *errhead = "Com_ParseMapDefinition: unexpected end of file (mapdef ";
	mapDef_t *md;
	const value_t *vp;
	const char *token;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("Com_ParseMapDefinition: mapdef \"%s\" without body ignored\n", name);
		return;
	}

	md = &csi.mds[csi.numMDs++];
	memset(md, 0, sizeof(*md));
	md->id = Mem_PoolStrDup(name, com_genericPool, 0);

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (vp = mapdef_vals; vp->string; vp++)
			if (!Q_strcmp(token, vp->string)) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (vp->type) {
				default:
					Com_ParseValue(md, token, vp->type, vp->ofs, vp->size);
					break;
				case V_TRANSLATION_MANUAL_STRING:
					if (*token == '_')
						token++;
				/* fall through */
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)md + (int)vp->ofs), com_genericPool, 0);
					break;
				}
				break;
			}

		if (!vp->string) {
			linkedList_t **list;
			if (!Q_strcmp(token, "ufos")) {
				list = &md->ufos;
			} else if (!Q_strcmp(token, "terrains")) {
				list = &md->terrains;
			} else if (!Q_strcmp(token, "populations")) {
				list = &md->populations;
			} else if (!Q_strcmp(token, "cultures")) {
				list = &md->cultures;
			} else if (!Q_strcmp(token, "gametypes")) {
				list = &md->gameTypes;
			} else {
				Com_Printf("Com_ParseMapDefinition: unknown token \"%s\" ignored (mapdef %s)\n", token, name);
				continue;
			}
			token = COM_EParse(text, errhead, name);
			if (!*text || *token != '{') {
				Com_Printf("Com_ParseMapDefinition: mapdef \"%s\" has ufos, gametypes, terrains, populations or cultures block with no opening brace\n", name);
				break;
			}
			do {
				token = COM_EParse(text, errhead, name);
				if (!*text || *token == '}')
					break;
				LIST_AddString(list, token);
			} while (*text);
		}
	} while (*text);

	if (!md->map) {
		Com_Printf("Com_ParseMapDefinition: mapdef \"%s\" with no map\n", name);
		csi.numMDs--;
	}
}

/**
 * @sa CL_ParseClientData
 * @sa CL_ParseScriptFirst
 * @sa CL_ParseScriptSecond
 * @sa Qcommon_Init
 */
void Com_ParseScripts (void)
{
	const char *type, *name, *text;

	Com_Printf("\n----------- parse scripts ----------\n");

	/* reset csi basic info */
	INVSH_InitCSI(&csi);
	csi.idRight = csi.idLeft = csi.idExtension = csi.idBackpack = csi.idBelt = csi.idHolster = csi.idArmour = csi.idFloor = csi.idEquip = csi.idHeadgear = NONE;
	csi.damNormal = csi.damBlast = csi.damFire = csi.damShock = csi.damLaser = csi.damPlasma = csi.damParticle = csi.damStunElectro = csi.damStunGas = NONE;

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
			Com_ParseItem(name, &text, qfalse);
		else if (!Q_strncmp(type, "mapdef", 6))
			Com_ParseMapDefinition(name, &text);
		else if (!Q_strncmp(type, "craftitem", 8))
			Com_ParseItem(name, &text, qtrue);
		else if (!Q_strncmp(type, "inventory", 9))
			Com_ParseInventory(name, &text);
		else if (!Q_strncmp(type, "terrain", 7))
			Com_ParseTerrain(name, &text);
		else if (!sv_dedicated->integer)
			CL_ParseClientData(type, name, &text);
	}

	/* Stage two parsing (weapon/inventory dependant stuff). */
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != 0) {
		/* server/client scripts */
		if (!Q_strncmp(type, "equipment", 9))
			Com_ParseEquipment(name, &text);
		else if (!Q_strncmp(type, "team", 4))
			Com_ParseTeam(name, &text);
	}

	Com_Printf("Shared Client/Server Info loaded\n");
	Com_Printf("...%3i items parsed\n", csi.numODs);
	Com_Printf("...%3i damage types parsed\n", csi.numDTs);
	Com_Printf("...%3i map definitions parsed\n", csi.numMDs);
	Com_Printf("...%3i equipment definitions parsed\n", csi.numEDs);
	Com_Printf("...%3i inventory definitions parsed\n", csi.numIDs);
	Com_Printf("...%3i team definitions parsed\n", csi.numTeamDefs);
}

/** @todo a mess - but i don't want to make the variables non static */
#ifndef DEDICATED_ONLY
#include "../client/client.h"
/**
 * @brief Precache all menu models for faster access
 * @sa CL_PrecacheModels
 */
void Com_PrecacheCharacterModels (void)
{
	teamDef_t *td;
	int i, j, num;
	char model[MAX_QPATH];
	const char *path;
	float loading = cls.loadingPercent;
	linkedList_t *list;
	const float percent = 55.0f;

	/* search the name */
	for (i = 0, td = csi.teamDef; i < csi.numTeamDefs; i++, td++)
		for (j = NAME_NEUTRAL; j < NAME_LAST; j++) {
			/* no models for this gender */
			if (!td->numModels[j])
				continue;
			/* search one of the model definitions */
			list = td->models[j];
			assert(list);
			for (num = 0; num < td->numModels[j]; num++) {
				assert(list);
				path = (const char*)list->data;
				list = list->next;
				/* register body */
				Com_sprintf(model, sizeof(model), "%s/%s", path, list->data);
				if (!R_RegisterModelShort(model))
					Com_Printf("Com_PrecacheCharacterModels: Could not register model %s\n", model);
				list = list->next;
				/* register head */
				Com_sprintf(model, sizeof(model), "%s/%s", path, list->data);
				if (!R_RegisterModelShort(model))
					Com_Printf("Com_PrecacheCharacterModels: Could not register model %s\n", model);

				/* skip skin */
				list = list->next;

				/* new path */
				list = list->next;

				cls.loadingPercent += percent / (td->numModels[j] * csi.numTeamDefs * NAME_LAST);
				SCR_DrawPrecacheScreen(qtrue);
			}
		}
	/* some genders may not have models - ensure that we do the wanted percent step */
	cls.loadingPercent = loading + percent;
}
#endif

#ifdef DEBUG
qboolean Com_ItemsSanityCheck (void)
{
	int i;
	qboolean result = qtrue;

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *item = &csi.ods[i];

		/* Warn if item has no size set. */
		if (item->size <= 0 && !(INV_ItemMatchesFilter(item, FILTER_DUMMY) && item->notOnMarket)) {
			result = qfalse;
			Com_Printf("Com_ItemsSanityCheck: Item %s has zero size set.\n", item->id);
		}

		/* Warn if no price is set. */
		if (item->price <= 0 && !item->notOnMarket) {
			result = qfalse;
			Com_Printf("Com_ItemsSanityCheck: Item %s has zero price set.\n", item->id);
		}

		if (item->price > 0 && item->notOnMarket) {
			result = qfalse;
			Com_Printf("Com_ItemsSanityCheck: Item %s has a price set though it is not available on the market.\n", item->id);
		}

		/* extension and headgear are mutual exclusive */
		if (item->extension && item->headgear) {
			result = qfalse;
			Com_Printf("Com_ItemsSanityCheck: Item %s has both extension and headgear set.\n",  item->id);
		}
	}

	return result;
}
#endif
