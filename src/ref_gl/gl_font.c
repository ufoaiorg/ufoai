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

// gl_font.c

#include "gl_local.h"

static const	SDL_Color color = {255, 255, 255, 0}; // The 4. value is unused
// holds the gettext string
static char	buf[2048];
static int	numInCache;
fontRenderStyle_t fontStyle[] = {
	{"TTF_STYLE_NORMAL", TTF_STYLE_NORMAL},
	{"TTF_STYLE_BOLD", TTF_STYLE_BOLD},
	{"TTF_STYLE_ITALIC", TTF_STYLE_ITALIC},
	{"TTF_STYLE_UNDERLINE", TTF_STYLE_UNDERLINE}
};

//==============================================================

/*===============
Font_Shutdown

frees the SDL_ttf fonts
===============*/
void Font_Shutdown ( void )
{
	int i;

	Font_CleanCache();

	for ( i = 0; i < numFonts; i++ )
		if ( fonts[i].font )
			TTF_CloseFont(fonts[i].font);

	// now quit SDL_ttf, too
	TTF_Quit();
}

/*================
Font_Analyze

TODO: Check whether font is already loaded
================*/
font_t *Font_Analyze( char *name, char *path, int renderStyle, int size )
{
	font_t	*f = NULL;
	SDL_RWops *rw = NULL;
	void *buffer = NULL;
	int ttfSize;

	if ( numFonts >= MAX_FONTS )
		return NULL;

	// allocate new font
	f = &fonts[numFonts];

	// copy fontname
	Q_strncpyz( f->name, name, MAX_VAR );

	ttfSize = ri.FS_LoadFile(path, &buffer);

	rw = SDL_RWFromMem(buffer, ttfSize);

	// norm size is 1024x768 (1.0)
	// scale the fontsize
	size *= vid.rx;

	f->font = TTF_OpenFontRW( rw, 0, size );
	if ( ! f->font )
		ri.Sys_Error(ERR_FATAL, "...could not load font file %s\n", path );

	// font style
	f->style = renderStyle;
	if ( f->style )
		TTF_SetFontStyle( f->font, f->style );

// 	SDL_RWclose( rw );
	numFonts++;

	// return the font
	return f;
}

/*================
Font_GetFont
================*/
static font_t *Font_GetFont( char *name )
{
	int	i;

	for ( i = 0; i < numFonts; i++ )
	{
		if ( !Q_strncmp( name, fonts[i].name, MAX_FONTNAME ) )
			return &fonts[i];
	}
	return NULL;
}

/*================
Font_Length
================*/
vec2_t *Font_Length (char *font, char *c)
{
	font_t	*f = NULL;
	static vec2_t	l;

	// get the font
	f = Font_GetFont( font );
	if ( !f ) return NULL;

	TTF_SizeUTF8( f->font, c, (int*)&l[0], (int*)&l[1] );
	return &l;
}

/*================
Font_CleanCache
================*/
void Font_CleanCache ( void )
{
	int i = 0;

	if ( numInCache < MAX_FONT_CACHE ) return;

	// free the surfaces
	for ( ; i < numInCache; i++ )
		if ( fontCache[i].pixel )
			SDL_FreeSurface( fontCache[i].pixel );

	memset( fontCache, 0, sizeof(fontCache) );
	memset( hash, 0, sizeof(hash) );
	numInCache = 0;
}

/*============
Font_ListCache_f
============*/
void Font_ListCache_f ( void )
{
	int i = 0;
	int collCount = 0, collSum = 0;
	fontCache_t *f = NULL;

	ri.Con_Printf(PRINT_ALL, "Font cache info\n========================\n" );
	ri.Con_Printf(PRINT_ALL, "...font cache size: %i - used %i\n", MAX_FONT_CACHE, numInCache );

	// free the surfaces
	for ( ; i < numInCache; i++ )
	{
		f = &fontCache[i];
		if ( ! f )
		{
			ri.Con_Printf(PRINT_ALL, "...hashtable inconsistency at %i\n", i );
			continue;
		}
		collCount = 0;
		while ( f->next )
		{
			collCount++;
			f = f->next;
		}
		if ( collCount )
			ri.Con_Printf(PRINT_ALL, "...%i collisions for %s\n", collCount, f->string );
		collSum += collCount;
	}
	ri.Con_Printf(PRINT_ALL, "...overall collisions %i\n", collSum );
}

