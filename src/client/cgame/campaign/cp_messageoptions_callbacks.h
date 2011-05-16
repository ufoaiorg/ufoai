/**
 * @file cp_messageoptions_callbacks.h
 * @brief Header file for menu related console command callbacks.
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

#ifndef CLIENT_CL_MESSAGEOPTIONS_CALLBACKS_H
#define CLIENT_CL_MESSAGEOPTIONS_CALLBACKS_H

typedef enum msoMenuState_s {
	MSO_MSTATE_REINIT,
	MSO_MSTATE_PREPARED,
	MSO_MSTATE_INITIALIZED
} msoMenuState_t;

void MSO_SetMenuState (const msoMenuState_t newState, const qboolean initNow, const qboolean preserveIndex);

void MSO_InitCallbacks(void);
void MSO_ShutdownCallbacks(void);

#endif /* CLIENT_CL_MESSAGEOPTIONS_CALLBACKS_H */
