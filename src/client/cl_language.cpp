/**
 * @file
 * @brief Language code
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

static cvar_t* fs_i18ndir;
static memPool_t* cl_msgidPool;

#define MAX_MSGIDS 512
/**
 * The msgids are reparsed each time that we change the language - we are only
 * pointing to the po file content to not waste memory for our long texts.
 */
typedef struct msgid_s {
	const char* id;				/**< the msgid id used for referencing via *msgid: 'id' */
	const char* text;			/**< the pointer to the po file */
	struct msgid_s* hash_next;	/**< hash map next pointer in case of collision */
} msgid_t;

static msgid_t msgIDs[MAX_MSGIDS];
static int numMsgIDs;
#define MAX_MSGIDHASH 256
static msgid_t* msgIDHash[MAX_MSGIDHASH];

#define MSGIDSIZE 65536
static char* msgIDText;

static void CL_ParseMessageID (const char* name, const char** text)
{
	/* get it's body */
	const char* token = Com_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("CL_ParseMessageID: msgid \"%s\" without body ignored\n", name);
		return;
	}

	/* search for game types with same name */
	int i;
	for (i = 0; i < numMsgIDs; i++)
		if (Q_streq(token, msgIDs[i].id))
			break;

	if (i == numMsgIDs) {
		msgid_t* msgid = &msgIDs[numMsgIDs++];

		if (numMsgIDs >= MAX_MSGIDS)
			Sys_Error("CL_ParseMessageID: MAX_MSGIDS exceeded");

		OBJZERO(*msgid);
		msgid->id = Mem_PoolStrDup(name, cl_msgidPool, 0);
		const unsigned int hash = Com_HashKey(msgid->id, MAX_MSGIDHASH);
		HASH_Add(msgIDHash, msgid, hash);

		do {
			const char* errhead = "CL_ParseMessageID: unexpected end of file (msgid ";
			token = Com_EParse(text, errhead, name);
			if (!*text)
				break;
			if (*token == '}')
				break;
			if (Q_streq(token, "text")) {
				/* found a definition */
				token = Com_EParse(text, errhead, name, msgIDText, MSGIDSIZE);
				if (!*text)
					break;
				if (token[0] == '_')
					token++;
				if (token[0] != '\0')
					msgid->text = _(token);
				else
					msgid->text = token;
				if (msgid->text == token) {
					msgid->text = Mem_PoolStrDup(token, cl_msgidPool, 0);
					Com_Printf("no translation for %s\n", msgid->id);
				}
			}
		} while (*text);
	} else {
		Com_Printf("CL_ParseMessageID: msgid \"%s\" with same already exists - ignore the second one\n", name);
		Com_SkipBlock(text);
	}
}

static const char* CL_GetMessageID (const char* id)
{
	const unsigned int hash = Com_HashKey(id, MAX_MSGIDHASH);
	for (msgid_t** anchor = &msgIDHash[hash]; *anchor; anchor = &(*anchor)->hash_next) {
		if (Q_streq(id, (*anchor)->id))
			return (*anchor)->text;
	}
	return id;
}

const char* CL_Translate (const char* t)
{
	if (t[0] == '_') {
		if (t[1] != '\0')
			t = _(++t);
	} else {
		const char* msgid = Q_strstart(t, "*msgid:");
		if (msgid != nullptr)
			t = CL_GetMessageID(msgid);
	}

	return t;
}

void CL_ParseMessageIDs (void)
{
	const char* type, *name, *text;

	numMsgIDs = 0;
	OBJZERO(msgIDHash);

	if (cl_msgidPool != nullptr) {
		Mem_FreePool(cl_msgidPool);
	} else {
		cl_msgidPool = Mem_CreatePool("msgids");
	}
	msgIDText = Mem_PoolAllocTypeN(char, MSGIDSIZE, cl_msgidPool);

	Com_Printf("\n----------- parse msgids -----------\n");

	Com_Printf("%i msgid files\n", FS_BuildFileList("ufos/msgid/*.ufo"));
	text = nullptr;

	FS_NextScriptHeader(nullptr, nullptr, nullptr);

	while ((type = FS_NextScriptHeader("ufos/msgid/*.ufo", &name, &text)) != nullptr) {
		if (Q_streq(type, "msgid"))
			CL_ParseMessageID(name, &text);
	}
}

/**
 * @brief List of all mappings for a locale
 */