/*============
Font_Hash
============*/
static int Font_Hash( const char *string, int maxlen )
{
	int register hash, i;

	hash = 0;
	for (i = 0; i < maxlen && string[i] != '\0'; i++)
	{
		hash += string[i] * (119 + i);
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	return hash & (MAX_FONT_CACHE - 1);
}

/*================
Font_GetFromCache
================*/
static void* Font_GetFromCache ( const char *s )
{
	fontCache_t *font = NULL;
	int hashValue;

	hashValue = Font_Hash( s, MAX_HASH_STRING );
	for (font=hash[hashValue]; font; font=font->next)
		if ( !Q_strncmp( (char*)s, font->string, MAX_HASH_STRING ) )
			return font->pixel;

	return NULL;
}

/*================
Font_AddToCache

We add the font string (e.g. f_small) to the beginning
of each string (char *s) because we can have the same strings
but other fonts.
================*/
static void Font_AddToCache( const char* s, void* pixel, int w, int h )
{
	int hashValue;
	fontCache_t* font = NULL;

	hashValue = Font_Hash( s, MAX_HASH_STRING );
	if ( hash[hashValue] )
	{
		font = hash[hashValue];
		// go to end of list
		while ( font->next )
			font = font->next;
		font = &fontCache[numInCache];
	}
	else
		hash[hashValue] = &fontCache[numInCache];

	if ( numInCache < MAX_FONT_CACHE )
	{
		Q_strncpyz( fontCache[numInCache].string, s, MAX_HASH_STRING );
		fontCache[numInCache].pixel = pixel;
		fontCache[numInCache].size[0] = w;
		fontCache[numInCache].size[1] = h;
	}
	else
		ri.Sys_Error( ERR_FATAL, "...font cache exceeded with %i\n", hashValue );

	numInCache++;
	if ( numInCache >= MAX_FONT_CACHE ) Font_CleanCache();
}

/*================
Font_GenerateCache
================*/
static void* Font_GenerateCache ( const char* s, const char* fontString, TTF_Font* font )
{
	int w, h;
	SDL_Surface *textSurface = NULL;
	SDL_Surface *openGLSurface = NULL;
	SDL_Rect rect = {0, 0, 0};

	textSurface = TTF_RenderUTF8_Blended(font, s, color);
	if ( ! textSurface )
	{
		ri.Con_Printf(PRINT_ALL, "%s\n", TTF_GetError() );
		return NULL;
	}

	// convert to power of two
	for ( w = 1; w < textSurface->w; w <<= 1 );
	for ( h = 1; h < textSurface->h; h <<= 1 );

	openGLSurface = SDL_CreateRGBSurface(
		SDL_SWSURFACE,
		w, h,
		32,
		textSurface->format->Rmask,
		textSurface->format->Gmask,
		textSurface->format->Bmask,
		textSurface->format->Amask
	);

	rect.x = rect.y = 0;
	rect.w = textSurface->w;
	rect.h = textSurface->h;

	SDL_SetAlpha(textSurface, 0, 0);

	SDL_LowerBlit(textSurface, &rect, openGLSurface, &rect);
	SDL_FreeSurface(textSurface);

// 	SDL_SaveBMP( openGLSurface, buffer );
	Font_AddToCache( fontString, openGLSurface, textSurface->w, textSurface->h );
	return openGLSurface;
}

/*================
Font_GetLineWrap

maxWidth is a pixel value
================*/
static char* Font_GetLineWrap ( char* buffer, TTF_Font* font, int maxWidth, vec2_t* l )
{
	char*	space = buffer;
	char*	newlineTest = NULL;
	int	width = 0;
	int	height = 0; // not needed yet

	if ( ! maxWidth )
		maxWidth = VID_NORM_WIDTH;

	// no line wrap needed?
	TTF_SizeUTF8( font, buffer, &width, &height );
	if ( ! width )
		return NULL;

	*l[1] = height;
	*l[0] = width;

	if ( width < maxWidth )
		return buffer;

	newlineTest = strstr( space, "\n" );
	if ( newlineTest )
	{
		*newlineTest = '\0';
		TTF_SizeUTF8( font, buffer, &width, &height );
		*l[0] = width;
		if ( width < maxWidth )
			return buffer;
		else // go on and check for spaces
			*newlineTest = '\n';
	}

	// uh - this line is getting longer than allowed...
	newlineTest = space;
	while ( ( space = strstr( space, " " ) ) != NULL )
	{
		*space='\0';
		TTF_SizeUTF8( font, buffer, &width, &height );
		maxWidth -= width;
		*l[0] = width;
		if ( maxWidth < 0 )
		{
			newlineTest = '\0';
			return buffer;
		}
		else if ( maxWidth == 0 )
			return space;
		// maybe there is space for one more word?
		*space = ' ';
		newlineTest = space;
		space++;
	};

	// return the start pos of this string
	return buffer;
}

/*================
Font_ConvertChars
================*/
static void Font_ConvertChars ( char* buffer )
{
	char* replace = NULL;

	// convert all \\ to \n
	replace = strstr( buffer, "\\" );
	while ( replace != NULL )
	{
		*replace = '\n';
		replace++;
		replace = strstr( replace, "\\" );
	}

	// convert all tabs to spaces
	replace = strstr( buffer, "\t" );
	while ( replace != NULL  )
	{
		*replace = ' ';
		replace++;
		replace = strstr( replace, "\t" );
	}
}

/*================
Font_GenerateGLSurface

generate the opengl texture out of the sdl-surface
bind it, and draw it
================*/
static void Font_GenerateGLSurface ( SDL_Surface* s, int x, int y )
{
	unsigned int texture = 0;
	int w = s->w;
	int h = s->h;

	/* Tell GL about our new texture */
	qglGenTextures(1, &texture);
	GL_Bind(texture);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s->pixels );

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// draw it
	qglEnable (GL_BLEND);

	qglBegin(GL_TRIANGLE_STRIP);
	qglTexCoord2f(0.0f, 0.0f);
	qglVertex2f(x, y);
	qglTexCoord2f(1.0f, 0.0f);
	qglVertex2f(x + w, y);
	qglTexCoord2f(0.0f, 1.0f);
	qglVertex2f(x, y + h);
	qglTexCoord2f(1.0f, 1.0f);
	qglVertex2f(x + w, y + h);
	qglEnd();

	qglDisable (GL_BLEND);

	qglFinish();
	/* Clean up */
	qglDeleteTextures(1, &texture);
}

