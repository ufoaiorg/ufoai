/**
 * @file r_font.c
 * @brief font handling with SDL_ttf font engine
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "r_local.h"
#include "r_font.h"
#include "r_error.h"

static const SDL_Color color = { 255, 255, 255, 0 };	/* The 4. value is unused */
static int numFonts = 0;

static fontCache_t fontCache[MAX_FONT_CACHE];
static fontCache_t *hash[MAX_FONT_CACHE];

static font_t fonts[MAX_FONTS];

/* holds the gettext string */
static char buf[BUF_SIZE];
static int numInCache = 0;
static const fontRenderStyle_t fontStyle[] = {
	{"TTF_STYLE_NORMAL", TTF_STYLE_NORMAL},
	{"TTF_STYLE_BOLD", TTF_STYLE_BOLD},
	{"TTF_STYLE_ITALIC", TTF_STYLE_ITALIC},
	{"TTF_STYLE_UNDERLINE", TTF_STYLE_UNDERLINE}
};

/*============================================================== */


/**
 * @brief Clears font cache and frees memory associated with the cache
 */
static void R_FontCleanCache (void)
{
	int i, count = 0;

	R_CheckError();

	/* free the surfaces */
	for (i = 0; i < numInCache; i++) {
		if (!fontCache[i].texPos)
			continue;
		qglDeleteTextures(1, &(fontCache[i].texPos));
		count++;
		R_CheckError();
	}

	memset(fontCache, 0, sizeof(fontCache));
	memset(hash, 0, sizeof(hash));
	numInCache = 0;
}

/**
 * @brief frees the SDL_ttf fonts
 * @sa R_FontCleanCache
 */
void R_FontShutdown (void)
{
	int i;

	R_FontCleanCache();

	for (i = 0; i < numFonts; i++)
		if (fonts[i].font) {
			TTF_CloseFont(fonts[i].font);
			FS_FreeFile(fonts[i].buffer);
			SDL_RWclose(fonts[i].rw);
		}

	/* now quit SDL_ttf, too */
	TTF_Quit();
}

/**
 * @todo Check whether font is already loaded
 */
static font_t *R_FontAnalyze (const char *name, const char *path, int renderStyle, int size)
{
	font_t *f;
	int ttfSize;

	if (numFonts >= MAX_FONTS)
		return NULL;

	/* allocate new font */
	f = &fonts[numFonts];
	memset(f, 0, sizeof(*f));

	/* copy fontname */
	f->name = name;

	ttfSize = FS_LoadFile(path, &f->buffer);

	f->rw = SDL_RWFromMem(f->buffer, ttfSize);

	f->font = TTF_OpenFontRW(f->rw, 0, size);
	if (!f->font)
		Sys_Error("...could not load font file %s\n", path);

	/* font style */
	f->style = renderStyle;
	if (f->style)
		TTF_SetFontStyle(f->font, f->style);

	numFonts++;
	f->lineSkip = TTF_FontLineSkip(f->font);
	f->height = TTF_FontHeight(f->font);

	/* return the font */
	return f;
}

/**
 * @brief Searches the array of available fonts (see fonts.ufo)
 * @return font_t pointer or NULL
 */
static font_t *R_FontGetFont (const char *name)
{
	int i;

	for (i = 0; i < numFonts; i++)
		if (!Q_strncmp(name, fonts[i].name, MAX_FONTNAME))
			return &fonts[i];

	return NULL;
}

/**
 * @param[in] maxWidth is a pixel value
 * @todo if maxWidth is too small to display even the first word this has bugs
 */