typedef struct localeMapping_s {
	char* localeMapping;	/**< string that contains e.g. en_US.UTF-8 */
	struct localeMapping_s* next;	/**< next entry in the linked list */
} localeMapping_t;

/**
 * @brief Struct that reflects parsed language definitions
 * from our script files
 */
typedef struct language_s {
	const char* localeID;			/**< short locale id */
	const char* localeString;		/**< translatable locale string to show in menus */
	const char* nativeString;		/**< Name of the language in the native language itself */
	localeMapping_t* localeMapping;	/**< mapping to real locale string for setlocale */
	struct language_s* next;	/**< next language in this list */
} language_t;

static language_t* languageList;	/**< linked list of all parsed languages */
static int languageCount; /**< how many languages do we have */

/**
 * @brief Searches the locale script id with the given locale string
 * @param[in] fullLocale The full locale string. E.g. en_US.UTF-8
 */
static const char* CL_GetLocaleID (const char* fullLocale)
{
	language_t* language = languageList;
	while (language) {
		localeMapping_t* mapping = language->localeMapping;
		while (mapping) {
			if (Q_streq(fullLocale, mapping->localeMapping))
				return language->localeID;
			mapping = mapping->next;
		}
		language = language->next;
	}
	Com_DPrintf(DEBUG_CLIENT, "CL_GetLocaleID: Could not find your system locale '%s'. "
		"Add it to the languages script file and send a patch please.\n", fullLocale);
	return nullptr;
}

/**
 * @brief Parse all language definitions from the script files
 */
