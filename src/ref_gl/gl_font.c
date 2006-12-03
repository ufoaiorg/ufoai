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

static cvar_t* gl_fontcache;

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
 * @brief Adds a new SDL_Surface to texture cache
 * @param[in] s Surface that should be added to cache
 * @sa Font_GenerateGLSurface
 * @sa Font_CleanCache
 * @return texture id in OpenGL context
 * @note a SDL_Surface won't be added twice to the cache
 */
static GLuint Font_TextureAddToCache(SDL_Surface * s)
{
	GLuint gltexnum;
	int i;

	for (i = firstTextureCache; i != lastTextureCache; i++, i %= (int)gl_fontcache->value)
		if (textureCache[i].surface == s)
			break;

	gltexnum = textureCache[i].texture;

	/* maybe cache is empty or surface wasn't found */
	if (i == lastTextureCache) {
		lastTextureCache++;
		lastTextureCache %= (int)gl_fontcache->value;

		/* delete old cache entries to maintain at least one free slot */
		if (lastTextureCache == firstTextureCache) {
			GLuint fgltexnum = textureCache[firstTextureCache].texture;
			qglDeleteTextures(1, &fgltexnum);
			/* don't free here but only set to NULL - will be freed in Font_CleanCache */
			textureCache[fgltexnum].surface = NULL;
			textureCache[fgltexnum].texture = 0;

			/* increment firstTextureCache position (the oldest used slot) */
			firstTextureCache++;
			firstTextureCache %= (int)gl_fontcache->value;
		}

		/* bind the 'new' surface and generate OpenGL texture id */
		textureCache[i].surface = s;
		
		qglGenTextures(1, &gltexnum);
	}

	return gltexnum;
}

/**
 * @brief
 * @sa Font_CleanCache
 */
static void Font_TextureCleanCache(void)
{
	int i;

	for (i = firstTextureCache; i != lastTextureCache; i++, i %= (int)gl_fontcache->value) {
		qglDeleteTextures(1, &textureCache[i].texture);
		textureCache[i].surface = NULL;
		textureCache[i].texture = 0;
	}

	firstTextureCache = lastTextureCache = 0;
}

/**
 * @brief frees the SDL_ttf fonts
 * @sa Font_CleanCache
 */
void Font_Shutdown(void)
{
	int i;

	Font_CleanCache();

	for (i = 0; i < numFonts; i++)
		if (fonts[i].font) {
			TTF_CloseFont(fonts[i].font);
			ri.FS_FreeFile(fonts[i].buffer);
			SDL_RWclose(fonts[i].rw);
		}

	/* now quit SDL_ttf, too */
	TTF_Quit();
}

/**
 * @brief
 * @TODO: Check whether font is already loaded
 */
font_t *Font_Analyze(const char *name, const char *path, int renderStyle, int size)
{
	font_t *f = NULL;
	int ttfSize;

	if (numFonts >= MAX_FONTS)
		return NULL;

	/* allocate new font */
	f = &fonts[numFonts];
	memset(f, 0, sizeof(f));

	/* copy fontname */
	Q_strncpyz(f->name, name, MAX_VAR);

	ttfSize = ri.FS_LoadFile(path, &f->buffer);

	f->rw = SDL_RWFromMem(f->buffer, ttfSize);

	/* norm size is 1024x768 (1.0) */
	/* scale the fontsize */
	size *= vid.rx;

	f->font = TTF_OpenFontRW(f->rw, 0, size);
	if (!f->font)
		ri.Sys_Error(ERR_FATAL, "...could not load font file %s\n", path);

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
static font_t *Font_GetFont(const char *name)
{
	int i;

	for (i = 0; i < numFonts; i++)
		if (!Q_strncmp(name, fonts[i].name, MAX_FONTNAME))
			return &fonts[i];

	return NULL;
}

/**
 * @brief
 * @sa Font_GetFont
 */
void Font_Length(const char *font, char *c, int *width, int *height)
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
 * @sa Font_TextureCleanCache
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
	Font_TextureCleanCache();
}

/**
 * @brief Console command binding to show the font cache
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
 * @param[in] string String to build the hash value for
 * @param[in] maxlen Max length that should be used to calculate the hash value
 * @return hash value for given string
 */
static int Font_Hash(const char *string, int maxlen)
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
 * @sa Font_Hash
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
 * @sa Font_GenerateCache
 * @sa Font_CleanCache
 * @sa Font_Hash
 */
