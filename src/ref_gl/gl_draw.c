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

// draw.c

#include "gl_local.h"

#ifdef __linux__
#define min(a,b) ((a)<(b) ? (a):(b))
#endif

extern	qboolean	scrap_dirty;
void Scrap_Upload (void);
image_t		*shadow;

#ifdef BUILD_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

#define _FLOOR(x)  ((x) & -64)
#define _CEIL(x)   (((x)+63) & -64)
#define _TRUNC(x)  ((x) >> 6)

FT_Library ftLibrary = NULL;
static fontInfo_t registeredFont[MAX_FONTS];
static int registeredFontCount = 0;
static int fdOffset;
static byte	*fdFile;
fontInfo_t big;
fontInfo_t normal;
fontInfo_t small;
typedef union {
	byte	fred[4];
	float	ffred;
} poor;

#endif /* BUILD_FREETYPE */

// font definitions
font_t	fonts[MAX_FONTS];

// console font
image_t		*draw_chars;

int		numFonts;

#ifdef USE_SDL_TTF
/*
===============
GL_ShutdownSDLFonts

frees the SDL_ttf fonts
===============
*/
void GL_ShutdownSDLFonts ( void )
{
	int i;

	for ( i = 0; i < numFonts; i++ )
		if ( fonts[i].font )
			TTF_CloseFont(fonts[i].font);

	// now quit SDL_ttf, too
	TTF_Quit();
}
#endif

/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	shadow = GL_FindImage ("pics/sfx/shadow.tga", it_pic);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	numFonts = 0;
	// load console characters (don't bilerp characters)
	draw_chars = GL_FindImage ("pics/conchars.pcx", it_pic);
	GL_Bind( draw_chars->texnum );

#ifdef USE_SDL_TTF
	// init the truetype font engine
	if ( TTF_Init() )
	{
		ri.Con_Printf( PRINT_ALL, "...SDL_ttf error: %s\n", TTF_GetError() );
		return;
	}

	ri.Con_Printf( PRINT_ALL, "...SDL_ttf inited\n" );
	atexit( TTF_Quit );
#else
	ri.Con_Printf( PRINT_ALL, "...using old font engine - without utf8 support\n" );

	GL_FindImage ("pics/f_small.tga", it_font);
	GL_FindImage ("pics/f_big.tga", it_font);
	GL_FindImage ("pics/f_menu.tga", it_font);
	GL_FindImage ("pics/f_menu_small.tga", it_font);
#endif /* USE_SDL_TTF */

#ifdef BUILD_FREETYPE
	R_InitFreeType();
#endif
}



/*
================
Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Char (int x, int y, int num)
{
	int row, col;
	float frow, fcol, sizefrow, sizefcol;

	num &= 255;

	if(num == ' ')
		return;

	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.03125;
	fcol = col*0.0625;
	sizefcol = 0.0625; // 16 cols (conchars.pcx)
	sizefrow = 0.03125; // 32 rows (conchars.pcx)

	GL_Bind (draw_chars->texnum);

	qglBegin (GL_QUADS);
	qglTexCoord2f (fcol, frow);
	qglVertex2f (x, y);
	qglTexCoord2f (fcol + sizefcol, frow);
	qglVertex2f (x+8, y);
	qglTexCoord2f (fcol + sizefcol, frow + sizefrow);
	qglVertex2f (x+8, y+8);
	qglTexCoord2f (fcol, frow + sizefrow);
	qglVertex2f (x, y+8);
	qglEnd ();
}

#ifndef USE_SDL_TTF
static byte R_FloatToByte( float x )
{
	union {
		float f;
		unsigned int i;
	} f2i;

	// shift float to have 8bit fraction at base of number
	f2i.f = x + 32768.0f;
	f2i.i &= 0x7FFFFF;

	// then read as integer and kill float bits...
	return ( byte )min( f2i.i, 255 );
}
#endif /* USE_SDL_TTF */

void Draw_ColorChar (int x, int y, int num, vec4_t color)
{
#ifdef USE_SDL_TTF
	// implement me
#else
	int				row, col;
	float frow, fcol, sizefrow, sizefcol;
	byte			colors[4];

	colors[0] = R_FloatToByte( color[0] );
	colors[1] = R_FloatToByte( color[1] );
	colors[2] = R_FloatToByte( color[2] );
	colors[3] = R_FloatToByte( color[3] );

	num &= 255;

	if(num == ' ')
		return;

	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.03125;
	fcol = col*0.0625;
	sizefcol = 0.0625; // 16 cols (conchars.pcx)
	sizefrow = 0.03125; // 32 rows (conchars.pcx)

	GL_Bind (draw_chars->texnum);

	qglColor4ubv( colors );
	GL_TexEnv(GL_MODULATE);
	qglBegin (GL_QUADS);
	qglTexCoord2f (fcol, frow);
	qglVertex2f (x, y);
	qglTexCoord2f (fcol + sizefcol, frow);
	qglVertex2f (x+8, y);
	qglTexCoord2f (fcol + sizefcol, frow + sizefrow);
	qglVertex2f (x+8, y+8);
	qglTexCoord2f (fcol, frow + sizefrow);
	qglVertex2f (x, y+8);
	qglEnd ();
	qglColor4f( 1,1,1,1 );
#endif /* USE_SDL_TTF */
}

