/**
 * @file cp_messageoptions.c
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "../../ui/ui_main.h"
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
	N_("base_attack")
};
CASSERT(lengthof(nt_strings) == NT_NUM_NOTIFYTYPE);

messageSettings_t messageSettings[NT_NUM_NOTIFYTYPE]; /**< array holding actual message settings for every notify type */

/**
 * @brief Function updates pause or notification settings.
 * @param listIndex listIndex in menu to update via confunc
 * @param type notification type to update
 * @param optionType option type that should be updated
 * @param activate flag indicating whether setting should be activated (qtrue) or deactivated
 * @param sendCommands flag indicating whether confunc command to update menu button should be sent
 * @sa MSO_Toggle_f
 * @sa MSO_Set_f
 * @note if sendCommands is qfalse, initialization of buttons is reactivated for next menu displaying
 */
void MSO_Set (const int listIndex, const notify_t type, const int optionType, const qboolean activate, const qboolean sendCommands)
{
	messageSettings_t *settings = &messageSettings[type];

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
		UI_ExecuteConfunc("ms_btnstate %i %i %i %i", listIndex, settings->doPause, settings->doNotify, settings->doSound);
	else
		/* ensure that message buttons will be initialized correctly if menu is shown next time */
		MSO_SetMenuState(MSO_MSTATE_PREPARED,qfalse, qfalse);
}

/**
 * @brief Function callback used to initialize values for messageoptions and for manual setting changes.
 * @sa MSO_Set
 */
static void MSO_Set_f (void)
{
	if (Cmd_Argc() != 4)
		Com_Printf("Usage: %s <messagetypename> <pause|notify|sound> <0|1>\n", Cmd_Argv(0));
	else {
		notify_t type;
		int optionsType;
		const char *messagetype = Cmd_Argv(1);

		for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
			if (Q_streq(nt_strings[type], messagetype))
				break;
		}
		if (type == NT_NUM_NOTIFYTYPE) {
			Com_Printf("Unrecognized messagetype during set '%s' ignored\n", messagetype);
			return;
		}

		if (Q_streq(Cmd_Argv(2), "pause"))
			optionsType = MSO_PAUSE;
		else if (Q_streq(Cmd_Argv(2), "notify"))
			optionsType = MSO_NOTIFY;
		else if (Q_streq(Cmd_Argv(2), "sound"))
			optionsType = MSO_SOUND;
		else {
			Com_Printf("Unrecognized optionstype during set '%s' ignored\n", Cmd_Argv(2));
			return;
		}
		MSO_Set(0, type, optionsType, atoi(Cmd_Argv(3)), qfalse);
	}
}

/**
 * @brief Function callback that sets all message options settings for one option type to given value.
 * @sa MSO_Set
 * @sa MSO_Init_f
 */
