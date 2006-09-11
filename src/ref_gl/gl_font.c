/**
 * @file gl_font.c
 * @brief font handling with SDL_ttf font engine
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

#include "gl_local.h"

#define BUF_SIZE 2048
static const SDL_Color color = { 255, 255, 255, 0 };	/* The 4. value is unused */

/* holds the gettext string */
static char buf[BUF_SIZE];
static int numInCache = 0;
static int firstTextureCache = 0;
static int lastTextureCache = 0;
fontRenderStyle_t fontStyle[] = {
	{"TTF_STYLE_NORMAL", TTF_STYLE_NORMAL},
	{"TTF_STYLE_BOLD", TTF_STYLE_BOLD},
	{"TTF_STYLE_ITALIC", TTF_STYLE_ITALIC},
	{"TTF_STYLE_UNDERLINE", TTF_STYLE_UNDERLINE}
};

/*============================================================== */

/**
 * @brief
 */
static GLuint Font_TextureAddToCache(SDL_Surface * s)
{
	int i;

	for (i = firstTextureCache; i != lastTextureCache; i++, i %= MAX_TEXTURE_CACHE)
		if (textureCache[i].surface == s)
			break;

	if (i == lastTextureCache) {
		lastTextureCache++;
		lastTextureCache %= MAX_TEXTURE_CACHE;

		if (lastTextureCache == firstTextureCache) {
			qglDeleteTextures(1, &textureCache[i].texture);
			textureCache[i].surface = 0;
			textureCache[i].texture = 0;

			firstTextureCache++;
			firstTextureCache %= MAX_TEXTURE_CACHE;
		}

		textureCache[i].surface = s;
		qglGenTextures(1, &textureCache[i].texture);
	}

	return textureCache[i].texture;
}

/**
 * @brief
 */
static void Font_TextureCleanCache(void)
{
	int i;

	for (i = firstTextureCache; i != lastTextureCache; i++, i %= MAX_TEXTURE_CACHE) {
		qglDeleteTextures(1, &textureCache[i].texture);
		textureCache[i].surface = 0;
		textureCache[i].texture = 0;
	}

	firstTextureCache = lastTextureCache = 0;
}

/**
 * @brief frees the SDL_ttf fonts
 */
void Font_Shutdown(void)
{
	int i;

	Font_CleanCache();
	Font_TextureCleanCache();

	for (i = 0; i < numFonts; i++)
		if (fonts[i].font)
			TTF_CloseFont(fonts[i].font);

	/* now quit SDL_ttf, too */
	TTF_Quit();
}

/**
 * @brief
 * @TODO: Check whether font is already loaded
 */
font_t *Font_Analyze(char *name, char *path, int renderStyle, int size)
{
	font_t *f = NULL;
	SDL_RWops *rw = NULL;
	void *buffer = NULL;
	int ttfSize;

	if (numFonts >= MAX_FONTS)
		return NULL;

	/* allocate new font */
	f = &fonts[numFonts];

	/* copy fontname */
	Q_strncpyz(f->name, name, MAX_VAR);

	ttfSize = ri.FS_LoadFile(path, &buffer);

	rw = SDL_RWFromMem(buffer, ttfSize);

	/* norm size is 1024x768 (1.0) */
	/* scale the fontsize */
	size *= vid.rx;

	f->font = TTF_OpenFontRW(rw, 0, size);
	if (!f->font)
		ri.Sys_Error(ERR_FATAL, "...could not load font file %s\n", path);

	/* font style */
	f->style = renderStyle;
	if (f->style)
		TTF_SetFontStyle(f->font, f->style);

	/* FIXME: We need to free this */
/* 	ri.FS_FreeFile( buffer ); */
/* 	SDL_RWclose( rw ); */

	numFonts++;
	f->lineSkip = TTF_FontLineSkip(f->font);
	f->height = TTF_FontHeight(f->font);

	/* return the font */
	return f;
}

/**
 * @brief
 */
static font_t *Font_GetFont(char *name)
{
	int i;

	for (i = 0; i < numFonts; i++)
		if (!Q_strncmp(name, fonts[i].name, MAX_FONTNAME))
			return &fonts[i];

	return NULL;
}

/**
 * @brief
 */
void Font_Length(char *font, char *c, int *width, int *height)
{
	font_t *f = NULL;

	if (width && height)
		*width = *height = 0;

	if (!c || !font)
		return;

	/* get the font */
	f = Font_GetFont(font);
	if (!f)
		return;
	TTF_SizeUTF8(f->font, c, width, height);
}

