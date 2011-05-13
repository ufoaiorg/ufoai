/**
 * @file cl_shared.h
 * @brief Share stuff between the different cgame implementations
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

#ifndef CL_SHARED_H
#define CL_SHARED_H

#include "../common/common.h"

/* i18n support via gettext */
#include <libintl.h>

/* the used textdomain for gettext */
#define TEXT_DOMAIN "ufoai"
#include <locale.h>
#define _(String) gettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

/* Macros for faster access to the inventory-container. */
#define CONTAINER(e, containerID) ((e)->i.c[(containerID)])
#define ARMOUR(e) (CONTAINER(e, csi.idArmour))
#define RIGHT(e) (CONTAINER(e, csi.idRight))
#define LEFT(e)  (CONTAINER(e, csi.idLeft))
#define FLOOR(e) (CONTAINER(e, csi.idFloor))
#define HEADGEAR(e) (CONTAINER(e, csi.idHeadgear))
#define EXTENSION(e) (CONTAINER(e, csi.idExtension))
#define HOLSTER(e) (CONTAINER(e, csi.idHolster))

#define INVDEF(containerID) (&csi.ids[(containerID)])

typedef struct chr_list_s {
	character_t* chr[MAX_ACTIVETEAM];
	int num;	/* Number of entries */
} chrList_t;

typedef enum {
	ca_uninitialized,
	ca_disconnected,			/**< not talking to a server */
	ca_connecting,				/**< sending request packets to the server */
	ca_connected,				/**< netchan_t established, waiting for svc_serverdata */
	ca_active					/**< game views should be displayed */
} connstate_t;

typedef struct mapDef_s {
	/* general */
	char *id;				/**< script file id */
	char *map;				/**< bsp or ump base filename (without extension and day or night char) */
	char *param;			/**< in case of ump file, the assembly to use */
	char *description;		/**< the description to show in the menus */
	char *size;				/**< small, medium, big */

	/* multiplayer */
	qboolean multiplayer;	/**< is this map multiplayer ready at all */
	int teams;				/**< multiplayer teams */
	linkedList_t *gameTypes;	/**< gametype strings this map is useable for */

	/* singleplayer */
	int maxAliens;				/**< Number of spawning points on the map */
	qboolean hurtAliens;		/**< hurt the aliens on spawning them - e.g. for ufocrash missions */

	linkedList_t *terrains;		/**< terrain strings this map is useable for */
	linkedList_t *populations;	/**< population strings this map is useable for */
	linkedList_t *cultures;		/**< culture strings this map is useable for */
	qboolean storyRelated;		/**< Is this a mission story related? */
	int timesAlreadyUsed;		/**< Number of times the map has already been used */
	linkedList_t *ufos;			/**< Type of allowed UFOs on the map */
	linkedList_t *aircraft;		/**< Type of allowed aircraft on the map */

	/** @note Don't end with ; - the trigger commands get the base index as
	 * parameter - see CP_ExecuteMissionTrigger - If you don't need the base index
	 * in your trigger command, you can seperate more commands with ; of course */
	char *onwin;		/**< trigger command after you've won a battle */
	char *onlose;		/**< trigger command after you've lost a battle */
} mapDef_t;

mapDef_t* Com_GetMapDefinitionByID(const char *mapDefID);
mapDef_t* Com_GetMapDefByIDX(int index);

#endif
