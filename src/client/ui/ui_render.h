/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

#include "ui_nodes.h"
#include "../cl_renderer.h"

const struct image_s* UI_LoadImage(const char* name);
const struct image_s* UI_LoadWrappedImage(const char* name);

void UI_DrawNormImage(bool flip, float x, float y, float w, float h, float sh, float th, float sl, float tl, const struct image_s* image);
const image_t* UI_DrawNormImageByName(bool flip, float x, float y, float w, float h, float sh, float th, float sl, float tl, const char* name);

void UI_DrawPanel(const vec2_t pos, const vec2_t size, const char* texture, int texX, int texY, const int panelDef[7]);
void UI_DrawBorderedPanel (const vec2_t pos, const vec2_t size, const char* texture, int texX, int texY, int texW, int texH, int border);
void UI_DrawFill(int x, int y, int w, int h, const vec4_t color);
int UI_DrawStringInBox(const char* fontID, align_t align, int x, int y, int width, int height, const char* text, longlines_t method = LONGLINES_PRETTYCHOP);
int UI_DrawString(const char* fontID, align_t align, int x, int y, int absX, int maxWidth, const int lineHeight, const char* c, int box_height = 0, int scroll_pos = 0, int* cur_line = nullptr, bool increaseLine = false, longlines_t method = LONGLINES_WRAP);
void UI_Transform(const vec3_t transform, const vec3_t rotate, const vec3_t scale);
void UI_DrawRect(int x, int y, int w, int h, const vec4_t color, float lineWidth, int pattern);
void UI_PushClipRect(int x, int y, int width, int height);
void UI_PopClipRect(void);

void UI_EnableFlashing(const vec4_t flashingColor, float speed = 1);
void UI_DisableFlashing(void);
