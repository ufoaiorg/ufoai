/**
 * @file
 * @brief In-game message settings
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "../../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_messageoptions.h"
#include "cp_messageoptions_callbacks.h"
#include "cp_time.h"
#include "save/save_messageoptions.h"

/** @brief valid notification types that may cause pause / notice */
char const* const nt_strings[NT_NUM_NOTIFYTYPE] = {
	N_("installation_installed"),
	N_("installation_removed"),
	N_("installation_replaced"),
	N_("aircraft_refueled"),
	N_("aircraft_cannotrefuel"),
	N_("aircraft_arrivedhome"),
	N_("installation_build_started"),
	N_("installation_build_finished"),
	N_("installation_destroyed"),
	N_("research_new_proposed"),
	N_("research_halted"),
	N_("research_completed"),
	N_("production_started"),
	N_("production_finished"),
	N_("production_failed"),
	N_("production_queue_empty"),
	N_("nation_happiness_changed"),
	N_("nation_unhappy"),
	N_("nation_pleased"),
	N_("transfer_started"),
	N_("transfer_completed_success"),
	N_("transfer_lost"),
	N_("transfer_aliens_defered"),
	N_("transfer_uforecovery_finished"),
	N_("ufo_spotted"),
	N_("ufo_signal_lost"),
	N_("ufo_attacking"),
	N_("base_attack"),
	N_("building_finished")
};
CASSERT(lengthof(nt_strings) == NT_NUM_NOTIFYTYPE);

messageSettings_t messageSettings[NT_NUM_NOTIFYTYPE]; /**< array holding actual message settings for every notify type */

/**
 * @brief Function updates pause or notification settings.
 * @param listIndex listIndex in menu to update via confunc
 * @param type notification type to update
 * @param optionType option type that should be updated
 * @param activate flag indicating whether setting should be activated (true) or deactivated
 * @param sendCommands flag indicating whether confunc command to update menu button should be sent
 * @sa MSO_Toggle_f
 * @sa MSO_Set_f
 * @note if sendCommands is false, initialization of buttons is reactivated for next menu displaying
 */
void MSO_Set (const int listIndex, const notify_t type, const int optionType, const bool activate, const bool sendCommands)
{
	messageSettings_t* settings = &messageSettings[type];

	if (activate) {
		if ((optionType & NTMASK_PAUSE) == NTMASK_PAUSE)
			settings->doPause = activate;
		if ((optionType & NTMASK_SOUND)== NTMASK_SOUND)
			settings->doSound = activate;
		/* notification anyway*/
		settings->doNotify = activate;
	} else {
		if ((optionType & NTMASK_PAUSE) == NTMASK_PAUSE)
			settings->doPause = activate;
		if ((optionType & NTMASK_SOUND)== NTMASK_SOUND)
			settings->doSound = activate;
		/* disable all if notify is disabled */
		if (optionType == MSO_NOTIFY) {
			settings->doNotify = activate;
			settings->doPause = activate;
			settings->doSound = activate;
		}
	}

	if (sendCommands)
		cgi->UI_ExecuteConfunc("ms_btnstate %i %i %i %i", listIndex, settings->doPause, settings->doNotify, settings->doSound);
	else
		/* ensure that message buttons will be initialized correctly if menu is shown next time */
		MSO_SetMenuState(MSO_MSTATE_PREPARED, false, false);
}

/**
 * @brief Parse notify type
 * @return A NT_ number, else return -1
 */
static int MSO_ParseNotifyType (const char* name)
{
	for (int idx = 0; idx < NT_NUM_NOTIFYTYPE; idx ++) {
		if (Q_streq(name, nt_strings[idx])) {
			return idx;
		}
	}
	return -1;
}

/**
 * @brief Parse option type
 * @return A MSO value, else 0
 */
static int MSO_ParseOptionType (const char* type)
{
	if (Q_strcaseeq(type, "pause"))
		return MSO_PAUSE;
	else if (Q_strcaseeq(type, "notify"))
		return MSO_NOTIFY;
	else if (Q_strcaseeq(type, "sound"))
		return MSO_SOUND;

	Com_Printf("Unrecognized optionstype during set '%s' ignored\n", type);
	return 0;
}

/**
 * @brief Function callback used to initialize values for messageoptions and for manual setting changes.
 * @sa MSO_Set
 */
