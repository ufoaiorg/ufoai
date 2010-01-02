/**
 * @file cp_messages.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../menu/m_main.h"
#include "../menu/m_nodes.h"
#include "../menu/m_popup.h"
#include "../mxml/mxml_ufoai.h"
#include "cp_campaign.h"
#include "cp_popup.h"
#include "cp_messages.h"
#include "cp_time.h"

char cp_messageBuffer[MAX_MESSAGE_TEXT];
message_t *cp_messageStack;

/**
 * @brief Returns formatted text of a message timestamp
 * @param[in] text Buffer to hold the final result
 * @param[in] message The message to convert into text
 * @param[in] textsize The maximum length for text
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
message_t *MS_AddNewMessage (const char *title, const char *text, qboolean popup, messageType_t type, void *pedia)
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
message_t *MS_AddNewMessageSound (const char *title, const char *text, qboolean popup, messageType_t type, void *pedia, qboolean playSound)
{
	message_t *mess;
	const char *sound = NULL;

	assert(type < MSG_MAX);

	/* allocate memory for new message - delete this with every new game */
	mess = (message_t *) Mem_PoolAlloc(sizeof(*mess), cp_campaignPool, 0);

	/* push the new message at the beginning of the stack */
	mess->next = cp_messageStack;
	cp_messageStack = mess;

	mess->type = type;
	mess->pedia = pedia;		/* pointer to UFOpaedia entry */

	mess->date = ccs.date;

	/** @todo (menu) Handle translated text - don't use single byte string copy here */
	Q_strncpyz(mess->title, title, sizeof(mess->title));
	mess->text = Mem_PoolStrDup(text, cp_campaignPool, 0);

	/* get formatted date text */
	MS_TimestampedText(mess->timestamp, mess, sizeof(mess->timestamp));

	/* they need to be translated already */
	if (popup)
		CP_PopupList(mess->title, mess->text);

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
	case MSG_RESEARCH_HALTED:
	case MSG_EVENT:
	case MSG_NEWS:
		/* reread the new mails in UP_GetUnreadMails */
		ccs.numUnreadMails = -1;
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
		S_StartLocalSample(sound, SND_VOLUME_DEFAULT);

	return mess;
}

/**
 * @brief Saved the complete message stack in xml
 * @sa SAV_GameSaveXML
 * @sa MN_AddNewMessage
 */
static void MS_MessageSaveXML (mxml_node_t *p, message_t * message)
{
	mxml_node_t *n;

	if (!message)
		return;
	/* bottom up */
	MS_MessageSaveXML(p, message->next);

	/* don't save these message types */
	if (message->type == MSG_INFO)
		return;

	n = mxml_AddNode(p, "message");
	mxml_AddString(n, "title", message->title);
	mxml_AddString(n, "text", message->text);
	mxml_AddInt(n, "type", message->type);
	/* store script id of event mail */
	if (message->type == MSG_EVENT) {
		mxml_AddString(n, "eventmailid", message->eventMail->id);
		mxml_AddBool(n, "eventmailread", message->eventMail->read);
	}
	if (message->pedia)
		mxml_AddString(n, "id", message->pedia->id);
	mxml_AddInt(n, "day", message->date.day);
	mxml_AddInt(n, "sec", message->date.sec);
}

/**
 * @sa MS_LoadXML
 * @sa MN_AddNewMessage
 * @sa MS_MessageSaveXML
 */
qboolean MS_SaveXML (mxml_node_t *p)
{
	mxml_node_t *n = mxml_AddNode(p, "messages");

	/* store message system items */
	MS_MessageSaveXML(n, cp_messageStack);
	return qtrue;
}

/**
 * @sa MS_SaveXML
 * @sa MN_AddNewMessageSound
 */
qboolean MS_LoadXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *n, *sn;
	n = mxml_GetNode(p, "messages");
	if (!n)
		return qfalse;

	for (sn = mxml_GetNode(n, "message"), i = 0; sn; sn = mxml_GetNextNode(sn, n, "message"), i++) {
		eventMail_t *mail;
		int mtype;
		char title[MAX_VAR], text[MAX_MESSAGE_TEXT];
		/* can contain high bits due to utf8 */
		Q_strncpyz(title, mxml_GetString(sn, "title"), sizeof(title));
		Q_strncpyz(text,  mxml_GetString(sn, "text"),  sizeof(text));
		mtype = mxml_GetInt(sn, "type", MSG_DEBUG);
		if (mtype == MSG_EVENT) {
			mail = CL_GetEventMail(mxml_GetString(sn, "eventmailid"), qfalse);
			if (mail)
				mail->read = mxml_GetBool(sn, "eventmailread", qfalse);
		} else
			mail = NULL;

		/* event and not mail means, dynamic mail - we don't save or load them */
		if (!((mtype == MSG_EVENT && !mail) || (mtype == MSG_DEBUG && developer->integer != 1))) {
			char id[MAX_VAR];
			technology_t *tech;
			message_t *mess;

			Q_strncpyz(id, mxml_GetString(sn, "id"), sizeof(id));
			if (id[0] == '\0') {	/**< @todo: Fallback for old savegames. Remove it before release. */
				const int idx = mxml_GetInt(sn, "idx", -1);
				tech = RS_GetTechByIDX(idx);
			} else {
				tech = RS_GetTechByID(id);
			}

			if (!tech && (mtype == MSG_RESEARCH_PROPOSAL || mtype == MSG_RESEARCH_FINISHED)) {
				/** No tech found drop message. */
				continue;
			}
			mess = MS_AddNewMessageSound(title, text, qfalse, mtype, tech, qfalse);
			mess->eventMail = mail;
			mess->date.day = mxml_GetInt(sn, "day", 0);
			mess->date.sec = mxml_GetInt(sn, "sec", 0);
			/* redo timestamp text after setting date */
			MS_TimestampedText(mess->timestamp, mess, sizeof(mess->timestamp));
		}
	}
	return qtrue;
}

void MS_MessageInit (void)
{
	MSO_Init();
}
