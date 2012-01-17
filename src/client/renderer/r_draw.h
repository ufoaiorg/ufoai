/**
 * @file r_draw.h
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#ifndef R_DRAW_H
#define R_DRAW_H

void R_DrawImage(float x, float y, const image_t *image);
void R_DrawStretchImage(float x, float y, int w, int h, const image_t *image);
const image_t *R_DrawImageArray(const vec2_t texcoords[4], const vec2_t verts[4], const image_t *image);
void R_DrawChar(int x, int y, int c, uint32_t color);
void R_DrawChars(void);
void R_DrawFills(void);
void R_DrawFill(int x, int y, int w, int h, const vec4_t color);
void R_DrawRect(int x, int y, int w, int h, const vec4_t color, float lineWidth, int pattern);
int R_UploadData(const char *name, byte *frame, int width, int height);
void R_DrawTexture(int texnum, int x, int y, int w, int h);
void R_DrawLineStrip(int points, int *verts);
void R_DrawLineLoop(int points, int *verts);
void R_DrawLine(int *verts, float thickness);
void R_DrawCircle(float radius, const vec4_t color, float thickness, const vec3_t shift);
void R_DrawPolygon(int points, int *verts);
void R_PushClipRect(int x, int y, int width, int height);
void R_PopClipRect(void);
void R_CleanupDepthBuffer(int x, int y, int width, int height);
void R_DrawBoundingBox(const vec3_t absmins, const vec3_t absmaxs);
void R_DrawBoundingBoxBatched(const vec3_t absmins, const vec3_t absmaxs);
void R_DrawBoundingBoxes(void);
void R_DrawTexturedBox (const vec3_t absmins, const vec3_t absmaxs);

#endif
