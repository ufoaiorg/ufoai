/**
 * @file cp_time.h
 * @brief Campaign geoscape time header
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

#ifndef CP_TIME_H
#define CP_TIME_H

void CP_UpdateTime(void);
void CP_GameTimeStop(void);
qboolean CP_IsTimeStopped(void);
void CP_GameTimeFast(void);
void CP_GameTimeSlow(void);
void CP_SetGameTime_f(void);

qboolean Date_LaterThan(const date_t *now, const date_t *compare);
qboolean Date_IsDue(const date_t *date);
date_t Date_Add(date_t a, date_t b);
date_t Date_Substract(date_t a, date_t b);
date_t Date_Random(date_t minFrame, date_t maxFrame);
const char *Date_GetMonthName(int month);

#endif
