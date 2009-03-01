/**
 * @file cl_parse.h
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

#ifndef CLIENT_CL_PARSE_H
#define CLIENT_CL_PARSE_H

extern const char *ev_format[EV_NUM_EVENTS];
extern qboolean blockBattlescapeEvents;
extern cvar_t *cl_block_battlescape_events;
extern cvar_t *cl_log_battlescape_events;

void CL_BlockBattlescapeEvents(void);
void CL_UnblockBattlescapeEvents(void);
void CL_ResetBattlescapeEvents(void);

void CL_SetLastMoving(le_t *le);
void CL_ParseServerMessage(int cmd, struct dbuffer *msg);
qboolean CL_CheckOrDownloadFile(const char *filename);
void CL_DrawLineOfSight(const le_t *watcher, const le_t *target);

#endif