/*
================
Draw_AnalyzeFont

i've missused the byte* pic and int w for SDL_ttf
byte* pic is the path of a ttf-file and int w is the
renderStyle of a ttf-file
================
*/
font_t *Draw_AnalyzeFont( char *name, byte *pic, int w, int h )
{
	font_t	*f;
#ifndef USE_SDL_TTF
	int	l, i, t, chars;
	int	tx, ty, sx, sy;
	byte	*tp;

	// get the font name without .tga
	l = strlen( name ) - 4;

	// just get the position of the last / in name
	for ( i = l; i > 0; i-- )
		if ( name[i-1] == '/' || name[i-1] == '\\' ) break;

	if ( l - i >= MAX_FONTNAME )
	{
		ri.Con_Printf( PRINT_ALL, "Font with too long name\n" );
		return NULL;
	}

	if ( numFonts >= MAX_FONTS )
		return NULL;

	// allocate new font
	f = &fonts[numFonts];
	// copy only fontname - without path
	Q_strncpyz( f->name, name+i, l-i );

	// check the font format (start with minimum 1x1 font)
	// it has to be a power of 2
	// minimum pixelwidth for each letter is 16 pixel
	for ( sx = 16; sx < w; sx <<= 1 );
	if ( sx != w )
	{
		ri.Con_Printf( PRINT_ALL, "Font '%s' doesn't have a valid width\n", f->name );
		return NULL;
	}

	for ( sy = 16; sy < h; sy <<= 1 );
	if ( sy != h )
	{
		ri.Con_Printf( PRINT_ALL, "Font '%s' doesn't have a valid height\n", f->name );
		return NULL;
	}

	// get character count
	if ( h == w << 1 ) {
		// small letters included 1:2
		// 32 rows => rh = 1/32
		f->rh = 0.03125;
		chars = 16*32;
	} else if ( h == w ) {
		// only big letters 1:1
		// 16 rows => rh = 1/16
		f->rh = 0.0625;
		chars = 16*16;
	} else {
		// wrong format
		ri.Con_Printf( PRINT_ALL, "Font '%s' doesn't have a valid format (1/2 or 1/1)\n", f->name );
		return NULL;
	}

	// get size for single chars
	// pixel size right shift by 4 means a division by 16
	// as sx will have the width dimensions of the bitmap font
	// this will lead to the pixelsize for each char
	// a bitmap font needs to have 16 chars/cols in each row
	f->w = sx >>= 4;
	f->rw = 0.0625; // 16 cols per row (1/16)
	if ( f->rh == 0.0625 ) f->h = sy >>= 4;
	else f->h = sy >>= 5;

	tx = 0;
	ty = 0;

	// analyze the width of the chars
	for ( i = 0; i < chars; i++ )
	{
		// 16 chars per row
		if ( i % 16 )
		{
			// same line
			tx += sx;
		}
		else // 0, 16, 32, 48...
		{
			// new line
			tx = sx-1;
			// shift 4 times to get the current row
			ty = (i>>4)*sy;
		}

		// test pixels (+3 for alpha, rgba)
		// get position of pixels for this char
		// <<2: rgba, 4 pixels
		tp = pic + ((tx + ty*w)<<2) + 3;

		// get the char length by looping through all pixel rows (sx)
		// and getting the
		for ( l = sx; l > 0; l-- )
		{
			// test a vertical line
			// w<<2: 4 pixels (rgba) next pixel row
			for ( t = 0; t < sy; t++, tp += w<<2 )
				if ( *tp ) break;

			if ( t < sy ) break;

			tp -= ((sy*w)<<2) + 4;
		}

		// store the char width
		f->wc[i] = l;
	}

	// fix lerp problems
	f->h--;
	f->rhl = f->rh * (float)f->h / (f->h+1);

	numFonts++;
	// return the font
	return f;
#else
	SDL_RWops *rw;
	void* buffer;
	int ttfSize;

	if ( numFonts >= MAX_FONTS )
		return NULL;

	// allocate new font
	f = &fonts[numFonts];

	// copy fontname
	Q_strncpyz( f->name, name, MAX_VAR );

	ttfSize = ri.FS_LoadFile((char*)pic, &buffer);

	rw = SDL_RWFromMem(buffer, ttfSize);

	f->font = TTF_OpenFontRW( rw, 0, h );
	if ( ! f->font )
		ri.Sys_Error(ERR_FATAL, "...could not load font file %s\n", (char*)pic );

	// font style
	f->style = w;
	if ( f->style )
		TTF_SetFontStyle( f->font, f->style );

// 	SDL_RWclose( rw );
	numFonts++;
	// return the font
	return f;
#endif /* USE_SDL_TTF */
}

/*
================
Draw_GetFont
================
*/
font_t *Draw_GetFont( char *name )
{
	int	i;

	for ( i = 0; i < numFonts; i++ )
	{
		if ( !Q_strncmp( name, fonts[i].name, MAX_FONTNAME ) )
			return &fonts[i];
	}
	return NULL;
}

/*
================
Draw_PropCharFont
texture NEEDS to be binded already when using old font-engine
================
*/
int Draw_PropCharFont (font_t *f, int x, int y, char c)
{
#ifdef USE_SDL_TTF
	// we don't need this anymore - we draw the hole string at once'
	return 0;
#else
	int			n, row, col;
	float		nx, ny, cx, cy;
	float		frow, fcol, sx, sy;

	// get the char
	n = (int)(c<32 ? 0 : c-32) & 255;

	// we have no lower case letters in here - so go back to upper case
	// 64 is the value where lower case chars start
	// not in ascii but in our case - because we start to count from 32 (SPACE)
	if ( f->rh == 0.0625 && n < 128 )
		while ( n >= 64 ) n -= 32;
	// here we also have lower case letters
	else if ( f->rh == 0.0625 && n > 223 )
		while ( n >= 224 ) n -= 32;

	if ( n < 0 || !f->wc[n] ) return (float)f->w * CHAR_EMPTYWIDTH;

	// transform from 1024x768 coordinates for drawing
	nx = (float)x * vid.rx;
	ny = (float)y * vid.ry;
	cx = (float)f->wc[n] * vid.rx;
	cy = (float)f->h * vid.ry;

	// get texture coordinates
	row = n>>4;
	col = n&15;

	frow = row*f->rh;
	fcol = col*f->rw;
	sy = f->rhl;
	sx = f->rw * f->wc[n] / f->w;

	// draw it
	qglEnable (GL_BLEND);

	qglBegin (GL_QUADS);
	qglTexCoord2f (fcol, frow);
	qglVertex2f (nx, ny);
	qglTexCoord2f (fcol + sx, frow);
	qglVertex2f (nx+cx, ny);

	qglTexCoord2f (fcol + sx, frow + sy);
	qglVertex2f (nx+cx, ny+cy);
	qglTexCoord2f (fcol, frow + sy);
	qglVertex2f (nx, ny+cy);
	qglEnd ();

	qglDisable (GL_BLEND);

	return f->wc[n]+1;
#endif
}

/*
================
Draw_PropChar
================
*/
int Draw_PropChar (char *font, int x, int y, char c)
{
	font_t	*f;

	// get the font
	f = Draw_GetFont( font );
	if ( !f ) return 0;

#ifndef USE_SDL_TTF
	// draw the char
	GL_Bind (f->image->texnum);
#endif
	return Draw_PropCharFont( f, x, y, c );
}

