/**
 * @file cl_messageoptions.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../client.h"
#include "cl_messageoptions.h"
#include "cl_messageoptions_callbacks.h"
#include "cp_time.h"
#include "../../shared/parse.h"
#include "../mxml/mxml_ufoai.h"

/** @brief valid notification types that may cause pause / notice */
const char *nt_strings[NT_NUM_NOTIFYTYPE] = {
	N_("installation_installed"),
	N_("installation_removed"),
	N_("installation_replaced"),
	N_("installation_build_started"),
	N_("installation_build_finished"),
	N_("installation_destroyed"),
	N_("research_new_proposed"),
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
	N_("ufo_spotted"),
	N_("ufo_signal_lost"),
	N_("ufo_attacking"),

};
CASSERT(lengthof(nt_strings) == NT_NUM_NOTIFYTYPE);

messageSettings_t messageSettings[NT_NUM_NOTIFYTYPE]; /**< array holding actual message settings for every notify type */

/**
 * @brief Function tries to retrieve actual category entry for given category id.
 * @param categoryid id of category to search
 * @return category entry or @code NULL @endcode
 */
static msgCategory_t *MSO_GetCategoryFromName(const char* categoryid)
{
	msgCategory_t *categoryEntry = NULL;
	int idx;

	for (idx = 0; idx < ccs.numMsgCategories; idx++) {
		if (!Q_strcmp(ccs.messageCategories[idx].id, categoryid)) {
			categoryEntry = &ccs.messageCategories[idx];
			break;
		}
	}

	return categoryEntry;
}

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
		MN_ExecuteConfunc("ms_btnstate %i %i %i %i", listIndex, settings->doPause, settings->doNotify, settings->doSound);
	else
		/* ensure that message buttons will be initialized correctly if menu is shown next time */
		MSO_SetMenuState(MSO_MSTATE_PREPARED,qfalse, qfalse);
}

/**
 * @brief Function updates given category based on state.
 * @param categoryEntry category entry to update
 * @param state new folding state of category
 * @param callInit @code qtrue @endcode to trigger initialization of messageoptions text
 * @sa MSO_Init_f
 */