static char *R_FontGetLineWrap (const font_t * f, char *buffer, int maxWidth, int *width, int *height)
{
	char *space;
	char *newlineTest;
	int w = 0, oldW = 0;
	int h = 0;

	/* TTF does not like empty strings... */
	assert(buffer);
	assert(strlen(buffer));

	if (!maxWidth)
		maxWidth = VID_NORM_WIDTH;

	/* no line wrap needed? */
	TTF_SizeUTF8(f->font, buffer, &w, &h);
	if (!w)
		return NULL;

	*width = w;
	*height = h;

	/* string fits */
	if (w <= maxWidth)
		return NULL;

	space = buffer;
	newlineTest = strstr(space, "\n");
	/* try to break at a newline */
	if (newlineTest) {
		*newlineTest = '\0';
		TTF_SizeUTF8(f->font, buffer, &w, &h);
		*width = w;
		/* Fine, the whole line (up to \n) has a length smaller than maxwidth. */
		if (w < maxWidth)
			return newlineTest + 1;
		/* Ok, doesn't fit - revert the change. */
		*newlineTest = '\n';
	}

	/* uh - this line is getting longer than allowed... */
	space = newlineTest = buffer;
	while ((space = strstr(space, " ")) != NULL) {
		*space = '\0';
		TTF_SizeUTF8(f->font, buffer, &w, &h);
		*width = w;
		/* otherwise even the first work would be too long */
		if (w > maxWidth) {
			*width = oldW;
			*space = ' ';
			*newlineTest = '\0';
			return newlineTest + 1;
		} else if (maxWidth == w)
			return space + 1;
		newlineTest = space;
		oldW = w;
		*space++ = ' ';
		/* maybe there is space for one more word? */
	};

	TTF_SizeUTF8(f->font, buffer, &w, &h);
	if (w > maxWidth) {
		/* last word - no more spaces but still to long? */
		*newlineTest = '\0';
		return newlineTest + 1;
	}
	return NULL;
}

/**
 * @sa R_FontGetFont
 */
void R_FontLength (const char *font, char *c, int *width, int *height)
{
	font_t *f;

	if (width && height)
		*width = *height = 0;

	if (!c || !*c || !font)
		return;

	/* get the font */
	f = R_FontGetFont(font);
	if (!f)
		return;
	R_FontGetLineWrap(f, c, VID_NORM_WIDTH, width, height);
}

/**
 * @brief Console command binding to show the font cache
 */
void R_FontListCache_f (void)
{
	int i = 0;
	int collCount = 0, collSum = 0;

	Com_Printf("Font cache info\n========================\n");
	Com_Printf("...font cache size: %i - used %i\n", MAX_FONT_CACHE, numInCache);

	for (; i < numInCache; i++) {
		const fontCache_t *f = &fontCache[i];
		if (!f) {
			Com_Printf("...hashtable inconsistency at %i\n", i);
			continue;
		}
		collCount = 0;
		while (f->next) {
			collCount++;
			f = f->next;
		}
		if (collCount)
			Com_Printf("...%i collisions for %s\n", collCount, f->string);
		collSum += collCount;
	}
	Com_Printf("...overall collisions %i\n", collSum);
}

/**
 * @param[in] string String to build the hash value for
 * @param[in] maxlen Max length that should be used to calculate the hash value
 * @return hash value for given string
 */
static int R_FontHash (const char *string, int maxlen)
{
	register int hashValue, i;

	hashValue = 0;
	for (i = 0; i < maxlen && string[i] != '\0'; i++)
		hashValue += string[i] * (119 + i);

	hashValue = (hashValue ^ (hashValue >> 10) ^ (hashValue >> 20));
	return hashValue & (MAX_FONT_CACHE - 1);
}

/**
 * @brief Searches the given string in cache
 * @return NULL if not found
 * @sa R_FontHash
 */
static fontCache_t *R_FontGetFromCache (const char *s)
{
	fontCache_t *font;
	int hashValue;

	hashValue = R_FontHash(s, MAX_HASH_STRING);
	for (font = hash[hashValue]; font; font = font->next)
		if (!Q_strncmp(s, font->string, MAX_HASH_STRING))
			return font;

	return NULL;
}

/**
 * @brief generate a new opengl texture out of the sdl-surface, bind and cache it
 */
static void R_FontCacheGLSurface (fontCache_t *cache, SDL_Surface *pixel)
{
	/* use a fixed texture number allocation scheme, starting offset at TEXNUM_FONTS */
	cache->texPos = TEXNUM_FONTS + numInCache;
	R_BindTexture(cache->texPos);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixel->w, pixel->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixel->pixels);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	R_CheckError();
}

/**
 * @brief We add the font string (e.g. f_small) to the beginning
 * of each string (char *s) because we can have the same strings
 * but other fonts.
 * @sa R_FontGenerateCache
 * @sa R_FontCleanCache
 * @sa R_FontHash
 * @sa R_FontCacheGLSurface
 */