static void MSO_Set_f (void)
{
	if (cgi->Cmd_Argc() != 4) {
		Com_Printf("Usage: %s <messagetypename> <pause|notify|sound> <0|1>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int optionsType = MSO_ParseOptionType(cgi->Cmd_Argv(1));
	if (optionsType == 0)
		return;

	const char* messagetype = cgi->Cmd_Argv(1);
	int type;
	for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
		if (Q_streq(nt_strings[type], messagetype))
			break;
	}
	if (type == NT_NUM_NOTIFYTYPE) {
		Com_Printf("Unrecognized messagetype during set '%s' ignored\n", messagetype);
		return;
	}

	MSO_Set(0, (notify_t)type, optionsType, atoi(cgi->Cmd_Argv(3)), false);
}

/**
 * @brief Function callback that sets all message options settings for one option type to given value.
 * @sa MSO_Set
 * @sa MSO_Init_f
 */
static void MSO_SetAll_f (void)
{
	if (cgi->Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <pause|notify|sound> <0|1>\n", cgi->Cmd_Argv(0));
		return;
	}

	const bool activate = atoi(cgi->Cmd_Argv(2));
	const int optionsType = MSO_ParseOptionType(cgi->Cmd_Argv(1));
	if (optionsType == 0)
		return;

	/* update settings for chosen type */
	for (int type = 0; type < NT_NUM_NOTIFYTYPE; ++type) {
		MSO_Set(0, (notify_t)type, optionsType, activate, false);
	}
	/* reinit menu */
	MSO_SetMenuState(MSO_MSTATE_PREPARED, true, true);
}

/**
 * @brief Adds a new message to message stack. It uses message settings to
 * verify whether sound should be played and whether game should stop.
 * @param messagecategory category for notification
 * @param[in] title Already translated message/mail title
 * @param[in] text Already translated message/mail body
 * @param[in] popup Show this as a popup, too?
 * @param[in] type The message type
 * @param[in] pedia Pointer to technology (only if needed)
 * @return message_t pointer if message was added
 * @sa MS_AddNewMessageSound
 */
uiMessageListNodeMessage_t* MSO_CheckAddNewMessage (const notify_t messagecategory, const char* title, const char* text, messageType_t type, technology_t* pedia, bool popup)
{
	uiMessageListNodeMessage_t* result = nullptr;
	const messageSettings_t* settings = &messageSettings[messagecategory];

	if (settings->doNotify)
		result = MS_AddNewMessage(title, text, type, pedia, popup, settings->doSound);
	if (settings->doPause)
		CP_GameTimeStop();
	return result;
}

/**
 * @brief saves current notification and pause settings
 * @sa MSO_LoadXML
 */
bool MSO_SaveXML (xmlNode_t* p)
{
	xmlNode_t* n = cgi->XML_AddNode(p, SAVE_MESSAGEOPTIONS_MESSAGEOPTIONS);

	/* save positive values */
	for (int type = 0; type < NT_NUM_NOTIFYTYPE; ++type) {
		messageSettings_t actualSetting = messageSettings[type];
		xmlNode_t* s = cgi->XML_AddNode(n, SAVE_MESSAGEOPTIONS_TYPE);

		cgi->XML_AddString(s, SAVE_MESSAGEOPTIONS_NAME, nt_strings[type]);
		cgi->XML_AddBoolValue(s, SAVE_MESSAGEOPTIONS_NOTIFY, actualSetting.doNotify);
		cgi->XML_AddBoolValue(s, SAVE_MESSAGEOPTIONS_PAUSE, actualSetting.doPause);
		cgi->XML_AddBoolValue(s, SAVE_MESSAGEOPTIONS_SOUND, actualSetting.doSound);
	}

	return true;
}

/**
 * @brief Restores the notification and pause settings from savegame
 * @sa MSO_SaveXML
 */
bool MSO_LoadXML (xmlNode_t* p)
{
	xmlNode_t* n, *s;

	n = cgi->XML_GetNode(p, SAVE_MESSAGEOPTIONS_MESSAGEOPTIONS);
	if (!n)
		return false;

	for (s = cgi->XML_GetNode(n, SAVE_MESSAGEOPTIONS_TYPE); s; s = cgi->XML_GetNextNode(s, n, SAVE_MESSAGEOPTIONS_TYPE)) {
		const char* messagetype = cgi->XML_GetString(s, SAVE_MESSAGEOPTIONS_NAME);
		int type;

		for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
			if (Q_streq(nt_strings[type], messagetype))
				break;
		}

		/** @todo (menu) check why this message is not shown anywhere in logs*/
		if (type == NT_NUM_NOTIFYTYPE) {
			Com_Printf("Unrecognized messagetype '%s' ignored while loading\n", messagetype);
			continue;
		}
		MSO_Set(0, (notify_t)type, MSO_NOTIFY, cgi->XML_GetBool(s, SAVE_MESSAGEOPTIONS_NOTIFY, false), false);
		MSO_Set(0, (notify_t)type, MSO_PAUSE, cgi->XML_GetBool(s, SAVE_MESSAGEOPTIONS_PAUSE, false), false);
		MSO_Set(0, (notify_t)type, MSO_SOUND, cgi->XML_GetBool(s, SAVE_MESSAGEOPTIONS_SOUND, false), false);
	}

	MSO_SetMenuState(MSO_MSTATE_REINIT, false, false);
	return true;
}

