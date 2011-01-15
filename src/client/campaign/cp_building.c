/**
 * @file cp_building.c
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

#include "cp_building.h"
#include "../client.h" /* cl, cls */
#include "../../shared/parse.h"
#include "../../common/scripts.h"
#include "cp_campaign.h"

/**
 * @brief Returns the building type for a given building identified by its building id
 * from the ufo script files
 * @sa B_ParseBuildings
 * @sa B_GetBuildingType
 * @param[in] buildingID The script building id that should get converted into the enum value
 * @note Do not use B_GetBuildingType here, this is also used for parsing the types!
 */
buildingType_t B_GetBuildingTypeByBuildingID (const char *buildingID)
{
	if (Q_streq(buildingID, "lab")) {
		return B_LAB;
	} else if (Q_streq(buildingID, "hospital")) {
		return B_HOSPITAL;
	} else if (Q_streq(buildingID, "aliencont")) {
		return B_ALIEN_CONTAINMENT;
	} else if (Q_streq(buildingID, "workshop")) {
		return B_WORKSHOP;
	} else if (Q_streq(buildingID, "storage")) {
		return B_STORAGE;
	} else if (Q_streq(buildingID, "hangar")) {
		return B_HANGAR;
	} else if (Q_streq(buildingID, "smallhangar")) {
		return B_SMALL_HANGAR;
	} else if (Q_streq(buildingID, "quarters")) {
		return B_QUARTERS;
	} else if (Q_streq(buildingID, "workshop")) {
		return B_WORKSHOP;
	} else if (Q_streq(buildingID, "power")) {
		return B_POWER;
	} else if (Q_streq(buildingID, "command")) {
		return B_COMMAND;
	} else if (Q_streq(buildingID, "amstorage")) {
		return B_ANTIMATTER;
	} else if (Q_streq(buildingID, "entrance")) {
		return B_ENTRANCE;
	} else if (Q_streq(buildingID, "missile")) {
		return B_DEFENCE_MISSILE;
	} else if (Q_streq(buildingID, "laser")) {
		return B_DEFENCE_LASER;
	} else if (Q_streq(buildingID, "radar")) {
		return B_RADAR;
	}
	return MAX_BUILDING_TYPE;
}

/**
 * @brief Holds the names of valid entries in the basemanagement.ufo file.
 *
 * The valid definition names for BUILDINGS (building_t) in the basemanagement.ufo file.
 * to the appropriate values in the corresponding struct
 */
static const value_t valid_building_vars[] = {
	{"map_name", V_CLIENT_HUNK_STRING, offsetof(building_t, mapPart), 0},	/**< Name of the map file for generating basemap. */
	{"max_count", V_INT, offsetof(building_t, maxCount), MEMBER_SIZEOF(building_t, maxCount)},	/**< How many building of the same type allowed? */
	{"level", V_FLOAT, offsetof(building_t, level), MEMBER_SIZEOF(building_t, level)},	/**< building level */
	{"name", V_TRANSLATION_STRING, offsetof(building_t, name), 0},	/**< The displayed building name. */
	{"pedia", V_CLIENT_HUNK_STRING, offsetof(building_t, pedia), 0},	/**< The pedia-id string for the associated pedia entry. */
	{"status", V_INT, offsetof(building_t, buildingStatus), MEMBER_SIZEOF(building_t, buildingStatus)},	/**< The current status of the building. */
	{"image", V_CLIENT_HUNK_STRING, offsetof(building_t, image), 0},	/**< Identifies the image for the building. */
	{"visible", V_BOOL, offsetof(building_t, visible), MEMBER_SIZEOF(building_t, visible)}, /**< Determines whether a building should be listed in the construction list. Set the first part of a building to 1 all others to 0 otherwise all building-parts will be on the list */
	{"needs", V_CLIENT_HUNK_STRING, offsetof(building_t, needs), 0},	/**< For buildings with more than one part; the other parts of the building needed.*/
	{"fixcosts", V_INT, offsetof(building_t, fixCosts), MEMBER_SIZEOF(building_t, fixCosts)},	/**< Cost to build. */
	{"varcosts", V_INT, offsetof(building_t, varCosts), MEMBER_SIZEOF(building_t, varCosts)},	/**< Costs that will come up by using the building. */
	{"build_time", V_INT, offsetof(building_t, buildTime), MEMBER_SIZEOF(building_t, buildTime)},	/**< How many days it takes to construct the building. */
	{"starting_employees", V_INT, offsetof(building_t, maxEmployees), MEMBER_SIZEOF(building_t, maxEmployees)},	/**< How many employees to hire on construction in the first base. */
	{"capacity", V_INT, offsetof(building_t, capacity), MEMBER_SIZEOF(building_t, capacity)},	/**< A size value that is used by many buildings in a different way. */

	/*event handler functions */
	{"onconstruct", V_STRING, offsetof(building_t, onConstruct), 0}, /**< Event handler. */
	{"onattack", V_STRING, offsetof(building_t, onAttack), 0}, /**< Event handler. */
	{"ondestroy", V_STRING, offsetof(building_t, onDestroy), 0}, /**< Event handler. */
	{"mandatory", V_BOOL, offsetof(building_t, mandatory), MEMBER_SIZEOF(building_t, mandatory)}, /**< Automatically construct this building when a base is set up. Must also set the pos-flag. */
	{NULL, 0, 0, 0}
};

