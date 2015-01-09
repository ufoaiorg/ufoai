/**
 * @file
 * @brief Alien cargo class header
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "cp_cgame_callbacks.h"

/**
 * @brief alien cargo entry
 */
typedef struct alienCargo_s {
	const teamDef_t* teamDef;		/**< Pointer to type (team) of alien race in global csi.teamDef array. */
	int alive;						/**< Amount of live captured aliens. */
	int dead;						/**< Amount of alien corpses. */
} alienCargo_t;

/**
 * @brief Alien cargo class
 */
class AlienCargo {
	protected:
		linkedList_t* cargo;		/**< internal linkedlist of teams and amounts */
		int sumAlive;				/**< cache of number of alive aliens of all kinds in the cargo*/
		int sumDead;				/**< cache of number of dead bodies of all kinds in the cargo*/
	public:
		virtual bool add (const teamDef_t* team, int alive, int dead);
		virtual bool add (const char* teamId, int alive, int dead);

		int getAlive (const teamDef_t* team) const;
		int getDead (const teamDef_t* team) const;
		int getAlive (void) const;
		int getDead (void) const;

		linkedList_t* list (void) const;

		bool load(xmlNode_t* root);
		bool save(xmlNode_t* root) const;

		AlienCargo (void);
		AlienCargo (AlienCargo& alienCargo);
		virtual ~AlienCargo (void);
};
