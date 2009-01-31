/**
 * @file cp_messages.c
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
#include "../menu/m_main.h"
#include "../menu/m_popup.h"
#include "cp_time.h"

char cp_messageBuffer[MAX_MESSAGE_TEXT];
message_t *cp_messageStack;
chatMessage_t *cp_chatMessageStack;

/**
 * @brief Script command to show all messages on the stack
 */
static void MS_ShowMessagesOnStack_f (void)
{
	message_t *m = cp_messageStack;

	while (m) {
		Com_Printf("%s: %s\n", m->title, m->text);
		m = m->next;
	}
}

#ifdef DEBUG
/**
 * @brief Script command to delete all messages
 */
static void MS_DeleteMessages_f (void)
{
	message_t *m = cp_messageStack;
	message_t *mtmp;

	while (m) {
		mtmp = m->next;
		Mem_Free(m);
		m = mtmp;
	}
	cp_messageStack = NULL;
}
#endif

/**
 * @brief Returns formatted text of a message timestamp
 * @param[in] text Buffer to hold the final result
 * @param[in] message The message to convert into text
 */
static void MS_TimestampedText (char *text, message_t *message, size_t textsize)
{
	dateLong_t date;
	CL_DateConvertLong(&message->date, &date);
	Com_sprintf(text, textsize, _("%i %s %02i, %02i:%02i: "), date.year,
		Date_GetMonthName(date.month - 1), date.day, date.hour, date.min);
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
 * @note this method forwards to @c MS_AddNewMessageSound with @code playSound = qtrue @endcode
 */
message_t *MS_AddNewMessage (const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia)
{
	return MS_AddNewMessageSound(title, text, popup, type, pedia, qtrue);
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
message_t *MS_AddNewMessageSound (const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia, qboolean playSound)
{
	message_t *mess;
	const char *sound = NULL;

	assert(type < MSG_MAX);

	/* allocate memory for new message - delete this with every new game */
	mess = (message_t *) Mem_PoolAlloc(sizeof(message_t), cl_localPool, CL_TAG_NONE);

	/* push the new message at the beginning of the stack */
	mess->next = cp_messageStack;
	cp_messageStack = mess;

	mess->type = type;
	mess->pedia = pedia;		/* pointer to UFOpaedia entry */

	mess->date = ccs.date;

	/** @todo (menu) Handle translated text - don't use single byte string copy here */
	Q_strncpyz(mess->title, title, sizeof(mess->title));
	mess->text = Mem_PoolStrDup(text, cl_localPool, CL_TAG_NONE);

	/* get formatted date text */
	MS_TimestampedText(mess->timestamp, mess, sizeof(mess->timestamp));

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

/**< @brief buffer that hold all printed chat messages for menu display */
static char *chatBuffer = NULL;
static menuNode_t* chatBufferNode = NULL;

/**
 * @brief Displays a chat on the hud and add it to the chat buffer
 */
void MS_AddChatMessage (const char *text)
{
	/* allocate memory for new chat message */
	chatMessage_t *chat = (chatMessage_t *) Mem_PoolAlloc(sizeof(chatMessage_t), cl_genericPool, CL_TAG_NONE);

	/* push the new chat message at the beginning of the stack */
	chat->next = cp_chatMessageStack;
	cp_chatMessageStack = chat;
	chat->text = Mem_PoolStrDup(text, cl_genericPool, CL_TAG_NONE);

	if (!chatBuffer) {
		chatBuffer = (char*)Mem_PoolAlloc(sizeof(char) * MAX_MESSAGE_TEXT, cl_genericPool, CL_TAG_NONE);
		if (!chatBuffer) {
			Com_Printf("Could not allocate chat buffer\n");
			return;
		}
		/* only link this once */
		MN_RegisterText(TEXT_CHAT_WINDOW, chatBuffer);
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
static void MS_ShowChatMessagesOnStack_f (void)
{
	chatMessage_t *m = cp_chatMessageStack;

	while (m) {
		Com_Printf("%s", m->text);
		m = m->next;
	}
}

/**
 * @brief Saved the complete message stack
 * @sa SAV_GameSave
 * @sa MS_AddNewMessage
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
 * @sa MS_AddNewMessage
 * @sa MS_MessageSave
 */
qboolean MS_Save (sizebuf_t* sb, void* data)
{
	int i = 0;
	message_t* message;

	/* store message system items */
	for (message = cp_messageStack; message; message = message->next) {
		if (message->type == MSG_INFO)
			continue;
		i++;
	}
	MSG_WriteLong(sb, i);
	MS_MessageSave(sb, cp_messageStack);
	return qtrue;
}

/**
 * @sa MS_Save
 * @sa MS_AddNewMessageSound
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
			mess = MS_AddNewMessageSound(title, text, qfalse, mtype, RS_GetTechByIDX(idx), qfalse);
			mess->eventMail = mail;
			mess->date.day = MSG_ReadLong(sb);
			mess->date.sec = MSG_ReadLong(sb);
			/* redo timestamp text after setting date */
			MS_TimestampedText(mess->timestamp, mess, sizeof(mess->timestamp));
		}
	}
	return qtrue;
}


void MS_MessageInit (void)
{
	Cmd_AddCommand("chatlist", MS_ShowChatMessagesOnStack_f, "Print all chat messages to the game console");
	Cmd_AddCommand("messagelist", MS_ShowMessagesOnStack_f, "Print all messages to the game console");
#ifdef DEBUG
	Cmd_AddCommand("debug_clear_messagelist", MS_DeleteMessages_f, "Clears messagelist");
#endif

	MSO_Init();
}