static fontCache_t* R_FontAddToCache (const char *s, SDL_Surface *pixel, int w, int h)
{
	int hashValue;

	if (numInCache >= MAX_FONT_CACHE)
		R_FontCleanCache();

	hashValue = R_FontHash(s, MAX_HASH_STRING);
	if (hash[hashValue]) {
		fontCache_t *font = hash[hashValue];
		/* go to end of list */
		while (font->next)
			font = font->next;
		font->next = &fontCache[numInCache];
	} else
		hash[hashValue] = &fontCache[numInCache];

	if (numInCache < MAX_FONT_CACHE) {
		Q_strncpyz(fontCache[numInCache].string, s, MAX_HASH_STRING);
		fontCache[numInCache].size[0] = w;
		fontCache[numInCache].size[1] = h;
		fontCache[numInCache].texsize[0] = pixel->w;
		fontCache[numInCache].texsize[1] = pixel->h;
		R_FontCacheGLSurface(&fontCache[numInCache], pixel);
	} else
		Sys_Error("...font cache exceeded with %i\n", hashValue);

	numInCache++;
	return &fontCache[numInCache - 1];
}

/**
 * @brief Renders the text surface and coverts to 32bit SDL_Surface that is stored in font_t structure
 * @todo maybe 32bit is overkill if the game is only using 16bit?
 * @sa R_FontAddToCache
 * @sa TTF_RenderUTF8_Blended
 * @sa SDL_CreateRGBSurface
 * @sa SDL_LowerBlit
 * @sa SDL_FreeSurface
 */
static fontCache_t *R_FontGenerateCache (const char *s, const char *fontString, const font_t *f, int maxWidth)
{
	int w, h;
	SDL_Surface *textSurface;
	SDL_Surface *openGLSurface;
	SDL_Rect rect = { 0, 0, 0, 0 };
	fontCache_t *result;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	Uint32 rmask = 0xff000000;
	Uint32 gmask = 0x00ff0000;
	Uint32 bmask = 0x0000ff00;
	Uint32 amask = 0x000000ff;
#else
	Uint32 rmask = 0x000000ff;
	Uint32 gmask = 0x0000ff00;
	Uint32 bmask = 0x00ff0000;
	Uint32 amask = 0xff000000;
#endif

	textSurface = TTF_RenderUTF8_Blended(f->font, s, color);
	if (!textSurface) {
		Com_Printf("%s (%s)\n", TTF_GetError(), fontString);
		return NULL;
	}

	/* convert to power of two */
	for (w = 2; w < textSurface->w; w <<= 1);
	for (h = 2; h < textSurface->h; h <<= 1);

	openGLSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, rmask, gmask, bmask, amask);
	if (!openGLSurface)
		return NULL;

	rect.x = rect.y = 0;
	rect.w = textSurface->w;
	if (maxWidth > 0 && textSurface->w > maxWidth)
		rect.w = maxWidth;
	rect.h = textSurface->h;

	/* ignore alpha when blitting - just copy it over */
	SDL_SetAlpha(textSurface, 0, 255);

	SDL_LowerBlit(textSurface, &rect, openGLSurface, &rect);
	w = textSurface->w;
	h = textSurface->h;
	SDL_FreeSurface(textSurface);

	result = R_FontAddToCache(fontString, openGLSurface, w, h);
	SDL_FreeSurface(openGLSurface);
	return result;
}

/**
 * @brief draw cached opengl texture
 * @param[in] cache
 * @param[in] x x coordinate on screen to draw the text to
 * @param[in] y y coordinate on screen to draw the text to
 * @param[in] absY This is the absolute y coodinate (e.g. of a node)
 * y coordinates will change for each linebreak - whereas the absY will be fix
 * @param[in] width The max width of the text
 * @param[in] height The max height of the text
 */
