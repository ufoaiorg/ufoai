/**
 * @file r_font.c
 * @brief font handling with SDL_ttf font engine
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../../shared/utf8.h"

#define MAX_CACHE_STRING	128
#define MAX_CHUNK_CACHE		1024 /* making this bigger uses more GL textures */
#define MAX_WRAP_CACHE		1024 /* making this bigger uses more memory */
#define MAX_WRAP_HASH		4096 /* making this bigger reduces collisions */
#define MAX_FONTS			16
#define MAX_FONTNAME		32
#define MAX_TRUNCMARKER		16   /* enough for 3 chinese chars */

#define BUF_SIZE 2048

/**
 * @brief This structure holds one piece of text (usually a whole line)
 * and the texture on which it is rendered. It also holds positioning
 * information about the place of this piece in a multiline text.
 * Further information is held in the wrapCache_t struct that points
 * to this struct.
 */
typedef struct {
	int pos;		/**< offset of this chunk in source string */
	int len;		/**< length of this chunk in source string */
	int linenum;	/**< 0-based line offset from first line of text */
	int width;		/**< text chunk rendered width in pixels */
	/* no need for individual line height, just use font->height */
	qboolean truncated;	/**< needs ellipsis after text */
	vec2_t texsize;	/**< texture width and height */
	GLuint texnum;	/**< bound texture ID (0 if not textured yet) */
} chunkCache_t;

/**
 * @brief This structure caches information about rendering a text
 * in one font wrapped to a specific width. It points to structures
 * in the chunkCache that cache detailed information and the textures used.
 *
 * @note Caching text-wrapping information is particularly important
 * for Cyrillic and possibly other non-ascii text, where TTF_SizeUTF8()
 * is almost as slow as rendering. Intro sequence went from 4 fps to 50
 * after introducing the wrapCache.
 */
typedef struct wrapCache_s {
	char text[MAX_CACHE_STRING];	/**< hash id */
	const font_t *font;	/**< font used for wrapping/rendering this text */
	struct wrapCache_s *next;		/**< next hash entry in case of collision */
	int maxWidth;		/**< width to which this text was wrapped */
	longlines_t method;		/**< were long lines wrapped or truncated? */
	int numChunks;		/**< number of (contiguous) chunks in chunkCache used */
	int numLines;		/**< total line count of wrapped text */
	int chunkIdx;		/**< first chunk in chunkCache for this text */
	qboolean aborted;	/**< true if we can't finish the chunk generation */
} wrapCache_t;

static int numFonts = 0;
static font_t fonts[MAX_FONTS];

static chunkCache_t chunkCache[MAX_CHUNK_CACHE];
static wrapCache_t wrapCache[MAX_WRAP_CACHE];
static wrapCache_t *hash[MAX_WRAP_HASH];
static int numChunks = 0;
static int numWraps = 0;

/**
 * @brief This string is added at the end of truncated strings.
 * By default it is an ellipsis, but the caller can change that.
 * @sa R_FontSetTruncationMarker
 */
static char truncmarker[MAX_TRUNCMARKER] =  "...";

typedef struct {
	const char *name;
	int renderStyle;
} fontRenderStyle_t;

#define NUM_FONT_STYLES (sizeof(fontStyle) / sizeof(fontRenderStyle_t))
static const fontRenderStyle_t fontStyle[] = {
	{"TTF_STYLE_NORMAL", TTF_STYLE_NORMAL},
	{"TTF_STYLE_BOLD", TTF_STYLE_BOLD},
	{"TTF_STYLE_ITALIC", TTF_STYLE_ITALIC},
	{"TTF_STYLE_UNDERLINE", TTF_STYLE_UNDERLINE}
};

/*============================================================== */

void R_FontSetTruncationMarker (const char *marker)
{
	Q_strncpyz(truncmarker, marker, sizeof(truncmarker));
}


/**
 * @brief Clears font cache and frees memory associated with the cache
 */