/**
 * @brief
 */
void Font_CleanCache(void)
{
	int i = 0;

	/* free the surfaces */
	for (; i < numInCache; i++)
		if (fontCache[i].pixel)
			SDL_FreeSurface(fontCache[i].pixel);

	memset(fontCache, 0, sizeof(fontCache));
	memset(hash, 0, sizeof(hash));
	numInCache = 0;
}

/**
 * @brief
 */
void Font_ListCache_f(void)
{
	int i = 0;
	int collCount = 0, collSum = 0;
	fontCache_t *f = NULL;

	ri.Con_Printf(PRINT_ALL, "Font cache info\n========================\n");
	ri.Con_Printf(PRINT_ALL, "...font cache size: %i - used %i\n", MAX_FONT_CACHE, numInCache);

	for (; i < numInCache; i++) {
		f = &fontCache[i];
		if (!f) {
			ri.Con_Printf(PRINT_ALL, "...hashtable inconsistency at %i\n", i);
			continue;
		}
		collCount = 0;
		while (f->next) {
			collCount++;
			f = f->next;
		}
		if (collCount)
			ri.Con_Printf(PRINT_ALL, "...%i collisions for %s\n", collCount, f->string);
		collSum += collCount;
	}
	ri.Con_Printf(PRINT_ALL, "...overall collisions %i\n", collSum);
}

/**
 * @brief
 */
static int Font_Hash(const char *string, int maxlen)
{
	int register hashValue, i;

	hashValue = 0;
	for (i = 0; i < maxlen && string[i] != '\0'; i++)
		hashValue += string[i] * (119 + i);

	hashValue = (hashValue ^ (hashValue >> 10) ^ (hashValue >> 20));
	return hashValue & (MAX_FONT_CACHE - 1);
}

/**
 * @brief
 */
static fontCache_t *Font_GetFromCache(const char *s)
{
	fontCache_t *font = NULL;
	int hashValue;

	hashValue = Font_Hash(s, MAX_HASH_STRING);
	for (font = hash[hashValue]; font; font = font->next)
		if (!Q_strncmp((char *) s, font->string, MAX_HASH_STRING))
			return font;

	return NULL;
}

/**
 * @brief We add the font string (e.g. f_small) to the beginning
 * of each string (char *s) because we can have the same strings
 * but other fonts.
 */
static fontCache_t* Font_AddToCache(const char *s, void *pixel, int w, int h)
{
	int hashValue;
	fontCache_t *font = NULL;

	if (numInCache >= MAX_FONT_CACHE) {
		Font_CleanCache();
		Font_TextureCleanCache();
	}

	hashValue = Font_Hash(s, MAX_HASH_STRING);
	if (hash[hashValue]) {
		font = hash[hashValue];
		/* go to end of list */
		while (font->next)
			font = font->next;
		font->next = &fontCache[numInCache];
	} else
		hash[hashValue] = &fontCache[numInCache];

	if (numInCache < MAX_FONT_CACHE) {
		Q_strncpyz(fontCache[numInCache].string, s, MAX_HASH_STRING);
		fontCache[numInCache].pixel = pixel;
		fontCache[numInCache].size[0] = w;
		fontCache[numInCache].size[1] = h;
	} else
		ri.Sys_Error(ERR_FATAL, "...font cache exceeded with %i\n", hashValue);

	numInCache++;
	return &fontCache[numInCache-1];
}

/**
 * @brief
 */
static fontCache_t *Font_GenerateCache(const char *s, const char *fontString, font_t * f)
{
	int w, h;
	SDL_Surface *textSurface = NULL;
	SDL_Surface *openGLSurface = NULL;
	SDL_Rect rect = { 0, 0, 0, 0 };

	textSurface = TTF_RenderUTF8_Blended(f->font, s, color);
	if (!textSurface) {
		ri.Con_Printf(PRINT_ALL, "%s (%s)\n", TTF_GetError(), fontString);
		return NULL;
	}

	/* convert to power of two */
	for (w = 1; w < textSurface->w; w <<= 1);
	for (h = 1; h < textSurface->h; h <<= 1);

	openGLSurface = SDL_CreateRGBSurface(SDL_SWSURFACE,
		w, h, 32, textSurface->format->Rmask, textSurface->format->Gmask,
		textSurface->format->Bmask, textSurface->format->Amask);

	rect.x = rect.y = 0;
	rect.w = textSurface->w;
	rect.h = textSurface->h;

	SDL_SetAlpha(textSurface, 0, 0);

	SDL_LowerBlit(textSurface, &rect, openGLSurface, &rect);
	w = textSurface->w;
	h = textSurface->h;
	SDL_FreeSurface(textSurface);

/* 	SDL_SaveBMP( openGLSurface, buffer ); */
	return Font_AddToCache(fontString, openGLSurface, w, h);
}

