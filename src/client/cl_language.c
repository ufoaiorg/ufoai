/**
 * @file cl_language.c
 * @brief Language code
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "cl_language.h"
#include "../shared/parse.h"
#include "../ports/system.h"

#include "ui/ui_main.h"
#include "ui/ui_font.h"
#include "ui/node/ui_node_abstractoption.h"

static cvar_t *fs_i18ndir;


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
	const char *localeID;			/**< short locale id */
	const char *localeString;		/**< translatable locale string to show in menus */
	const char *nativeString;		/**< Name of the language in the native language itself */
	localeMapping_t *localeMapping;	/**< mapping to real locale string for setlocale */
	struct language_s *next;	/**< next language in this list */
} language_t;

static language_t *languageList;	/**< linked list of all parsed languages */
static int languageCount; /**< how many languages do we have */

/**
 * @brief Searches the locale script id with the given locale string
 * @param[in] fullLocale The full locale string. E.g. en_US.UTF-8
 */
static const char *CL_GetLocaleID (const char *fullLocale)
{
	int i;
	language_t *language;

	for (i = 0, language = languageList; i < languageCount; language = language->next, i++) {
		localeMapping_t *mapping = language->localeMapping;

		while (mapping) {
			if (Q_streq(fullLocale, mapping->localeMapping))
				return language->localeID;
			mapping = mapping->next;
		}
	}
	Com_DPrintf(DEBUG_CLIENT, "CL_GetLocaleID: Could not find your system locale '%s'. "
		"Add it to the languages script file and send a patch please.\n", fullLocale);
	return NULL;
}

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

	token = Com_EParse(text, errhead, name);
	if (!*text || *token != '{') {
		Com_Printf("CL_ParseLanguages: language without body ignored (%s)\n", name);
		return;
	}

	language = (language_t *)Mem_PoolAlloc(sizeof(*language), cl_genericPool, 0);
	language->localeID = Mem_PoolStrDup(name, cl_genericPool, 0);
	language->localeString = "";
	language->nativeString = "";
	language->localeMapping = NULL;

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
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
				token = Com_EParse(text, errhead, name);
				/* end of locale mappings reached */
				if (!*text || *token == '}')
					break;
				mapping = (localeMapping_t *)Mem_PoolAlloc(sizeof(*mapping), cl_genericPool, 0);
				mapping->localeMapping = Mem_PoolStrDup(token, cl_genericPool, 0);
				/* link it in */
				mapping->next = language->localeMapping;
				language->localeMapping = mapping;
			} while (*text);
		} else if (strcmp(token, "name") == 0) {
			token = Com_EParse(text, errhead, name);
			if (!*text || *token == '}')
				Com_Error(ERR_FATAL, "CL_ParseLanguages: Name expected for language \"%s\".\n", name);
			if (*token != '_') {
				Com_Printf("CL_ParseLanguages: language: '%s' - not marked translatable (%s)\n", name, token);
			}
			language->localeString = Mem_PoolStrDup(token, cl_genericPool, 0);
		} else if (strcmp(token, "native") == 0) {
			token = Com_EParse(text, errhead, name);
			if (!*text || *token == '}')
				Com_Error(ERR_FATAL, "CL_ParseLanguages: Native expected for language \"%s\".\n", name);
			language->nativeString = Mem_PoolStrDup(token, cl_genericPool, 0);
		}
	} while (*text);

	language->next = languageList;
	languageList = language;
	languageCount++;
}

/**
 * @brief Test given language by trying to set locale.
 * @param[in] localeID language abbreviation.
 * @return qtrue if setting given language is possible.
 */
