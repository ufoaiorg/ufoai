/**
 * @file
 * @brief Alien containment class header
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

#include "aliencargo.h"
#include "cp_capacity.h"

/**
 * @brief Alien containment class
 */
class AlienContainment : public AlienCargo {
	protected:
		capacities_t* aliveCapacity;
		capacities_t* deadCapacity;

	public:
		virtual bool add (const teamDef_t* team, int alive, int dead);
		virtual bool add (const char* teamId, int alive, int dead);

		static bool isLifeSupported (const teamDef_t* team);

		AlienContainment (capacities_t* aliveCapacity, capacities_t* deadCapacity);
		virtual ~AlienContainment (void);
	private:
		void resetCurrentCapacities (void);
		int getCapacityNeedForAlien (const teamDef_t* teamDef, const bool isDead) const;
};