/**
 * @brief parses message options settings from file.
 */
static int MSO_ParseOption (const char* blockName, const char** text)
{
	const char* errhead = "MSO_ParseSettings: unexpected end of file (names ";
	const char* token;

	/* get name list body body */
	token = Com_Parse(text);

	if (!*text || *token !='{') {
		Com_Printf("MSO_ParseOption: settingslist \"%s\" without body ignored\n", blockName);
		return -1;
	}

	int messageType = -1;
	linkedList_t* status = nullptr;

	do {
		/* get the msg type*/
		token = cgi->Com_EParse(text, errhead, blockName);
		if (!*text) {
			Com_Printf("MSO_ParseOption: end of file not expected \"%s\"\n", blockName);
			return -1;
		}
		if (token[0] == '}')
			break;

		if (Q_streq(token, "type")) {
			token = cgi->Com_EParse(text, errhead, blockName);
			messageType = MSO_ParseNotifyType(token);
		} else if (Q_streq(token, "status")) {
			if (status != nullptr) {
				Com_Printf("MSO_ParseOption: status already defined. Previous definition ignored.\n");
				cgi->LIST_Delete(&status);
			} else if (!cgi->Com_ParseList(text, &status)) {
				Com_Printf("MSO_ParseOption: error while parsing option status.\n");
				return -1;
			}
		} else {
			Com_Printf("MSO_ParseOption: token \"%s\" in \"%s\" not expected.\n", token, blockName);
			return -1;
		}
	} while (*text);

	if (messageType == -1) {
		Com_Printf("MSO_ParseOption: message option type undefined.\n");
		return -1;
	}

	for (linkedList_t* element = status; element != nullptr; element = element->next) {
		const char* value = (const char*)element->data;
		const int optionType = MSO_ParseOptionType(value);
		if (optionType == 0) {
			Com_Printf("MSO_ParseOption: message option type \"%s\" undefined.\n", value);
			continue;
		}
		MSO_Set(0, (notify_t)messageType, optionType, 1, false);
	}

	/* reset menu state, was updated by msgoptions_set */
	MSO_SetMenuState(MSO_MSTATE_REINIT, false, false);

	return messageType;
}

/**
 * @brief Parses a messagecategory script section. These categories are used to group notification types.
 * @sa MSO_InitTextList
 */
