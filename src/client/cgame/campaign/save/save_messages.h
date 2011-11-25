/**
 * @file save_messages.h
 * @brief XML tag constants for savegame.
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

#ifndef SAVE_MESSAGES_H
#define SAVE_MESSAGES_H

#define SAVE_MESSAGES_MESSAGES "messages"
#define SAVE_MESSAGES_MESSAGE "message"
#define SAVE_MESSAGES_TITLE "title"
#define SAVE_MESSAGES_TEXT "text"
#define SAVE_MESSAGES_TYPE "type"
#define SAVE_MESSAGES_EVENTMAILID "eventMailid"
#define SAVE_MESSAGES_EVENTMAILREAD "eventMailRead"
#define SAVE_MESSAGES_PEDIAID "pediaid"
#define SAVE_MESSAGES_DATE "date"

#define SAVE_MESSAGETYPE_NAMESPACE "saveMessageType"
static const constListEntry_t saveMessageConstants[] = {
	{SAVE_MESSAGETYPE_NAMESPACE"::debug", MSG_DEBUG},
	{SAVE_MESSAGETYPE_NAMESPACE"::info", MSG_INFO},
	{SAVE_MESSAGETYPE_NAMESPACE"::standard", MSG_STANDARD},
	{SAVE_MESSAGETYPE_NAMESPACE"::rsproposal", MSG_RESEARCH_PROPOSAL},
	{SAVE_MESSAGETYPE_NAMESPACE"::rshalted", MSG_RESEARCH_HALTED},
	{SAVE_MESSAGETYPE_NAMESPACE"::rsfinished", MSG_RESEARCH_FINISHED},
	{SAVE_MESSAGETYPE_NAMESPACE"::construction", MSG_CONSTRUCTION},
	{SAVE_MESSAGETYPE_NAMESPACE"::ufospotted", MSG_UFOSPOTTED},
	{SAVE_MESSAGETYPE_NAMESPACE"::ufolost", MSG_UFOLOST},
	{SAVE_MESSAGETYPE_NAMESPACE"::terrorsite", MSG_TERRORSITE},
	{SAVE_MESSAGETYPE_NAMESPACE"::baseattack", MSG_BASEATTACK},
	{SAVE_MESSAGETYPE_NAMESPACE"::transfinished", MSG_TRANSFERFINISHED},
	{SAVE_MESSAGETYPE_NAMESPACE"::promotion", MSG_PROMOTION},
	{SAVE_MESSAGETYPE_NAMESPACE"::production", MSG_PRODUCTION},
	{SAVE_MESSAGETYPE_NAMESPACE"::news", MSG_NEWS},
	{SAVE_MESSAGETYPE_NAMESPACE"::death", MSG_DEATH},
	{SAVE_MESSAGETYPE_NAMESPACE"::crashsite", MSG_CRASHSITE},
	{SAVE_MESSAGETYPE_NAMESPACE"::event", MSG_EVENT},
	{NULL, -1}
};

#endif

/*
DTD:

<!ELEMENT messages message*>
<!ELEMENT message date>
<!ATTLIST message
	title			CDATA		#IMPLIED
	text			CDATA		#IMPILED
	type			CDATA		#REQUIRED
	eventMailid		CDATA		#IMPLIED
	eventmailRead	CDATA		#IMPLIED
	pediaid			CDATA		#IMPLIED
>

<!ELEMENT date EMPTY>
<!ATTLIST date
	day				CDATA		'0'
	sec				CDATA		'0'
>
*/
