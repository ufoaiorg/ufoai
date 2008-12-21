/**
 * @file m_messages.c
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
#include "../cl_global.h"
#include "m_main.h"
#include "m_messages.h"
#include "m_popup.h"

/** @brief valid notification types that may cause pause / notice */
static const char *nt_strings[NT_NUM_NOTIFYTYPE] = {
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
messageSettings_t backupMessageSettings[NT_NUM_NOTIFYTYPE]; /**< array holding backup message settings (used for restore function in options popup) */
int messageList_scroll = 0; /**< actual messageSettings list begin index due to scrolling */
int visibleMSOEntries = 0; /**< actual visible entry count */
qboolean messageOptionsInitialized = qfalse; /**< flag indicating whether message options menu is initialized @sa MSO_Init_f */
qboolean messageOptionsPrepared = qfalse; /**< flag indicating whether parsed category data is prepared @sa MSO_ParseCategories */
static menuNode_t *msoTextNode; /**< reference to text node for easier references */

/**
 * @brief Script command to show all messages on the stack
 */
static void CL_ShowMessagesOnStack_f (void)
{
	message_t *m = mn.messageStack;

	while (m) {
		Com_Printf("%s: %s\n", m->title, m->text);
		m = m->next;
	}
}

#ifdef DEBUG
/**
 * @brief Script command to delete all messages
 */
static void CL_DeleteMessages_f (void)
{
	message_t *m = mn.messageStack;
	message_t *mtmp;

	while (m) {
		mtmp = m->next;
		Mem_Free(m);
		m = mtmp;
	}
	mn.messageStack = NULL;
}
#endif

/**
 * @brief Returns formatted text of a message timestamp
 * @param[in] text Buffer to hold the final result
 * @param[in] message The message to convert into text
 */
static void MN_TimestampedText (char *text, message_t *message, size_t textsize)
{
	dateLong_t date;
	CL_DateConvertLong(&message->date, &date);
	Com_sprintf(text, textsize, _("%i %s %02i, %02i:%02i: "), date.year,
		CL_DateGetMonthName(date.month - 1), date.day, date.hour, date.min);
}

/**
 * @brief Adds a new message to message stack
 * @note These are the messages that are displayed at geoscape
 * @param[in] title Already translated message/mail title
 * @param[in] text Already translated message/mail body
 * @param[in] popup Show this as a popup, too?
 * @param[in] type The message type
 * @param[in] pedia Pointer to technology (only if needed)
 * @return message_t pointer
 * @sa UP_OpenMail_f
 * @sa CL_EventAddMail_f
 * @note this method forwards to @c MN_AddNewMessageSound with @code playSound = qtrue @endcode
 */
message_t *MN_AddNewMessage (const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia)
{
	return MN_AddNewMessageSound(title, text, popup, type, pedia, qtrue);
}

/**
 * @brief Adds a new message to message stack
 * @note These are the messages that are displayed at geoscape
 * @param[in] title Already translated message/mail title
 * @param[in] text Already translated message/mail body
 * @param[in] popup Show this as a popup, too?
 * @param[in] type The message type
 * @param[in] pedia Pointer to technology (only if needed)
 * @param[in] playSound Play notification sound?
 * @return message_t pointer
 * @sa UP_OpenMail_f
 * @sa CL_EventAddMail_f
 */
message_t *MN_AddNewMessageSound (const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia, qboolean playSound)
{
	message_t *mess;
	const char *sound = NULL;

	assert(type < MSG_MAX);

	/* allocate memory for new message - delete this with every new game */
	mess = (message_t *) Mem_PoolAlloc(sizeof(message_t), cl_localPool, CL_TAG_NONE);

	/* push the new message at the beginning of the stack */
	mess->next = mn.messageStack;
	mn.messageStack = mess;

	mess->type = type;
	mess->pedia = pedia;		/* pointer to UFOpaedia entry */

	mess->date = ccs.date;

	/** @todo (menu) Handle translated text - don't use single byte string copy here */
	Q_strncpyz(mess->title, title, sizeof(mess->title));
	mess->text = Mem_PoolStrDup(text, cl_localPool, CL_TAG_NONE);

	/* get formatted date text */
	MN_TimestampedText(mess->timestamp, mess, sizeof(mess->timestamp));

	/* they need to be translated already */
	if (popup)
		MN_Popup(mess->title, mess->text);

	switch (type) {
	case MSG_DEBUG:
		break;
	case MSG_STANDARD:
		sound = "geoscape/standard";
		break;
	case MSG_INFO:
	case MSG_TRANSFERFINISHED:
	case MSG_DEATH:
	case MSG_CONSTRUCTION:
	case MSG_PRODUCTION:
		sound = "geoscape/info";
		break;
	case MSG_RESEARCH_PROPOSAL:
	case MSG_RESEARCH_FINISHED:
		assert(pedia);
	case MSG_EVENT:
	case MSG_NEWS:
		/* reread the new mails in UP_GetUnreadMails */
		gd.numUnreadMails = -1;
		sound = "geoscape/mail";
		break;
	case MSG_UFOSPOTTED:
		sound = "geoscape/ufospotted";
		break;
	case MSG_BASEATTACK:
		sound = "geoscape/attack";
		break;
	case MSG_TERRORSITE:
	case MSG_CRASHSITE:
		sound = "geoscape/newmission";
		break;
	case MSG_PROMOTION:
		sound = "geoscape/promotion";
		break;
	case MSG_MAX:
		break;
	}

	if (playSound)
		S_StartLocalSound(sound);

	return mess;
}

void MN_RemoveMessage (const char *title)
{
	message_t *m = mn.messageStack;
	message_t *prev = NULL;

	while (m) {
		if (!Q_strncmp(m->title, title, MAX_VAR)) {
			if (prev)
				prev->next = m->next;
			Mem_Free(m->text);
			Mem_Free(m);
			return;
		}
		prev = m;
		m = m->next;
	}
	Com_Printf("Could not remove message from stack - %s was not found\n", title);
}

/**< @brief buffer that hold all printed chat messages for menu display */
static char *chatBuffer = NULL;
static menuNode_t* chatBufferNode = NULL;

/**
 * @brief Displays a chat on the hud and add it to the chat buffer
 */
void MN_AddChatMessage (const char *text)
{
	/* allocate memory for new chat message */
	chatMessage_t *chat = (chatMessage_t *) Mem_PoolAlloc(sizeof(chatMessage_t), cl_genericPool, CL_TAG_NONE);

	/* push the new chat message at the beginning of the stack */
	chat->next = mn.chatMessageStack;
	mn.chatMessageStack = chat;
	chat->text = Mem_PoolStrDup(text, cl_genericPool, CL_TAG_NONE);

	if (!chatBuffer) {
		chatBuffer = (char*)Mem_PoolAlloc(sizeof(char) * MAX_MESSAGE_TEXT, cl_genericPool, CL_TAG_NONE);
		if (!chatBuffer) {
			Com_Printf("Could not allocate chat buffer\n");
			return;
		}
		/* only link this once */
		mn.menuText[TEXT_CHAT_WINDOW] = chatBuffer;
	}
	if (!chatBufferNode) {
		const menu_t* menu = MN_GetMenu(mn_hud->string);
		if (!menu)
			Sys_Error("Could not get hud menu: %s\n", mn_hud->string);
		chatBufferNode = MN_GetNode(menu, "chatscreen");
	}

	*chatBuffer = '\0'; /* clear buffer */
	do {
		if (strlen(chatBuffer) + strlen(chat->text) >= MAX_MESSAGE_TEXT)
			break;
		Q_strcat(chatBuffer, chat->text, MAX_MESSAGE_TEXT); /* fill buffer */
		chat = chat->next;
	} while (chat);

	/* maybe the hud doesn't have a chatscreen node - or we don't have a hud */
	if (chatBufferNode) {
		Cmd_ExecuteString("unhide_chatscreen");
		chatBufferNode->menu->eventTime = cls.realtime;
	}
}

/**
 * @brief Script command to show all chat messages on the stack
 */
static void CL_ShowChatMessagesOnStack_f (void)
{
	chatMessage_t *m = mn.chatMessageStack;

	while (m) {
		Com_Printf("%s", m->text);
		m = m->next;
	}
}

/**
 * @brief Saved the complete message stack
 * @sa SAV_GameSave
 * @sa MN_AddNewMessage
 */
static void MS_MessageSave (sizebuf_t * sb, message_t * message)
{
	int idx = -1;

	if (!message)
		return;
	/* bottom up */
	MS_MessageSave(sb, message->next);

	/* don't save these message types */
	if (message->type == MSG_INFO)
		return;

	if (message->pedia)
		idx = message->pedia->idx;

	Com_DPrintf(DEBUG_CLIENT, "MS_MessageSave: Save '%s' - '%s'; type = %i; idx = %i\n", message->title, message->text, message->type, idx);
	MSG_WriteString(sb, message->title);
	MSG_WriteString(sb, message->text);
	MSG_WriteByte(sb, message->type);
	/* store script id of event mail */
	if (message->type == MSG_EVENT) {
		MSG_WriteString(sb, message->eventMail->id);
		MSG_WriteByte(sb, message->eventMail->read);
	}
	MSG_WriteLong(sb, idx);
	MSG_WriteLong(sb, message->date.day);
	MSG_WriteLong(sb, message->date.sec);
}

/**
 * @sa MS_Load
 * @sa MN_AddNewMessage
 * @sa MS_MessageSave
 */
qboolean MS_Save (sizebuf_t* sb, void* data)
{
	int i = 0;
	message_t* message;

	/* store message system items */
	for (message = mn.messageStack; message; message = message->next) {
		if (message->type == MSG_INFO)
			continue;
		i++;
	}
	MSG_WriteLong(sb, i);
	MS_MessageSave(sb, mn.messageStack);
	return qtrue;
}

/**
 * @sa MS_Save
 * @sa MN_AddNewMessageSound
 */
qboolean MS_Load (sizebuf_t* sb, void* data)
{
	int i, mtype, idx;
	char title[MAX_VAR], text[MAX_MESSAGE_TEXT];
	message_t *mess;
	eventMail_t *mail;
	qboolean read;

	/* how many message items */
	i = MSG_ReadLong(sb);
	for (; i--;) {
		mail = NULL;
		/* can contain high bits due to utf8 */
		Q_strncpyz(title, MSG_ReadStringRaw(sb), sizeof(title));
		Q_strncpyz(text, MSG_ReadStringRaw(sb), sizeof(text));
		mtype = MSG_ReadByte(sb);
		if (mtype == MSG_EVENT) {
			mail = CL_GetEventMail(MSG_ReadString(sb), qfalse);
			read = MSG_ReadByte(sb);
			if (mail)
				mail->read = read;
		} else
			mail = NULL;
		idx = MSG_ReadLong(sb);
		/* event and not mail means, dynamic mail - we don't save or load them */
		if ((mtype == MSG_EVENT && !mail) || (mtype == MSG_DEBUG && developer->integer != 1)) {
			MSG_ReadLong(sb);
			MSG_ReadLong(sb);
		} else {
			mess = MN_AddNewMessageSound(title, text, qfalse, mtype, RS_GetTechByIDX(idx), qfalse);
			mess->eventMail = mail;
			mess->date.day = MSG_ReadLong(sb);
			mess->date.sec = MSG_ReadLong(sb);
			/* redo timestamp text after setting date */
			MN_TimestampedText(mess->timestamp, mess, sizeof(mess->timestamp));
		}
	}
	return qtrue;
}

/**
 * @brief Returns the category entry that is shown at given selection index.
 * @param selection index in visible list as returned by text list
 * @param visibleIndex flag indicating that index is from visible lines, not from text row
 * @return category entry from gd.msgCategoryEntries
 * @note this method takes into account scroll index and folding state of categories.
 * @sa MSO_Toggle_f
 * @sa MSO_OptionsClick_f
 */
static const msgCategoryEntry_t *MSO_GetEntryFromSelectionIndex (const int selection, const qboolean visibleIndex)
{
	int entriesToCheck = visibleIndex ? selection + messageList_scroll : selection;
	int realIndex = 0;
	for (; entriesToCheck > 0; realIndex++) {
		const msgCategoryEntry_t *entry = &gd.msgCategoryEntries[realIndex];
		entriesToCheck--;
		if (entry->isCategory && entry->category->isFolded) {
			/* first entry of category is category itself, count other entries */
			const msgCategoryEntry_t *invisibleEntry = entry->next;
			while (invisibleEntry) {
				realIndex++;
				invisibleEntry = invisibleEntry->next;
			}
		}
	}
	if (realIndex >= gd.numMsgCategoryEntries)
		return NULL;
	return &gd.msgCategoryEntries[realIndex];
}

/**
 * @brief Function tries to retrieve actual category entry for given category id.
 * @param categoryid id of category to search
 * @return category entry or @code NULL @endcode
 */
static msgCategory_t *MSO_GetCategoryFromName(const char* categoryid)
{
	msgCategory_t *categoryEntry = NULL;
	int idx;

	for (idx = 0; idx < gd.numMsgCategories; idx++) {
		if (!Q_strcmp(gd.messageCategories[idx].id, categoryid)) {
			categoryEntry = &gd.messageCategories[idx];
			break;
		}
	}

	return categoryEntry;
}

/**
 * @brief Initializes menu texts for scrollable area
 * @sa MSO_Init_f
 */
static void MSO_InitTextList (void)
{
	char lineprefix[64], categoryLine[128];
	int idx;
	const int oldVisibleEntries = visibleMSOEntries;

	linkedList_t *messageSettingsList = NULL;

	MN_MenuTextReset(TEXT_MESSAGEOPTIONS);
	visibleMSOEntries = 0;

	for (idx = 0; idx < gd.numMsgCategoryEntries; idx++) {
		const msgCategoryEntry_t *entry = &gd.msgCategoryEntries[idx];
		if (!entry->isCategory && entry->category->isFolded)
			continue;
		if (entry->isCategory) {
			/** @todo replace placeholder images with correct ones, perhaps fix indent for non-categories */
			Com_sprintf(lineprefix, sizeof(lineprefix), TEXT_IMAGETAG"icons/ufopedia_%s", entry->category->isFolded ? "aliens" : "artifacts");
		} else
			Com_sprintf(lineprefix, sizeof(lineprefix), "   ");
		Com_sprintf(categoryLine, sizeof(categoryLine), "%s %s", lineprefix, _(entry->notifyType));
		LIST_AddString(&messageSettingsList, categoryLine);
		visibleMSOEntries++;
	}
	mn.menuTextLinkedList[TEXT_MESSAGEOPTIONS] = messageSettingsList;
	messageOptionsInitialized = qfalse;
	if (oldVisibleEntries > visibleMSOEntries && messageList_scroll > visibleMSOEntries - msoTextNode->u.text.rows) {
		messageList_scroll = visibleMSOEntries - msoTextNode->u.text.rows;
		if (messageList_scroll < 0)
			messageList_scroll = 0;
		msoTextNode->u.text.textScroll = messageList_scroll;
	}
}

/**
 * @brief Executes confuncs to update visible message options lines.
 * @sa MSO_Init_f
 */
static void MSO_UpdateVisibleButtons (void)
{
	int idx;
	int visible = 0;/* visible lines*/

	/* update visible button lines based on current displayed values */
	for (idx = 0; visible < msoTextNode->u.text.rows && idx < gd.numMsgCategoryEntries; idx++) {
		const msgCategoryEntry_t *entry = MSO_GetEntryFromSelectionIndex(idx,qtrue);
		if (!entry)
			break;
		if (entry->isCategory) {
			/* category is visible anyway*/
			MN_ExecuteConfunc("ms_disable %i", visible);
			visible++;
		} else {
			assert(entry->category);
			if (!entry->category->isFolded) {
				MN_ExecuteConfunc("ms_enable %i", visible);
				MN_ExecuteConfunc("ms_pause%s %i", entry->settings->doPause ? "e" : "d", visible);
				MN_ExecuteConfunc("ms_notify%s %i", entry->settings->doNotify ? "e" : "d", visible);
				MN_ExecuteConfunc("ms_sound%s %i", entry->settings->doSound ? "e" : "d", visible);
				visible++;
			}
		}
	}

	for (; visible < msoTextNode->u.text.rows && idx < lengthof(gd.msgCategoryEntries); idx++) {
		MN_ExecuteConfunc("ms_disable %i", visible);
		visible++;
	}
}

/**
 * @brief initializes message options menu by showing as much button lines as needed.
 * @note This must only be done once as long as settings are changed in menu, so
 * messageOptionsInitialized is checked whether this is done yet. This function will be
 * reenabled if settings are changed via MSO_Set_f. If text list is not initialized
 * after parsing, MSO_InitTextList will be called first.
 */
static void MSO_Init_f (void)
{
	if (!msoTextNode)
		msoTextNode = MN_GetNodeFromCurrentMenu("messagetypes");
	if (!messageOptionsPrepared) {
		MSO_InitTextList();
		messageOptionsPrepared = qtrue;
	}

	if (!messageOptionsInitialized) {
		MSO_UpdateVisibleButtons();
		messageOptionsInitialized = qtrue;
	}
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
static void MSO_Set (const int listIndex, const notify_t type, const mso_t optionType, const qboolean activate, const qboolean sendCommands)
{
	const char* option;

	if (optionType == MSO_PAUSE) {
		messageSettings[type].doPause = activate;
		option = "pause";
	} else if (optionType == MSO_NOTIFY) {
		messageSettings[type].doNotify = activate;
		option = "notify";
	} else {
		messageSettings[type].doSound = activate;
		option = "sound";
	}
	if (sendCommands)
		MN_ExecuteConfunc("ms_%s%s %i", option, activate ? "e" : "d", listIndex);
	else
		/* ensure that message buttons will be initialized correctly if menu is shown next time */
		messageOptionsInitialized = qfalse;
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

	messageOptionsPrepared = qfalse;
	if (callInit)
		MSO_Init_f();
}

/**
 * @brief Function for menu buttons to update message settings.
 * @sa MSO_Set
 */
static void MSO_Toggle_f (void)
{
	if (Cmd_Argc() != 3)
		Com_Printf("Usage: %s <listId> <pause|notify|sound>\n", Cmd_Argv(0));
	else {
		const int listIndex = atoi(Cmd_Argv(1));
		const msgCategoryEntry_t *selectedEntry = MSO_GetEntryFromSelectionIndex(listIndex,qtrue);
		mso_t optionType;
		qboolean activate;
		notify_t type;

		if (!selectedEntry)
			return;
		if (selectedEntry->isCategory) {
			Com_Printf("Toggle command with selected category entry ignored.\n");
			return;
		}
		for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
			if (!Q_strcmp(nt_strings[type], selectedEntry->notifyType))
				break;
		}
		if (type == NT_NUM_NOTIFYTYPE) {
			Com_Printf("Unrecognized messagetype during toggle '%s' ignored\n", selectedEntry->notifyType);
			return;
		}

		if (!Q_strcmp(Cmd_Argv(2), "pause")) {
			optionType = MSO_PAUSE;
			activate = !selectedEntry->settings->doPause;
		} else if (!Q_strcmp(Cmd_Argv(2), "notify")) {
			optionType = MSO_NOTIFY;
			activate = !selectedEntry->settings->doNotify;
		} else {
			optionType = MSO_SOUND;
			activate = !selectedEntry->settings->doSound;
		}
		MSO_Set(listIndex, type, optionType, activate, qtrue);
	}
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
		mso_t optionsType;
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
 * @brief Function callback that sets all message options settings for one mso_t to given value.
 * @sa MSO_Set
 * @sa MSO_Init_f
 */
static void MSO_SetAll_f(void)
{
	if (Cmd_Argc() != 3)
		Com_Printf("Usage: %s <pause|notify|sound> <0|1>\n", Cmd_Argv(0));
	else {
		mso_t optionsType;
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
		MSO_Init_f();
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
 * @brief Function to update message options menu after scrolling.
 * Updates all visible button lines based on configuration.
 */
static void MSO_Scroll_f (void)
{
	const int oldScrollIdx = messageList_scroll;

	if (!msoTextNode)
		return;

	/* no scrolling if visible entry count is less than max on page (due to folding) */
	if (visibleMSOEntries < msoTextNode->u.text.rows)
		return;

	messageList_scroll = msoTextNode->u.text.textScroll;

	if (messageList_scroll >= visibleMSOEntries - msoTextNode->u.text.rows) {
		messageList_scroll = visibleMSOEntries - msoTextNode->u.text.rows;
		msoTextNode->u.text.textScroll = messageList_scroll;
	}

	if (messageList_scroll != oldScrollIdx)
		MSO_UpdateVisibleButtons();
}

/**
 * @brief Function toggles visibility of a message category to show or hide entries associated to this category.
 */
static void MSO_OptionsClick_f(void)
{
	int num;
	const msgCategoryEntry_t *category;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num >= gd.numMsgCategoryEntries || num < 0)
		return;

	category = MSO_GetEntryFromSelectionIndex(num, qfalse);
	assert(msoTextNode);
	/* don't highlight selection */
	MN_TextNodeSelectLine(msoTextNode, -1);
	if (!category || !category->isCategory)
		return;
	category->category->isFolded = !category->category->isFolded;
	messageOptionsPrepared = qfalse;
	MSO_Init_f();
}

/**
 * @brief Saves actual settings into backup settings variable.
 */
static void MSO_BackupSettings_f (void)
{
	memcpy(backupMessageSettings, messageSettings, sizeof(backupMessageSettings));
}

/**
 * @brief Restores actual settings from backup settings variable.
 * @note message options are caused to be re-initialized (for menu display)
 * @sa MSO_Init_f
 */
static void MSO_RestoreSettings_f (void)
{
	memcpy(messageSettings, backupMessageSettings, sizeof(messageSettings));
	messageOptionsInitialized = qfalse;
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
 * @sa MN_AddNewMessageSound
 */
message_t *MSO_CheckAddNewMessage (const notify_t messagecategory, const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia)
{
	message_t *result = NULL;

	if (messageSettings[messagecategory].doNotify)
		result = MN_AddNewMessageSound(title, text, popup, type, pedia, messageSettings[messagecategory].doSound);
	if (messageSettings[messagecategory].doPause)
		CL_GameTimeStop();
	return result;
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
	const int categoryCount = gd.numMsgCategories;

	/* save amount of available entries (forward compatible for additional types) */
	MSG_WriteLong(sb, optionsCount);

	/* save positive values */
	for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
		byte bitmask = 0;
		messageSettings_t actualSetting = messageSettings[type];
		if (actualSetting.doNotify) {
			bitmask |= NTMASK_NOTIFY;
		}
		if (actualSetting.doPause) {
			bitmask |= NTMASK_PAUSE;
		}
		if (actualSetting.doSound) {
			bitmask |= NTMASK_SOUND;
		}
		MSG_WriteString(sb, nt_strings[type]);
		MSG_WriteByte(sb, bitmask);
	}

	MSG_WriteLong(sb, categoryCount);
	for (idx = 0; idx < categoryCount; idx++) {
		byte bitmask = 0;
		msgCategory_t actualCategory = gd.messageCategories[idx];
		if (actualCategory.isFolded) {
			bitmask |= MSGCATMASK_FOLDED;
		}
		MSG_WriteString(sb, actualCategory.id);
		MSG_WriteByte(sb, bitmask);
	}
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

	for (; categoryCount> 0; categoryCount--) {
		const char *categoryId = MSG_ReadString(sb);
		const byte categoryState = MSG_ReadByte(sb);
		msgCategory_t *category = MSO_GetCategoryFromName(categoryId);
		if (!category) {
			Com_Printf("Unrecognized messagecategoryid '%s' ignored while loading\n", categoryId);
			continue;
		}
		MSO_SetCategoryState(category, categoryState, qfalse);
	}
	messageOptionsInitialized = qfalse;
	visibleMSOEntries = 0;
	messageList_scroll = 0;
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
		tmpCommand = va("%s",msgtype);
		/* build command from msgtype, settingstype (pause|notify|sound) */
		token = va("msgoptions_set %s %s 1",tmpCommand,COM_EParse(text, errhead, name));
		Cmd_ExecuteString(token);
	} while (*text);
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
	if (gd.numMsgCategories >= MAX_MESSAGECATEGORIES) {
		Com_Printf("MSO_ParseCategories: too many messagecategory defs\n");
		return;
	}

	category = &gd.messageCategories[gd.numMsgCategories];

	memset(category, 0, sizeof(*category));
	category->id = Mem_PoolStrDup(name, cl_localPool, CL_TAG_REPARSE_ON_NEW_GAME);
	category->idx = gd.numMsgCategories;	/* set self-link */
	category->isFolded = qtrue;

	entry = &gd.msgCategoryEntries[gd.numMsgCategoryEntries];

	/* first entry is category */
	memset(entry, 0, sizeof(*entry));
	entry->category = &gd.messageCategories[gd.numMsgCategories];
	category->last = category->first = &gd.msgCategoryEntries[gd.numMsgCategoryEntries];
	entry->previous = NULL;
	entry->next = NULL;
	entry->isCategory = qtrue;
	entry->notifyType = category->id;

	gd.numMsgCategoryEntries++;

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
					msgCategoryEntry_t *old = gd.messageCategories[gd.numMsgCategories].last;

					memset(&gd.msgCategoryEntries[gd.numMsgCategoryEntries], 0, sizeof(gd.msgCategoryEntries[gd.numMsgCategoryEntries]));
					gd.msgCategoryEntries[gd.numMsgCategoryEntries].category = &gd.messageCategories[gd.numMsgCategories];

					gd.messageCategories[gd.numMsgCategories].last = &gd.msgCategoryEntries[gd.numMsgCategoryEntries];
					old->next = &gd.msgCategoryEntries[gd.numMsgCategoryEntries];
					gd.msgCategoryEntries[gd.numMsgCategoryEntries].previous = old;
					gd.msgCategoryEntries[gd.numMsgCategoryEntries].next = NULL;
					gd.msgCategoryEntries[gd.numMsgCategoryEntries].notifyType = nt_strings[idx];
					gd.msgCategoryEntries[gd.numMsgCategoryEntries].settings = &messageSettings[idx];
					gd.numMsgCategoryEntries++;
					break;
				}
			}
		}
	} while (*text);

	gd.numMsgCategories++;
	messageOptionsPrepared = qfalse;
}

