/**
 * @file cl_spawn.h
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

#ifndef CL_SPAWN_H_
#define CL_SPAWN_H_

typedef struct {
	char classname[MAX_VAR];
	char target[MAX_VAR];
	char targetname[MAX_VAR];
	char tagname[MAX_VAR];
	char anim[MAX_VAR];
	char model[MAX_QPATH];
	char particle[MAX_VAR];
	char noise[MAX_QPATH];
	vec3_t origin;
	vec3_t angles;
	vec3_t scale;
	vec3_t color;
	vec3_t ambientNightColor;
	vec_t nightLight;
	vec2_t nightSunAngles;
	vec3_t nightSunColor;
	vec3_t ambientDayColor;
	vec_t dayLight;
	vec2_t daySunAngles;
	vec3_t daySunColor;
	vec2_t wait;
	int maxLevel;
	int maxMultiplayerTeams;
	int skin;
	int frame;
	int light;
	int spawnflags;
	float volume;
	float attenuation;
	float angle;
	int maxteams;

	/* not filled from entity string */
	const char *entStringPos;
	int entnum;
} localEntityParse_t;

void CL_SpawnParseEntitystring(void);

#endif /* CL_SPAWN_H_ */
