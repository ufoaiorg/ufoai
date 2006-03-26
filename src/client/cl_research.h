/*

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

// cl_research.h

#ifndef TECHNOLOGY_DEFINED
#define TECHNOLOGY_DEFINED 1

#define MAX_RESEARCHLIST 32
#define MAX_TECHNOLOGIES 256
#define MAX_TECHLINKS 32

typedef enum researchStatus_s
{
	RS_NONE,
	RS_RUNNING,
	RS_PAUSED,
	RS_FINISH
} researchStatus_t;

typedef enum researchType_s
{
	RS_TECH,
	RS_WEAPON,
	RS_ARMOR,
	RS_CRAFT,
	RS_BUILDING
} researchType_t;

typedef struct research_requirements_s
{
	char	list[MAX_TECHLINKS][MAX_VAR];
	int		numEntries;
} research_requirements_t;

typedef struct technology_s
{
	char	id[MAX_VAR];
	char	name[MAX_VAR];
	float	time;
	research_requirements_t	requires;
	char	provides[MAX_VAR];
	char	description[MAX_VAR];
	researchType_t	type;
	researchStatus_t	statusResearch;
	byte	statusCollected;	// Did we loot this item?
	
} technology_t;

#define	TECHFS(x)	(int)&(((technology_t *)0)->x)

// use this for saving and allocating
extern	technology_t	technologies[MAX_TECHNOLOGIES];

void MN_ResetResearch( void );
void CL_CheckResearchStatus( void );
void R_UpdateData ( void );
void MN_ParseTechnologies ( char* title, char** text );
byte R_ItemIsResearched(char *id_provided );

#endif /*TECHNOLOGY_DEFINED*/
