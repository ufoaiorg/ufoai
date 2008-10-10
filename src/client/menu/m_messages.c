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
	"installation_installed",
	"installation_removed",
	"installation_replaced",
};
CASSERT(lengthof(nt_strings) == NT_NUM_NOTIFYTYPE);

messageSettings_t messageSettings[NT_NUM_NOTIFYTYPE];/* array holding message settings for every notify type */
int messageList_scroll; /* actual messageSettings list begin index due to scrolling */
static char ms_messageSettingsList[1024];/* buffer for message settings text node */
qboolean messageOptionsInitialized = qfalse; /* flag indicating whether message options menu is initialized @sa MSO_Init_f */

/** @brief how many message settings may be shown at once on option page */
#define MAX_MESSAGESETTINGS_ENTRIES 23

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

	/** @todo Handle translated text - don't use single byte string copy here */
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
 * @sa MN_AddNewMessage
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
 * @brief Initializes menu texts for scrollable area
 * @note Calls confuncs to enable enough button lines in menu
 */
static void MSO_InitTextList (void)
{
	char categoryLine[36];
	const char *category;
	int idx;

	*ms_messageSettingsList = '\0';

	/** add all available texts, enable as much menu button lines as needed*/
	for (idx = 0; idx < NT_NUM_NOTIFYTYPE; idx++) {
		if (idx < MAX_MESSAGESETTINGS_ENTRIES) {
			MN_ExecuteConfunc(va("ms_enable%i", idx));
		}
		category = nt_strings[idx];
		Com_sprintf(categoryLine, sizeof(categoryLine), "%s\n", _(category));
		Q_strcat(ms_messageSettingsList, categoryLine, sizeof(ms_messageSettingsList));
	}
	/** disable menu button lines that are not needed */
	for (idx = NT_NUM_NOTIFYTYPE; idx < MAX_MESSAGESETTINGS_ENTRIES; idx++) {
		MN_ExecuteConfunc(va("ms_disable%i", idx));
	}
	mn.menuText[TEXT_MESSAGEOPTIONS] = ms_messageSettingsList;
}

/**
 * @brief initializes message options menu by showing as much button lines as needed.
 * @note this must only be done once as long as settings are changed in menu, so
 * messageOptionsInitialized is checked whether this is done yet. This function will be
 * reenabled if settings are changed via MSO_Set_f
 */
static void MSO_Init_f (void)
{
	if (!messageOptionsInitialized) {
		int idx;
		for (idx = 0; idx < NT_NUM_NOTIFYTYPE; idx++) {
			if (idx < MAX_MESSAGESETTINGS_ENTRIES) {
				MN_ExecuteConfunc(va("ms_enable%i", idx));
				MN_ExecuteConfunc(va("ms_pause%s%i",messageSettings[idx].doPause ? "e" : "d", idx));
				MN_ExecuteConfunc(va("ms_notify%s%i",messageSettings[idx].doNotify ? "e" : "d", idx));
			}
		}
		/** disable menu button lines that are not needed */
		for (idx = NT_NUM_NOTIFYTYPE; idx < MAX_MESSAGESETTINGS_ENTRIES; idx++) {
			MN_ExecuteConfunc(va("ms_disable%i", idx));
		}
		messageOptionsInitialized = qtrue;
	}
}

/**
 * @brief Function updates pause or notification settings.
 * @param listIndex listIndex in menu to update via confunc
 * @param type notification type to update
 * @param pause flag indicating whether to update pause (qtrue) or notify (qfalse) setting
 * @param activate flag indicating whether setting should be activated (qtrue) or deactivated
 * @param sendCommands flag indicating whether confunc command to update menu button should be sent
 * @sa MSO_Toggle_f
 * @sa MSO_Set_f
 * @note if sendCommands is qfalse, initialization of buttons is reactivated for next menu displaying
 */
static void MSO_Set (const int listIndex, const notify_t type, const qboolean pause, const qboolean activate, const qboolean sendCommands)
{
	if (pause)
		messageSettings[type].doPause = activate;
	else
		messageSettings[type].doNotify = activate;
	if (sendCommands)
		MN_ExecuteConfunc(va("ms_%s%s%i", pause ? "pause" : "notify", activate ? "e" : "d", listIndex));
	else
		/* ensure that message buttons will be initialized correctly if menu is shown next time */
		messageOptionsInitialized = qfalse;
}

/**
 * @brief Function for menu buttons to update message settings.
 * @sa MSO_Set
 */
static void MSO_Toggle_f (void)
{
	if (Cmd_Argc() != 3)
		Com_Printf("Usage: %s <listId> <pause|notice>\n", Cmd_Argv(0));
	else {
		const int listIndex = atoi(Cmd_Argv(1));
		const notify_t type = listIndex + messageList_scroll;
		qboolean pause;
		qboolean activate;

		if (!Q_strcmp(Cmd_Argv(2), "pause")) {
			pause = qtrue;
			activate = !messageSettings[type].doPause;
		} else {
			pause = qfalse;
			activate = !messageSettings[type].doNotify;
		}
		MSO_Set(listIndex, type, pause, activate, qtrue);
	}
}

/**
 * @brief Function callback used to initialize values for messageoptions and for manual setting changes.
 * @sa MSO_Set
 */
static void MSO_Set_f (void)
{
	if (Cmd_Argc() != 4)
		Com_Printf("Usage: %s <messagetypename> <pause|notify> <0|1>\n", Cmd_Argv(0));
	else {
		notify_t type;
		qboolean pause = qfalse;
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
			pause = qtrue;
		MSO_Set(0, type, pause, atoi(Cmd_Argv(3)), qfalse);
	}
}