static void MSO_SetCategoryState (msgCategory_t *categoryEntry, const byte state, const qboolean callInit) {
	if ((state & MSGCATMASK_FOLDED) == MSGCATMASK_FOLDED)
		categoryEntry->isFolded = qtrue;
	else
		categoryEntry->isFolded = qfalse;

	MSO_SetMenuState(MSO_MSTATE_REINIT, callInit, qtrue);
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
			if (!Q_strcmp(nt_strings[type], messagetype))
				break;
		}
		if (type == NT_NUM_NOTIFYTYPE) {
			Sys_ConsoleOutput(va("Unrecognized messagetype during set '%s' ignored\n", messagetype));
			return;
		}

		if (!Q_strcmp(Cmd_Argv(2), "pause"))
			optionsType = MSO_PAUSE;
		else if (!Q_strcmp(Cmd_Argv(2), "notify"))
			optionsType = MSO_NOTIFY;
		else if (!Q_strcmp(Cmd_Argv(2), "sound"))
			optionsType = MSO_SOUND;
		else {
			Sys_ConsoleOutput(va("Unrecognized optionstype during set '%s' ignored\n", Cmd_Argv(2)));
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
		if (!Q_strcmp(Cmd_Argv(1), "pause"))
			optionsType = MSO_PAUSE;
		else if (!Q_strcmp(Cmd_Argv(1), "notify"))
			optionsType = MSO_NOTIFY;
		else if (!Q_strcmp(Cmd_Argv(1), "sound"))
			optionsType = MSO_SOUND;
		else {
			Sys_ConsoleOutput(va("Unrecognized optionstype during set '%s' ignored\n", Cmd_Argv(2)));
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
 * @brief Function callback that sets the current state of a messageoptions category.
 * @note Calling this function causes the messageoptions to be reinitialized.
 * @sa MSO_SetCategoryState
 */
static void MSO_SetCategoryState_f(void)
{
	if (Cmd_Argc() != 3)
		Com_Printf("Usage: %s <categoryid> <state>\n", Cmd_Argv(0));
	else {
		const char *categoryId = Cmd_Argv(1);
		const byte state = atoi(Cmd_Argv(2));
		msgCategory_t *categoryEntry = MSO_GetCategoryFromName(categoryId);

		if (!categoryEntry) {
			Sys_ConsoleOutput(va("Unrecognized categoryid during setCategoryState '%s' ignored\n", categoryId));
			return;
		}
		MSO_SetCategoryState(categoryEntry, state, qtrue);
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
message_t *MSO_CheckAddNewMessage (const notify_t messagecategory, const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia)
{
	message_t *result = NULL;

	if (messageSettings[messagecategory].doNotify)
		result = MS_AddNewMessageSound(title, text, popup, type, pedia, messageSettings[messagecategory].doSound);
	if (messageSettings[messagecategory].doPause)
		CL_GameTimeStop();
	return result;
}

/**
 * @brief saves current notification and pause settings
 * @sa MSO_LoadXML
 */
qboolean MSO_SaveXML (mxml_node_t *p)
{
	notify_t type;
	int idx;
	const int optionsCount = NT_NUM_NOTIFYTYPE;
	const int categoryCount = ccs.numMsgCategories;
	mxml_node_t *n = mxml_AddNode(p, "messageoptions");

	/* save amount of available entries (forward compatible for additional types) */
	mxml_AddInt(n, "optionscount", optionsCount);

	/* save positive values */
	for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
		messageSettings_t actualSetting = messageSettings[type];
		mxml_node_t *s = mxml_AddNode(n, "type");
		mxml_AddString(s, "name", nt_strings[type]);
		mxml_AddBool(s, "notify", actualSetting.doNotify);
		mxml_AddBool(s, "pause", actualSetting.doPause);
		mxml_AddBool(s, "sound", actualSetting.doSound);
	}

	mxml_AddInt(n, "categorycount",categoryCount);
	for (idx = 0; idx < categoryCount; idx++) {
		msgCategory_t actualCategory = ccs.messageCategories[idx];
		mxml_node_t *s = mxml_AddNode(n, "category");
		mxml_AddString(s, "name", actualCategory.id);
		mxml_AddBool(s, "folded", actualCategory.isFolded);
	}
	return qtrue;
}

/**
 * @brief saves current notification and pause settings
 * @sa MSO_Load
 */
qboolean MSO_Save (sizebuf_t* sb, void* data)
{
	notify_t type;
	int idx;
	const int optionsCount = NT_NUM_NOTIFYTYPE;
	const int categoryCount = ccs.numMsgCategories;

	/* save amount of available entries (forward compatible for additional types) */
	MSG_WriteLong(sb, optionsCount);

	/* save positive values */
	for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
		byte bitmask = 0;
		messageSettings_t actualSetting = messageSettings[type];
		if (actualSetting.doNotify)
			bitmask |= NTMASK_NOTIFY;
		if (actualSetting.doPause)
			bitmask |= NTMASK_PAUSE;
		if (actualSetting.doSound)
			bitmask |= NTMASK_SOUND;
		MSG_WriteString(sb, nt_strings[type]);
		MSG_WriteByte(sb, bitmask);
	}

	MSG_WriteLong(sb, categoryCount);
	for (idx = 0; idx < categoryCount; idx++) {
		byte bitmask = 0;
		msgCategory_t actualCategory = ccs.messageCategories[idx];
		if (actualCategory.isFolded)
			bitmask |= MSGCATMASK_FOLDED;
		MSG_WriteString(sb, actualCategory.id);
		MSG_WriteByte(sb, bitmask);
	}

	return qtrue;
}

/**
 * @brief Restores the notification and pause settings from savegame
 * @sa MSO_SaveXML
 */
qboolean MSO_LoadXML (mxml_node_t *p)
{
	int optionsCount;
	int categoryCount;
	mxml_node_t *n, *s;
	/* reset current message settings (default to set for undefined settings)*/
	memset(messageSettings, 1, sizeof(messageSettings));

	n = mxml_GetNode(p, "messageoptions");
	if (!n)
		return qfalse;

	/* load all msgoptions settings */
	optionsCount = mxml_GetInt(n, "optionscount", 0);
	if (optionsCount < 0) {
		Com_Printf("Can't load negative number of message settings, probably old savegame.\n");
		return qfalse;
	}

	for (s = mxml_GetNode(n, "type"); s; s = mxml_GetNextNode(s,n, "type")) {
		const char *messagetype = mxml_GetString(s, "name");
		notify_t type;

		for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
			if (Q_strcmp(nt_strings[type], messagetype) == 0)
				break;
		}

		/** @todo (menu) check why this message is not shown anywhere in logs*/
		if (type == NT_NUM_NOTIFYTYPE) {
			Com_Printf("Unrecognized messagetype '%s' ignored while loading\n", messagetype);
			continue;
		}
		MSO_Set(0, type, MSO_NOTIFY, mxml_GetBool(s, "notify", qfalse), qfalse);
		MSO_Set(0, type, MSO_PAUSE, mxml_GetBool(s, "pause", qfalse), qfalse);
		MSO_Set(0, type, MSO_SOUND, mxml_GetBool(s, "sound", qfalse), qfalse);
	}

	categoryCount = mxml_GetInt(n, "categorycount",0);
	if (categoryCount < 0) {
		Com_Printf("Can't load negative number of message category settings, probably old savegame.\n");
		return qfalse;
	}

	for (s = mxml_GetNode(n, "category"); s; s = mxml_GetNextNode(s,n, "category")) {
		const char *categoryId = mxml_GetString(s, "name");
		msgCategory_t *category = MSO_GetCategoryFromName(categoryId);
		if (!category) {
			Com_Printf("Unrecognized messagecategoryid '%s' ignored while loading\n", categoryId);
			continue;
		}
		MSO_SetCategoryState(category, mxml_GetBool(s, "folded", qfalse)?MSGCATMASK_FOLDED:0, qfalse);
	}

	MSO_SetMenuState(MSO_MSTATE_REINIT,qfalse, qfalse);
	return qtrue;
}

/**
 * @brief Restores the notification and pause settings from savegame
 * @sa MSO_Save
 */
qboolean MSO_Load (sizebuf_t* sb, void* data)
{
	int optionsCount;
	int categoryCount;

	/* reset current message settings (default to set for undefined settings)*/
	memset(messageSettings, 1, sizeof(messageSettings));

	/* load all msgoptions settings */
	optionsCount = MSG_ReadLong(sb);
	if (optionsCount < 0) {
		Com_Printf("Can't load negative number of message settings, probably old savegame.\n");
		return qfalse;
	}

	for (; optionsCount > 0; optionsCount--) {
		const char *messagetype = MSG_ReadString(sb);
		const byte pauseOrNotify = MSG_ReadByte(sb);
		notify_t type;

		for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
			if (Q_strcmp(nt_strings[type], messagetype) == 0)
				break;
		}
		/** @todo (menu) check why this message is not shown anywhere in logs*/
		if (type == NT_NUM_NOTIFYTYPE) {
			Com_Printf("Unrecognized messagetype '%s' ignored while loading\n", messagetype);
			continue;
		}
		MSO_Set(0, type, MSO_NOTIFY, ((pauseOrNotify & NTMASK_NOTIFY) == NTMASK_NOTIFY), qfalse);
		MSO_Set(0, type, MSO_PAUSE, ((pauseOrNotify & NTMASK_PAUSE) == NTMASK_PAUSE), qfalse);
		MSO_Set(0, type, MSO_SOUND, ((pauseOrNotify & NTMASK_SOUND) == NTMASK_SOUND), qfalse);
	}

	categoryCount = MSG_ReadLong(sb);
	if (categoryCount < 0) {
		Com_Printf("Can't load negative number of message category settings, probably old savegame.\n");
		return qfalse;
	}

	for (; categoryCount > 0; categoryCount--) {
		const char *categoryId = MSG_ReadString(sb);
		const byte categoryState = MSG_ReadByte(sb);
		msgCategory_t *category = MSO_GetCategoryFromName(categoryId);
		if (!category) {
			Com_Printf("Unrecognized messagecategoryid '%s' ignored while loading\n", categoryId);
			continue;
		}
		MSO_SetCategoryState(category, categoryState, qfalse);
	}

	MSO_SetMenuState(MSO_MSTATE_REINIT,qfalse, qfalse);
	return qtrue;
}

