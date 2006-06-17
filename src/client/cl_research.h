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

#define MAX_RESEARCHDISPLAY 28	/* number of the available string (in the list) in the research menu */
#define MAX_RESEARCHLIST 32
#define MAX_TECHNOLOGIES 256
#define MAX_TECHLINKS 32



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
	RS_BUILDING,
	RS_ALIEN
} researchType_t;

typedef struct stringlist_s {
	int numEntries;				/* The number of used strings/techs. */
	char string[MAX_TECHLINKS][MAX_VAR];	/* A list of strings. */
	int idx[MAX_TECHLINKS];		/* holds the indices of the tech for faster operation after finding it once */
} stringlist_t;

typedef struct technology_s {
	char id[MAX_VAR];			/* Short (unique) id/name. */
	int idx;					/* Self-link in the global list */
	char name[MAX_VAR];			/* Full name of this technology. */
	char description[MAX_VAR];	/* Just a short text-id to get this via gettext. */
	researchType_t type;


	stringlist_t requires;
	char provides[MAX_VAR];		/* The item that this technology enables. */
	float overalltime, time;	/* The time that is needed to research this tech. (in days) */
	researchStatus_t statusResearch;

	int base_idx;				/* The base this tech is researched in. */
	int lab;					/* Where this techology is currently researched. */
	/*int workshop; // Where the 'provided' item is currently produced. */


	char image_top[MAX_VAR];
	char image_bottom[MAX_VAR];
	char mdl_top[MAX_QPATH];
	char mdl_bottom[MAX_QPATH];

	int statusCollected;		/* Did we loot this item (and how mach of it -> aliens/corpses) ? */
	byte needsCollected;		/* Is a collected item neccessary to research this item? */
	byte statusResearchable;	/* Is this item researchable? */

	/* Pedia info */
	int up_chapter;				/* pedia chapter as stored in research.ufo. */
	int prev;					/* Previous tech in pedia. */
	int next;					/* Next tech in pedia. */
} technology_t;

#define	TECHFS(x)	(int)&(((technology_t *)0)->x)

/* use this for saving and allocating */
/*extern	technology_t	technologies[MAX_TECHNOLOGIES]; */
/*extern	int	numTechnologies; */

void RS_ResetResearch(void);
void CL_CheckResearchStatus(void);
void RS_UpdateData(void);
void RS_ParseTechnologies(char *title, char **text);
qboolean RS_ItemIsResearched(char *id_provided);
qboolean RS_TechIsResearched(int tech_idx);
qboolean RS_IsResearched_idx(int idx);
qboolean RS_IsResearched_ptr(technology_t * tech);
int RS_Collected_(technology_t * tech);
qboolean RS_ItemCollected(char *id_provided);

void RS_AddObjectTechs(void);
void RS_InitTree(void);
void RS_MarkCollected(void);
void RS_MarkResearchable(void);
technology_t *RS_GetTechByID(const char *id);
technology_t *RS_GetTechByProvided(const char *id_provided);

#endif							/* CLIENT_CL_RESEARCH_H */