/**
 * @brief
 * @param[in] maxWidth is a pixel value
 * FIXME: if maxWidth is too small to display even the first word this has bugs
 */
static char *Font_GetLineWrap(font_t * f, char *buffer, int maxWidth, int *width, int *height)
{
	char *space = NULL;
	char *newlineTest = NULL;
	int w = 0, oldW = 0;
	int h = 0;

	/* TTF does not like empty strings... */
	assert (buffer);
	assert (strlen(buffer));

	if (!maxWidth)
		maxWidth = VID_NORM_WIDTH * vid.rx;

	/* no line wrap needed? */
	TTF_SizeUTF8(f->font, buffer, &w, &h);
	if (!w)
		return NULL;

	*width = w;
	*height = h;

	if (w < maxWidth)
		return NULL;

	space = buffer;
	newlineTest = strstr(space, "\n");
	if (newlineTest) {
		*newlineTest = '\0';
		TTF_SizeUTF8(f->font, buffer, &w, &h);
		*width = w;
		if (w < maxWidth)
			return newlineTest + 1;
		*newlineTest = '\n';
	}

	/* uh - this line is getting longer than allowed... */
	newlineTest = space;
	while ((space = strstr(space, " ")) != NULL) {
		*space = '\0';
		TTF_SizeUTF8(f->font, buffer, &w, &h);
		*width = w;
		if (maxWidth - w < 0) {
			*width = oldW;
			*space = ' ';
			*newlineTest = '\0';
			return newlineTest + 1;
		} else if (maxWidth - w == 0)
			return space + 1;
		newlineTest = space;
		oldW = w;
		*space++ = ' ';
		/* maybe there is space for one more word? */
	};

	return NULL;
}

/**
 * @brief generate the opengl texture out of the sdl-surface, bind it, and draw it
 * FIXME: Make scrolling possible
 * @param[in] s SDL surface pointer which holds the image/text
 * @param[in] x x coordinate on screen to draw the text to
 * @param[in] y y coordinate on screen to draw the text to
 * @param[in] absX This is the absolute x coodinate (e.g. of a node)
 * @param[in] absY This is the absolute y coodinate (e.g. of a node)
 * y coordinates will change for each linebreak - whereas the absY will be fix
 * @param[in] width The max width of the text
 * @param[in] height The max height of the text
 * @return -1 for scrolling down (TODO)
 * @return +1 for scrolling up (TODO)
 */
static int Font_GenerateGLSurface(fontCache_t *cache, int x, int y, int absX, int absY, int width, int height)
{
	GLuint texture = Font_TextureAddToCache(cache->pixel);
	int h = cache->size[1];
	vec2_t start = {0.0f, 0.0f}, end = {1.0f, 1.0f};

	/* Tell GL about our new texture */
	GL_Bind(texture);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cache->pixel->w, cache->pixel->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, cache->pixel->pixels);

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* if height is too much we shoudl be able to scroll down */
	if (height > 0 && y+h > absY+height)
		return 1;

	/* draw it */
	qglEnable(GL_BLEND);

	qglBegin(GL_TRIANGLE_STRIP);
	qglTexCoord2f(start[0], start[1]);
	qglVertex2f(x, y);
	qglTexCoord2f(end[0], start[1]);
	qglVertex2f(x + cache->pixel->w, y);
	qglTexCoord2f(start[0], end[1]);
	qglVertex2f(x, y + cache->pixel->h);
	qglTexCoord2f(end[0], end[1]);
	qglVertex2f(x + cache->pixel->w, y + cache->pixel->h);
	qglEnd();

	qglDisable(GL_BLEND);

	qglFinish();
	return 0;
}

/**
 * @brief
 */
static void Font_ConvertChars(char *buffer)
{
	char *replace = NULL;

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
	replace = strstr(buffer, "\n\0");
	if (replace)
		*replace = '\0';
}

/**
 * @brief
 */
