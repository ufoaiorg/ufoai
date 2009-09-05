/**
 * @file e_main.h
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

#ifndef E_MAIN_H_
#define E_MAIN_H_

typedef struct eventRegister_s {
	const int type;
	const char *name;
	const char *formatString;
	void (*eventCallback)(const struct eventRegister_s *self, struct dbuffer *msg);
	int (*timeCallback)(const struct eventRegister_s *self, struct dbuffer *msg, const int dt);
	
	/** Called to determine if this event is ok to run at this point.  Should check any 
	 * conflicts with other ongoing events (see le_t->locked).  Return true if OK to run, false if not. */
	qboolean (*eventCheck)(const struct eventRegister_s *self, const struct dbuffer *msg);
} eventRegister_t;

const eventRegister_t *CL_GetEvent(const int eType);

#endif
