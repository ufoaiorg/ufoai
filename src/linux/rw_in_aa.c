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

/*
 * Text-mode renderer using AA-lib
 *
 * version 0.1
 *
 * Jacek Fedorynski <jfedor@jfedor.org>
 *
 * http://www.jfedor.org/aaquake2/
 *
 * This file is a modified rw_in_svgalib.c.
 *
 * Graphics stuff is in rw_aa.c.
 *
 * Changelog:
 *
 *      2001-12-30      0.1    Initial release
 *
 */

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <sys/mman.h>

#if defined (__linux__)
#include <asm/io.h>
#include <sys/vt.h>
#endif

#include <aalib.h>

#include "../ref_soft/r_local.h"
#include "../client/keys.h"
#include "../linux/rw_linux.h"

extern aa_context *aac;

/*****************************************************************************/
/* KEYBOARD                                                                  */
/*****************************************************************************/

Key_Event_fp_t Key_Event_fp;

void KBD_Init(Key_Event_fp_t fp)
{
	Key_Event_fp = fp;
	if (!aac)
		Sys_Error("aac is NULL\n");

	if (!aa_autoinitkbd(aac, AA_SENDRELEASE))
		Sys_Error("aa_autoinitkbd() failed\n");
}

void KBD_Update(void)
{
	int ev;
	int down;
	
	while (ev = aa_getevent(aac, 0)) {
		down = 1;
release:
		switch (ev) {
			case AA_UP:
				Key_Event_fp(K_UPARROW, down);
				break;
			case AA_DOWN:
				Key_Event_fp(K_DOWNARROW, down);
				break;
			case AA_LEFT:
				Key_Event_fp(K_LEFTARROW, down);
				break;
			case AA_RIGHT:
				Key_Event_fp(K_RIGHTARROW, down);
				break;
			case AA_BACKSPACE:
				Key_Event_fp(K_BACKSPACE, down);
				break;
			case AA_ESC:
				Key_Event_fp(K_ESCAPE, down);
				break;
		}
		if (ev < 256) {
			Key_Event_fp(ev, down);
		} else if (ev > AA_RELEASE) {
			ev &= ~AA_RELEASE;
			down = 0;
			goto release;
		}
	}
}

void KBD_Close(void)
{
	if (!aac)
		Sys_Error("aac is NULL\n");

	aa_uninitkbd(aac);
}

/*****************************************************************************/
/* MOUSE                                                                     */
/*****************************************************************************/

// this is inside the renderer shared lib, so these are called from vid_so

void RW_IN_Init(in_state_t *in_state_p)
{
}

void RW_IN_Shutdown(void)
{
}

/*
===========
IN_Commands
===========
*/
void RW_IN_Commands (void)
{
}

/*
===========
IN_Move
===========
*/
void RW_IN_Move (usercmd_t *cmd)
{
}

void RW_IN_Frame (void)
{
}

void RW_IN_Activate(void)
{
}



