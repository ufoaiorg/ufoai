/**
 * @file cl_global.h
 * @brief Defines global data structure.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef CLIENT_CL_GLOBAL_H
#define CLIENT_CL_GLOBAL_H

#include "campaign/cl_messageoptions.h" /**< msgCategory_t */

#define MAX_PROJECTILESONGEOSCAPE 32

/* Maximum alien team category defined in scripts */
#define ALIENCATEGORY_MAX	8

/**
 * @brief This struct/variable holds the global data for a game.
 * @sa CL_ReadSinglePlayerData
 * @todo move everything campaign-mode only into the @c ccs_t structure
 */
typedef struct globalData_s
{
	/* == technolgies == */
	/* A list of all research-topics resp. the research-tree. */
	technology_t technologies[MAX_TECHNOLOGIES];
	/* Total nubmer of technologies. */
	int numTechnologies;

	/* == pedia == */
	/* A list of all UFOpaedia chapters. */
	pediaChapter_t upChapters[MAX_PEDIACHAPTERS];
	/* Total number uf UFOpaedia chapters */
	int numChapters;
	int numUnreadMails; /**< only for faster access (don't cycle all techs every frame) */

	eventMail_t eventMails[MAX_EVENTMAILS];	/**< holds all event mails (cl_event.c) */
	int numEventMails;	/**< how many eventmails (script-id: mail) parsed */

	/* == bases == */
	/* A list of _all_ bases ... even unbuilt ones. */
	base_t bases[MAX_BASES];
	/* used for unique aircraft ids */
	int numAircraft;
	/* Total number of parsed base-names. */
	int numBaseNames;
	/* Total number of built bases (how many are enabled). */
	int numBases;
	/* a list of all templates for building bases */
	baseTemplate_t baseTemplates[MAX_BASETEMPLATES];
	int numBaseTemplates;

	/* == buildings in bases == */
	/* A list of all possible unique buldings. */
	building_t buildingTemplates[MAX_BUILDINGS];
	int numBuildingTemplates;
	/*  A list of the building-list per base. (new buildings in a base get copied from buildingTypes) */
	building_t buildings[MAX_BASES][MAX_BUILDINGS];
	/* Total number of buildings per base. */
	int numBuildings[MAX_BASES];

	/* == installations == */
	/* A template for each possible installation with configurable values */
	installationTemplate_t installationTemplates[MAX_INSTALLATION_TEMPLATES];
	int numInstallationTemplates;

	/* A list of _all_ installations ... even unbuilt ones. */
	installation_t installations[MAX_INSTALLATIONS];
	/* Total number of parsed installation-names. */
	int numInstallationNames;
	/* Total number of built installations (how many are enabled). */
	int numInstallations;

	/* == misc == */
	/* MA_NEWBASE, MA_INTERCEPT, MA_BASEATTACK, ... */
	mapAction_t mapAction;
	/* BA_NEWBUILDING ... */
	baseAction_t baseAction;
	/* how fast the game is running */
	int gameTimeScale;
	/* already paid in this month? */
	qboolean fund;

	qboolean autosell[MAX_OBJDEFS];		/**< True if this objDef_t has autosell enabled. */

	/* == production == */
	/* we will allow only one active production at the same time for each base */
	/* NOTE The facility to produce equipment should have the once-flag set */
	production_queue_t productions[MAX_BASES];

	/* == Ranks == */
	/* Global list of all ranks defined in medals.ufo. */
	rank_t ranks[MAX_RANKS];
	/* The number of entries in the list above. */
	int numRanks;

	/* == Nations == */
	nation_t nations[MAX_NATIONS];
	int numNations;

	/* == Cities == */
	city_t cities[MAX_CITIES];
	int numCities;

	/* == UGVs == */
	ugv_t ugvs[MAX_UGV];
	int numUGV;

	/* unified character id */
	int nextUCN;

	/* == Aircraft == */
	/* UFOs on geoscape: @todo update their inner pointers if needed */
	aircraft_t ufos[MAX_UFOONGEOSCAPE];
	int numUFOs;	/**< The current amount of UFOS on the geoscape. */

	/* UFO to follow while in combat zoom */
	aircraft_t *combatZoomedUFO;
	qboolean combatZoomOn;
	combatZoomLevel_t combatZoomLevel;

	/* Projectiles on geoscape (during fights) */
	aircraftProjectile_t projectiles[MAX_PROJECTILESONGEOSCAPE];
	int numProjectiles;

	/* All transfers. */
	transfer_t alltransfers[MAX_TRANSFERS];

	/* UFO recoveries. */
	ufoRecoveries_t recoveries[MAX_RECOVERIES];

	/* UFO components. */
	int numComponents;
	components_t components[MAX_ASSEMBLIES];

	/* Alien Team Definitions. */
	int numAliensTD;

	/* Alien Team Package used during battle */
	alienTeamCategory_t alienCategories[ALIENCATEGORY_MAX];	/**< different alien team available
														 * that will be used in mission */
	int numAlienCategories;		/** number of alien team categories defined */

	/* message categories */
	msgCategory_t messageCategories[MAX_MESSAGECATEGORIES];
	int numMsgCategories;

	/* entries for message categories */
	msgCategoryEntry_t msgCategoryEntries[NT_NUM_NOTIFYTYPE + MAX_MESSAGECATEGORIES];
	int numMsgCategoryEntries;
} globalData_t;


extern globalData_t gd;

globalData_t gd;


#endif /* CLIENT_CL_GLOBAL_H */