/*================
Font_DrawString
================*/
vec2_t *Font_DrawString (char *font, int align, int x, int y, int maxWidth, char *c)
{
	static vec2_t l;
	vec2_t dim = {0,0};
	font_t	*f = NULL;
	char* buffer = buf;
	SDL_Surface *openGLSurface = NULL;
	int max; // calculated maxWidth
	max = 0;

	// get the font
	f = Font_GetFont( font );
	if ( !f ) ri.Sys_Error(ERR_FATAL, "...could not find font: %s\n", font );

	Q_strncpyz( buffer, c, sizeof(buf) );

	// transform from 1024x768 coordinates for drawing
	x = (float)x * vid.rx;
	y = (float)y * vid.ry;
	maxWidth = (float)maxWidth * vid.rx;

	Font_ConvertChars( buffer );

	do
	{
		buffer = Font_GetLineWrap( buffer, f->font, maxWidth, &dim );
		if ( ! buffer || !strlen(buffer) )
			return NULL;
		
		// TODO: i do not have a clou why this is need a second timen but after the Font_GetLineWrap function "f" is f... up.
		f = Font_GetFont( font );
		if ( !f ) ri.Sys_Error(ERR_FATAL, "...could not find font: %s\n", font );
			
		// check whether this line is bigger than every other
		if ( dim[0] > max )
			max = dim[0];

		if ( align > 0 && align < ALIGN_LAST )
		{
			switch ( align % 3 )
			{
			case 1: x -= dim[0] / 2; break;
			case 2: x -= dim[0]; break;
			}

			switch ( align / 3 )
			{
			case 1: y -= dim[1] / 2; break;
			case 2: y -= dim[1]; break;
			}
		}

		openGLSurface = Font_GetFromCache( va("%s%s", font, buffer) );
		if ( !openGLSurface )
		{
			Font_GenerateCache( buffer, va("%s%s", font, buffer), f->font );
			openGLSurface = Font_GetFromCache( va("%s%s", font, buffer) );
		}

		if ( !openGLSurface )
		{
// 			ri.Sys_Error(ERR_FATAL, "...could not generate font surface\n" );
			// FIXME: This "should" be a sys_error and not a return null
			ri.Con_Printf(PRINT_DEVELOPER, "...could not generate font surface\n" );
			return NULL;
		}

		Font_GenerateGLSurface( openGLSurface, x, y );

		// skip for next line
		y += TTF_FontLineSkip( f->font );
		buffer += strlen(buffer) + 1;
	} while ( buffer );

	// return width and height
	l[0] = max;
	l[1] = y;

	return &l;
}

/*================
Font_Init
================*/
void Font_Init ( void )
{
	numFonts = 0;

	// init the truetype font engine
	if ( TTF_Init() )
		ri.Sys_Error( ERR_FATAL, "...SDL_ttf error: %s\n", TTF_GetError() );

	ri.Con_Printf( PRINT_ALL, "...SDL_ttf inited\n" );
	atexit( TTF_Quit );
}

/*===============
Font_Register
===============*/
void Font_Register( char *name, int size, char* path, char* style )
{
	int renderstyle = 0; // NORMAL is standard
	int i;

	if ( style && *style )
		for (i=0 ; i< NUM_FONT_STYLES ; i++)
			if ( !Q_stricmp( fontStyle[i].name, style ) )
			{
				renderstyle = fontStyle[i].renderStyle;
				break;
			}

	Font_Analyze(name, path, renderstyle, size);
}

