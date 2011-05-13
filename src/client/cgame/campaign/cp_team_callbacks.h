/**
 * @file cp_team_callbacks.h
 * @brief Menu related callback functions for the team menu
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#ifndef CP_TEAM_CALLBACKS_H
#define CP_TEAM_CALLBACKS_H

/* List of (hired) emplyoees in the current category (employeeType). */
extern linkedList_t *employeeList;	/** @sa E_GetEmployeeByMenuIndex */
/* How many employees in current list (changes on every category change, too) */
extern int employeesInCurrentList;

void CP_TEAM_InitCallbacks(void);
void CP_TEAM_ShutdownCallbacks(void);

#endif