static int R_FontGenerateGLSurface (const fontCache_t *cache, int x, int y, int absY, int width, int height)
{
	float nx = x * viddef.rx;
	float ny = y * viddef.ry;
	float nw = cache->texsize[0] * viddef.rx;
	float nh = cache->texsize[1] * viddef.ry;

	/* if height is too much we should be able to scroll down */
	if (height > 0 && y + cache->size[1] > absY + height)
		return 1;

	R_BindTexture(cache->texPos);

	/* draw it */
	R_EnableBlend(qtrue);

	qglVertexPointer(2, GL_SHORT, 0, r_state.vertex_array_2d);

	memcpy(&texunit_diffuse.texcoord_array, default_texcoords, sizeof(float) * 8);

	r_state.vertex_array_2d[0] = nx;
	r_state.vertex_array_2d[1] = ny;
	r_state.vertex_array_2d[2] = nx + nw;
	r_state.vertex_array_2d[3] = ny;

	r_state.vertex_array_2d[4] = nx;
	r_state.vertex_array_2d[5] = ny + nh;
	r_state.vertex_array_2d[6] = nx + nw;
	r_state.vertex_array_2d[7] = ny + nh;

	qglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	/* set back to standard 3d pointer */
	qglVertexPointer(3, GL_FLOAT, 0, r_state.vertex_array_3d);

	R_EnableBlend(qfalse);

	return 0;
}

static void R_FontConvertChars (char *buffer)
{
	char *replace;

	/* convert all \\ to \n */
	replace = strstr(buffer, "\\");
	while (replace != NULL) {
		*replace++ = '\n';
		replace = strstr(replace, "\\");
	}

	/* convert all tabs to spaces */
	replace = strstr(buffer, "\t");
	while (replace != NULL) {
		*replace++ = ' ';
		replace = strstr(replace, "\t");
	}
}

static fontCacheList_t cacheList;
/**
 * @param[in] fontID the font id (defined in ufos/fonts.ufo)
 * @param[in] x Current x position (may differ from absX due to tabs e.g.)
 * @param[in] y Current y position (may differ from absY due to linebreaks)
 * @param[in] absX Absolute x value for this string
 * @param[in] absY Absolute y value for this string
 * @param[in] maxWidth Max width - relative from absX
 * @param[in] maxHeight Max height - relative from absY
 * @param[in] lineHeight The lineheight of that node
 * @param[in] c The string to draw
 * @param[in] scroll_pos Starting line in this node (due to scrolling)
 * @param[in] cur_line Current line (see lineHeight)
 * @param[in] increaseLine If true cur_line is increased with every linebreak
 * @note the x, y, width and height values are all normalized here - don't use the
 * viddef settings for drawstring calls - make them all relative to VID_NORM_WIDTH
 * and VID_NORM_HEIGHT
 */
int R_FontDrawString (const char *fontID, int align, int x, int y, int absX, int absY, int maxWidth, int maxHeight,
	const int lineHeight, const char *c, int box_height, int scroll_pos, int *cur_line, qboolean increaseLine)
{
	const int returnHeight = R_FontGenerateCacheList(fontID, align, x, y,
		absX, absY, maxWidth, lineHeight, c, box_height, scroll_pos, cur_line,
		increaseLine, &cacheList);

	R_FontRenderCacheList(&cacheList, absY, maxWidth, maxHeight, 0, 0);

	return returnHeight;
}

/**
 * @param[in] fontID the font id (defined in ufos/fonts.ufo)
 * @param[in] x Current x position (may differ from absX due to tabs e.g.)
 * @param[in] y Current y position (may differ from absY due to linebreaks)
 * @param[in] absX Absolute x value for this string
 * @param[in] absY Absolute y value for this string
 * @param[in] maxWidth Max width - relative from absX
 * @param[in] lineHeight The lineheight of that node
 * @param[in] c The string to draw
 * @param[in] scroll_pos Starting line in this node (due to scrolling)
 * @param[in] cur_line Current line (see lineHeight)
 * @param[in] increaseLine If true cur_line is increased with every linebreak
 * @param[out] cacheList A List of fontCache_t pointers will be created that can be drawwn later on.
 * @note the x, y, width and height values are all normalized here - don't use the
 * viddef settings for drawstring calls - make them all relative to VID_NORM_WIDTH
 * and VID_NORM_HEIGHT
 * @todo replace R_FontDrawString with this function.
 */
