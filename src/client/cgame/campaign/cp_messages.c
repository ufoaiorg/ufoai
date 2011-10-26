/**
 * @file cp_messages.c
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
#include "../../sound/s_main.h"
#include "cp_campaign.h"
#include "cp_popup.h"
#include "cp_messages.h"
#include "cp_time.h"
#include "save/save_messages.h"

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
	CP_DateConvertLong(&message->date, &date);
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
message_t *MS_AddNewMessage (const char *title, const char *text, qboolean popup, messageType_t type, technology_t *pedia)
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
message_t *MS_AddNewMessageSound (const char *title, const char *text, qboolean popup, messageType_t type, technology_t *pedia, qboolean playSound)
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
	case MSG_UFOLOST:
		sound = "geoscape/ufolost";
		break;
	case MSG_UFOSPOTTED:
		sound = "geoscape/ufospotted";
		break;
	case MSG_BASEATTACK:
		sound = "geoscape/basealert";
		break;
	case MSG_TERRORSITE:
		sound = "geoscape/alien-activity";
		break;
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
 * @brief Save a list of messages to xml
 * @param[out] p XML Node structure, where we write the information to
 * @param[in] message The first message to save
 * @note this saves messages in reversed order
 */
static void MS_MessageSaveXML (xmlNode_t *p, message_t *message)
{
	xmlNode_t *n;

	if (!message)
		return;

	/* bottom up */
	MS_MessageSaveXML(p, message->next);

	/* don't save these message types */
	if (message->type == MSG_INFO)
		return;

	Com_RegisterConstList(saveMessageConstants);
	n = XML_AddNode(p, SAVE_MESSAGES_MESSAGE);
	XML_AddString(n, SAVE_MESSAGES_TYPE, Com_GetConstVariable(SAVE_MESSAGETYPE_NAMESPACE, message->type));
	XML_AddStringValue(n, SAVE_MESSAGES_TITLE, message->title);
	XML_AddStringValue(n, SAVE_MESSAGES_TEXT, message->text);
	/* store script id of event mail */
	if (message->type == MSG_EVENT) {
		XML_AddString(n, SAVE_MESSAGES_EVENTMAILID, message->eventMail->id);
		XML_AddBoolValue(n, SAVE_MESSAGES_EVENTMAILREAD, message->eventMail->read);
	}
	if (message->pedia)
		XML_AddString(n, SAVE_MESSAGES_PEDIAID, message->pedia->id);
	XML_AddDate(n, SAVE_MESSAGES_DATE, message->date.day, message->date.sec);
	Com_UnregisterConstList(saveMessageConstants);
}

/**
 * @brief Save callback for messages
 * @param[out] p XML Node structure, where we write the information to
 * @sa MS_MessageSaveXML
 */
qboolean MS_SaveXML (xmlNode_t *p)
{
	xmlNode_t *n = XML_AddNode(p, SAVE_MESSAGES_MESSAGES);

	/* store message system items */
	MS_MessageSaveXML(n, cp_messageStack);
	return qtrue;
}

/**
 * @brief Load callback for messages
 * @param[in] p XML Node structure, where we get the information from
 * @sa MS_SaveXML
 * @sa UI_AddNewMessageSound
 */
qboolean MS_LoadXML (xmlNode_t *p)
{
	int i;
	xmlNode_t *n, *sn;
	n = XML_GetNode(p, SAVE_MESSAGES_MESSAGES);

	if (!n)
		return qfalse;

	/* we have to set this a little bit higher here, otherwise the samples that are played when adding
	 * a message to the stack would all played a few milliseconds after each other - that doesn't sound
	 * nice */
	S_SetSampleRepeatRate(500);

	Com_RegisterConstList(saveMessageConstants);
	for (sn = XML_GetNode(n, SAVE_MESSAGES_MESSAGE), i = 0; sn; sn = XML_GetNextNode(sn, n, SAVE_MESSAGES_MESSAGE), i++) {
		eventMail_t *mail;
		const char *type = XML_GetString(sn, SAVE_MESSAGES_TYPE);
		int mtype;
		char title[MAX_VAR];
		char text[MAX_MESSAGE_TEXT];
		char id[MAX_VAR];
		technology_t *tech = NULL;
		message_t *mess;

		if (!Com_GetConstIntFromNamespace(SAVE_MESSAGETYPE_NAMESPACE, type, (int*) &mtype)) {
			Com_Printf("Invalid message type '%s'\n", type);
			continue;
		}

		/* can contain high bits due to utf8 */
		Q_strncpyz(title, XML_GetString(sn, SAVE_MESSAGES_TITLE), sizeof(title));
		Q_strncpyz(text,  XML_GetString(sn, SAVE_MESSAGES_TEXT),  sizeof(text));

		if (mtype == MSG_EVENT) {
			mail = CL_GetEventMail(XML_GetString(sn, SAVE_MESSAGES_EVENTMAILID), qfalse);
			if (mail)
				mail->read = XML_GetBool(sn, SAVE_MESSAGES_EVENTMAILREAD, qfalse);
		} else
			mail = NULL;

		/* event and not mail means, dynamic mail - we don't save or load them */
		if (mtype == MSG_EVENT && !mail)
			continue;
		/** @todo is this really meant to depend on DEBUG_ALL with NO individual bit like DEBUG_MSG ?? */
		if (mtype == MSG_DEBUG && developer->integer != 1)
			continue;

		Q_strncpyz(id, XML_GetString(sn, SAVE_MESSAGES_PEDIAID), sizeof(id));
		if (id[0] != '\0')
			tech = RS_GetTechByID(id);
		if (!tech && (mtype == MSG_RESEARCH_PROPOSAL || mtype == MSG_RESEARCH_FINISHED)) {
			/** No tech found drop message. */
			continue;
		}
		mess = MS_AddNewMessageSound(title, text, qfalse, mtype, tech, qfalse);
		mess->eventMail = mail;
		XML_GetDate(sn, SAVE_MESSAGES_DATE, &mess->date.day, &mess->date.sec);
		/* redo timestamp text after setting date */
		MS_TimestampedText(mess->timestamp, mess, sizeof(mess->timestamp));

		if (mail) {
			dateLong_t date;
			char dateBuf[MAX_VAR] = "";

			CP_DateConvertLong(&mess->date, &date);
			Com_sprintf(dateBuf, sizeof(dateBuf), _("%i %s %02i"),
				date.year, Date_GetMonthName(date.month - 1), date.day);
			mail->date = Mem_PoolStrDup(dateBuf, cp_campaignPool, 0);
		}
	}
	Com_UnregisterConstList(saveMessageConstants);

	/* reset the sample repeat rate */
	S_SetSampleRepeatRate(0);

	return qtrue;
}

void MS_MessageInit (void)
{
	MSO_Init();
}
