/**
 * @file cp_aliencont.h
 * @brief Header file for Alien Containment stuff.
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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

#ifndef CLIENT_CL_ALIENCONT_H
#define CLIENT_CL_ALIENCONT_H

#define MAX_ALIENCONT_CAP	32

/** @todo remove harcoded tech */
#define BREATHINGAPPARATUS_TECH "rs_alien_breathing"

/** cases of alien amount calculation */
typedef enum {
	AL_RESEARCH,			/**< When we remove alien(s) because of research topic. */
	AL_KILL,			/**< When we kill all aliens in base because of base defence mission. */
	AL_KILLONE,			/**< When we kill one alien of given type in AC menu. */
	AL_ADDALIVE,			/**< When we add one alien by hand. */
	AL_ADDDEAD			/**< When we kill one alien by hand. */
} alienCalcType_t;

/** structure of Alien Containment being a member of base_t */
typedef struct aliensCont_s {
	const teamDef_t* teamDef;		/**< Pointer to type (team) of alien race in global csi.teamDef array. */
	int amountAlive;		/**< Amount of live captured aliens. */
	int amountDead;		/**< Amount of alien corpses. */
	technology_t *tech;		/**< Team defintion related technology. */
} aliensCont_t;

/**
 * Collecting aliens functions.
 */

void AL_FillInContainment(struct base_s *base);
void AL_AddAliens(struct aircraft_s *aircraft);
void AL_RemoveAliens(struct base_s *base, const teamDef_t *alienType, int amount, const alienCalcType_t action);
const teamDef_t* AL_GetAlienTeamDef(int alienTeamDefIdx);
int AL_GetAlienAmount(const teamDef_t *alienType, requirementType_t reqtype, const struct base_s *base);
void AL_ChangeAliveAlienNumber(struct base_s *base, aliensCont_t *containment, int num);
qboolean AL_CheckAliveFreeSpace(const struct base_s *base, const aliensCont_t *containment, const int num);
int AL_CountInBase(const struct base_s *base);
#define AL_GetAircraftAlienCargo(a) (a)->alienCargo
#define AL_GetAircraftAlienCargoTypes(a) ((a)->alienCargoTypes)
#define AL_SetAircraftAlienCargoTypes(a, i) (a)->alienCargoTypes = (i)
qboolean AL_AddAlienTypeToAircraftCargo(struct aircraft_s *aircraft, const teamDef_t *teamDef, int amount, qboolean dead);

void AL_RemoveAliensExceedingCapacity(struct base_s *base);
/**
 * Menu functions
 */
int AL_CountAll(void);
void AC_InitStartup(void);
void AC_KillAll(struct base_s *base);

#endif /* CLIENT_CL_ALIENCONT_H */
