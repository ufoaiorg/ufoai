/**
 * @file
 * @brief Alien cargo class
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

#include "aliencargo.h"

#define SAVE_ALIENCARGO_ITEM "cargo"
#define SAVE_ALIENCARGO_TEAMDEFID "teamdefid"
#define SAVE_ALIENCARGO_ALIVE "alive"
#define SAVE_ALIENCARGO_DEAD "dead"

/**
 * @brief Add aliens to the cargo by teamDef
 * @param[in] team Pointer to the alien Team Definition
 * @param[in] alive Number of alive aliens
 * @param[in] dead Number of dead aliens
 */
bool AlienCargo::add(const teamDef_t* team, int alive, int dead)
{
	if (!team)
		return false;
	if (alive == 0 && dead == 0)
		return true;

	LIST_Foreach(this->cargo, alienCargo_t, item) {
		if (item->teamDef != team)
			continue;

		if (alive + item->alive < 0)
			return false;
		if (dead + item->dead < 0)
			return false;

		item->alive += alive;
		item->dead += dead;

		this->sumAlive += alive;
		this->sumDead += dead;

		if (item->alive == 0 && item->dead == 0)
			cgi->LIST_Remove(&this->cargo, (void*)item);

		return true;
	}

	if (alive < 0 || dead < 0)
		return false;

	const alienCargo_t cargoItem = { team, alive, dead };

	if (cgi->LIST_Add(&this->cargo, (const void*)&cargoItem, sizeof(cargoItem))) {
		this->sumAlive += alive;
		this->sumDead += dead;
		return true;
	}

	return false;
}

/**
 * @brief Add aliens to the cargo by team Id
 * @param[in] teamId Scripted Id of an alien Team
 * @param[in] alive Number of alive aliens
 * @param[in] dead Number of dead aliens
 */
bool AlienCargo::add(const char* teamId, int alive, int dead)
{
	if (!teamId)
		return false;
	const teamDef_t* team = cgi->Com_GetTeamDefinitionByID(teamId);
	if (!team)
		return false;
	return this->add(team, alive, dead);
}

/**
 * @brief Return number of alive aliens of a type in the cargo
 * @param[in] team Pointer to the alien Team Definition entry
 */
int AlienCargo::getAlive(const teamDef_t* team) const
{
	if (!team)
		return -1;

	LIST_Foreach(this->cargo, alienCargo_t, item) {
		if (item->teamDef != team)
			continue;
		return item->alive;
	}
	return 0;
}

/**
 * @brief Return number of dead alien bodies of a type in the cargo
 * @param[in] team Pointer to the alien Team Definition entry
 */
int AlienCargo::getDead(const teamDef_t* team) const
{
	if (!team)
		return -1;

	LIST_Foreach(this->cargo, alienCargo_t, item) {
		if (item->teamDef != team)
			continue;
		return item->dead;
	}
	return 0;
}

/**
 * @brief Return number of all alive aliens in the cargo
 */
int AlienCargo::getAlive(void) const
{
	return this->sumAlive;
}

/**
 * @brief Return number of all dead bodies in the cargo
 */
int AlienCargo::getDead(void) const
{
	return this->sumDead;
}

/**
 * @brief Returns a copy of the cargo list
 * @return linked list of alienCargo_t structures
 */
linkedList_t* AlienCargo::list(void) const
{
	linkedList_t* listing = 0;

	LIST_Foreach(this->cargo, alienCargo_t, item) {
		if (!cgi->LIST_Add(&listing, (void*)item, sizeof(*item))) {
			cgi->LIST_Delete(&listing);
			return 0;
		}
	}
	return listing;
}

/**
 * @brief Load alien cargo from xml savegame
 * @param[in] root Root xml node to load data from
 */
bool AlienCargo::load(xmlNode_t* root)
{
	if (!root)
		return false;

	for (xmlNode_t* alienNode = cgi->XML_GetNode(root, SAVE_ALIENCARGO_ITEM); alienNode;
		alienNode = cgi->XML_GetNextNode(alienNode, root, SAVE_ALIENCARGO_ITEM))
	{
		const char* teamId = cgi->XML_GetString(alienNode, SAVE_ALIENCARGO_TEAMDEFID);
		const int alive = cgi->XML_GetInt(alienNode, SAVE_ALIENCARGO_ALIVE, 0);
		const int dead = cgi->XML_GetInt(alienNode, SAVE_ALIENCARGO_DEAD, 0);
		if (!add(teamId, alive, dead))
			cgi->Com_Printf("AlienCargo::load: Could add aliens to cargo: %s, %d, %d\n", teamId, alive, dead);
	}
	return true;
}

/**
 * @brief Save alien cargo to xml savegame
 * @param[out] root Root xml node to save data under
 */
bool AlienCargo::save(xmlNode_t* root) const
{
	if (!root)
		return false;
	LIST_Foreach(this->cargo, alienCargo_t, item) {
		xmlNode_t* alienNode = cgi->XML_AddNode(root, SAVE_ALIENCARGO_ITEM);
		if (!alienNode)
			return false;
		cgi->XML_AddString(alienNode, SAVE_ALIENCARGO_TEAMDEFID, item->teamDef->id);
		cgi->XML_AddIntValue(alienNode, SAVE_ALIENCARGO_ALIVE, item->alive);
		cgi->XML_AddIntValue(alienNode, SAVE_ALIENCARGO_DEAD, item->dead);
	}
	return true;
}

/**
 * @brief Creates and initializes AlienCargo object
 */
AlienCargo::AlienCargo(void) : cargo (0), sumAlive (0), sumDead(0)
{
}

/**
 * @brief Creates and initializes AlienCargo object from another one
 * @param[in] alienCargo Other object to make copy of
 */
AlienCargo::AlienCargo(AlienCargo& alienCargo) : cargo (0), sumAlive (0), sumDead(0)
{
	linkedList_t* list = alienCargo.list();

	LIST_Foreach(list, alienCargo_t, cargoItem) {
		if (cgi->LIST_Add(&this->cargo, (void*)cargoItem, sizeof(*cargoItem))) {
			this->sumAlive += cargoItem->alive;
			this->sumDead += cargoItem->dead;
		}
	}
	cgi->LIST_Delete(&list);
}

/**
 * @brief Destroys AlienCargo with it's internal data
 */
AlienCargo::~AlienCargo(void)
{
	cgi->LIST_Delete(&this->cargo);
}