void MN_MessageInit (void)
{
	Cmd_AddCommand("chatlist", CL_ShowChatMessagesOnStack_f, "Print all chat messages to the game console");
	Cmd_AddCommand("messagelist", CL_ShowMessagesOnStack_f, "Print all messages to the game console");
	Cmd_AddCommand("msgoptions_toggle", MSO_Toggle_f, "Toggles pause, notification or sound setting for a message category");
	Cmd_AddCommand("msgoptions_setall", MSO_SetAll_f, "Sets pause, notification or sound setting for all message categories");
	Cmd_AddCommand("msgoptions_set", MSO_Set_f, "Sets pause, notification or sound setting for a message category");
	Cmd_AddCommand("msgoptions_setcat", MSO_SetCategoryState_f, "Sets the new state for a category");
	Cmd_AddCommand("msgoptions_scroll", MSO_Scroll_f, "Scroll callback function for message options menu text");
	Cmd_AddCommand("messagetypes_click", MSO_OptionsClick_f, "Callback function to (un-)fold visible categories");
	Cmd_AddCommand("msgoptions_init", MSO_Init_f, "Initializes message options menu");
	Cmd_AddCommand("msgoptions_backup", MSO_BackupSettings_f, "Backup message settings");
	Cmd_AddCommand("msgoptions_restore",MSO_RestoreSettings_f, "Restore message settings from backup");
#ifdef DEBUG
	Cmd_AddCommand("debug_clear_messagelist", CL_DeleteMessages_f, "Clears messagelist");
#endif
	memset(backupMessageSettings, 1, sizeof(backupMessageSettings));
}
