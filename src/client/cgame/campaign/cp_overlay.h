/**
 * @file cp_overlay.h
 * @brief Functions to generate and render overlay for geoscape
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


void CP_DecreaseXVILevelEverywhere(void);
void CP_ChangeXVILevel(const vec2_t pos, float factor);
void CP_InitializeXVIOverlay(const byte *data);
void CP_InitOverlay(void);
void CP_GetXVIMapDimensions(int *width, int *height);
int CP_GetXVILevel(int x, int y);
void CP_SetXVILevel(int x, int y, int value);
void CP_InitializeRadarOverlay(qboolean source);
void CP_AddRadarCoverage(const vec2_t pos, float innerRadius, float outerRadius, qboolean source);
void CP_UploadRadarCoverage(void);
void CP_ShutdownOverlay(void);
void CP_CalcAndUploadDayAndNightTexture(float q);

#define OVERLAY_NATION		(1<<0)
#define OVERLAY_XVI			(1<<1)
#define OVERLAY_RADAR		(1<<2)
