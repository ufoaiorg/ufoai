/**
 * @file cp_messages.h
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

#ifndef CLIENT_CP_MESSAGES_H
#define CLIENT_CP_MESSAGES_H

#include "../menu/m_messages.h"
#include "cl_messageoptions.h"

/**
 * @brief Human readable time information in the game.
 * @note Use this on runtime - please avoid for structs that get saved.
 * @sa date_t	For storage & network transmitting (engine only).
 * @sa CL_DateConvertLong
 */
typedef struct dateLong_s {
	short year;	/**< Year in yyyy notation. */
	byte month;	/**< Number of month (starting with 1). */
	byte day;	/**< Number of day (starting with 1). */
	byte hour;	/**< Hour of the day. @todo check what number-range this gives) */
	byte min;	/**< Minute of the hour. */
	byte sec;	/**< Second of the minute. */
} dateLong_t;

message_t *MS_AddNewMessage(const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia);
message_t *MS_AddNewMessageSound(const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia, qboolean playSound);
void MS_AddChatMessage(const char *text);
void MS_MessageInit(void);


#endif /* CLIENT_CP_MESSAGES_H */