static qboolean CL_LanguageTest (const char *localeID)
{
#ifndef _WIN32
	int i;
	language_t* language;
	localeMapping_t* mapping;
#endif
	char languagePath[MAX_OSPATH];

	assert(localeID);

	/* Find the proper *.mo file. */
	if (fs_i18ndir->string[0] != '\0')
		Q_strncpyz(languagePath, fs_i18ndir->string, sizeof(languagePath));
	else
#ifdef LOCALEDIR
		Com_sprintf(languagePath, sizeof(languagePath), LOCALEDIR);
#else
		Com_sprintf(languagePath, sizeof(languagePath), "%s/"BASEDIRNAME"/i18n/", FS_GetCwd());
#endif
	Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: using mo files from '%s'\n", languagePath);
	Q_strcat(languagePath, localeID, sizeof(languagePath));
	Q_strcat(languagePath, "/LC_MESSAGES/ufoai.mo", sizeof(languagePath));

	/* No *.mo file -> no language. */
	if (!FS_FileExists(languagePath)) {
		Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: locale '%s' not found.\n", localeID);
		return qfalse;
	}

#ifdef _WIN32
	if (Sys_Setenv("LANGUAGE=%s", localeID) == 0) {
		Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: locale '%s' found.\n", localeID);
		return qtrue;
	}
#else
	for (i = 0, language = languageList; i < languageCount; language = language->next, i++) {
		if (Q_streq(localeID, language->localeID))
			break;
	}
	if (i == languageCount) {
		Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: Could not find locale with id '%s'\n", localeID);
		return qfalse;
	}

	mapping = language->localeMapping;
	if (!mapping) {
		Com_DPrintf(DEBUG_CLIENT, "No locale mappings for locale with id '%s'\n", localeID);
		return qfalse;
	}
	/* Cycle through all mappings, but stop at first locale possible to set. */
	do {
		/* setlocale() will return NULL if no setting possible. */
		if (setlocale(LC_MESSAGES, mapping->localeMapping)) {
			Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: language '%s' with locale '%s' found.\n", localeID, mapping->localeMapping);
			return qtrue;
		} else
			Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: language '%s' with locale '%s' not found on your system.\n", localeID, mapping->localeMapping);
		mapping = mapping->next;
	} while (mapping);
	Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: not possible to use language '%s'.\n", localeID);
#endif

	return qfalse;
}

/**
 * @brief Fills the options language menu node with the parsed language mappings
 * @sa CL_InitAfter
 * @sa CL_LocaleSet
 */
void CL_LanguageInit (void)
{
	int i;
	language_t* language;
	uiNode_t *languageOption = NULL;
	char systemLanguage[MAX_VAR];

	fs_i18ndir = Cvar_Get("fs_i18ndir", "", 0, "System path to language files");

	if (s_language->string[0] != '\0') {
		Com_Printf("CL_LanguageInit: language settings are stored in configuration: %s\n", s_language->string);
		Q_strncpyz(systemLanguage, s_language->string, sizeof(systemLanguage));
	} else {
		const char *currentLocale = Sys_GetLocale();

		if (currentLocale) {
			const char *localeID = CL_GetLocaleID(currentLocale);
			if (localeID)
				Q_strncpyz(systemLanguage, localeID, sizeof(systemLanguage));
			else
				systemLanguage[0] = '\0';
		} else
			systemLanguage[0] = '\0';
	}

	Com_DPrintf(DEBUG_CLIENT, "CL_LanguageInit: system language is: '%s'\n", systemLanguage);

	for (i = 0, language = languageList; i < languageCount; language = language->next, i++) {
		qboolean available;
		available = Q_streq(language->localeID, "none") || CL_LanguageTest(language->localeID);
		uiNode_t *option;
#if 0
		option = UI_AddOption(&languageOption, "", language->localeString, language->localeID);
#else
		option = UI_AddOption(&languageOption, "", language->nativeString, language->localeID);
#endif
		option->disabled = !available;
	}

	/* sort the list, and register it to the menu */
	UI_SortOptions(&languageOption);
	UI_RegisterOption(OPTION_LANGUAGES, languageOption);

	/* Set to the locale remembered previously. */
	CL_LanguageTryToSet(systemLanguage);
}

/**
 * @brief Adjust game for new language: reregister fonts, etc.
 */
static void CL_NewLanguage (void)
{
	R_FontShutdown();
	R_FontInit();
	UI_InitFonts();
	R_FontSetTruncationMarker(_("..."));
}

/**
 * @brief Cycle through all parsed locale mappings and try to set one after another
 * @param[in] localeID the locale id parsed from scriptfiles (e.g. en or de [the short id])
 * @sa CL_LocaleSet
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
		if (Q_streq(localeID, language->localeID))
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

	Cvar_Set("s_language", localeID);
	s_language->modified = qfalse;

	do {
		Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTryToSet: %s (%s)\n", mapping->localeMapping, localeID);
		if (Sys_SetLocale(mapping->localeMapping)) {
			CL_NewLanguage();
			return qtrue;
		}
		mapping = mapping->next;
	} while (mapping);

#ifndef _WIN32
	Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTryToSet: Finally try: '%s'\n", localeID);
	Sys_SetLocale(localeID);
	CL_NewLanguage();
#endif

	return qfalse;
}
