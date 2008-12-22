/**
 * @file m_messages.h
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

#ifndef CLIENT_MENU_M_MESSAGES_H
#define CLIENT_MENU_M_MESSAGES_H

#include "../../game/q_shared.h"

/* message systems */
typedef enum {
	MSG_DEBUG,			/**< only save them in debug mode */
	MSG_INFO,			/**< don't save these messages */
	MSG_STANDARD,
	MSG_RESEARCH_PROPOSAL,
	MSG_RESEARCH_FINISHED,
	MSG_CONSTRUCTION,
	MSG_UFOSPOTTED,
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
} messagetype_t;

/** @brief also used for chat message buffer */
#define MAX_MESSAGE_TEXT 256
/* Russian timestamp (with UTF-8) is 23 bytes long */
#define TIMESTAMP_TEXT 24
typedef struct message_s {
	char title[MAX_VAR];
	char timestamp[TIMESTAMP_TEXT];
	char *text;
	messagetype_t type;
	struct technology_s *pedia;		/**< link to UFOpaedia if a research has finished. */
	struct eventMail_s *eventMail;
	struct message_s *next;
	date_t date;
} message_t;

/** @brief Stores all chat messages from a multiplayer game */
typedef struct chatMessage_s {
	char *text;
	struct chatMessage_s *next;
} chatMessage_t;

/** @brief Notify types */
typedef enum nt_s {
	NT_INSTALLATION_INSTALLED,
	NT_INSTALLATION_REMOVED,
	NT_INSTALLATION_REPLACE,
	NT_INSTALLATION_BUILDSTART,
	NT_INSTALLATION_BUILDFINISH,
	NT_INSTALLATION_DESTROY,
	NT_RESEARCH_PROPOSED,
	NT_RESEARCH_COMPLETED,
	NT_PRODUCTION_STARTED,
	NT_PRODUCTION_FINISHED,
	NT_PRODUCTION_FAILED,
	NT_PRODUCTION_QUEUE_EMPTY,
	NT_HAPPINESS_CHANGED,
	NT_HAPPINESS_MIN,
	NT_HAPPINESS_PLEASED,
	NT_TRANSFER_STARTED,
	NT_TRANSFER_COMPLETED_SUCCESS,
	NT_TRANSFER_LOST,
	NT_TRANSFER_ALIENBODIES_DEFERED,
	NT_UFO_SPOTTED,
	NT_UFO_SIGNAL_LOST,
	NT_UFO_ATTACKING,

	NT_NUM_NOTIFYTYPE
} notify_t;

/** @brief bitmask values for storing msgoptions */
typedef enum ntmask_s {
	NTMASK_NOTIFY = 1 << 0,
	NTMASK_PAUSE  = 1 << 1,
	NTMASK_SOUND  = 1 << 2
} notifymask_t;

/** @brief bitmask values for storing msgcategory state */
typedef enum msgcategorymask_s {
	MSGCATMASK_FOLDED = 1 << 0
} msgcategorymask_t;

/** @brief notification types */
typedef enum mso_s {
	MSO_PAUSE = NTMASK_PAUSE | NTMASK_NOTIFY,	/**< pause game */
	MSO_NOTIFY = NTMASK_NOTIFY,					/**< add notification message */
	MSO_SOUND = NTMASK_SOUND | NTMASK_NOTIFY	/**< play notification sound */
} mso_t;

/**
 * @brief structure holding pause and notify settings for a notify type.
 */
typedef struct messageSettings_s {
	qboolean doPause;	/**< flag whether game should pause */
	qboolean doNotify;	/**< flag whether game should notify */
	qboolean doSound;	/**< flag whether game should play sound notification */
} messageSettings_t;

#define MAX_MESSAGECATEGORIES 16
typedef struct msgCategoryEntry_s {
	const char *notifyType;					/**< notification type or category name */
	struct msgCategory_s *category; 		/**< associated category */
	struct msgCategoryEntry_s *next;		/**< pointer to next in category */
	struct msgCategoryEntry_s *previous;	/**< pointer to previous in category */
	messageSettings_t *settings;			/**< associated settings */
	qboolean isCategory;					/**< flag indicating that this is a category and no notification type */
} msgCategoryEntry_t;

typedef struct msgCategory_s {
	int idx;				/**< self-link */
	const char *id;			/**< script file id / translateable category name */
	qboolean isFolded;		/**< flag indicating that category is folded and so it is not visible in menu */
	msgCategoryEntry_t *first;
	msgCategoryEntry_t *last;
} msgCategory_t;

extern messageSettings_t messageSettings[NT_NUM_NOTIFYTYPE];

message_t *MN_AddNewMessage(const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia);
message_t *MN_AddNewMessageSound(const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia, qboolean playSound);
void MN_RemoveMessage(const char *title);
void MN_AddChatMessage(const char *text);
void MN_MessageInit(void);
message_t *MSO_CheckAddNewMessage(const notify_t messagecategory, const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia);
void MSO_ParseSettings(const char *name, const char **text);
void MSO_ParseCategories(const char *name, const char **text);

#endif
