/**
 * @file e_parse.h
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

#ifndef CLIENT_CL_PARSE_EVENTS_H
#define CLIENT_CL_PARSE_EVENTS_H

extern cvar_t *cl_log_battlescape_events;

void CL_ParseEvent(struct dbuffer *msg);
int CL_ClearBattlescapeEvents(void);
void CL_BlockBattlescapeEvents(qboolean block);

#endif
