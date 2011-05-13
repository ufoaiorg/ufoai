/**
 * @file save_uforecovery.h
 * @brief XML tag constants for savegame.
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

#ifndef SAVE_UFORECOVERY_H
#define SAVE_UFORECOVERY_H

#define SAVE_UFORECOVERY_STOREDUFOS "storedUFOs"
#define SAVE_UFORECOVERY_UFO "UFO"
#define SAVE_UFORECOVERY_UFOIDX "idx"
#define SAVE_UFORECOVERY_UFOID "id"
#define SAVE_UFORECOVERY_DATE "date"
#define SAVE_UFORECOVERY_STATUS "status"
#define SAVE_UFORECOVERY_CONDITION "condition"
#define SAVE_UFORECOVERY_INSTALLATIONIDX "installationIDX"

#define SAVE_STOREDUFOSTATUS_NAMESPACE "saveStoredUFOStatus"
static const constListEntry_t saveStoredUFOConstants[] = {
	{SAVE_STOREDUFOSTATUS_NAMESPACE"::recovered", SUFO_RECOVERED},
	{SAVE_STOREDUFOSTATUS_NAMESPACE"::stored", SUFO_STORED},
	{SAVE_STOREDUFOSTATUS_NAMESPACE"::transfered", SUFO_TRANSFERED},
	{NULL, -1}
};

#endif

/*
DTD:

<!ELEMENT storedUFOs UFO*>
<!ELEMENT UFO date>
<!ATTLIST UFO
	idx				CDATA		#REQUIRED
	id				CDATA		#REQUIRED
	status			(recovered,
					stored,
					transfered)	#REQUIRED
	condition		CDATA		'1.0'
	installationIDX CDATA		#REQUIRED
>

<!ELEMENT date EMPTY>
<!ATTLIST date
	day				CDATA		'0'
	sec				CDATA		'0'
>
*/