static void R_FontCleanCache (void)
{
	int i;

	R_CheckError();

	/* free the surfaces */
	for (i = 0; i < numChunks; i++) {
		if (!chunkCache[i].texnum)
			continue;
		glDeleteTextures(1, &(chunkCache[i].texnum));
		R_CheckError();
	}

	memset(chunkCache, 0, sizeof(chunkCache));
	memset(wrapCache, 0, sizeof(wrapCache));
	memset(hash, 0, sizeof(hash));
	numChunks = 0;
	numWraps = 0;
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

	memset(fonts, 0, sizeof(fonts));
	numFonts = 0;

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
		Com_Error(ERR_FATAL, "...could not load font file %s", path);

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
 */
font_t *R_GetFont (const char *name)
{
	int i;

	for (i = 0; i < numFonts; i++)
		if (!strcmp(name, fonts[i].name))
			return &fonts[i];

	Com_Error(ERR_FATAL, "Could not find font: %s", name);
}


/**
 * @brief Console command binding to show the font cache
 */
void R_FontListCache_f (void)
{
	int i;
	int collSum = 0;

	Com_Printf("Font cache info\n========================\n");
	Com_Printf("...wrap cache size: %i - used %i\n", MAX_WRAP_CACHE, numWraps);
	Com_Printf("...chunk cache size: %i - used %i\n", MAX_CHUNK_CACHE, numChunks);

	for (i = 0; i < numWraps; i++) {
		const wrapCache_t *wrap = &wrapCache[i];
		int collCount = 0;
		while (wrap->next) {
			collCount++;
			wrap = wrap->next;
		}
		if (collCount)
			Com_Printf("...%i collisions for %s\n", collCount, wrap->text);
		collSum += collCount;
	}
	Com_Printf("...overall collisions %i\n", collSum);
}

/**
 * @param[in] string String to build the hash value for
 * @return hash value for given string
 */
static int R_FontHash (const char *string)
{
	register int hashValue, i;

	hashValue = 0;
	for (i = 0; string[i] != '\0'; i++)
		hashValue += string[i] * (119 + i);

	hashValue = (hashValue ^ (hashValue >> 10) ^ (hashValue >> 20));
	return hashValue & (MAX_WRAP_HASH - 1);
}

/**
 * @brief Calculate the width in pixels needed to render a piece of text.
 * Can temporarily modify the caller's string but leaves it unchanged.
 */
static int R_FontChunkLength (const font_t *f, char *text, int len)
{
	int width;
	char old;

	if (len == 0)
		return 0;

	old = text[len];
	text[len] = '\0';
	TTF_SizeUTF8(f->font, text, &width, NULL);
	text[len] = old;

	return width;
}

/**
 * @brief Find longest part of text that fits in maxWidth pixels,
 * with a clean break such as at a word boundary.
 * Can temporarily modify the caller's string but leaves it unchanged.
 * Assumes whole string won't fit.
 * @param[out] widthp Pixel width of part that fits.
 * @return String length of part that fits.
 */
static int R_FontFindFit (const font_t *f, char *text, int maxlen, int maxWidth, int *widthp)
{
	int bestbreak = 0;
	int width;
	int len;

	*widthp = 0;

	/* Fit whole words */
	for (len = 1; len < maxlen; len++) {
		if (text[len] == ' ') {
			width = R_FontChunkLength(f, text, len);
			if (width > maxWidth)
				break;
			bestbreak = len;
			*widthp = width;
		}
	}

	/* Fit hyphenated word parts */
	for (len = bestbreak + 1; len < maxlen; len++) {
		if (text[len] == '-') {
			width = R_FontChunkLength(f, text, len + 1);
			if (width > maxWidth)
				break;
			bestbreak = len + 1;
			*widthp = width;
		}
	}

	if (bestbreak > 0)
		return bestbreak;

	/** @todo Smart breaking of Chinese text */

	/* Can't fit even one word. Break first word anywhere. */
	for (len = 1; len < maxlen; len++) {
		if (UTF8_CONTINUATION_BYTE(text[len]))
			continue;
		width = R_FontChunkLength(f, text, len);
		if (width > maxWidth)
			break;
		bestbreak = len;
		*widthp = width;
	}

	return bestbreak;
}

/**
 * @brief Find longest part of text that fits in maxWidth pixels,
 * with a marker (ellipsis) at the end to show that part of the text was
 * truncated.
 * Assumes whole string won't fit.
 */
static int R_FontFindTruncFit (const font_t *f, const char *text, int maxlen, int maxWidth, qboolean mark, int *widthp)
{
	char buf[BUF_SIZE];
	int width;
	int len;
	int breaklen;

	breaklen = 0;
	*widthp = 0;

	for (len = 1; len < maxlen; len++) {
		buf[len - 1] = text[len - 1];
		if (UTF8_CONTINUATION_BYTE(text[len]))
			continue;
		if (mark)
			Q_strncpyz(&buf[len], truncmarker, sizeof(buf) - len);
		else
			buf[len] = '\0';
		TTF_SizeUTF8(f->font, buf, &width, NULL);
		if (width > maxWidth)
			return breaklen;
		breaklen = len;
		*widthp = width;
	}

	return maxlen;
}

/**
 * @brief Split text into chunks that fit on one line, and create cache
 * entries for those chunks.
 * @return number of chunks allocated in chunkCache.
 */
static int R_FontMakeChunks (const font_t *f, const char *text, int maxWidth, longlines_t method, int *lines, qboolean *aborted)
{
	int lineno = 0;
	int pos = 0;
	int startChunks = numChunks;
	char buf[BUF_SIZE];

	assert(text);

	Q_strncpyz(buf, text, sizeof(buf));

	do {
		int width;
		int len;
		int utf8len;
		int skip = 0;
		qboolean truncated = qfalse;

		/* find mandatory break */
		len = strcspn(&buf[pos], "\n");

		/* tidy up broken UTF-8 at end of line which may have been
		 * truncated by caller by use of functions like Q_strncpyz */
		utf8len = 1;
		while (len > utf8len && UTF8_CONTINUATION_BYTE(buf[pos + len - utf8len]))
			utf8len++;
		if (len > 0 && utf8len != UTF8_char_len(buf[pos + len - utf8len])) {
			len -= utf8len;
			skip += utf8len;
		}

		/* delete trailing spaces */
		while (len > 0 && buf[pos + len - 1] == ' ') {
			len--;
			skip++;
		}

		width = R_FontChunkLength(f, &buf[pos], len);
		if (maxWidth > 0 && width > maxWidth) {
			if (method == LONGLINES_WRAP) {
				/* full chunk didn't fit; try smaller */
				len = R_FontFindFit(f, &buf[pos], len, maxWidth, &width);
				/* skip following spaces */
				skip = 0;
				while (buf[pos + len + skip] == ' ')
					skip++;
				if (len + skip == 0) {
					*aborted = qtrue;
					break; /* could not fit even one character */
				}
			} else {
				truncated = (method == LONGLINES_PRETTYCHOP);
				len = R_FontFindTruncFit(f, &buf[pos], len, maxWidth, truncated, &width);
				skip = strcspn(&buf[pos + len], "\n");
			}
		}
		if (width > 0) {
			/* add chunk to cache */
			if (numChunks >= MAX_CHUNK_CACHE) {
				/* whoops, ran out of cache, wipe cache and start over */
				R_FontCleanCache();
				/** @todo check for infinite recursion here */
				return R_FontMakeChunks(f, text, maxWidth, method, lines, aborted);
			}
			chunkCache[numChunks].pos = pos;
			chunkCache[numChunks].len = len;
			chunkCache[numChunks].linenum = lineno;
			chunkCache[numChunks].width = width;
			chunkCache[numChunks].truncated = truncated;
			numChunks++;
		}
		pos += len + skip;
		if (buf[pos] == '\n' || buf[pos] == '\\') {
			pos++;
		}
		lineno++;
	} while (buf[pos] != '\0');

	/* If there were empty lines at the end of the text, then lineno might
	 * be greater than the linenum of the last chunk. Some callers need to
	 * know this to count lines accurately. */
	*lines = lineno;
	return numChunks - startChunks;
}

/**
 * @brief Wrap text according to provided parameters.
 * Pull wrapping from cache if possible.
 */
static wrapCache_t *R_FontWrapText (const font_t *f, const char *text, int maxWidth, longlines_t method)
{
	wrapCache_t *wrap;
	int hashValue = R_FontHash(text);
	int chunksUsed;
	int lines;
	qboolean aborted = qfalse;

	/* String is considered a match if the part that fit in entry->string
	 * matches. Since the hash value also matches and the hash was taken
	 * over the whole string, this is good enough. */
	for (wrap = hash[hashValue]; wrap; wrap = wrap->next)
		/* big string are cut, we must not test the 256e character ('\0') */
		if (!strncmp(text, wrap->text, sizeof(wrap->text) - 1)
		 && wrap->font == f
		 && wrap->method == method
		 && (wrap->maxWidth == maxWidth
			|| (wrap->numLines == 1 && !wrap->aborted
				&& !chunkCache[wrap->chunkIdx].truncated
				&& (chunkCache[wrap->chunkIdx].width <= maxWidth || maxWidth <= 0))))
			return wrap;

	if (numWraps >= MAX_WRAP_CACHE)
		R_FontCleanCache();

	/* It is possible that R_FontMakeChunks will wipe the cache,
	 * so do not rely on numWraps until it completes. */
	chunksUsed = R_FontMakeChunks(f, text, maxWidth, method, &lines, &aborted);

	wrap = &wrapCache[numWraps];
	strncpy(wrap->text, text, sizeof(wrap->text));
	wrap->text[MAX_CACHE_STRING - 1] = '\0';
	wrap->font = f;
	wrap->maxWidth = maxWidth;
	wrap->method = method;
	wrap->aborted = aborted;
	wrap->numChunks = chunksUsed;
	wrap->numLines = lines;
	wrap->chunkIdx = numChunks - chunksUsed;

	/* insert new text into wrap cache */
	wrap->next = hash[hashValue];
	hash[hashValue] = wrap;
	numWraps++;

	return wrap;
}

/**
 * @brief Supply information about the size of the text when it is
 * linewrapped and rendered, without actually rendering it. Any of the
 * output parameters may be NULL.
 * @param[out] width receives width in pixels of the longest line in the text
 * @param[out] height receives height in pixels when rendered with standard line height
 * @param[out] lines receives total number of lines in text, including blank ones
 */
void R_FontTextSize (const char *fontId, const char *text, int maxWidth, longlines_t method, int *width, int *height, int *lines, qboolean *isTruncated)
{
	const font_t *font = R_GetFont(fontId);
	const wrapCache_t *wrap = R_FontWrapText(font, text, maxWidth, method);

	if (width) {
		int i;
		*width = 0;
		for (i = 0; i < wrap->numChunks; i++) {
			if (chunkCache[wrap->chunkIdx + i].width > *width)
				*width = chunkCache[wrap->chunkIdx + i].width;
		}
	}

	if (height)
		*height = (wrap->numLines - 1) * font->lineSkip + font->height;

	if (lines)
		*lines = wrap->numLines;

	if (isTruncated)
		*isTruncated = chunkCache[wrap->chunkIdx].truncated;
}

/**
 * @brief Renders the text surface and converts to 32bit SDL_Surface that is stored in font_t structure
 * @sa R_FontCacheGLSurface
 * @sa TTF_RenderUTF8_Blended
 * @sa SDL_CreateRGBSurface
 * @sa SDL_LowerBlit
 * @sa SDL_FreeSurface
 */
static void R_FontGenerateTexture (const font_t *font, const char *text, chunkCache_t *chunk)
{
	int w, h;
	SDL_Surface *textSurface;
	SDL_Surface *openGLSurface;
	SDL_Rect rect = {0, 0, 0, 0};
	char buf[BUF_SIZE];
	static const SDL_Color color = {255, 255, 255, 0};	/* The 4th value is unused */
	const int samples = r_config.gl_compressed_alpha_format ? r_config.gl_compressed_alpha_format : r_config.gl_alpha_format;

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

	if (chunk->texnum != 0)
		return;  /* already generated */

	assert(strlen(text) >= chunk->pos + chunk->len);
	if (chunk->len >= sizeof(buf))
		return;
	memcpy(buf, &text[chunk->pos], chunk->len);
	buf[chunk->len] = 0;

	if (chunk->truncated)
		Q_strncpyz(buf + chunk->len, truncmarker, sizeof(buf) - chunk->len);

	textSurface = TTF_RenderUTF8_Blended(font->font, buf, color);
	if (!textSurface) {
		Com_Printf("%s (%s)\n", TTF_GetError(), buf);
		return;
	}

	/* copy text to a surface of suitable size for a texture (power of two) */
	for (w = 2; w < textSurface->w; w <<= 1) {}
	for (h = 2; h < textSurface->h; h <<= 1) {}

	openGLSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, rmask, gmask, bmask, amask);
	if (!openGLSurface)
		return;

	rect.x = rect.y = 0;
	rect.w = textSurface->w;
	if (rect.w > chunk->width)
		rect.w = chunk->width;
	rect.h = textSurface->h;

	/* ignore alpha when blitting - just copy it over */
	SDL_SetAlpha(textSurface, 0, 255);

	SDL_LowerBlit(textSurface, &rect, openGLSurface, &rect);
	SDL_FreeSurface(textSurface);

	/* use a fixed texture number allocation scheme */
	chunk->texnum = TEXNUM_FONTS + (chunk - chunkCache);
	R_BindTexture(chunk->texnum);
	glTexImage2D(GL_TEXTURE_2D, 0, samples, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, openGLSurface->pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	Vector2Set(chunk->texsize, w, h);
	R_CheckError();
	SDL_FreeSurface(openGLSurface);
}

static const float font_texcoords[] = {
	0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0
};

static void R_FontDrawTexture (int texId, int x, int y, int w, int h)
{
	const float nx = x * viddef.rx;
	const float ny = y * viddef.ry;
	const float nw = w * viddef.rx;
	const float nh = h * viddef.ry;

	R_BindTexture(texId);

	glVertexPointer(2, GL_SHORT, 0, r_state.vertex_array_2d);

	memcpy(&texunit_diffuse.texcoord_array, font_texcoords, sizeof(font_texcoords));

	r_state.vertex_array_2d[0] = nx;
	r_state.vertex_array_2d[1] = ny;
	r_state.vertex_array_2d[2] = nx + nw;
	r_state.vertex_array_2d[3] = ny;

	r_state.vertex_array_2d[4] = nx;
	r_state.vertex_array_2d[5] = ny + nh;
	r_state.vertex_array_2d[6] = nx + nw;
	r_state.vertex_array_2d[7] = ny + nh;

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	/* set back to standard 3d pointer */
	glVertexPointer(3, GL_FLOAT, 0, r_state.vertex_array_3d);
}

/**
 * @param[in] fontId the font id (defined in ufos/fonts.ufo)
 * @param[in] x Current x position (may differ from absX due to tabs e.g.)
 * @param[in] y Current y position (may differ from absY due to linebreaks)
 * @param[in] absX Absolute x value for this string
 * @param[in] absY Absolute y value for this string
 * @param[in] maxWidth Max width - relative from absX
 * @param[in] lineHeight The lineheight of that node
 * @param[in] c The string to draw
 * @param[in] scrollPos Starting line in this node (due to scrolling)
 * @param[in] curLine Current line (see lineHeight)
 * @note the x, y, width and height values are all normalized here - don't use the
 * viddef settings for drawstring calls - make them all relative to VID_NORM_WIDTH
 * and VID_NORM_HEIGHT
 * @todo This could be replaced with a set of much simpler interfaces.
 */
int R_FontDrawString (const char *fontId, int align, int x, int y, int absX, int maxWidth,
		int lineHeight, const char *c, int boxHeight, int scrollPos, int *curLine, longlines_t method)
{
	const font_t *font = R_GetFont(fontId);
	const wrapCache_t *wrap;
	int i;
	const int horizontalAlign = align % 3; /* left, center, right */
	int xalign = 0;

	wrap = R_FontWrapText(font, c, maxWidth - (x - absX), method);

	if (boxHeight <= 0)
		boxHeight = wrap->numLines;

	for (i = 0; i < wrap->numChunks; i++) {
		chunkCache_t *chunk = &chunkCache[wrap->chunkIdx + i];
		int linenum = chunk->linenum;

		if (curLine)
			linenum += *curLine;

		if (horizontalAlign == 1)
			xalign = -(chunk->width / 2);
		else if (horizontalAlign == 2)
			xalign = -chunk->width;
		else
			xalign = 0;

		if (linenum < scrollPos || linenum >= scrollPos + boxHeight)
			continue;

		R_FontGenerateTexture(font, c, chunk);
		R_FontDrawTexture(chunk->texnum, x + xalign, y + (linenum - scrollPos) * lineHeight, chunk->texsize[0], chunk->texsize[1]);
	}

	return wrap->numLines;
}

void R_FontInit (void)
{
#ifdef SDL_TTF_VERSION
	SDL_version version;

	SDL_TTF_VERSION(&version)
	Com_Printf("SDL_ttf version %i.%i.%i - we need at least 2.0.7\n",
		version.major,
		version.minor,
		version.patch);
#else
	Com_Printf("could not get SDL_ttf version - we need at least 2.0.7\n");
#endif

	numFonts = 0;
	memset(fonts, 0, sizeof(fonts));

	memset(chunkCache, 0, sizeof(chunkCache));
	memset(wrapCache, 0, sizeof(wrapCache));
	memset(hash, 0, sizeof(hash));
	numChunks = 0;
	numWraps = 0;

	/* init the truetype font engine */
	if (TTF_Init() == -1)
		Com_Error(ERR_FATAL, "SDL_ttf error: %s", TTF_GetError());
}

void R_FontRegister (const char *name, int size, const char *path, const char *style)
{
	int renderstyle = 0;		/* NORMAL is standard */
	int i;

	if (style && style[0] != '\0')
		for (i = 0; i < NUM_FONT_STYLES; i++)
			if (!Q_strcasecmp(fontStyle[i].name, style)) {
				renderstyle = fontStyle[i].renderStyle;
				break;
			}

	R_FontAnalyze(name, path, renderstyle, size);
}
