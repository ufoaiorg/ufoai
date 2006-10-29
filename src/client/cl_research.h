/**
 * @file cl_research.h
 * @brief Header for research related stuff.
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

#ifndef CLIENT_CL_RESEARCH
#define CLIENT_CL_RESEARCH

#include "cl_ufopedia.h"

#define MAX_RESEARCHDISPLAY 28	/* Number of the available string (in the list) in the research menu */
#define MAX_RESEARCHLIST 32
#define MAX_TECHNOLOGIES 256	/* Maximum number of technologies overall. */
#define MAX_TECHLINKS 16		/* Maximum number of requirements in a technology (i.e in require_AND and require_OR). */


typedef enum researchStatus_s {
	RS_NONE,
	RS_RUNNING,
	RS_PAUSED,
	RS_FINISH
} researchStatus_t;

typedef enum researchType_s {
	RS_TECH,
	RS_WEAPON,
	RS_ARMOR,
	RS_CRAFT,
	RS_CRAFTWEAPON,
	RS_CRAFTSHIELD,
	RS_BUILDING,
	RS_ALIEN,
	RS_UGV
} researchType_t;

#define DEPENDENCIES_OVERHAUL 1
/* TODO: dependencies overhaul. Set to 1 to change to new dependency-calculation. */

#if DEPENDENCIES_OVERHAUL
typedef enum requirementType_s {
	RS_LINK_TECH,
	RS_LINK_ITEM,
	RS_LINK_EVENT,
	MAX_RS_LINKTYPES
} requirementType_t;

typedef struct requirements_s {
	int numLinks;			/* The number of requirements for this tech. */
	requirementType_t type[MAX_TECHLINKS];	/* What type the requ. is: item (in store/quarantine), event, etc... */
	char id[MAX_TECHLINKS][MAX_VAR];	/* dependency id (text-id) */
	int idx[MAX_TECHLINKS];		/* Dependency index (index in relation to array depends on the type) */
	int amount[MAX_TECHLINKS];	/* How many items are needed for research ... if any. (fix) */
	int collected[MAX_TECHLINKS];	/* How many items have been collected. (varies) */
	int delay;			/* Number in days the system should wait until the tech is available. */
					/* Starting from the time all other dependencies have been fulfilled. */
} requirements_t;
#endif

typedef struct stringlist_s {
	int numEntries;				/* The number of used strings/techs. */
	char string[MAX_TECHLINKS][MAX_VAR];	/* A list of strings. */
	int idx[MAX_TECHLINKS];		/* holds the indices of the tech for faster operation after finding it once */
} stringlist_t;

typedef struct technology_s {
	char id[MAX_VAR];			/* Short (unique) id/name. */
	int idx;				/* Self-link in the global list */
	char name[MAX_VAR];			/* Full name of this technology. */
	char description[MAX_VAR];	/* Description of researched item. Short text-id to get the full text via gettext. */
	char pre_description[MAX_VAR];	/* Description of item before it's researched. Short text-id to get the full text via gettext. */
	researchType_t type;


#if DEPENDENCIES_OVERHAUL
	requirements_t require_AND;	/* A list of requirements that ALL need to be met (= AND-related) See struct above. */
	requirements_t require_OR;	/* A list of requirements where ANY need to be met (= OR-related) See struct above. */
	qboolean statusCollected;	/* Did we loot all required items? This is (or _should be_) updated from the info stored in the require_X lists. */
#else /* overhaul */
	stringlist_t requires;
	/* TODO: replace with 'require' entries. */
	byte needsCollected;		/* Is a collected item neccessary to research this item? */
	int statusCollected;		/* Did we loot this item (and how mach of it -> aliens/corpses) ? */
#endif /* overhaul */

	char provides[MAX_VAR];		/* The item that this technology enables. */
	float overalltime, time;	/* The time that is needed to research this tech. (in days). */
					/* "overalltime" stays always the same, */
					/* "time" will be modified when it is under research.*/

	researchStatus_t statusResearch;	/* Current status of the research. */

	int base_idx;				/* The base this tech is researched in. */
	int scientists;					/* How many scientists are researching this tech. */

	char image_top[MAX_VAR];
	char image_bottom[MAX_VAR];
	char mdl_top[MAX_QPATH];
	char mdl_bottom[MAX_QPATH];

	byte statusResearchable;	/* Is this item researchable? */

	int produceTime;			/* how many days for self production */

	/* Pedia info */
	int up_chapter;				/* pedia chapter as stored in research.ufo. */
	int prev;					/* Previous tech in pedia. */
	int next;					/* Next tech in pedia. */
} technology_t;

/* use this for saving and allocating */
/*extern	technology_t	technologies[MAX_TECHNOLOGIES]; */
/*extern	int	numTechnologies; */

void RS_ResetResearch(void);
void CL_CheckResearchStatus(void);
void RS_UpdateData(void);
void RS_ParseTechnologies(char *title, char **text);
qboolean RS_ItemIsResearched(char *id_provided);
qboolean RS_TechIsResearched(int tech_idx);
qboolean RS_TechIsResearchable(technology_t * tech);
qboolean RS_IsResearched_idx(int idx);
qboolean RS_IsResearched_ptr(technology_t * tech);
#if DEPENDENCIES_OVERHAUL
int RS_Collected_(technology_t * tech);
void RS_CheckAllCollected(void);
#else /* overhaul */
qboolean RS_ItemCollected(char *id_provided);
#endif /* overhaul */

void RS_AddObjectTechs(void);
void RS_InitTree(void);
void RS_MarkCollected(equipDef_t *ed);
void RS_MarkResearchable(void);
void RS_MarkOneResearchable(int tech_idx);
void RS_MarkResearched(char *id);
void RS_AssignScientist(technology_t* tech);
technology_t *RS_GetTechByID(const char *id);
technology_t *RS_GetTechByProvided(const char *id_provided);
technology_t **RS_GetTechsByType(researchType_t type);
technology_t* RS_GetTechByIDX(int tech_idx);
technology_t *RS_GetTechWithMostScientists( int base_idx );


/* ufopedia function - but needs technology_t */
void UP_DrawEntry( technology_t* tech );

#endif /* CLIENT_CL_RESEARCH_H */