/*
================
Draw_PropLength
================
*/
int Draw_PropLength (char *font, char *c)
{
	font_t	*f;
	int		l;
#ifdef USE_SDL_TTF
	int h;
#else
	int n;
#endif
	// get the font
	f = Draw_GetFont( font );
	if ( !f ) return 0;

#ifdef USE_SDL_TTF
	TTF_SizeUTF8( f->font, c, &l, &h );
#else
	// parse the string
	for ( l = 0; *c; c++ )
	{
		n = (int)(*c<32 ? 0 : *c-32) & 255;
		// we have no lower case letters in here - so go back to upper case
		// 64 is the value where lower case chars start
		// not in ascii but in our case - because we start to count from 32 (SPACE)
		if ( f->rh == 0.0625 && n < 128 )
			while ( n >= 64 ) n -= 32;
		else if ( f->rh == 0.0625 && n > 223 )
			while ( n >= 224 ) n -= 32;

		if ( n < 0 || !f->wc[n] ) l += (float)f->w * CHAR_EMPTYWIDTH;
		else l += f->wc[n]+1;
	}
#endif
	return l;
}

/*
================
Draw_PropString
================
*/
int Draw_PropString (char *font, int align, int x, int y, char *c)
{
	int		l;
	font_t	*f;
#ifdef USE_SDL_TTF
	int h, texture, w;
	SDL_Surface *textSurface, *openGLSurface;
	SDL_Color color = {255,255,255};
	SDL_Rect rect;
	char	*newline;
#endif

	// get the font
	f = Draw_GetFont( font );
	if ( !f )
	{
		ri.Con_Printf(PRINT_ALL, "...could not find font: %s\n", font );
		return 0;
	}

#ifdef USE_SDL_TTF
	// TODO: this needs caching
	// TTF_RenderUTF8_Blended allocates memory - which is slow
	// SDL_CreateRGBSurface allocates memory - which is slow, too
	// SDL_LowerBlit is the faster blitfunction - but again - all together: slow
	do
	{
		newline = strstr( c, "\\" );
		if ( newline )
			newline='\0';

		TTF_SizeUTF8( f->font, c, &l, &h );
		if ( ! l )
			return 0;

		if ( align > 0 && align < ALIGN_LAST )
		{
			switch ( align % 3 )
			{
			case 1: x -= l / 2; break;
			case 2: x -= l; break;
			}

			switch ( align / 3 )
			{
			case 1: y -= h / 2; break;
			case 2: y -= h; break;
			}
		}
		textSurface = TTF_RenderUTF8_Blended(f->font, c, color);
		if ( ! textSurface )
		{
			ri.Con_Printf(PRINT_ALL, "%s\n", TTF_GetError() );
			return 0;
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

// 		SDL_SaveBMP( openGLSurface, c );

		/* Tell GL about our new texture */
		qglGenTextures(1, &texture);
		GL_Bind(texture);
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, openGLSurface->pixels );

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

		/* Bad things happen if we delete the texture before it finishes */
		qglFinish();

		/* Clean up */
		SDL_FreeSurface(textSurface);
		SDL_FreeSurface(openGLSurface);
		qglDeleteTextures(1, &texture);

		if ( newline )
		{
			c = newline + 1;
			y += TTF_FontLineSkip( f->font );
		}
	} while ( newline );
#else
	// get alignement
	// (do nothing if left aligned or unknown)
	if ( align > 0 && align < ALIGN_LAST )
	{
		switch ( align % 3 )
		{
		case 1: x -= Draw_PropLength( font, c ) / 2; break;
		case 2: x -= Draw_PropLength( font, c ); break;
		}

		switch ( align / 3 )
		{
		case 1: y -= f->h / 2; break;
		case 2: y -= f->h; break;
		}
	}

	// parse the string
	GL_Bind (f->image->texnum);
	for ( l = 0; *c; c++ )
	{
		if ( *c == '\n' || *c == '\\' )
		{
			y+=f->h;
			l=0;
		}
		else
		    	l += Draw_PropCharFont( f, x + l, y, *c );
	}
#endif
	return l;
}

/*
================
Draw_Color
================
*/
void Draw_Color (float *rgba)
{
	if ( rgba )
	{
		if ( rgba[3] < 1.0f )
			qglEnable (GL_BLEND);
		qglColor4fv( rgba );
	}
	else
	{
		qglDisable (GL_BLEND);
		qglColor4f( 1, 1, 1, 1 );
	}
}

