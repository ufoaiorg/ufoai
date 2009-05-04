/**
 * @file cp_installationmanagement.h
 * @brief Header for installation management related stuff.
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

#ifndef CLIENT_CL_INSTALLATION_H
#define CLIENT_CL_INSTALLATION_H

#define MAX_INSTALLATIONS	16
#define MAX_INSTALLTAIONS_PER_BASE 3
#define MAX_INSTALLATION_TEMPLATES	6

#define MAX_INSTALLATION_DAMAGE	100
#define MAX_INSTALLATION_BATTERIES	5
#define MAX_INSTALLATION_SLOT	4

#define INS_GetInstallationIDX(installation) ((ptrdiff_t)((installation) - ccs.installations))

/**
 * @brief Possible installation states
 * @note: Don't change the order or you have to change the installationmenu scriptfiles, too
 */
typedef enum {
	INSTALLATION_NOT_USED,				/**< installation is not set yet */
	INSTALLATION_UNDER_CONSTRUCTION,	/**< installation is under construction */
	INSTALLATION_WORKING				/**< nothing special */
} installationStatus_t;

typedef struct installationTemplate_s {
	char *id;		/**< id of the installation. */
	char *name;		/**< Name of the installation (as you see it ingame). */

	int cost;				/**< Price of the installation. */
	int radarRange;			/**< The range of the installation's radar.  Units is the angle of the two points from center of earth. */
	int trackingRange;		/**< The tracking range of the installation's radar. Units are degrees. */
	int maxBatteries;		/**< The maximum number of battery slots that can be used in an installation. */
	int maxUFOsStored;		/**< The maximum number of ufos that can be stored in an installation. */
	int maxDamage;			/**< The maximum amount of damage an installation can sustain before it is destroyed. */
	int buildTime;			/**< Time to build the installation, in days. */
	char *model;			/**< Model used on geoscape */
} installationTemplate_t;

typedef struct installationUFOs_s {
	aircraft_t *aircraftTemplate;
	int amount;
} installationUFOs_t;

typedef struct installationWeapon_s {
	/* int idx; */
	aircraftSlot_t slot;	/**< Weapon. */
	aircraft_t *target;		/**< Aimed target for the weapon. */
} installationWeapon_t;

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

	radar_t	radar;			/**< Radar of the installation (for radar towers) */

	baseWeapon_t batteries[MAX_INSTALLATION_BATTERIES];	/**< Missile/Laser batteries assigned to this installation. For Sam Sites only. */
	int numBatteries;		/**< how many batteries are installed? */

	equipDef_t storage;	/**< weapons, etc. stored in base */

	/** All ufo aircraft in this installation. This is used for UFO Yards. **/
	installationUFOs_t installationUFOs[MAX_AIRCRAFT];
	int numUFOsInInstallation;	/**< How many ufos are in this installation. */

	capacities_t aircraftCapacity;		/**< Capacity of UFO Yard. */

	int installationDamage;			/**< Hit points of installation */
	int buildStart;					/**< Date when the installation building started */
	qboolean selected;				/**< current selected installation? */
} installation_t;

/** Currently displayed/accessed base. */
extern installation_t *installationCurrent;

installation_t* INS_GetInstallationByIDX(int instIdx);
void INS_SetUpInstallation(installation_t* installation, installationTemplate_t *installationTemplate, vec2_t pos);

installationType_t INS_GetType(const installation_t *installation);

/** Coordinates to place the new installation at (long, lat) */
extern vec2_t newInstallationPos;

int INS_GetFoundedInstallationCount(void);
installation_t* INS_GetFoundedInstallationByIDX(int installationIdx);
installation_t *INS_GetFirstUnfoundedInstallation(void);

void INS_SelectInstallation(installation_t *installation);

installation_t *INS_GetCurrentSelectedInstallation(void);
void INS_SetCurrentSelectedInstallation(const installation_t *installation);

installationTemplate_t* INS_GetInstallationTemplateFromInstallationID(const char *id);

aircraft_t *INS_GetAircraftFromInstallationByIndex(installation_t* installation, int index);

void INS_DestroyInstallation(installation_t *installation);

void INS_InitStartup(void);
void INS_ParseInstallations(const char *name, const char **text);
void INS_UpdateInstallationData(void);

qboolean INS_Load(sizebuf_t* sb, void* data);
qboolean INS_Save(sizebuf_t* sb, void* data);

#endif /* CLIENT_CL_INSTALLATION_H */
