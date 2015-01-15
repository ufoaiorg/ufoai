/**
 * @file
 * @brief Alien containment class
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

#include "aliencontainment.h"
/** @todo cp_research.h would be enough but it has an unresolved depenency on cp_camapign.h */
#include "cp_campaign.h"
#include "cp_research.h"

#define BREATHINGAPPARATUS_TECH "rs_alien_breathing"

/**
 * @brief Returns the number of capacity needed for an alien in the containment
 * @param[in] teamDef Pointer to the alien Team Definition
 * @param[in] isDead If the alien to calculate for is dead
 */
int AlienContainment::getCapacityNeedForAlien(const teamDef_t* teamDef, const bool isDead) const
{
	return 1;
}

/**
 * @brief Private metod to reset current capacities
 */
void AlienContainment::resetCurrentCapacities(void)
{
	if (this->aliveCapacity)
		this->aliveCapacity->cur = 0;
	if (this->deadCapacity)
		this->deadCapacity->cur = 0;
}

/**
 * @brief Returns if storing a specific life form is supported by the containment
 * @param[in] team Pointer to the alien Team Definition
 */
bool AlienContainment::isLifeSupported(const teamDef_t* team)
{
	/* No team - not supported */
	if (!team)
		return false;

	/* humans supported */
	if (!CHRSH_IsTeamDefAlien(team))
		return true;

	/* Robots are supported */
	if (CHRSH_IsTeamDefRobot(team))
		return true;

	/* Organic aliens need breathing apparatus known */
	/** @todo find a way that doesn't need a tech ID hardcoded */
	const technology_t* tech = RS_GetTechByID(BREATHINGAPPARATUS_TECH);
	if (!tech)
		return false;

	return RS_IsResearched_ptr(tech);
}

/**
 * @brief Add aliens to the containment by teamDef
 * @param[in] team Pointer to the alien Team Definition
 * @param[in] alive Number of alive aliens
 * @param[in] dead Number of dead aliens
 */
bool AlienContainment::add(const teamDef_t* team, int alive, int dead)
{
	if (!team)
		return false;

	if (!isLifeSupported(team)) {
		dead += alive;
		alive = 0;
	}

	if (AlienCargo::add(team, alive, dead)) {
		if (this->aliveCapacity)
			this->aliveCapacity->cur += alive * getCapacityNeedForAlien(team, false);
		if (this->deadCapacity)
			this->deadCapacity->cur += dead * getCapacityNeedForAlien(team, true);
		if (this->getAlive(team) > 0 || this->getDead(team) > 0) {
			technology_t* tech = RS_GetTechForTeam(team);
			RS_MarkCollected(tech);
		}
		return true;
	}
	return false;
}

/**
 * @brief Add aliens to the containment by scripted team ID
 * @param[in] teamId Scripted team ID string
 * @param[in] alive Number of alive aliens
 * @param[in] dead Number of dead aliens
 */
bool AlienContainment::add(const char* teamId, int alive, int dead)
{
	if (!teamId)
		return false;
	const teamDef_t* team = cgi->Com_GetTeamDefinitionByID(teamId);
	if (!team)
		return false;
	return this->add(team, alive, dead);
}

/**
 * @brief Creates and initializes AlienContainment object
 */
AlienContainment::AlienContainment (capacities_t* aliveCapacity, capacities_t* deadCapacity) : aliveCapacity(aliveCapacity), deadCapacity(deadCapacity)
{
	this->resetCurrentCapacities();
}

/**
 * @brief Destroys AlienContainer with it's internal data
 */
AlienContainment::~AlienContainment (void)
{
	this->resetCurrentCapacities();
}