int Font_DrawString(char *fontID, int align, int x, int y, int absX, int absY, int maxWidth, int maxHeight, const int lineHeight, const char *c)
{
	int w = 0, h = 0, locX;
	float returnHeight = 0; /* rounding errors break mouse-text corelation */
	font_t *f = NULL;
	char *buffer = buf;
	char *pos;
	fontCache_t *cache;
	static char searchString[MAX_FONTNAME + MAX_HASH_STRING];
	int max = 0;				/* calculated maxWidth */
	float texh0, fh, fy; /* rounding errors break mouse-text corelation */

	/* transform from 1024x768 coordinates for drawing */
	absX = (float) absX *vid.rx;
	absY = (float) absY *vid.ry;
	x = (float) x *vid.rx;
	fy = (float) y *vid.ry;
	maxWidth = (float) maxWidth *vid.rx;
	maxHeight = (float) maxHeight *vid.ry;
	texh0 = (float) lineHeight *vid.ry;

	/* get the font */
	f = Font_GetFont(fontID);
	if (!f) {
		ri.Sys_Error(ERR_FATAL, "...could not find font: %s\n", fontID);
		return 0;				/* never reached. need for code analyst. */
	}

	cache = Font_GetFromCache(c);
	if (cache) { /* TODO: check that cache.font = fontID and that texh0 was the same */
		Font_GenerateGLSurface(cache, x, fy, absX, absY, maxWidth, maxHeight);
		return lineHeight;
	}

	Q_strncpyz(buffer, c, BUF_SIZE);

	Font_ConvertChars(buf);
	/* for linebreaks */
	locX = x;

	do {
		/* TTF does not like empty strings... */
		if (!strlen(buffer))
			return returnHeight / vid.ry;

		pos = Font_GetLineWrap(f, buffer, maxWidth, &w, &h);
		fh = h;

		if (texh0 > 0) {
			if (fh > texh0)
/* something is broken with that warning, please test
				ri.Con_Printf(PRINT_DEVELOPER, "Warning: font %s height=%f bigger than allowed line height=%f.\n", fontID, fh, texh0);
*/
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

		/* This will cut down the string to 160 chars */
		/* NOTE: There can be a non critical overflow in Com_sprintf */
		Com_sprintf(searchString, MAX_FONTNAME + MAX_HASH_STRING, "%s%s", fontID, buffer);
		cache = Font_GetFromCache(searchString);
		if (!cache)
			cache = Font_GenerateCache(buffer, searchString, f);

		if (!cache)
			ri.Sys_Error(ERR_FATAL, "...could not generate font surface\n");

		Font_GenerateGLSurface(cache, x, fy, absX, absY, maxWidth, maxHeight);

		/* skip for next line */
		fy += fh;
		buffer = pos;
		returnHeight += (texh0 > 0) ? texh0 : h;
		x = locX;
	} while (buffer);

	return returnHeight / vid.ry;
}

/**
 * @brief
 */
void Font_Init(void)
{
#ifdef SDL_TTF_VERSION
	SDL_version version;

	SDL_TTF_VERSION(&version);
	ri.Con_Printf(PRINT_ALL, "...SDL_ttf version %i.%i.%i - we need at least 2.0.7\n",
		version.major,
		version.minor,
		version.patch);
#else
	ri.Con_Printf(PRINT_ALL, "...could not get SDL_ttf version - we need at least 2.0.7\n");
#endif

	numFonts = 0;
	memset(fonts, 0, sizeof(fonts));

	numInCache = 0;
	memset(fontCache, 0, sizeof(fontCache));
	memset(hash, 0, sizeof(hash));

	firstTextureCache = 0;
	lastTextureCache = 0;
	memset(textureCache, 0, sizeof(textureCache));

	/* init the truetype font engine */
	if (TTF_Init())
		ri.Sys_Error(ERR_FATAL, "...SDL_ttf error: %s\n", TTF_GetError());

	ri.Con_Printf(PRINT_ALL, "...SDL_ttf inited\n");
	atexit(TTF_Quit);
}

/**
 * @brief
 */
void Font_Register(char *name, int size, char *path, char *style)
{
	int renderstyle = 0;		/* NORMAL is standard */
	int i;

	if (style && *style)
		for (i = 0; i < NUM_FONT_STYLES; i++)
			if (!Q_stricmp(fontStyle[i].name, style)) {
				renderstyle = fontStyle[i].renderStyle;
				break;
			}

	Font_Analyze(name, path, renderstyle, size);
}
