/**
 * @file cp_messageoptions.h
 * @brief Header file for messageoptions related stuff
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

#ifndef CLIENT_CL_MESSAGEOPTIONS_H
#define CLIENT_CL_MESSAGEOPTIONS_H

#include "cp_messages.h"

/** @brief Notify types */
typedef enum nt_s {
	NT_INSTALLATION_INSTALLED,
	NT_INSTALLATION_REMOVED,
	NT_INSTALLATION_REPLACE,
	NT_AIRCRAFT_REFUELED,
	NT_AIRCRAFT_CANNOTREFUEL,
	NT_INSTALLATION_BUILDSTART,
	NT_INSTALLATION_BUILDFINISH,
	NT_INSTALLATION_DESTROY,
	NT_RESEARCH_PROPOSED,
	NT_RESEARCH_HALTED,
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
	NT_TRANSFER_UFORECOVERY_FINISHED,
	NT_UFO_SPOTTED,
	NT_UFO_SIGNAL_LOST,
	NT_UFO_ATTACKING,
	NT_BASE_ATTACK,

	NT_NUM_NOTIFYTYPE
} notify_t;

/** @brief bitmask values for storing msgoptions */
typedef enum ntmask_s {
	NTMASK_NOTIFY = 1 << 0,
	NTMASK_PAUSE  = 1 << 1,
	NTMASK_SOUND  = 1 << 2
} notifyMask_t;

/** @brief bitmask values for storing msgcategory state */
typedef enum msgcategorymask_s {
	MSGCATMASK_FOLDED = 1 << 0
} msgCategoryMask_t;

/** @brief notification type: pause game */
#define MSO_PAUSE (NTMASK_PAUSE | NTMASK_NOTIFY)
/** @brief notification type: add notification message */
#define MSO_NOTIFY (NTMASK_NOTIFY)
/** @brief notification type: play notification sound */
#define MSO_SOUND (NTMASK_SOUND | NTMASK_NOTIFY)

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
	const char *id;			/**< script file id / translatable category name */
	msgCategoryEntry_t *first;
	msgCategoryEntry_t *last;
} msgCategory_t;

extern messageSettings_t messageSettings[NT_NUM_NOTIFYTYPE];
extern char const* const nt_strings[NT_NUM_NOTIFYTYPE];

message_t *MSO_CheckAddNewMessage(const notify_t messagecategory, const char *title, const char *text, qboolean popup, messageType_t type, technology_t *pedia);
void MSO_ParseSettings(const char *name, const char **text);
void MSO_ParseCategories(const char *name, const char **text);
void MSO_Set(const int listIndex, const notify_t type, const int optionType, const qboolean activate, const qboolean sendCommands);
void MSO_Init(void);
void MSO_Shutdown(void);

#endif /* CLIENT_CL_MESSAGEOPTIONS_H */
