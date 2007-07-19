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
 * @brief List of all mappings for a locale
 */
typedef struct localeMapping_s {
	char *localeMapping;	/**< string that contains e.g. en_US.UTF-8 */
	struct localeMapping_s *next;	/**< next entry in the linked list */
} localeMapping_t;

/**
 * @brief Struct that reflects parsed language definitions
 * from our script files
 */
typedef struct language_s {
	char *localeID;			/**< short locale id */
	char *localeString;		/**< translateable locale string to show in menus */
	localeMapping_t *localeMapping;	/**< mapping to real locale string for setlocale */
	struct language_s *next;	/**< next language in this list */
} language_t;

static language_t *languageList;	/**< linked list of all parsed languages */
static int languageCount; /**< how many languages do we have */

/**
 * @brief Parse all language definitions from the script files
 */
void CL_ParseLanguages (const char *name, const char **text)
{
	const char *errhead = "CL_ParseLanguages: unexpected end of file (language ";
	const char	*token;
	language_t *language = NULL;
	localeMapping_t *mapping = NULL;

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
				/* get the locale mappings now type */
				token = COM_EParse(text, errhead, name);
				/* end of locale mappings reached */
				if (!*text || *token == '}')
					break;
				mapping = Mem_PoolAlloc(sizeof(localeMapping_t), cl_genericPool, CL_TAG_NONE);
				mapping->localeMapping = Mem_PoolStrDup(token, cl_genericPool, CL_TAG_NONE);
				/* link it in */
				mapping->next = language->localeMapping;
				language->localeMapping = mapping;
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
 * @brief Test given language by trying to set locale.
 * @param[in] *localeID language abbreviation.
 * @return qtrue if setting given language is possible.
 */
static qboolean CL_LanguageTest (const char *localeID)
{
#ifndef _WIN32
	int i;
	language_t* language;
	localeMapping_t* mapping;
	qboolean possible = qtrue;
#endif
	char languagePath[MAX_OSPATH];
	cvar_t *fs_i18ndir;

	assert(localeID);

	/* Find the proper *.mo file. */
	fs_i18ndir = Cvar_Get("fs_i18ndir", "", 0, "System path to language files");
	if (*fs_i18ndir->string)
		Q_strncpyz(languagePath, fs_i18ndir->string, sizeof(languagePath));
	else
		Com_sprintf(languagePath, sizeof(languagePath), "%s/base/i18n/", FS_GetCwd());
	Com_DPrintf("Sys_TestLanguage()... using mo files from %s\n", languagePath);
	Q_strcat(languagePath, localeID, sizeof(languagePath));
	Q_strcat(languagePath, "/LC_MESSAGES/ufoai.mo", sizeof(languagePath));
	
#ifdef _WIN32
	if ((Q_putenv("LANGUAGE", localeID) == 0) && FS_FileExists(languagePath)) {
		Com_DPrintf("CL_LanguageTest()... locale %s found.\n", localeID);
		return qtrue;
	} else {
		Com_DPrintf("CL_LanguageTest()... locale %s not found.\n", localeID);
		return qfalse;
	}		
#else
	for (i = 0, language = languageList; i < languageCount; language = language->next, i++) {
		if (!Q_strcmp(localeID, language->localeID))
			break;
	}
	if (i == languageCount) {
		Com_DPrintf("Could not find locale with id '%s'\n", localeID);
		return qfalse;
	}

	/* No *.mo file -> no language. */
	if (!FS_FileExists(languagePath))
		return qfalse;

	mapping = language->localeMapping;
	if (!mapping) {
		Com_DPrintf("No locale mappings for locale with id '%s'\n", localeID);
		return qfalse;
	}
	/* Cycle through all mappings, but stop at first locale possible to set. */
	do {
		/* setlocale() will return NULL if no setting possible. */
		if (setlocale(LC_MESSAGES, mapping->localeMapping)) {
			Com_DPrintf("CL_LanguageTest()... language %s with locale %s found.\n", localeID, mapping->localeMapping);
			return qtrue;
		} else {
			Com_DPrintf("CL_LanguageTest()... language %s with locale %s not found.\n", localeID, mapping->localeMapping);
			possible = qfalse;
		}
		mapping = mapping->next;
	} while (mapping);
	if (!possible)
		Com_DPrintf("CL_LanguageTest()... not possible to use %s language.\n", localeID);
	return qfalse;
#endif
}

/**
 * @brief Fills the options language menu node with the parsed language mappings
 * @sa CL_InitAfter
 */
void CL_LanguageInit (void)
{
	int i;
	menu_t* menu;
	menuNode_t* languageOptions;
	selectBoxOptions_t* selectBoxOption;
	language_t* language;
	char deflang[MAX_VAR];

	if (*s_language->string) {
		Com_Printf("CL_LanguageInit()... language settings are stored in configuration: %s\n", s_language->string);
		Q_strncpyz(deflang, s_language->string, MAX_VAR);
	} else {
#ifdef _WIN32
		if (getenv("LANGUAGE"))
			Q_strncpyz(deflang, getenv("LANGUAGE"), MAX_VAR);
		else {
			/* Setting to en will always work in every windows. */
			Q_strncpyz(deflang, "en", MAX_VAR);
		}	
#else
		/* Calling with NULL param should return current system settings. */
		Q_strncpyz(deflang, setlocale(LC_MESSAGES, NULL), MAX_VAR);
		if (!deflang)
			Q_strncpyz(deflang, "C", MAX_VAR);
#endif
	}
	
	Com_DPrintf("CL_LanguageInit()... deflang: %s\n", deflang);

	menu = MN_GetMenu("options_game");
	if (!menu)
		Sys_Error("Could not find menu options_game\n");
	languageOptions = MN_GetNode(menu, "select_language");
	if (!languageOptions)
		Sys_Error("Could not find node select_language in menu options_game\n");
	for (i = 0, language = languageList; i < languageCount; language = language->next, i++) {
		/* No language option available only for DEBUG. */
		if (Q_strncmp(language->localeID, "none", 4) == 0) {
#ifndef DEBUG
			continue;
#endif
		}
		/* Test the locale first, add to list if setting given locale possible. */
		if (CL_LanguageTest(language->localeID) || (Q_strncmp(language->localeID, "none", 4) == 0)) {
			selectBoxOption = MN_AddSelectboxOption(languageOptions);
			if (!selectBoxOption)
				return;
			Com_sprintf(selectBoxOption->label, sizeof(selectBoxOption->label), language->localeString);
			Com_sprintf(selectBoxOption->value, sizeof(selectBoxOption->value), language->localeID);
		}
	}

	menu = MN_GetMenu("checkcvars");
	if (!menu)
		Sys_Error("Could not find menu checkcvars\n");
	languageOptions = MN_GetNode(menu, "select_language");
	if (!languageOptions)
		Sys_Error("Could not find node select_language in menu checkcvars\n");
	for (i = 0, language = languageList; i < languageCount; language = language->next, i++) {
		/* No language option available only for DEBUG. */
		if (Q_strncmp(language->localeID, "none", 4) == 0) {
#ifndef DEBUG
			continue;
#endif
		}			
		/* Test the locale first, add to list if setting given locale possible. */
		if (CL_LanguageTest(language->localeID) || (Q_strncmp(language->localeID, "none", 4) == 0)) {
			selectBoxOption = MN_AddSelectboxOption(languageOptions);
			if (!selectBoxOption)
				return;
			Com_sprintf(selectBoxOption->label, sizeof(selectBoxOption->label), language->localeString);
			Com_sprintf(selectBoxOption->value, sizeof(selectBoxOption->value), language->localeID);
		}
	}
	/* Set to the locale remembered previously. */
#ifdef _WIN32
	CL_LanguageTryToSet(deflang);
#else
	setlocale(LC_MESSAGES, deflang);
#endif
}

/**
 * @brief Cycle through all parsed locale mappings and try to set one after another
 * @param[in] localeID the locale id parsed from scriptfiles
 * @sa Qcommon_LocaleSet
 */
qboolean CL_LanguageTryToSet (const char *localeID)
{
	int i;
	language_t* language;
	localeMapping_t* mapping;

	assert(localeID);

	/* in case of an error we really don't want a flooded console */
	s_language->modified = qfalse;

	for (i = 0, language = languageList; i < languageCount; language = language->next, i++) {
		if (!Q_strcmp(localeID, language->localeID))
			break;
	}

	if (i == languageCount) {
		Com_Printf("Could not find locale with id '%s'\n", localeID);
		return qfalse;
	}

	mapping = language->localeMapping;
	if (!mapping) {
		Com_Printf("No locale mappings for locale with id '%s'\n", localeID);
		return qfalse;
	}
	do {
		Cvar_Set("s_language", mapping->localeMapping);
#if _WIN32
		Com_Printf("CL_LanguageTryToSet: %s\n", mapping->localeMapping);
		Q_putenv("LANGUAGE", mapping->localeMapping);
		Cvar_Set("s_language", language->localeID);
		s_language->modified = qfalse;
		return qtrue;
#else
		if(setlocale(LC_MESSAGES, mapping->localeMapping)) {
			Cvar_Set("s_language", language->localeID);
			s_language->modified = qfalse;
			return qtrue;
		}
#endif
		mapping = mapping->next;
	} while (mapping);
	return qfalse;
}
