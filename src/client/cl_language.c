/**
 * @file cl_language.c
 * @brief Language code
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/
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

#include "client.h"

/**
 * @brief Struct that reflects parsed language definitions
 * from our script files
 */
typedef struct language_s {
	char *localeID;			/**< short locale id */
	char *localeString;		/**< translateable locale string to show in menus */
	char *localeMapping;	/**< mapping to real locale string for setlocale */
	struct language_s *next;	/**< next language in this list */
} language_t;

static language_t *languageList;	/**< linked list of all parsed languages */
static int languageCount; /**< how many languages do we have */

/**
 * @brief Parse all language definitions from the script files
 */
extern void CL_ParseLanguages (const char *name, char **text)
{
	const char *errhead = "CL_ParseLanguages: unexpected end of file (language ";
	char	*token;
	language_t *language = NULL;

	if (!*text) {
		Com_Printf("CL_ParseLanguages: language without body ignored (%s)\n", name);
		return;
	}

	token = COM_EParse(text, errhead, name);
	if (!*text || *token != '{') {
		Com_Printf("CL_ParseLanguages: language without body ignored (%s)\n", name);
		return;
	}

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text || *token == '}')
			break;
		/* inner locale id definition */
		if (*token == '{') {
			if (!language) {
				Com_Printf("CL_ParseLanguages: language: '%s' - found mappings with language string - ignore it\n", name);
				return;
			}
			do {
				/* get the name type */
				token = COM_EParse(text, errhead, name);
				if (!*text || *token == '}')
					break;
				/* FIXME: currently only one locale is supported */
				if (!language->localeMapping)
					language->localeMapping = Mem_PoolStrDup(token, cl_genericPool, CL_TAG_NONE);
				else
					Com_Printf("CL_ParseLanguages: language: '%s' has more than one mapping\n", name);
			} while (*text);
			language = NULL;
		} else {
			if (*token != '_') {
				Com_Printf("CL_ParseLanguages: language: '%s' - not marked translateable (%s) - ignore it\n", name, token);
				continue;
			}
			language = Mem_PoolAlloc(sizeof(language_t), cl_genericPool, CL_TAG_NONE);
			language->localeID = Mem_PoolStrDup(name, cl_genericPool, CL_TAG_NONE);
			language->localeString = Mem_PoolStrDup(token + 1, cl_genericPool, CL_TAG_NONE);
			language->localeMapping = NULL;
			language->next = languageList;
			languageList = language;
			languageCount++;
		}
	} while (*text);
}

/**
 * @brief Fills the options language menu node with the parsed language mappings
 * @sa CL_InitAfter
 */
extern void CL_LanguageInit (void)
{
	int i;
	menu_t* menu;
	menuNode_t* languageOptions;
	selectBoxOptions_t* selectBoxOption;
	language_t* language;

	menu = MN_GetMenu("options_game");
	if (!menu)
		Sys_Error("Could not find menu options_game\n");
	languageOptions = MN_GetNode(menu, "select_language");
	if (!languageOptions)
		Sys_Error("Could not find node select_language in menu options_game\n");
	for (i = 0, language = languageList; i < languageCount; language = language->next, i++) {
		selectBoxOption = MN_AddSelectboxOption(languageOptions);
		if (!selectBoxOption)
			return;
		Com_sprintf(selectBoxOption->label, sizeof(selectBoxOption->label), language->localeString);
		Com_sprintf(selectBoxOption->value, sizeof(selectBoxOption->value), language->localeMapping);
	}
}
