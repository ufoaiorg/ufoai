/**
 * @file r_overlay.h
 * @brief Functions to generate and render overlay for geoscape
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

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


void R_DecreaseXVILevelEverywhere(void);
void R_ChangeXVILevel(const vec2_t pos, float factor);
void R_InitializeXVIOverlay(const byte *data);
void R_InitOverlay(void);
void R_GetXVIMapDimensions(int *width, int *height);
int R_GetXVILevel(int x, int y);
void R_SetXVILevel(int x, int y, int value);
void R_InitializeRadarOverlay(qboolean source);
void R_AddRadarCoverage(const vec2_t pos, float innerRadius, float outerRadius, qboolean source);
void R_UploadRadarCoverage(void);
void R_ShutdownOverlay(void);

extern image_t *r_xviTexture;
extern image_t *r_radarTexture;
