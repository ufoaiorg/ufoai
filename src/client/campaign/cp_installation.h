/**
 * @file cp_installation.h
 * @brief Header for installation management related stuff.
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef CLIENT_CP_INSTALLATION_H
#define CLIENT_CP_INSTALLATION_H

#define MAX_INSTALLATIONS	16
#define MAX_INSTALLTAIONS_PER_BASE 3
#define MAX_INSTALLATION_TEMPLATES	6

#define MAX_INSTALLATION_DAMAGE	100
#define MAX_INSTALLATION_BATTERIES	5

#define INS_GetInstallationIDX(installation) ((ptrdiff_t)((installation) - ccs.installations))
#define INS_GetFoundedInstallationCount() (ccs.numInstallations)

/**
 * @brief Possible installation states
 * @note: Don't change the order or you have to change the installationmenu scriptfiles, too
 */
typedef enum {
	INSTALLATION_NOT_USED,				/**< installation is not set yet */
	INSTALLATION_UNDER_CONSTRUCTION,	/**< installation is under construction */
	INSTALLATION_WORKING				/**< nothing special, it's working */
} installationStatus_t;

typedef struct installationTemplate_s {
	char *id;							/**< id of the installation. */
	char *name;							/**< Name of the installation (as you see it ingame). */
	char *description;					/**< Short description in build dialog */

	int cost;							/**< Price of the installation. */
	int radarRange;						/**< The range of the installation's radar.  Units is the angle of the two points from center of earth. */
	int trackingRange;					/**< The tracking range of the installation's radar. Units are degrees. */
	int maxBatteries;					/**< The maximum number of battery slots that can be used in an installation. */
	int maxUFOsStored;					/**< The maximum number of ufos that can be stored in an installation. */
	int maxDamage;						/**< The maximum amount of damage an installation can sustain before it is destroyed. */
	int buildTime;						/**< Time to build the installation, in days. */
	char *model;						/**< Model used on 3D geoscape */
	char *image;						/**< Image used on 2D geoscape */
} installationTemplate_t;

typedef enum {
	INSTALLATION_RADAR,
	INSTALLATION_DEFENCE,
	INSTALLATION_UFOYARD,

	INSTALLATION_TYPE_MAX
} installationType_t;

/** @brief A installation with all it's data */
typedef struct installation_s {
	int idx;					/**< Self link. Index in the global installation-list. */
	char name[MAX_VAR];			/**< Name of the installation */

	installationTemplate_t *installationTemplate; /** The template used for the installation. **/

	qboolean founded;	/**< already founded? */
	vec3_t pos;		/**< pos on geoscape */

	installationStatus_t installationStatus; /**< the current installation status */

	float alienInterest;	/**< How much aliens know this installation (and may attack it) */

	struct radar_s radar;			/**< Radar of the installation (for radar towers) */

	baseWeapon_t batteries[MAX_INSTALLATION_BATTERIES];	/**< Missile/Laser batteries assigned to this installation. For Sam Sites only. */
	int numBatteries;		/**< how many batteries are installed? */

	capacities_t ufoCapacity;		/**< Capacity of UFO Yard. */

	int installationDamage;			/**< Hit points of installation */
	int buildStart;					/**< Date when the installation building started */
	qboolean selected;				/**< current selected installation? */
} installation_t;

/** Currently displayed/accessed base. */
extern installation_t *installationCurrent;

/** Coordinates to place the new installation at (long, lat) */
extern vec2_t newInstallationPos;

/* Functions */
installation_t *INS_GetInstallationByIDX(int instIdx);
installation_t *INS_GetFoundedInstallationByIDX(int installationIdx);
installation_t *INS_GetFirstUnfoundedInstallation(void);

installationTemplate_t *INS_GetInstallationTemplateFromInstallationID(const char *id);

installationType_t INS_GetType(const installation_t *installation);

installation_t *INS_GetFirstUFOYard(qboolean free);

void INS_SetUpInstallation(installation_t* installation, installationTemplate_t *installationTemplate, vec2_t pos);

installation_t *INS_GetCurrentSelectedInstallation(void);
void INS_SetCurrentSelectedInstallation(const installation_t *installation);
void INS_SelectInstallation(installation_t *installation);

void INS_UpdateInstallationData(void);

void INS_DestroyInstallation(installation_t *installation);

void INS_InitStartup(void);
void INS_ParseInstallations(const char *name, const char **text);

#endif /* CLIENT_CP_INSTALLATION_H */
