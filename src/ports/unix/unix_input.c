/**
 * @file unix_input.c
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "../../client/client.h"
#include "unix_input.h"


/** KEYBOARD **************************************************************/

void (*KBD_Update_fp)(void);
void (*KBD_Init_fp)(Key_Event_fp_t fp);
void (*KBD_Close_fp)(void);

/** MOUSE *****************************************************************/

in_state_t in_state;

void (*RW_IN_Init_fp)(in_state_t *in_state_p);
void (*RW_IN_Shutdown_fp)(void);
void (*RW_IN_Activate_fp)(qboolean active);
void (*RW_IN_GetMousePos_fp)(int *mx, int *my);

/**
 * @brief
 */
static void Do_Key_Event (int key, qboolean down)
{
	Key_Event(key, down, Sys_Milliseconds());
}

/**
 * @brief
 */
void Real_IN_Init (void *lib)
{
	/* Init IN (Mouse) */
	in_state.Key_Event_fp = Do_Key_Event;

	if ((RW_IN_Init_fp = Sys_GetProcAddress(lib, "RW_IN_Init")) == NULL ||
		(RW_IN_Shutdown_fp = Sys_GetProcAddress(lib, "RW_IN_Shutdown")) == NULL ||
		(RW_IN_Activate_fp = Sys_GetProcAddress(lib, "RW_IN_Activate")) == NULL ||
		(RW_IN_GetMousePos_fp = Sys_GetProcAddress(lib, "RW_IN_GetMousePos")) == NULL)
		Sys_Error("No RW_IN functions in REF.\n");

	if ((KBD_Init_fp = Sys_GetProcAddress(lib, "KBD_Init")) == NULL ||
		(KBD_Update_fp = Sys_GetProcAddress(lib, "KBD_Update")) == NULL ||
		(KBD_Close_fp = Sys_GetProcAddress(lib, "KBD_Close")) == NULL)
		Sys_Error("No KBD functions in REF.\n");
	KBD_Init_fp(Do_Key_Event);

	if (RW_IN_Init_fp)
		RW_IN_Init_fp(&in_state);
}

/**
 * @brief
 * @sa IN_Shutdown
 */
void IN_Init (void)
{
}

/**
 * @brief
 * @sa IN_Init
 * @sa Real_IN_Init
 */
void IN_Shutdown (void)
{
	if (RW_IN_Shutdown_fp)
		RW_IN_Shutdown_fp();
	if (KBD_Close_fp)
		KBD_Close_fp();

	KBD_Init_fp = NULL;
	KBD_Update_fp = NULL;
	KBD_Close_fp = NULL;
	RW_IN_Init_fp = NULL;
	RW_IN_Shutdown_fp = NULL;
	RW_IN_Activate_fp = NULL;
	RW_IN_GetMousePos_fp = NULL;
}

/**
 * @brief
 */
void IN_GetMousePos (int *mx, int *my)
{
	if (RW_IN_GetMousePos_fp)
		RW_IN_GetMousePos_fp(mx, my);
}

/**
 * @brief
 */
void IN_Frame (void)
{
	if (RW_IN_Activate_fp) {
		if (cls.key_dest == key_console)
			RW_IN_Activate_fp(qfalse);
		else
			RW_IN_Activate_fp(qtrue);
	}
}

/**
 * @brief
 */
void IN_Activate (qboolean active)
{
}

/**
 * @brief
 */
void Sys_SendKeyEvents (void)
{
#ifndef DEDICATED_ONLY
	if (KBD_Update_fp)
		KBD_Update_fp();
#endif
	/* grab frame time */
	sys_frame_time = Sys_Milliseconds();
}