static bool MSO_ParseCategory (const char* blockName, const char** text)
{
	const char* errhead = "MSO_ParseCategory: unexpected end of file (names ";
	const char* token;
	msgCategory_t* category;
	msgCategoryEntry_t* categoryEntry;

	/* get name list body body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("MSO_ParseCategory: category without body\n");
		return false;
	}

	/* add category */
	if (ccs.numMsgCategories >= MAX_MESSAGECATEGORIES) {
		Com_Printf("MSO_ParseCategory: too many messagecategory defs\n");
		return false;
	}

	/* QUESTION this structure looks useless, categoryEntry is enough */
	category = &ccs.messageCategories[ccs.numMsgCategories];

	OBJZERO(*category);
	category->idx = ccs.numMsgCategories;	/* set self-link */

	categoryEntry = &ccs.msgCategoryEntries[ccs.numMsgCategoryEntries];

	/* first entry is category */
	OBJZERO(*categoryEntry);
	categoryEntry->category = &ccs.messageCategories[ccs.numMsgCategories];
	category->last = category->first = &ccs.msgCategoryEntries[ccs.numMsgCategoryEntries];
	categoryEntry->previous = nullptr;
	categoryEntry->next = nullptr;
	categoryEntry->isCategory = true;
	ccs.numMsgCategoryEntries++;

	do {
		/* get entries and add them to category */
		token = cgi->Com_EParse(text, errhead, blockName);
		if (!*text) {
			Com_Printf("MSO_ParseMessageSettings: end of file not expected\n");
			return false;
		}
		if (token[0] == '}')
			break;

		if (Q_streq(token, "option")) {
			int optionId = MSO_ParseOption(blockName, text);
			if (optionId == -1) {
				cgi->Com_Error(ERR_DROP, "MSO_ParseMessageSettings: error while parsing option from \"%s\".\n", blockName);
			}
			/* prepare a new msgcategory entry */
			msgCategoryEntry_t* previous = ccs.messageCategories[ccs.numMsgCategories].last;
			msgCategoryEntry_t* entry = &ccs.msgCategoryEntries[ccs.numMsgCategoryEntries];
			OBJZERO(*entry);
			ccs.messageCategories[ccs.numMsgCategories].last = entry;
			previous->next = entry;

			entry->category = &ccs.messageCategories[ccs.numMsgCategories];
			entry->previous = previous;
			entry->next = nullptr;
			entry->notifyType = nt_strings[optionId];
			entry->settings = &messageSettings[optionId];
			ccs.numMsgCategoryEntries++;
		} else if (Q_streq(token, "name")) {
			token = cgi->Com_EParse(text, errhead, blockName);
			if (!*text) {
				Com_Printf("MSO_ParseMessageSettings: end of file not expected\n");
				return false;
			}
			/* skip translation token */
			if (token[0] == '_') {
				token++;
			}
			category->name = cgi->PoolStrDup(token, cp_campaignPool, 0);
			categoryEntry->notifyType = category->name;
		} else {
			cgi->Com_Error(ERR_DROP, "MSO_ParseMessageSettings: token \"%s\" in \"%s\" not expected.\n", token, blockName);
		}
	} while (*text);

	if (category->name == nullptr) {
		Com_Printf("MSO_ParseMessageSettings: category do not have name\n");
		return false;
	}

	ccs.numMsgCategories++;
	MSO_SetMenuState(MSO_MSTATE_REINIT, false, false);
	return true;
}

/**
 * @brief parses message options settings from file.
 */
void MSO_ParseMessageSettings (const char* name, const char** text)
{
	const char* errhead = "MSO_ParseMessageSettings: unexpected end of file (names ";
	const char* token;

	/* settings available, reset previous settings */
	OBJZERO(messageSettings);

	/* get name list body body */
	token = Com_Parse(text);

	if (!*text || token[0] != '{') {
		cgi->Com_Error(ERR_DROP, "MSO_ParseMessageSettings: msgoptions \"%s\" without body.\n", name);
		return;
	}

	while (*text) {
		/* get entries and add them to category */
		token = cgi->Com_EParse(text, errhead, name);
		if (!*text)
			cgi->Com_Error(ERR_DROP, "MSO_ParseMessageSettings: end of file not expected \"%s\".\n", name);
		if (token[0] == '}')
			break;

		if (Q_streq(token, "category")) {
			if (!MSO_ParseCategory(name, text)) {
				cgi->Com_Error(ERR_DROP, "MSO_ParseMessageSettings: error while parsing category from \"%s\".\n", name);
			}
		} else {
			cgi->Com_Error(ERR_DROP, "MSO_ParseMessageSettings: token \"%s\" in \"%s\" not expected.\n", token, name);
		}
	}

}

static const cmdList_t msgOptionsCmds[] = {
	{"msgoptions_setall", MSO_SetAll_f, "Sets pause, notification or sound setting for all message categories"},
	{"msgoptions_set", MSO_Set_f, "Sets pause, notification or sound setting for a message category"},
	{nullptr, nullptr, nullptr}
};
void MSO_Init (void)
{
	cgi->Cmd_TableAddList(msgOptionsCmds);
	MSO_InitCallbacks();
}

void MSO_Shutdown (void)
{
	cgi->Cmd_TableRemoveList(msgOptionsCmds);
	MSO_ShutdownCallbacks();
}