/*
=============
Draw_FindPic
=============
*/
image_t	*Draw_FindPic (char *name)
{
	image_t *gl;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
		Com_sprintf (fullname, sizeof(fullname), "pics/%s", name);
	else
		strncpy( fullname, name+1, MAX_QPATH );

	gl = GL_FindImage (fullname, it_pic);
	return gl;
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize (int *w, int *h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		*w = *h = -1;
		return;
	}
	*w = gl->width;
	*h = gl->height;
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int w, int h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload ();

	GL_Bind (gl->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2f (x+w, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (x+w, y+h);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (x, y+h);
	qglEnd ();
}


/*
=============
Draw_NormPic
=============
*/
void Draw_NormPic (float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, qboolean blend, char *name)
{
	float	nx, ny, nw = 0.0, nh = 0.0;
	image_t *gl;

	gl = Draw_FindPic( name );
	if (!gl)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", name);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload ();

	// normalize
	nx = x * vid.rx;
	ny = y * vid.ry;

	if ( w && h ) {
		nw = w * vid.rx;
		nh = h * vid.ry;
	}

	if ( sh && th )
	{
		if ( !w || !h ) {
			nw = (sh - sl) * vid.rx;
			nh = (th - tl) * vid.ry;
		}
		sl = sl / gl->width;  sh = sh / gl->width;
		tl = tl / gl->height; th = th / gl->height;
	} else {
		if ( !w || !h ) {
			nw = (float)gl->width  * vid.rx;
			nh = (float)gl->height * vid.ry;
		}
		sh = 1; th = 1;
	}

	// get alignement
	// (do nothing if left aligned or unknown)
	if ( align > 0 && align < ALIGN_LAST )
	{
		switch ( align % 3 )
		{
		case 1: nx -= nw * 0.5; break;
		case 2: nx -= nw; break;
		}

		switch ( align / 3 )
		{
		case 1: ny -= nh * 0.5; break;
		case 2: ny -= nh; break;
		}
	}

	if ( blend ) qglEnable( GL_BLEND );

	GL_Bind (gl->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (sl, tl);
	qglVertex2f (nx, ny);
	qglTexCoord2f (sh, tl);
	qglVertex2f (nx+nw, ny);
	qglTexCoord2f (sh, th);
	qglVertex2f (nx+nw, ny+nh);
	qglTexCoord2f (sl, th);
	qglVertex2f (nx, ny+nh);
	qglEnd ();

	if ( blend ) qglDisable( GL_BLEND );
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload ();

	GL_Bind (gl->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2f (x+gl->width, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (x+gl->width, y+gl->height);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (x, y+gl->height);
	qglEnd ();
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, char *name)
{
	image_t	*image;

	image = Draw_FindPic (name);
	if (!image)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", name);
		return;
	}

	GL_Bind (image->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (x/64.0, y/64.0);
	qglVertex2f (x, y);
	qglTexCoord2f ( (x+w)/64.0, y/64.0);
	qglVertex2f (x+w, y);
	qglTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
	qglVertex2f (x+w, y+h);
	qglTexCoord2f ( x/64.0, (y+h)/64.0 );
	qglVertex2f (x, y+h);
	qglEnd ();
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int style, vec4_t color)
{
	float nx, ny, nw, nh;

	nx = x * vid.rx;
	ny = y * vid.ry;
	nw = w * vid.rx;
	nh = h * vid.ry;

	Draw_Color( color );

	qglDisable (GL_TEXTURE_2D);
	qglBegin (GL_QUADS);

	switch ( style )
	{
	case ALIGN_CL:
		qglVertex2f (nx, ny);
		qglVertex2f (nx+nh, ny);
		qglVertex2f (nx+nh, ny-nw);
		qglVertex2f (nx, ny-nw);
		break;
	case ALIGN_CC:
		qglVertex2f (nx, ny);
		qglVertex2f (nx+nh, ny-nh);
		qglVertex2f (nx+nh, ny-nw-nh);
		qglVertex2f (nx, ny-nw);
		break;
	case ALIGN_UC:
		qglVertex2f (nx, ny);
		qglVertex2f (nx+nw, ny);
		qglVertex2f (nx+nw-nh, ny+nh);
		qglVertex2f (nx-nh, ny+nh);
		break;
	default:
		qglVertex2f (nx, ny);
		qglVertex2f (nx+nw, ny);
		qglVertex2f (nx+nw, ny+nh);
		qglVertex2f (nx, ny+nh);
		break;
	}

	qglEnd ();
	Draw_Color( NULL );
	qglEnable (GL_TEXTURE_2D);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	qglEnable (GL_BLEND);
	qglDisable (GL_TEXTURE_2D);
	qglColor4f (0, 0, 0, 0.8);
	qglBegin (GL_QUADS);

	qglVertex2f (0,0);
	qglVertex2f (vid.width, 0);
	qglVertex2f (vid.width, vid.height);
	qglVertex2f (0, vid.height);

	qglEnd ();
	qglColor4f (1,1,1,1);
	qglEnable (GL_TEXTURE_2D);
	qglDisable (GL_BLEND);
}


//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
extern unsigned	r_rawpalette[256];

void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	unsigned	image32[256*256];
	int			i, j, trows;
	byte		*source;
	int			frac, fracstep;
	float		hscale;
	int			row;
	float		t;
	unsigned *dest;

	GL_Bind (0);

	if (rows<=256)
	{
		hscale = 1;
		trows = rows;
	}
	else
	{
		hscale = rows/256.0;
		trows = 256;
	}
	t = rows*hscale / 256;

	for (i=0 ; i<trows ; i++)
	{
		row = (int)(i*hscale);
		if (row > rows)
			break;
		source = data + cols*row;
		dest = &image32[i*256];
		fracstep = cols*0x10000/256;
		frac = fracstep >> 1;
		for (j=0 ; j<256 ; j++)
		{
			dest[j] = r_rawpalette[source[frac>>16]];
			frac += fracstep;
		}
	}

	qglTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, image32);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )
		qglDisable (GL_ALPHA_TEST);

	qglBegin (GL_QUADS);
	qglTexCoord2f (0, 0);
	qglVertex2f (x, y);
	qglTexCoord2f (1, 0);
	qglVertex2f (x+w, y);
	qglTexCoord2f (1, t);
	qglVertex2f (x+w, y+h);
	qglTexCoord2f (0, t);
	qglVertex2f (x, y+h);
	qglEnd ();

	if ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )
		qglEnable (GL_ALPHA_TEST);
}


/*
================
Draw_DayAndNight
================
*/
float	lastQ;

void Draw_DayAndNight( int x, int y, int w, int h, float p, float q, float cx, float cy, float iz )
{
	image_t *gl;
	float nx, ny, nw, nh;

	// normalize
	nx = x * vid.rx;
	ny = y * vid.ry;
	nw = w * vid.rx;
	nh = h * vid.ry;

	// load day image
	gl = GL_FindImage( "pics/menu/map_earth_day", it_wrappic );

	// draw day image
	GL_Bind (gl->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f( cx-iz, cy-iz );
	qglVertex2f (nx, ny);
	qglTexCoord2f( cx+iz, cy-iz );
	qglVertex2f (nx+nw, ny);
	qglTexCoord2f( cx+iz, cy+iz );
	qglVertex2f (nx+nw, ny+nh);
	qglTexCoord2f( cx-iz, cy+iz );
	qglVertex2f (nx, ny+nh);
	qglEnd ();

	// test for multitexture and env_combine support
	if ( !qglSelectTextureSGIS && !qglActiveTextureARB )
		return;

	// init combiner
	qglEnable( GL_BLEND );

	GL_SelectTexture( gl_texture0 );
	gl = GL_FindImage( "pics/menu/map_earth_night", it_wrappic );
	GL_Bind( gl->texnum );

	GL_SelectTexture( gl_texture1 );
	if ( !DaN || lastQ != q )
	{
		GL_CalcDayAndNight( q );
		lastQ = q;
	}

	GL_Bind( DaN->texnum );
	qglEnable( GL_TEXTURE_2D );

	// draw night image
	qglBegin (GL_QUADS);
	qglMTexCoord2fSGIS( gl_texture0, cx-iz, cy-iz );
	qglMTexCoord2fSGIS( gl_texture1, p+cx-iz, cy-iz );
	qglVertex2f (nx, ny);
	qglMTexCoord2fSGIS( gl_texture0, cx+iz, cy-iz );
	qglMTexCoord2fSGIS( gl_texture1, p+cx+iz, cy-iz );
	qglVertex2f (nx+nw, ny);
	qglMTexCoord2fSGIS( gl_texture0, cx+iz, cy+iz );
	qglMTexCoord2fSGIS( gl_texture1, p+cx+iz, cy+iz );
	qglVertex2f (nx+nw, ny+nh);
	qglMTexCoord2fSGIS( gl_texture0, cx-iz, cy+iz );
	qglMTexCoord2fSGIS( gl_texture1, p+cx-iz, cy+iz );
	qglVertex2f (nx, ny+nh);
	qglEnd ();

	// reset mode
	qglDisable( GL_TEXTURE_2D );
	GL_SelectTexture( gl_texture0 );

	qglDisable( GL_BLEND );
}

/*
================
Draw_LineStrip
================
*/
#define MAX_LINEVERTS 256
void Draw_LineStrip( int points, int *verts )
{
	static int vs[MAX_LINEVERTS*2];
	int i;
//	int *v;

	// fit it on screen
	if ( points > MAX_LINEVERTS*2 ) points = MAX_LINEVERTS*2;

	for ( i = 0; i < points*2; i += 2 )
	{
		vs[i] = verts[i] * vid.rx;
		vs[i+1] = verts[i+1] * vid.ry;
	}

	// init vertex array
	qglDisable( GL_TEXTURE_2D );
	qglEnableClientState( GL_VERTEX_ARRAY );
	qglVertexPointer( 2, GL_INT, 0, vs );

	// draw
	qglDrawArrays( GL_LINE_STRIP, 0, points );

	// reset state
	qglDisableClientState( GL_VERTEX_ARRAY );
	qglEnable( GL_TEXTURE_2D );
}

/* vertices of a unit icosahedron */
static globe_triangle_t icosahedron[MAX_ICOSAHEDRON]= {
        /* "north" pole */

        { {Ip1, Ip0, Ip2}, },
        { {Ip5, Ip0, Ip1}, },
        { {Ip4, Ip0, Ip5}, },
        { {Ip3, Ip0, Ip4}, },
        { {Ip2, Ip0, Ip3}, },

        /* mid */
        { {Ip1, Im1, Im0}, },
        { {Im0, Ip5, Ip1}, },
        { {Ip5, Im0, Im4}, },
        { {Im4, Ip4, Ip5}, },
        { {Ip4, Im4, Im3}, },
        { {Im3, Ip3, Ip4}, },
        { {Ip3, Im3, Im2}, },
        { {Im2, Ip2, Ip3}, },
        { {Ip2, Im2, Im1}, },
        { {Im1, Ip1, Ip2}, },

        /* "south" pole */
        { {Im3, Im5, Im2}, },
        { {Im4, Im5, Im3}, },
        { {Im0, Im5, Im4}, },
        { {Im1, Im5, Im0}, },
        { {Im2, Im5, Im1}, },
};

/*
================
Globe_CoordToVec

This function fills v with the x-, y-, and z-coordinate
for the specified point on the surface.
================
*/
void Globe_CoordToVec ( double lat, double lon, vec3_t v )
{
	v[0] = (float)sin(lon*GLOBE_TORAD)*cos(lat*GLOBE_TORAD);
	v[1] = (float)sin(lat*GLOBE_TORAD);
	v[2] = (float)cos(lon*GLOBE_TORAD)*cos(lat*GLOBE_TORAD);
}

/*
================
Globe_ToLong

returns the longitude of a point on the unit sphere.
(Longitudes are the "x" axis on a mercator projection map.)
================
*/
double Globe_ToLong ( vec3_t v )
{
	double s;
	if(v[1] == 1.0 || v[1] == -1.0)
		return(5000.0);    /* Problem! The texture will warp! Actually these */
	s = sqrt(1 - v[1]*v[1]);   /* points (the poles) are on ALL the longitudes! */
	return((atan2(v[0]/s,v[2]/s) * GLOBE_TODEG));
}

/*
================
Globe_ToLat

returns the latitude of a point on the unit sphere.
(Latitudes are the "y" axis on a mercator projection map.)
================
*/
double Globe_ToLat ( vec3_t v )
{
	return(atan2(v[1], sqrt(1 - v[1]*v[1])) * GLOBE_TODEG);
}

/*
================
Globe_Distance

returns the actual theoretical minimum
distance (in meters) the signal has to travel.
================
*/
unsigned long Globe_Distance( int site )
{
	unsigned long distance_covered = 0;
#if 0
	int i;
	float angle;
	vec3_t v1, v2;

	for(i=1 ; i<=site ; i++)
	{
		/* This is not really the way to do this. This assumes that
			earth is round (it isn't!). I'm sure there's a good way to
			compute the actual distance between two locations on earth. */

		/*    Angle is arccos(Ax*Bx + Ay*By + Az*Bz)    */

		Globe_CoordToVec ( sites[i].lat, sites[i].lon, v1 );
		Globe_CoordToVec ( sites[i-1].lat, sites[i-1].lon, v2 );

		angle = acos((float)(v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2]) * GLOBE_TODEG;

		distance_covered += (int)rint((angle/360.0) * PHYSICAL_EARTH_CIRC);
	}
#endif
	return distance_covered;
}

/*
================
Globe_Normalize

normalize vector r
================
*/
void Globe_Normalize(vec3_t r)
{
	float mag;

	mag = r[0] * r[0] + r[1] * r[1] + r[2] * r[2];
	if (mag != 0.0f)
	{
		mag = 1.0f / sqrt(mag);
		r[0] *= mag;
		r[1] *= mag;
		r[2] *= mag;
	}
}

/*
================
Globe_Lerp

linearly interpolate between a & b, by fraction f
================
*/
void Globe_Lerp(vec3_t a, vec3_t b, float f, vec3_t r)
{
	r[0] = a[0] + f*(b[0]-a[0]);
	r[1] = a[1] + f*(b[1]-a[1]);
	r[2] = a[2] + f*(b[2]-a[2]);
}

/*
================
Globe_AddVertex

Convenience function used only in Draw_3DGlobe.
Adds a vertex to a tstrip.
================
*/
void Globe_AddVertex(vec3_t v, double *lastlon)
{
	double a, b, dist;
	vec3_t x;

	VectorCopy( x, v );
	Globe_Normalize(x);
	qglNormal3fv(x);
	a = Globe_ToLong(x);
	b = Globe_ToLat(x);

	if(a == 5000.0)	/* 5000 is the slightly unintuitive value tolon() */
	{		/* returns when it finds a value on a pole. */
		if(b > 0)	/* If it's the north pole */
			a = *lastlon - 36.0;
		else		/* South pole. */
			a = *lastlon + 36.0;
	}
	else if(*lastlon != 4000.0)   /* Not the first vertex in a tstrip.*/
	{
		/* This stuff is to get the wraparoud at the backside of the
			sphere to work. (Or you'd get a weird zigzag line down the
			back of the sphere.) */

		dist = a - *lastlon;
		if(dist > 100.0)
			a -= 360.0;
		else if(dist < -100.0)
			a += 360.0;
	}
	*lastlon = a;
	qglTexCoord2f(a/360.0+0.5, -(b/180.0+0.5));
	qglVertex3fv(x);
}

/*
================
Draw_3DMapMarkers
================
*/
void Draw_3DMapMarkers ( float latitude, float longitude, char* image )
{

}

/*
================
This is only used from makeearth, to plot a line on the globe.
There is some difficultish maths in this one.
"to" and "from" in the comments in the code refers to the two
points given as arguments.
================
*/
void Draw_3DMapLine ( int n, float dist, vec2_t *path )
{
	float	vink;
	float	linesweep;
	vec3_t	v1, v2;

	qglPushMatrix();
// 	qglMaterialfv(GL_FRONT, GL_DIFFUSE, lineColor);

	/*    Angle is arccos(Ax*Bx + Ay*By + Az*Bz)    */
	Globe_CoordToVec ( path[n][0], path[n][1], v1 );
	Globe_CoordToVec ( path[0][0], path[0][1], v2 );
	linesweep = acos((float)(v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2]) * GLOBE_TODEG);

	/*   This mess calculates the angle to rotate "to" to a position
	'south' of "from". It's a somewhat adapted version of a formula
	that calculates an angle in a triangle on a sphere given all three
	sides. (Found the original in Beta.)                           */
	vink = GLOBE_TODEG*acos((sin(path[n][0]*GLOBE_TORAD)-sin(path[0][0]*GLOBE_TORAD)*cos(linesweep*GLOBE_TORAD))/
			( cos(path[0][0]*GLOBE_TORAD) * sin(linesweep*GLOBE_TORAD) )      );

	/*   Compensate for the usual arccos problem. (It checks if "to" is
	'to the right' or 'to the left' of "from")                 */

	if ( path[0][1] > 0.0 && ( path[n][1] > path[0][1] || path[n][1] < path[0][1] - 180.0 ) )
		vink = -vink;
	else if( path[n][1] > path[0][1] && path[n][1] < path[0][1] + 180.0 )
		vink = -vink;

	/*   Rotate "to" to a position 'south' of "from".   */
	qglRotatef(vink, v2[0], v2[1], v2[2] );

	/*   ...and rotate them onto the z=0 plane.   */
	qglRotatef(90.0+path[0][1], 0.0, 1.0, 0.0);

	/*   Fix incorrect names on the lines, identifying them as the
	sitemarker we just drew.             */
// 	qglLoadName(NOT_SELECTABLE);

// 	partialtorus((GLfloat)path[0]->[1]-180.0, (GLfloat)linesweep, zoom);
	qglPopMatrix();
}

/*
================
Draw_3DGlobe

responsible for drawing the 3d globe on geoscape
================
*/
void Draw_3DGlobe ( int x, int y, int w, int h, float p, float q, float cx, float cy, float iz )
{
	int nrows = 1 << 3;
	int s, i, j;
	double last_lon;
	vec3_t v0, v1, v2, v3, va, vb;
	globe_triangle_t *t = NULL;
	image_t *gl;
// 	float nx, ny, nw, nh;

	/* iterate over the 20 sides of the icosahedron */
	for(s = 0; s < MAX_ICOSAHEDRON; s++)
	{
		t = &icosahedron[s];
		for(i = 0; i < nrows; i++)
		{
			/* create a tstrip for each row */
			/* number of triangles in this row is number in previous +2 */
			/* strip the ith trapezoid block */
			Globe_Lerp(t->vec[1], t->vec[0], (float)(i+1)/nrows, v0);
			Globe_Lerp(t->vec[1], t->vec[0], (float)i/nrows, v1);
			Globe_Lerp(t->vec[1], t->vec[2], (float)(i+1)/nrows, v2);
			Globe_Lerp(t->vec[1], t->vec[2], (float)i/nrows, v3);

			/* Hack to get the texturing right. */
			last_lon = 4000.0;

			qglBegin(GL_TRIANGLE_STRIP);
			Globe_AddVertex(v0, &last_lon);
			Globe_AddVertex(v1, &last_lon);
			for(j = 0; j < i; j++)
			{
				/* calculate 2 more vertices at a time */
				Globe_Lerp(v0, v2, (float)(j+1)/(i+1), va);
				Globe_Lerp(v1, v3, (float)(j+1)/i, vb);
				Globe_AddVertex(va, &last_lon);
				Globe_AddVertex(vb, &last_lon);
			}
			Globe_AddVertex(v2, &last_lon);
			qglEnd(); /* TRIANGLE_STRIP */
		}
	}
	gl = GL_FindImage( "pics/menu/map_earth_day", it_wrappic );
	GL_Bind (gl->texnum);

	// TODO: map to sphere and display

	// test for multitexture and env_combine support
	if ( !qglSelectTextureSGIS && !qglActiveTextureARB )
		return;

	// init combiner
	qglEnable( GL_BLEND );

	GL_SelectTexture( gl_texture0 );
	gl = GL_FindImage( "pics/menu/map_earth_night", it_wrappic );
	GL_Bind( gl->texnum );

	GL_SelectTexture( gl_texture1 );
	if ( !DaN || lastQ != q )
	{
		GL_CalcDayAndNight( q );
		lastQ = q;
	}

	GL_Bind( DaN->texnum );

	// TODO: map to sphere and display

	qglDisable( GL_BLEND );
}

#ifdef BUILD_FREETYPE

void R_GetGlyphInfo(FT_GlyphSlot glyph, int *left, int *right, int *width, int *top, int *bottom, int *height, int *pitch)
{
	*left  = _FLOOR( glyph->metrics.horiBearingX );
	*right = _CEIL( glyph->metrics.horiBearingX + glyph->metrics.width );
	*width = _TRUNC(*right - *left);

	*top    = _CEIL( glyph->metrics.horiBearingY );
	*bottom = _FLOOR( glyph->metrics.horiBearingY - glyph->metrics.height );
	*height = _TRUNC( *top - *bottom );
	*pitch  = ( true ? (*width+3) & -4 : (*width+7) >> 3 );
}


FT_Bitmap *R_RenderGlyph(FT_GlyphSlot glyph, glyphInfo_t* glyphOut)
{
	FT_Bitmap  *bit2;
	int left, right, width, top, bottom, height, pitch, size;

	R_GetGlyphInfo(glyph, &left, &right, &width, &top, &bottom, &height, &pitch);

	if ( glyph->format == ft_glyph_format_outline )
	{
		size   = pitch*height;

		bit2 = malloc(sizeof(FT_Bitmap));

		bit2->width      = width;
		bit2->rows       = height;
		bit2->pitch      = pitch;
		bit2->pixel_mode = ft_pixel_mode_grays;
		//bit2->pixel_mode = ft_pixel_mode_mono;
		bit2->buffer     = malloc(pitch*height);
		bit2->num_grays = 256;

		memset( bit2->buffer, 0, size );

		FT_Outline_Translate( &glyph->outline, -left, -bottom );

		FT_Outline_Get_Bitmap( ftLibrary, &glyph->outline, bit2 );

		glyphOut->height = height;
		glyphOut->pitch = pitch;
		glyphOut->top = (glyph->metrics.horiBearingY >> 6) + 1;
		glyphOut->bottom = bottom;

		return bit2;
	} else {
		ri.Con_Printf(PRINT_ALL, "Non-outline fonts are not supported\n");
	}
	return NULL;
}

static glyphInfo_t *RE_ConstructGlyphInfo(unsigned char *imageOut, int *xOut, int *yOut, int *maxHeight, FT_Face face, const unsigned char c, qboolean calcHeight)
{
	int i;
	static glyphInfo_t glyph;
	unsigned char *src, *dst;
	float scaled_width, scaled_height;
	FT_Bitmap *bitmap = NULL;

	memset(&glyph, 0, sizeof(glyphInfo_t));
	// make sure everything is here
	if (face != NULL)
	{
		FT_Load_Glyph(face, FT_Get_Char_Index( face, c ), FT_LOAD_DEFAULT );
		bitmap = R_RenderGlyph(face->glyph, &glyph);
		if (bitmap)
			glyph.xSkip = (face->glyph->metrics.horiAdvance >> 6) + 1;
		else
			return &glyph;

		if (glyph.height > *maxHeight)
			*maxHeight = glyph.height;

		if (calcHeight)
		{
			free(bitmap->buffer);
			free(bitmap);
			return &glyph;
		}

		scaled_width = glyph.pitch;
		scaled_height = glyph.height;

		// we need to make sure we fit
		if (*xOut + scaled_width + 1 >= 255)
		{
			if (*yOut + *maxHeight + 1 >= 255)
			{
				*yOut = -1;
				*xOut = -1;
				free(bitmap->buffer);
				free(bitmap);
				return &glyph;
			} else {
				*xOut = 0;
				*yOut += *maxHeight + 1;
			}
		}
		else if (*yOut + *maxHeight + 1 >= 255)
		{
			*yOut = -1;
			*xOut = -1;
			free(bitmap->buffer);
			free(bitmap);
			return &glyph;
		}

		src = bitmap->buffer;
		dst = imageOut + (*yOut * 256) + *xOut;

		if (bitmap->pixel_mode == ft_pixel_mode_mono)
		{
			for (i = 0; i < glyph.height; i++)
			{
				int j;
				unsigned char *_src = src;
				unsigned char *_dst = dst;
				unsigned char mask = 0x80;
				unsigned char val = *_src;
				for (j = 0; j < glyph.pitch; j++)
				{
					if (mask == 0x80)
						val = *_src++;
					if (val & mask)
						*_dst = 0xff;

					mask >>= 1;

					if ( mask == 0 )
						mask = 0x80;

					_dst++;
				}

				src += glyph.pitch;
				dst += 256;

			}
		} else {
			for (i = 0; i < glyph.height; i++)
			{
				memcpy(dst, src, glyph.pitch);
				src += glyph.pitch;
				dst += 256;
			}
		}

		// we now have an 8 bit per pixel grey scale bitmap
		// that is width wide and pf->ftSize->metrics.y_ppem tall
		glyph.imageHeight = scaled_height;
		glyph.imageWidth = scaled_width;
		glyph.s = (float)*xOut / 256;
		glyph.t = (float)*yOut / 256;
		glyph.s2 = glyph.s + (float)scaled_width / 256;
		glyph.t2 = glyph.t + (float)scaled_height / 256;

		*xOut += scaled_width + 1;
	}

	free(bitmap->buffer);
	free(bitmap);

	return &glyph;
}

int readInt( void )
{
	int i = fdFile[fdOffset]+(fdFile[fdOffset+1]<<8)+(fdFile[fdOffset+2]<<16)+(fdFile[fdOffset+3]<<24);
	fdOffset += 4;
	return i;
}

float readFloat( void )
{
	poor	me;
#ifdef __BIG_ENDIAN__
	me.fred[0] = fdFile[fdOffset+3];
	me.fred[1] = fdFile[fdOffset+2];
	me.fred[2] = fdFile[fdOffset+1];
	me.fred[3] = fdFile[fdOffset+0];
#else
	me.fred[0] = fdFile[fdOffset+0];
	me.fred[1] = fdFile[fdOffset+1];
	me.fred[2] = fdFile[fdOffset+2];
	me.fred[3] = fdFile[fdOffset+3];
#endif
	fdOffset += 4;
	return me.ffred;
}

void RE_RegisterFTFont(const char *fontName, int pointSize, fontInfo_t *font)
{
	FT_Face face;
	int k, xOut, yOut, lastStart, imageNumber;
	int scaledSize, newSize, maxHeight, left, satLevels;
	unsigned char *out, *imageBuff;
	glyphInfo_t *glyph;
	image_t *image;
	float max;
	void *faceData;
	int i, len;
	char name[1024];
	float dpi = 72;
	// change the scale to be relative to 1 based on
	// 72 dpi ( so dpi of 144 means a scale of .5 )
	float glyphScale =  72.0f / dpi;

	if (!fontName)
	{
		ri.Con_Printf(PRINT_ALL, "RE_RegisterFTFont: called with empty name\n");
		return;
	}

	if (pointSize <= 0)
		pointSize = 12;

	// we also need to adjust the scale based on point size relative to 48 points as the ui scaling is based on a 48 point font
	glyphScale *= 48.0f / pointSize;

	if (registeredFontCount >= MAX_FONTS)
	{
		ri.Con_Printf(PRINT_ALL, "RE_RegisterFTFont: Too many fonts registered already.\n");
		return;
	}

	Com_sprintf(name, sizeof(name), "fonts/fontImage_%i.dat",pointSize);
	for (i = 0; i < registeredFontCount; i++)
	{
		if (Q_stricmp(name, registeredFont[i].name) == 0)
		{
			memcpy(font, &registeredFont[i], sizeof(fontInfo_t));
			return;
		}
	}

	len = ri.FS_LoadFile(name, NULL);
	if (len == sizeof(fontInfo_t))
	{
		ri.FS_LoadFile(name, &faceData);
		fdOffset = 0;
		fdFile = faceData;
		for(i=0; i<GLYPHS_PER_FONT; i++)
		{
			font->glyphs[i].height		= readInt();
			font->glyphs[i].top			= readInt();
			font->glyphs[i].bottom		= readInt();
			font->glyphs[i].pitch		= readInt();
			font->glyphs[i].xSkip		= readInt();
			font->glyphs[i].imageWidth	= readInt();
			font->glyphs[i].imageHeight = readInt();
			font->glyphs[i].s			= readFloat();
			font->glyphs[i].t			= readFloat();
			font->glyphs[i].s2			= readFloat();
			font->glyphs[i].t2			= readFloat();
			font->glyphs[i].glyph		= readInt();
			fdOffset += 32;
		}
		font->glyphScale = readFloat();
		memcpy(font->name, &fdFile[fdOffset], MAX_QPATH);

		Com_sprintf(font->name, sizeof(font->name), name);
		memcpy(&registeredFont[registeredFontCount++], font, sizeof(fontInfo_t));
		return;
	}

	if (ftLibrary == NULL)
	{
		ri.Con_Printf(PRINT_ALL, "RE_RegisterFTFont: FreeType not initialized.\n");
		return;
	}

	len = ri.FS_LoadFile(va("%s", fontName ), &faceData);
	if (len <= 0)
	{
		ri.Con_Printf(PRINT_ALL, "RE_RegisterFTFont: Unable to read font file\n");
		return;
	}

	// allocate on the stack first in case we fail
	if (FT_New_Memory_Face( ftLibrary, faceData, len, 0, &face ))
	{
		ri.Con_Printf(PRINT_ALL, "RE_RegisterFTFont: FreeType2, unable to allocate new face.\n");
		return;
	}

	if (FT_Set_Char_Size( face, pointSize << 6, pointSize << 6, dpi, dpi))
	{
		ri.Con_Printf(PRINT_ALL, "RE_RegisterFTFont: FreeType2, Unable to set face char size.\n");
		return;
	}

	//*font = &registeredFonts[registeredFontCount++];

	// make a 256x256 image buffer, once it is full, register it, clean it and keep going
	// until all glyphs are rendered

	out = malloc(1024*1024);
	if (out == NULL)
	{
		ri.Con_Printf(PRINT_ALL, "RE_RegisterFTFont: malloc failure during output image creation.\n");
		return;
	}
	memset(out, 0, 1024*1024);

	maxHeight = 0;

	for (i = GLYPH_START; i < GLYPH_END; i++)
		glyph = RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, true);

	xOut = 0;
	yOut = 0;
	i = GLYPH_START;
	lastStart = i;
	imageNumber = 0;

	while ( i <= GLYPH_END )
	{
		glyph = RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, false);

		if (xOut == -1 || yOut == -1 || i == GLYPH_END)
		{
			// ran out of room
			// we need to create an image from the bitmap, set all the handles in the glyphs to this point
			scaledSize = 256*256;
			newSize = scaledSize * 4;
			imageBuff = malloc(newSize);
			left = 0;
			max = 0;
			satLevels = 255;
			for ( k = 0; k < (scaledSize) ; k++ )
				if (max < out[k])
					max = out[k];

			if (max > 0)
				max = 255/max;

			for ( k = 0; k < (scaledSize) ; k++ )
			{
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;
				imageBuff[left++] = ((float)out[k] * max);
			}

			Com_sprintf (name, sizeof(name), "fonts/fontImage_%i_%i.tga", imageNumber++, pointSize);
			if (r_saveFontData->value)
				WriteTGA(name, imageBuff, 256, 256);

			image = GL_LoadPic (name, imageBuff, 256, 256, it_font, 8);

			lastStart = i;
			memset(out, 0, 1024*1024);
			xOut = 0;
			yOut = 0;
			free(imageBuff);
			i++;
		} else {
			memcpy(&font->glyphs[i], glyph, sizeof(glyphInfo_t));
			i++;
		}
	}

	registeredFont[registeredFontCount].glyphScale = glyphScale;
	font->glyphScale = glyphScale;
	memcpy(&registeredFont[registeredFontCount++], font, sizeof(fontInfo_t));

	if (r_saveFontData->value)
		ri.FS_WriteFile(va("fonts/fontImage_%i.dat", pointSize), sizeof(fontInfo_t), (void*)font);

	free(out);

	ri.FS_FreeFile(faceData);
}



void R_InitFreeType(void)
{
	if (FT_Init_FreeType( &ftLibrary ))
		ri.Con_Printf(PRINT_ALL, "R_InitFreeType: Unable to initialize FreeType.\n");
	numFonts = 0;

// 	RE_RegisterFTFont("fonts/new.ttf", 12, &big );
// 	RE_RegisterFTFont("fonts/new.ttf", 10, &normal );
// 	RE_RegisterFTFont("fonts/new.ttf", 8, &small );
}


void R_DoneFreeType(void)
{
	if (ftLibrary)
	{
		FT_Done_FreeType( ftLibrary );
		ftLibrary = NULL;
	}
	numFonts = 0;
}
#endif /* BUILD_FREETYPE */