int R_FontGenerateCacheList (const char *fontID, int align, int x, int y, int absX, int absY, int maxWidth,
	const int lineHeight, const char *c, int box_height, int scroll_pos, int *cur_line, qboolean increaseLine, fontCacheList_t *cacheList)
{
	int w = 0, h = 0, locX;
	float returnHeight = 0; /* Rounding errors break mouse-text correlation. */
	const font_t *f;
	char *buffer = buf;
	char *pos;
	static char searchString[MAX_FONTNAME + MAX_HASH_STRING];
	int max = 0;				/* calculated maxWidth */
	int line = 0;
	float texh0, fh, fy; /* Rounding errors break mouse-text correlation. */

	fy = y;
	texh0 = lineHeight;

	/* get the font */
	f = R_FontGetFont(fontID);
	if (!f)
		Sys_Error("...could not find font: %s\n", fontID);

	if (!cacheList)
		Sys_Error("...no pointer to cachelist given!\n");

	if (cacheList->numCaches >= MAX_FONTCACHE_ENTRIES)
		Sys_Error("...out of space in cachelist!\n");

	/* Init Cachelist */
	cacheList->numCaches = 0;
	cacheList->height = 0;
	cacheList->width = 0;

	cacheList->cache[cacheList->numCaches] = R_FontGetFromCache(c);
	if (cacheList->cache[cacheList->numCaches]) { /** @todo check that cache.font = fontID and that texh0 was the same */
		if (cur_line) {
			/* Com_Printf("h %i - s %i - l %i\n", box_height, scroll_pos, *cur_line); */
			if (increaseLine)
				(*cur_line)++; /* Increment the number of processed lines (overall). */
			line = *cur_line;

			if (box_height > 0 && line > box_height + scroll_pos) {
				/* Due to scrolling this line and the following are not visible */
				return -1;
			}
		}
		cacheList->height += cacheList->cache[cacheList->numCaches]->size[1];
		if (cacheList->width < cacheList->cache[cacheList->numCaches]->size[0])
			cacheList->width = cacheList->cache[cacheList->numCaches]->size[0];
		cacheList->posX[cacheList->numCaches] = x;
		cacheList->posY[cacheList->numCaches] = fy;
		cacheList->numCaches++;
		/* R_FontGenerateGLSurface(cache, x, fy, absY, maxWidth, maxHeight);*/
		return lineHeight;
	}


	Q_strncpyz(buffer, c, BUF_SIZE);
	buffer = strtok(buf, "\n");

	R_FontConvertChars(buf);
	/* for linebreaks */
	locX = x;

	do {
		qboolean skipline = qfalse;
		if (cur_line) {
			/* Com_Printf("h %i - s %i - l %i\n", box_height, scroll_pos, *cur_line); */
			if (increaseLine)
				(*cur_line)++; /* Increment the number of processed lines (overall). */
			line = *cur_line;

			if (box_height > 0 && line > box_height + scroll_pos) {
				/* due to scrolling this line and the following are not visible, but we need to continue to get a proper line count */
				skipline = qtrue;
			}
			if (line <= scroll_pos) {
				/* Due to scrolling this line is not visible. See if (!skipline)" code below.*/
				skipline = qtrue;
				/*Com_Printf("skipline line: %i scroll_pos: %i\n", line, scroll_pos);*/
			}
		}

		/* TTF does not like empty strings... */
		if (!buffer || !strlen(buffer))
			return returnHeight;

		pos = R_FontGetLineWrap(f, buffer, maxWidth - (x - absX), &w, &h);

		fh = h;

		if (texh0 > 0) {
			/** @todo something is broken with that warning, please test */
#if 0
			if (fh > texh0)
				Com_DPrintf(DEBUG_RENDERER, "Warning: font %s height=%f bigger than allowed line height=%f.\n", fontID, fh, texh0);
#endif
			fh = texh0; /* some extra space below the line */
		}

		/* check whether this line is bigger than every other */
		if (w > max)
			max = w;

		if (align > 0 && align < ALIGN_LAST) {
			switch (align % 3) {
			case 1:
				x -= w / 2;
				break;
			case 2:
				x -= w;
				break;
			}

			/* Warning: this works OK only for single-line texts! */
			switch (align / 3) {
			case 1:
				fy -= fh / 2;
				fh += fh / 2;
				break;
			case 2:
				fy -= fh;
				fh += fh;
				break;
			}
		}

		if (!skipline && strlen(buffer)) {
			/* This will cut down the string to 160 chars */
			/* NOTE: There can be a non critical overflow in Com_sprintf */
			Com_sprintf(searchString, sizeof(searchString), "%s%s", fontID, buffer);

			if (cacheList->numCaches >= MAX_FONTCACHE_ENTRIES)
				Sys_Error("...out of space in cachelist!\n");

			cacheList->cache[cacheList->numCaches] = R_FontGetFromCache(searchString);
			if (!cacheList->cache[cacheList->numCaches])
				cacheList->cache[cacheList->numCaches] = R_FontGenerateCache(buffer, searchString, f, maxWidth);

			if (!cacheList->cache[cacheList->numCaches]) {
				/* maybe we are running out of mem */
				R_FontCleanCache();
				cacheList->cache[cacheList->numCaches] = R_FontGenerateCache(buffer, searchString, f, maxWidth);
			}
			if (!cacheList->cache[cacheList->numCaches])
				Sys_Error("...could not generate font surface '%s'\n", buffer);

			cacheList->height += cacheList->cache[cacheList->numCaches]->size[1];
			if (cacheList->width < cacheList->cache[cacheList->numCaches]->size[0])
				cacheList->width = cacheList->cache[cacheList->numCaches]->size[0];
			cacheList->posX[cacheList->numCaches] = x;
			cacheList->posY[cacheList->numCaches] = fy;
			cacheList->numCaches++;
			/* R_FontGenerateGLSurface(cache, x, fy, absY, maxWidth, maxHeight); */

			fy += fh;
			returnHeight += (texh0 > 0) ? texh0 : h;
		}

		/* skip for next line */
		if (pos) {
			/* We have to break this line */
			buffer = pos;
		} else {
			/* Check if this line has any linebreaks and update buffer if that is the case. */
			buffer = strtok(NULL, "\n");
		}
		x = locX;
	} while (buffer);

	return returnHeight;
}

