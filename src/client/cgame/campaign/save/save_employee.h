/**
 * @file save_employee.h
 * @brief XML tag constants for savegame.
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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

#ifndef SAVE_EMPLOYEE_H
#define SAVE_EMPLOYEE_H

#define SAVE_EMPLOYEE_EMPLOYEES "employees"
#define SAVE_EMPLOYEE_TYPE "type"
#define SAVE_EMPLOYEE_EMPLOYEE "employee"
#define SAVE_EMPLOYEE_IDX "IDX"
#define SAVE_EMPLOYEE_BASEHIRED "baseHired"
#define SAVE_EMPLOYEE_ASSIGNED "assigned"
#define SAVE_EMPLOYEE_NATION "nation"
#define SAVE_EMPLOYEE_CHR "character"

#define SAVE_EMPLOYEETYPE_NAMESPACE "saveEmployeeType"
static const constListEntry_t saveEmployeeConstants[] = {
	{SAVE_EMPLOYEETYPE_NAMESPACE"::soldier", EMPL_SOLDIER},
	{SAVE_EMPLOYEETYPE_NAMESPACE"::scientist", EMPL_SCIENTIST},
	{SAVE_EMPLOYEETYPE_NAMESPACE"::worker", EMPL_WORKER},
	{SAVE_EMPLOYEETYPE_NAMESPACE"::pilot", EMPL_PILOT},
	{NULL, -1}
};

#endif

/*
DTD:

<!ELEMENT employees employee*>
<!ATTLIST employees
	type		soldier|
				scientist|
				worker|
				pilot		#REQUIRED
>

<!ELEMENT employee character>
<!ATTLIST employee
	IDX			CDATA		#REQUIRED
	baseHired	CDATA		#IMPLIED
	assigned	CDATA		#IMPLIED
	nation		CDATA		#REQUIRED
>

** for <character> check save_character.h
*/