/**
 * @brief Function to update message options menu after scrolling.
 * Updates all visible button lines based on configuration.
 */
static void MSO_Scroll_f (void)
{
	menuNode_t *textNode;
	int i;

	/* no scrolling available if displaying less notify types that max on page */
	if (NT_NUM_NOTIFYTYPE < MAX_MESSAGESETTINGS_ENTRIES)
		return;

	textNode = MN_GetNodeFromCurrentMenu("messagetypes");

	if (!textNode)
		return;

	messageList_scroll = textNode->textScroll;

	if (messageList_scroll >= NT_NUM_NOTIFYTYPE - MAX_MESSAGESETTINGS_ENTRIES) {
		messageList_scroll = NT_NUM_NOTIFYTYPE - MAX_MESSAGESETTINGS_ENTRIES;
		textNode->textScroll = messageList_scroll;
	}

	/** update visible button lines based on current displayed values */
	for (i = 0; i < MAX_MESSAGESETTINGS_ENTRIES; i++) {
		const notify_t type = i + messageList_scroll;
		if (type < NT_NUM_NOTIFYTYPE) {
			MN_ExecuteConfunc(va("ms_pause%s%i", messageSettings[type].doPause ? "e" : "d", i));
			MN_ExecuteConfunc(va("ms_notify%s%i", messageSettings[type].doNotify ? "e" : "d", i));
		}
	}
}

/**
 * @brief Checks whether to stop game for given messagecategory
 * @param[in] messagecategory notify type of messagecategory
 * @sa CL_GameTimeStop
 * @note if game should stop, CL_GameTimeStop is invoked
 */
void MSO_CheckTimestop (const notify_t messagecategory)
{
	if (messageSettings[messagecategory].doPause)
		CL_GameTimeStop();
}

/**
 * @brief Returns the actual setting whether to produce a notify message for given messagecategory.
 * @param[in] messagecategory notify type of messagecategory
 * @return qtrue if message should be generated
 */
qboolean MSO_ShouldNotify (const notify_t messagecategory)
{
	return messageSettings[messagecategory].doNotify;
}

/**
 * @brief saves current notification and pause settings
 * @sa MSO_Load
 */
qboolean MSO_Save (sizebuf_t* sb, void* data)
{
	notify_t type;
	int count = 0;
	/* count positiv values */
	for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
		if (messageSettings[type].doNotify)
			count++;
		if (messageSettings[type].doPause)
			count++;
	}
	MSG_WriteLong(sb, count);
	/* save positiv values (negativ won't be saved) */
	for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
		if (messageSettings[type].doNotify)	{
			MSG_WriteString(sb, nt_strings[type]);
			MSG_WriteChar(sb, 'n');
		}
		if (messageSettings[type].doPause) {
			MSG_WriteString(sb, nt_strings[type]);
			MSG_WriteChar(sb, 'p');
		}
	}
	messageOptionsInitialized = qfalse;
	return qtrue;
}

/**
 * @brief Restores the notification and pause settings from savegame
 * @sa MSO_Save
 */
qboolean MSO_Load (sizebuf_t* sb, void* data)
{
	int count;

	/* reset current message settings */
	memset(messageSettings, 0, sizeof(messageSettings));
	/* load all positiv settings */
	count = MSG_ReadLong(sb);
	for (; count--;) {
		const char *messagetype = MSG_ReadString(sb);
		const char pauseOrNotify = MSG_ReadChar(sb);
		notify_t type;
		qboolean pause;

		for (type = 0; type < NT_NUM_NOTIFYTYPE; type++) {
			if (Q_strcmp(nt_strings[type], messagetype) == 0)
				break;
		}
		/** @todo check why this message is not shown anywhere in logs*/
		if (type == NT_NUM_NOTIFYTYPE) {
			Com_Printf("Unrecognized messagetype during load '%s' ignored\n", messagetype);
			continue;
		}
		if (pauseOrNotify == 'p') {
			pause = qtrue;
		} else if (pauseOrNotify == 'n') {
			pause = qfalse;
		} else {
			return 0;
		}
		MSO_Set(0, type, pause, qtrue, qfalse);
	}
	return qtrue;
}

/**
 * @brief parses message options settings from file.
 * @param name
 * @param text
 */
void MSO_ParseSettings(const char *name, const char **text)
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
		if (!*text)
			break;
		if (*msgtype == '}')
			break;
		/* temporarly store type */
		tmpCommand = va("%s",msgtype);
		/* build command from msgtype, settingstype (pause|notice) */
		token = va("msgoptions_set %s %s 1",tmpCommand,COM_EParse(text, errhead, name));
		Cmd_ExecuteString(token);
	} while (*text);
}

void MN_MessageInit (void)
{
	Cmd_AddCommand("chatlist", CL_ShowChatMessagesOnStack_f, "Print all chat messages to the game console");
	Cmd_AddCommand("messagelist", CL_ShowMessagesOnStack_f, "Print all messages to the game console");
	Cmd_AddCommand("msgoptions_toggle", MSO_Toggle_f, "Toggles pause or notification setting for a message category");
	Cmd_AddCommand("msgoptions_set", MSO_Set_f, "Sets pause or notification setting for a message category");
	Cmd_AddCommand("msgoptions_scroll", MSO_Scroll_f, "Scroll callback function for message options menu text");
	Cmd_AddCommand("msgoptions_init", MSO_Init_f, "Initializes message options menu");
#ifdef DEBUG
	Cmd_AddCommand("debug_clear_messagelist", CL_DeleteMessages_f, "Clears messagelist");
#endif

	messageList_scroll = 0;
	MSO_InitTextList();
}