static void MSO_SetAll_f(void)
{
	if (Cmd_Argc() != 3)
		Com_Printf("Usage: %s <pause|notify|sound> <0|1>\n", Cmd_Argv(0));
	else {
		int optionsType;
		notify_t type;
		qboolean activate = atoi(Cmd_Argv(2));
		if (Q_streq(Cmd_Argv(1), "pause"))
			optionsType = MSO_PAUSE;
		else if (Q_streq(Cmd_Argv(1), "notify"))
			optionsType = MSO_NOTIFY;
		else if (Q_streq(Cmd_Argv(1), "sound"))
			optionsType = MSO_SOUND;
		else {
			Com_Printf("Unrecognized optionstype during set '%s' ignored\n", Cmd_Argv(2));
			return;
		}
		/* update settings for chosen type */
		for (type = 0; type < NT_NUM_NOTIFYTYPE; type ++) {
			MSO_Set(0, type, optionsType, activate, qfalse);
		}
		/* reinit menu */
		MSO_SetMenuState(MSO_MSTATE_PREPARED,qtrue,qtrue);
	}
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
message_t *MSO_CheckAddNewMessage (const notify_t messagecategory, const char *title, const char *text, qboolean popup, messageType_t type, technology_t *pedia)
{
	message_t *result = NULL;
	const messageSettings_t *settings = &messageSettings[messagecategory];

	if (settings->doNotify)
		result = MS_AddNewMessageSound(title, text, popup, type, pedia, settings->doSound);
	if (settings->doPause)
		CP_GameTimeStop();
	return result;
}

/**
 * @brief saves current notification and pause settings
 * @sa MSO_LoadXML
 */
qboolean MSO_SaveXML (xmlNode_t *p)
{
	notify_t type;
	xmlNode_t *n = XML_AddNode(p, SAVE_MESSAGEOPTIONS_MESSAGEOPTIONS);

	/* save positive values */
	for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
		messageSettings_t actualSetting = messageSettings[type];
		xmlNode_t *s = XML_AddNode(n, SAVE_MESSAGEOPTIONS_TYPE);

		XML_AddString(s, SAVE_MESSAGEOPTIONS_NAME, nt_strings[type]);
		XML_AddBoolValue(s, SAVE_MESSAGEOPTIONS_NOTIFY, actualSetting.doNotify);
		XML_AddBoolValue(s, SAVE_MESSAGEOPTIONS_PAUSE, actualSetting.doPause);
		XML_AddBoolValue(s, SAVE_MESSAGEOPTIONS_SOUND, actualSetting.doSound);
	}

	return qtrue;
}

/**
 * @brief Restores the notification and pause settings from savegame
 * @sa MSO_SaveXML
 */
qboolean MSO_LoadXML (xmlNode_t *p)
{
	xmlNode_t *n, *s;
	/* reset current message settings (default to set for undefined settings)*/
	OBJSET(messageSettings, 1);

	n = XML_GetNode(p, SAVE_MESSAGEOPTIONS_MESSAGEOPTIONS);
	if (!n)
		return qfalse;

	for (s = XML_GetNode(n, SAVE_MESSAGEOPTIONS_TYPE); s; s = XML_GetNextNode(s,n, SAVE_MESSAGEOPTIONS_TYPE)) {
		const char *messagetype = XML_GetString(s, SAVE_MESSAGEOPTIONS_NAME);
		notify_t type;

		for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
			if (Q_streq(nt_strings[type], messagetype))
				break;
		}

		/** @todo (menu) check why this message is not shown anywhere in logs*/
		if (type == NT_NUM_NOTIFYTYPE) {
			Com_Printf("Unrecognized messagetype '%s' ignored while loading\n", messagetype);
			continue;
		}
		MSO_Set(0, type, MSO_NOTIFY, XML_GetBool(s, SAVE_MESSAGEOPTIONS_NOTIFY, qfalse), qfalse);
		MSO_Set(0, type, MSO_PAUSE, XML_GetBool(s, SAVE_MESSAGEOPTIONS_PAUSE, qfalse), qfalse);
		MSO_Set(0, type, MSO_SOUND, XML_GetBool(s, SAVE_MESSAGEOPTIONS_SOUND, qfalse), qfalse);
	}

	MSO_SetMenuState(MSO_MSTATE_REINIT,qfalse, qfalse);
	return qtrue;
}

/**
 * @brief parses message options settings from file.
 */
void MSO_ParseSettings (const char *name, const char **text)
{
	const char *errhead = "MSO_ParseSettings: unexpected end of file (names ";
	const char *token, *tmpCommand, *msgtype;

	/* get name list body body */
	token = Com_Parse(text);

	if (!*text || *token !='{') {
		Com_Printf("MSO_ParseSettings: settingslist \"%s\" without body ignored\n", name);
		return;
	}
	/* settings available, reset previous settings */
	OBJZERO(messageSettings);

	do {
		/* get the msg type*/
		msgtype = Com_EParse(text, errhead, name);
		if (text[0] == '\0')
			break;
		if (msgtype[0] == '}')
			break;
		/* temporarly store type */
		tmpCommand = va("%s", msgtype);
		/* build command from msgtype, settingstype (pause|notify|sound) */
		token = va("msgoptions_set %s %s 1", tmpCommand, Com_EParse(text, errhead, name));
		Cmd_ExecuteString(token);
	} while (*text);
	/* reset menu state, was updated by msgoptions_set */
	MSO_SetMenuState(MSO_MSTATE_REINIT,qfalse,qfalse);
}