/**
 * @brief parses message options settings from file.
 * @param name
 * @param text
 */
void MSO_ParseSettings (const char *name, const char **text)
{
	const char *errhead = "MSO_ParseSettings: unexpected end of file (names ";
	const char *token, *tmpCommand, *msgtype;

	/* get name list body body */
	token = COM_Parse(text);

	if (!*text || *token !='{') {
		Com_Printf("MSO_ParseSettings: settingslist \"%s\" without body ignored\n", name);
		return;
	}
	/* settings available, reset previous settings */
	memset(messageSettings, 0, sizeof(messageSettings));

	do {
		/* get the msg type*/
		msgtype = COM_EParse(text, errhead, name);
		if (text[0] == '\0')
			break;
		if (msgtype[0] == '}')
			break;
		/* temporarly store type */
		tmpCommand = va("%s", msgtype);
		/* build command from msgtype, settingstype (pause|notify|sound) */
		token = va("msgoptions_set %s %s 1", tmpCommand, COM_EParse(text, errhead, name));
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
	token = COM_Parse(text);

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

	memset(category, 0, sizeof(*category));
	category->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
	category->idx = ccs.numMsgCategories;	/* set self-link */
	category->isFolded = qtrue;

	entry = &ccs.msgCategoryEntries[ccs.numMsgCategoryEntries];

	/* first entry is category */
	memset(entry, 0, sizeof(*entry));
	entry->category = &ccs.messageCategories[ccs.numMsgCategories];
	category->last = category->first = &ccs.msgCategoryEntries[ccs.numMsgCategoryEntries];
	entry->previous = NULL;
	entry->next = NULL;
	entry->isCategory = qtrue;
	entry->notifyType = category->id;

	ccs.numMsgCategoryEntries++;

	do {
		/* get entries and add them to category */
		token = COM_EParse(text, errhead, name);
		if (text[0] == '\0')
			break;
		if (token[0] == '}')
			break;

		if (token[0] != '\0') {
			for (idx = 0; idx < NT_NUM_NOTIFYTYPE; idx ++) {
				if (!Q_strncmp(token, nt_strings[idx],MAX_VAR)) {
					/* prepare a new msgcategory entry */
					msgCategoryEntry_t *old = ccs.messageCategories[ccs.numMsgCategories].last;

					memset(&ccs.msgCategoryEntries[ccs.numMsgCategoryEntries], 0, sizeof(ccs.msgCategoryEntries[ccs.numMsgCategoryEntries]));
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
	Cmd_AddCommand("msgoptions_setcat", MSO_SetCategoryState_f, "Sets the new state for a category");
	MSO_InitCallbacks();
}
