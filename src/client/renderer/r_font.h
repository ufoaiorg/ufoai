/**
 * @file r_font.h
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

#ifndef R_FONTS_H
#define R_FONTS_H

/* starting offset for texture numbers used in text chunk cache */
#define TEXNUM_FONTS		(TEXNUM_DELUXEMAPS + MAX_GL_DELUXEMAPS)

typedef struct font_s {
	const char *name;
	TTF_Font *font;
	SDL_RWops *rw;				/**< ttf font reading structure for SDL_RWops */
	byte *buffer;				/**< loaded file */
	int style;					/**< see also fontRenderStyle_t */
	int lineSkip;				/**< TTF_FontLineSkip */
	int height;
} font_t;

/* public */
void R_FontShutdown(void);
void R_FontInit(void);
void R_FontListCache_f(void);

void R_FontSetTruncationMarker(const char *marker);
void R_FontTextSize(const char *fontId, const char *text, int maxWidth, longlines_t method, int *width, int *height, int *lines, qboolean *isTruncated);
int R_FontDrawString(const char *fontID, int align, int x, int y, int absX, int absY, int maxWidth, int maxHeight, const int lineHeight, const char *c, int box_height, int scroll_pos, int *cur_line, qboolean increaseLine, longlines_t method);

#endif
