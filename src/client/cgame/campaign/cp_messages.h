/**
 * @file cp_messages.h
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

#ifndef CLIENT_CP_MESSAGES_H
#define CLIENT_CP_MESSAGES_H

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
	MSG_EVENT,

	MSG_MAX
} messageType_t;

/* Russian timestamp (with UTF-8) is 23 bytes long */
#define TIMESTAMP_TEXT 24
typedef struct message_s {
	char title[MAX_VAR];
	char timestamp[TIMESTAMP_TEXT];
	char *text;
	messageType_t type;
	struct technology_s *pedia;		/**< link to UFOpaedia if a research has finished. */
	struct eventMail_s *eventMail;
	struct message_s *next;
	date_t date;
	int lineUsed;		/**< used my the node to cache the number of lines need (often =1) */
} message_t;

message_t *MS_AddNewMessage(const char *title, const char *text, qboolean popup, messageType_t type, struct technology_s *pedia);
message_t *MS_AddNewMessageSound(const char *title, const char *text, qboolean popup, messageType_t type, struct technology_s *pedia, qboolean playSound);
void MS_MessageInit(void);

extern char cp_messageBuffer[MAX_MESSAGE_TEXT];
extern message_t *cp_messageStack;

#endif /* CLIENT_CP_MESSAGES_H */
