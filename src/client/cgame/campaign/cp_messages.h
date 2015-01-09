/**
 * @file
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

#pragma once

#define MAX_MESSAGE_TEXT 256

/* message systems */
typedef enum {
	MSG_DEBUG,			/**< only save them in debug mode */
	MSG_INFO,			/**< don't save these messages */
	MSG_STANDARD,
	MSG_RESEARCH_PROPOSAL,
	MSG_RESEARCH_HALTED,
	MSG_RESEARCH_FINISHED,
	MSG_CONSTRUCTION,
	MSG_UFOSPOTTED,
	MSG_UFOLOST,
	MSG_TERRORSITE,
	MSG_BASEATTACK,
	MSG_TRANSFERFINISHED,
	MSG_PROMOTION,
	MSG_PRODUCTION,
	MSG_NEWS,
	MSG_DEATH,
	MSG_CRASHSITE,

	MSG_EVENT,			/**< Eventmail only! used by CL_EventAddMail */

	MSG_MAX
} messageType_t;

/* Russian timestamp (with UTF-8) is 23 bytes long */
#define TIMESTAMP_TEXT 24
struct uiMessageListNodeMessage_s {
	char title[MAX_VAR];
	char timestamp[TIMESTAMP_TEXT];
	char* text;
	date_t date;
	const char* iconName;
	int lineUsed;		/**< used by the node to cache the number of lines need (often =1) */
	struct uiMessageListNodeMessage_s* next;

	/** override some campaign specific stuff */
	messageType_t type;
	struct technology_s* pedia;		/**< link to UFOpaedia if a research has finished. */
	struct eventMail_s* eventMail;
};

typedef struct uiMessageListNodeMessage_s uiMessageListNodeMessage_t;

uiMessageListNodeMessage_t* MS_AddNewMessage(const char* title, const char* text, messageType_t type = MSG_STANDARD, struct technology_s* pedia = nullptr, bool popup = false, bool playSound = true);
void MS_MessageInit(void);

extern char cp_messageBuffer[MAX_MESSAGE_TEXT];
