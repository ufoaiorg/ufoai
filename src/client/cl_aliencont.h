/**
 * @file cl_aliencont.h
 * @brief Header file for alien containment stuff
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#define MAX_CARGO		256
#define MAX_ALIENCONT_CAP	512

/** types of aliens */
typedef enum {
	AL_ORTNOK,
	AL_TAMAN,
	AL_SHEVAAR,
	AL_UNKNOWN	/**< dummy, to get all */
} alienType_t;

/** specializations of aliens */
/*TODO: this is not used anywhere yet */
typedef enum {
	AS_PILOT,
	AS_GUNNER,
	AS_HARVESTER,
	AS_SOLDIER
} alienSpec_t;

/** cases of alien amount calculation */
typedef enum {
	AL_RESEARCH,
	AL_KILL
} alienCalcType_t;

/** structure of Alien Containment being a member of base_t */
typedef struct aliensCont_s {
	int idx;			/**< self link */
	char alientype[MAX_VAR];	/**< type of alien */ /* FIXME: alienType_t here */
	int amount_alive;		/**< Amount of live captured aliens. */
	int amount_dead;		/**< Amount of alien corpses. */
	int techIdx;			/**< Idx of related tech. */
} aliensCont_t;

/** alien cargo in aircraft_t, carrying aliens and bodies from a mission to base */
typedef struct aliensTmp_s {
	char alientype[MAX_VAR];	/**< type of alien */
	int amount_alive;		/**< Amount of live captured aliens. */
	int amount_dead;		/**< Amount of alien corpses. */
} aliensTmp_t;

/**
 * Collecting aliens functions
 */

void AL_FillInContainment(void);
char *AL_AlienTypeToName(int teamDescIdx);
void CL_CollectingAliens(void);
void AL_AddAliens(void);
void AL_RemoveAliens(alienType_t alientype, int amount, alienCalcType_t action);
int AL_GetAlienIdx(char *id);
int AL_GetAlienAmount(int idx, requirementType_t reqtype);

/**
 * Menu functions
 */
int AL_CountAll(void);
int AL_CountInBase(void);

#endif /* CLIENT_CL_ALIENCONT_H */