/**
 * @brief Render all the entries in a list of caches (e.g. generated in R_FontDrawString)
 * @param[in] cacheList A List of fontCache_t pointers that will be drawn.
 * @param[in] absY Absolute y value for the string(s).
 * @param[in] maxWidth Max width - relative from absX (as used in R_FontGenerateCacheList)
 * @param[in] maxHeight Max height - relative from absY
 * @param[in] dx Modifier to displace strings by a relative x value.
 * @param[in] dy Modifier to displace strings by a relative y value.
 */
void R_FontRenderCacheList (const fontCacheList_t *cacheList, int absY, int maxWidth, int maxHeight, int dx, int dy)
{
	int i;

	if (!cacheList)
		Sys_Error("...no pointer to cachelist given!\n");

	for (i = 0; i < cacheList->numCaches; i++) {
		if (cacheList->cache[i]) {
			R_FontGenerateGLSurface(cacheList->cache[i],
				cacheList->posX[i] + dx,
				cacheList->posY[i] + dy,
				absY,
				maxWidth,
				maxHeight);
		} else {
			Sys_Error("...no font-cache pointer found!\n");
		}
	}
}

void R_FontInit (void)
{
#ifdef SDL_TTF_VERSION
	SDL_version version;

	SDL_TTF_VERSION(&version);
	Com_Printf("SDL_ttf version %i.%i.%i - we need at least 2.0.7\n",
		version.major,
		version.minor,
		version.patch);
#else
	Com_Printf("could not get SDL_ttf version - we need at least 2.0.7\n");
#endif

	numFonts = 0;
	memset(fonts, 0, sizeof(fonts));

	numInCache = 0;
	memset(fontCache, 0, sizeof(fontCache));
	memset(hash, 0, sizeof(hash));

	/* init the truetype font engine */
	if (TTF_Init() == -1)
		Sys_Error("SDL_ttf error: %s\n", TTF_GetError());
}

void R_FontRegister (const char *name, int size, const char *path, const char *style)
{
	int renderstyle = 0;		/* NORMAL is standard */
	int i;

	if (style && style[0] != '\0')
		for (i = 0; i < NUM_FONT_STYLES; i++)
			if (!Q_stricmp(fontStyle[i].name, style)) {
				renderstyle = fontStyle[i].renderStyle;
				break;
			}

	R_FontAnalyze(name, path, renderstyle, size);
}