/**
 * @brief Parses a messagecategory script section. These categories are used to group notification types.
 * @param name
 * @param text
 * @sa MSO_InitTextList
 */
void MSO_ParseCategories (const char *name, const char **text)
{
	const char *errhead = "MSO_ParseCategories: unexpected end of file (names ";
	const char *token;
	int idx;
	msgCategory_t *category;
	msgCategoryEntry_t *entry;

	name++;

	/* get name list body body */
	token = Com_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("MSO_ParseCategories: category \"%s\" without body ignored\n", name);
		return;
	}

	/* add category */
	if (ccs.numMsgCategories >= MAX_MESSAGECATEGORIES) {
		Com_Printf("MSO_ParseCategories: too many messagecategory defs\n");
		return;
	}

	category = &ccs.messageCategories[ccs.numMsgCategories];

	OBJZERO(*category);
	category->id = Mem_PoolStrDup(name, cp_campaignPool, 0);
	category->idx = ccs.numMsgCategories;	/* set self-link */

	entry = &ccs.msgCategoryEntries[ccs.numMsgCategoryEntries];

	/* first entry is category */
	OBJZERO(*entry);
	entry->category = &ccs.messageCategories[ccs.numMsgCategories];
	category->last = category->first = &ccs.msgCategoryEntries[ccs.numMsgCategoryEntries];
	entry->previous = NULL;
	entry->next = NULL;
	entry->isCategory = qtrue;
	entry->notifyType = category->id;

	ccs.numMsgCategoryEntries++;

	do {
		/* get entries and add them to category */
		token = Com_EParse(text, errhead, name);
		if (text[0] == '\0')
			break;
		if (token[0] == '}')
			break;

		if (token[0] != '\0') {
			for (idx = 0; idx < NT_NUM_NOTIFYTYPE; idx ++) {
				if (Q_streq(token, nt_strings[idx])) {
					/* prepare a new msgcategory entry */
					msgCategoryEntry_t *old = ccs.messageCategories[ccs.numMsgCategories].last;

					OBJZERO(ccs.msgCategoryEntries[ccs.numMsgCategoryEntries]);
					ccs.msgCategoryEntries[ccs.numMsgCategoryEntries].category = &ccs.messageCategories[ccs.numMsgCategories];

					ccs.messageCategories[ccs.numMsgCategories].last = &ccs.msgCategoryEntries[ccs.numMsgCategoryEntries];
					old->next = &ccs.msgCategoryEntries[ccs.numMsgCategoryEntries];
					ccs.msgCategoryEntries[ccs.numMsgCategoryEntries].previous = old;
					ccs.msgCategoryEntries[ccs.numMsgCategoryEntries].next = NULL;
					ccs.msgCategoryEntries[ccs.numMsgCategoryEntries].notifyType = nt_strings[idx];
					ccs.msgCategoryEntries[ccs.numMsgCategoryEntries].settings = &messageSettings[idx];
					ccs.numMsgCategoryEntries++;
					break;
				}
			}
		}
	} while (*text);

	ccs.numMsgCategories++;
	MSO_SetMenuState(MSO_MSTATE_REINIT,qfalse,qfalse);
}

void MSO_Init (void)
{
	Cmd_AddCommand("msgoptions_setall", MSO_SetAll_f, "Sets pause, notification or sound setting for all message categories");
	Cmd_AddCommand("msgoptions_set", MSO_Set_f, "Sets pause, notification or sound setting for a message category");
	MSO_InitCallbacks();
}

void MSO_Shutdown (void)
{
	Cmd_RemoveCommand("msgoptions_setall");
	Cmd_RemoveCommand("msgoptions_set");
	MSO_ShutdownCallbacks();
}