/**
 * @brief Copies an entry from the building description file into the list of building types.
 * @note Parses one "building" entry in the basemanagement.ufo file and writes
 * it into the next free entry in bmBuildings[0], which is the list of buildings
 * in the first base (building_t).
 * @param[in] name Unique script id of a building. This is parsed from "building xxx" -> id=xxx.
 * @param[in] text the whole following text that is part of the "building" item definition in .ufo.
 * @param[in] link Bool value that decides whether to link the tech pointer in or not
 * @sa CL_ParseScriptFirst (link is false here)
 * @sa CL_ParseScriptSecond (link it true here)
 */
void B_ParseBuildings (const char *name, const char **text, qboolean link)
{
	building_t *building;
	technology_t *techLink;
	const value_t *vp;
	const char *errhead = "B_ParseBuildings: unexpected end of file (names ";
	const char *token;

	/* get id list body */
	token = Com_Parse(text);
	if (!*text || *token != '{') {
		Com_Printf("B_ParseBuildings: building \"%s\" without body ignored\n", name);
		return;
	}

	if (ccs.numBuildingTemplates >= MAX_BUILDINGS)
		Com_Error(ERR_DROP, "B_ParseBuildings: too many buildings");

	if (!link) {
		int i;
		for (i = 0; i < ccs.numBuildingTemplates; i++) {
			if (Q_streq(ccs.buildingTemplates[i].id, name)) {
				Com_Printf("B_ParseBuildings: Second building with same name found (%s) - second ignored\n", name);
				return;
			}
		}

		/* new entry */
		building = &ccs.buildingTemplates[ccs.numBuildingTemplates];
		OBJZERO(*building);
		building->id = Mem_PoolStrDup(name, cp_campaignPool, 0);

		Com_DPrintf(DEBUG_CLIENT, "...found building %s\n", building->id);

		/* set standard values */
		building->tpl = building;	/* Self-link just in case ... this way we can check if it is a template or not. */
		building->idx = -1;			/* No entry in buildings list (yet). */
		building->base = NULL;
		building->buildingType = MAX_BUILDING_TYPE;
		building->dependsBuilding = NULL;
		building->visible = qtrue;
		building->maxCount = -1;	/* Default: no limit */

		ccs.numBuildingTemplates++;
		do {
			/* get the name type */
			token = Com_EParse(text, errhead, name);
			if (!*text)
				break;
			if (*token == '}')
				break;

			/* get values */
			if (Q_streq(token, "type")) {
				token = Com_EParse(text, errhead, name);
				if (!*text)
					return;

				building->buildingType = B_GetBuildingTypeByBuildingID(token);
				if (building->buildingType >= MAX_BUILDING_TYPE)
					Com_Printf("didn't find buildingType '%s'\n", token);
			} else {
				/* no linking yet */
				if (Q_streq(token, "depends")) {
					token = Com_EParse(text, errhead, name);
					if (!*text)
						return;
				} else {
					for (vp = valid_building_vars; vp->string; vp++)
						if (Q_streq(token, vp->string)) {
							/* found a definition */
							token = Com_EParse(text, errhead, name);
							if (!*text)
								return;

							switch (vp->type) {
							case V_NULL:
								break;
							case V_TRANSLATION_STRING:
								token++;
							case V_CLIENT_HUNK_STRING:
								Mem_PoolStrDupTo(token, (char**) ((char*)building + (int)vp->ofs), cp_campaignPool, 0);
								break;
							default:
								Com_EParseValue(building, token, vp->type, vp->ofs, vp->size);
								break;
							}
							break;
						}
					if (!vp->string)
						Com_Printf("B_ParseBuildings: unknown token \"%s\" ignored (building %s)\n", token, name);
				}
			}
		} while (*text);
	} else {
		building = B_GetBuildingTemplate(name);
		if (!building)
			Com_Error(ERR_DROP, "B_ParseBuildings: Could not find building with id %s\n", name);

		techLink = RS_GetTechByProvided(name);
		if (techLink)
			building->tech = techLink;
		else if (building->visible)
			Com_Error(ERR_DROP, "B_ParseBuildings: Could not find tech that provides %s\n", name);

		do {
			/* get the name type */
			token = Com_EParse(text, errhead, name);
			if (!*text)
				break;
			if (*token == '}')
				break;
			/* get values */
			if (Q_streq(token, "depends")) {
				const building_t *dependsBuilding = B_GetBuildingTemplate(Com_EParse(text, errhead, name));
				if (!dependsBuilding)
					Com_Error(ERR_DROP, "Could not find building depend of %s\n", building->id);
				building->dependsBuilding = dependsBuilding;
				if (!*text)
					return;
			}
		} while (*text);
	}
}

/**
 * @brief Check that the dependences of a building is operationnal
 * @param[in] building Pointer to the building to check
 * @return true if base contains needed dependence for entering building
 */
qboolean B_CheckBuildingDependencesStatus (const building_t* building)
{
	assert(building);

	if (!building->dependsBuilding)
		return qtrue;

	/* Make sure the dependsBuilding pointer is really a template .. just in case. */
	assert(building->dependsBuilding == building->dependsBuilding->tpl);

	return B_GetBuildingStatus(building->base, building->dependsBuilding->buildingType);
}