static fontCache_t* Font_AddToCache(const char *s, void *pixel, int w, int h)
{
	int hashValue;
	fontCache_t *font = NULL;

	if (numInCache >= MAX_FONT_CACHE)
		Font_CleanCache();

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
 * @brief Renders the text surface and coverts to 32bit SDL_Surface that is stored in font_t structure
 * @todo maybe 32bit is overkill if the game is only using 16bit?
 * @sa Font_AddToCache
 * @sa TTF_RenderUTF8_Blended
 * @sa SDL_CreateRGBSurface
 * @sa SDL_LowerBlit
 * @sa SDL_FreeSurface
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
 * @sa Font_TextureAddToCache
 */
static int Font_GenerateGLSurface(fontCache_t *cache, int x, int y, int absX, int absY, int width, int height)
{
	GLuint texture = 0;
	int h = cache->size[1];
	vec2_t start = {0.0f, 0.0f}, end = {1.0f, 1.0f};

	/* if height is too much we should be able to scroll down */
	if (height > 0 && y+h > absY+height)
		return 1;

	if (gl_fontcache->modified) {
		if (gl_fontcache->value > MAX_TEXTURE_CACHE || gl_fontcache->value < 32) {
			ri.Con_Printf(PRINT_ALL, "setting gl_fontcache to %i\n", MAX_TEXTURE_CACHE);
			ri.Cvar_SetValue("gl_fontcache", MAX_TEXTURE_CACHE);
		}
		gl_fontcache->modified = qfalse;
		ri.Con_Printf(PRINT_ALL, "resetting fontcache\n");
		Font_TextureCleanCache();
	}

	texture = Font_TextureAddToCache(cache->pixel);

	/* Tell GL about our new texture */
	GL_Bind(texture);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cache->pixel->w, cache->pixel->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, cache->pixel->pixels);

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
 * @param[in] fontID the font id (defined in ufos/fonts.ufo)
 * @param[in] align
 * @param[in] x Current x position (may differ from absX due to tabs e.g.)
 * @param[in] y Current y position (may differ from absY due to linebreaks)
 * @param[in] absX Absolute x value for this string
 * @param[in] absY Absolute y value for this string
 * @param[in] maxWidth Max width - relative from absX
 * @param[in] maxHeight Max height - relative from absY
 * @param[in] lineHeight The lineheight of that node
 * @param[in] c The string to draw
 * @param[in] box_height
 * @param[in] scroll_pos Starting line in this node (due to scrolling)
 * @param[in] cur_line Current line (see lineHeight)
 * @param[in] increaseLine If true cur_line is increased with every linebreak
 */
int Font_DrawString(const char *fontID, int align, int x, int y, int absX, int absY, int maxWidth, int maxHeight,
	const int lineHeight, const char *c, int box_height, int scroll_pos, int *cur_line, qboolean increaseLine)
{
	int w = 0, h = 0, locX;
	float returnHeight = 0; /* rounding errors break mouse-text corelation */
	font_t *f = NULL;
	char *buffer = buf;
	char *pos;
	fontCache_t *cache;
	static char searchString[MAX_FONTNAME + MAX_HASH_STRING];
	int max = 0;				/* calculated maxWidth */
	int line = 0;
	float texh0, fh, fy; /* rounding errors break mouse-text corelation */
	qboolean skipline = qfalse;

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
		if (cur_line) {
			/* ri.Con_Printf(PRINT_ALL, "h %i - s %i - l %i\n", box_height, scroll_pos, *cur_line); */
			if (increaseLine)
				(*cur_line)++; /* Increment the number of processed lines (overall). */
			line = *cur_line;

			if (box_height > 0 && line > box_height + scroll_pos) {
				/* Due to scrolling this line and the following are not visible */
				return -1;
			}
		}

		Font_GenerateGLSurface(cache, x, fy, absX, absY, maxWidth, maxHeight);
		return lineHeight;
	}

	Q_strncpyz(buffer, c, BUF_SIZE);

	Font_ConvertChars(buf);
	/* for linebreaks */
	locX = x;

	do {
		skipline = qfalse;
		if (cur_line) {
			/* ri.Con_Printf(PRINT_ALL, "h %i - s %i - l %i\n", box_height, scroll_pos, *cur_line); */
			if (increaseLine)
				(*cur_line)++; /* Increment the number of processed lines (overall). */
			line = *cur_line;

			if (box_height > 0 && line > box_height + scroll_pos) {
				/* Due to scrolling this line and the following are not visible */
				return -1;
			}
			if (line < scroll_pos) {
				/* Due to scrolling this line is not visible. See if (!skipline)" code below.*/
				skipline = qtrue;
				/*ri.Con_Printf(PRINT_ALL, "skipline line: %i scroll_pos: %i\n", line, scroll_pos);*/
			}
		}

		/* TTF does not like empty strings... */
		if (!strlen(buffer))
			return returnHeight / vid.ry;

		pos = Font_GetLineWrap(f, buffer, maxWidth - (x - absX), &w, &h);
		fh = h;

		if (texh0 > 0) {
			/* FIXME: something is broken with that warning, please test */
#if 0
			if (fh > texh0)
				ri.Con_Printf(PRINT_DEVELOPER, "Warning: font %s height=%f bigger than allowed line height=%f.\n", fontID, fh, texh0);
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

			cache = Font_GetFromCache(searchString);
			if (!cache)
				cache = Font_GenerateCache(buffer, searchString, f);

			if (!cache) {
				/* maybe we are running out of mem */
				Font_CleanCache();
				cache = Font_GenerateCache(buffer, searchString, f);
			}
			if (!cache)
				ri.Sys_Error(ERR_FATAL, "...could not generate font surface '%s'\n", buffer);

			Font_GenerateGLSurface(cache, x, fy, absX, absY, maxWidth, maxHeight);
			fy += fh;
			returnHeight += (texh0 > 0) ? texh0 : h;
		}

		/* skip for next line */
		buffer = pos;
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
	gl_fontcache = ri.Cvar_Get("gl_fontcache", "256", CVAR_ARCHIVE, "Max allowed entries int font texture cache");
	gl_fontcache->modified = qfalse;

	/* try and init SDL VIDEO if not previously initialized */
	if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
			ri.Con_Printf(PRINT_ALL,"video SDL_Init failed: %s\n", SDL_GetError());
	} else if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
			ri.Con_Printf(PRINT_ALL,"video SDL_InitSubsystem failed: %s\n", SDL_GetError());
	}

	/* init the truetype font engine */
	if (TTF_Init() == -1)
		ri.Sys_Error(ERR_FATAL, "...SDL_ttf error: %s\n", TTF_GetError());

	ri.Con_Printf(PRINT_ALL, "...SDL_ttf inited\n");
	atexit(TTF_Quit);
}

/**
 * @brief
 */
void Font_Register(const char *name, int size, char *path, char *style)
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