void CL_ParseLanguages (const char* name, const char** text)
{
	const char* errhead = "CL_ParseLanguages: unexpected end of file (language ";
	const char* token;

	if (!*text) {
		Com_Printf("CL_ParseLanguages: language without body ignored (%s)\n", name);
		return;
	}

	token = Com_EParse(text, errhead, name);
	if (!*text || *token != '{') {
		Com_Printf("CL_ParseLanguages: language without body ignored (%s)\n", name);
		return;
	}

	language_t* const language = Mem_PoolAllocType(language_t, cl_genericPool);
	language->localeID = Mem_PoolStrDup(name, cl_genericPool, 0);
	language->localeString = "";
	language->nativeString = "";
	language->localeMapping = nullptr;

	do {
		/* get the name type */
		token = Com_EParse(text, errhead, name);
		if (!*text || *token == '}')
			break;
		/* inner locale id definition */
		if (Q_streq(token, "code")) {
			linkedList_t* list;
			if (!Com_ParseList(text, &list)) {
				Com_Error(ERR_DROP, "CL_ParseLanguages: error while reading language codes \"%s\"", name);
			}
			for (linkedList_t* element = list; element != nullptr; element = element->next) {
				localeMapping_t* const mapping = Mem_PoolAllocType(localeMapping_t, cl_genericPool);
				mapping->localeMapping = Mem_PoolStrDup((char*)element->data, cl_genericPool, 0);
				/* link it in */
				mapping->next = language->localeMapping;
				language->localeMapping = mapping;
			}
			LIST_Delete(&list);
		} else if (Q_streq(token, "name")) {
			token = Com_EParse(text, errhead, name);
			if (!*text || *token == '}')
				Com_Error(ERR_FATAL, "CL_ParseLanguages: Name expected for language \"%s\".\n", name);
			if (*token != '_') {
				Com_Printf("CL_ParseLanguages: language: '%s' - not marked translatable (%s)\n", name, token);
			}
			language->localeString = Mem_PoolStrDup(token, cl_genericPool, 0);
		} else if (Q_streq(token, "native")) {
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
 * @return true if setting given language is possible.
 */
static bool CL_LanguageTest (const char* localeID)
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
		Com_sprintf(languagePath, sizeof(languagePath), "%s/" BASEDIRNAME "/i18n/", FS_GetCwd());
#endif
	Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: using mo files from '%s'\n", languagePath);
	Q_strcat(languagePath, sizeof(languagePath), "%s/LC_MESSAGES/ufoai.mo", localeID);

	/* No *.mo file -> no language. */
	if (!FS_FileExists("%s", languagePath)) {
		Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: locale '%s' not found.\n", localeID);
		return false;
	}

#ifdef _WIN32
	if (Sys_Setenv("LANGUAGE=%s", localeID) == 0) {
		Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: locale '%s' found.\n", localeID);
		return true;
	}
#else
	for (i = 0, language = languageList; i < languageCount; language = language->next, i++) {
		if (Q_streq(localeID, language->localeID))
			break;
	}
	if (i == languageCount) {
		Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: Could not find locale with id '%s'\n", localeID);
		return false;
	}

	mapping = language->localeMapping;
	if (!mapping) {
		Com_DPrintf(DEBUG_CLIENT, "No locale mappings for locale with id '%s'\n", localeID);
		return false;
	}
	/* Cycle through all mappings, but stop at first locale possible to set. */
	do {
		/* setlocale() will return nullptr if no setting possible. */
		if (setlocale(LC_MESSAGES, mapping->localeMapping)) {
			Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: language '%s' with locale '%s' found.\n", localeID, mapping->localeMapping);
			return true;
		} else
			Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: language '%s' with locale '%s' not found on your system.\n", localeID, mapping->localeMapping);
		mapping = mapping->next;
	} while (mapping);
	Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTest: not possible to use language '%s'.\n", localeID);
#endif

	return false;
}

void CL_LanguageShutdown (void)
{
	languageCount = 0;
	languageList = nullptr;
	Mem_DeletePool(cl_msgidPool);
	cl_msgidPool = nullptr;
	msgIDText = nullptr;
	numMsgIDs = 0;
	OBJZERO(msgIDHash);
}

void CL_LanguageInitMenu (void)
{
	uiNode_t* languageOption = nullptr;
	language_t* language = languageList;
	while (language) {
		const bool available = Q_streq(language->localeID, "none") || CL_LanguageTest(language->localeID);
		uiNode_t* option = UI_AddOption(&languageOption, "", language->nativeString, language->localeID);
		option->disabled = !available;
		language = language->next;
	}

	/* sort the list, and register it to the menu */
	UI_SortOptions(&languageOption);
	UI_RegisterOption(OPTION_LANGUAGES, languageOption);

	/* Set to the locale remembered previously. */
	CL_LanguageTryToSet(s_language->string);
}

/**
 * @brief Fills the options language menu node with the parsed language mappings
 * @sa CL_InitAfter
 * @sa CL_LocaleSet
 */
void CL_LanguageInit (void)
{
	fs_i18ndir = Cvar_Get("fs_i18ndir", "", 0, "System path to language files");

	char systemLanguage[MAX_VAR] = "";
	if (Q_strvalid(s_language->string)) {
		Com_Printf("CL_LanguageInit: language settings are stored in configuration: %s\n", s_language->string);
		Q_strncpyz(systemLanguage, s_language->string, sizeof(systemLanguage));
	} else {
		const char* currentLocale = Sys_GetLocale();
		if (currentLocale) {
			const char* localeID = CL_GetLocaleID(currentLocale);
			if (localeID)
				Q_strncpyz(systemLanguage, localeID, sizeof(systemLanguage));
		}
	}

	Com_DPrintf(DEBUG_CLIENT, "CL_LanguageInit: system language is: '%s'\n", systemLanguage);
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
	CL_ParseMessageIDs();
}

/**
 * @brief Cycle through all parsed locale mappings and try to set one after another
 * @param[in] localeID the locale id parsed from scriptfiles (e.g. en or de [the short id])
 * @sa CL_LocaleSet
 */
bool CL_LanguageTryToSet (const char* localeID)
{
	int i;
	language_t* language;
	localeMapping_t* mapping;

	assert(localeID);

	/* in case of an error we really don't want a flooded console */
	s_language->modified = false;

	for (i = 0, language = languageList; i < languageCount; language = language->next, i++) {
		if (Q_streq(localeID, language->localeID))
			break;
	}

	if (i == languageCount) {
		Com_Printf("Could not find locale with id '%s'\n", localeID);
		return false;
	}

	mapping = language->localeMapping;
	if (!mapping) {
		Com_Printf("No locale mappings for locale with id '%s'\n", localeID);
		return false;
	}

	Cvar_Set("s_language", "%s", localeID);
	s_language->modified = false;

	do {
		Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTryToSet: %s (%s)\n", mapping->localeMapping, localeID);
		if (Sys_SetLocale(mapping->localeMapping)) {
			CL_NewLanguage();
			return true;
		}
		mapping = mapping->next;
	} while (mapping);

#ifndef _WIN32
	Com_DPrintf(DEBUG_CLIENT, "CL_LanguageTryToSet: Finally try: '%s'\n", localeID);
	Sys_SetLocale(localeID);
	CL_NewLanguage();
#endif

	return false;
}
