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

image_t		*draw_chars;
image_t		*shadow;

extern	qboolean	scrap_dirty;
void Scrap_Upload (void);

// font definitions
font_t	fonts[MAX_FONTS];
int		numFonts;


/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	// load console characters (don't bilerp characters)
	numFonts = 0;
	draw_chars = GL_FindImage ("pics/conchars.pcx", it_pic);
	shadow = GL_FindImage ("pics/sfx/shadow.tga", it_pic);
	GL_FindImage ("pics/f_small.tga", it_font);
	GL_FindImage ("pics/f_big.tga", it_font);

	GL_Bind( draw_chars->texnum );
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

	if ( (num&255) == 32 )
		return;		// space

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

/*
================
Draw_AnalyzeFont
================
*/
font_t *Draw_AnalyzeFont( char *name, byte *pic, int w, int h )
{
	font_t	*f;
	int		i, l, t;
	int		tx, ty, sx, sy;
	int		chars;
	byte	*tp;

	// get the font name
	l = strlen( name ) - 4;
	for ( i = l; i > 0; i-- )
		if ( name[i-1] == '/' || name[i-1] == '\\' ) break;

	if ( l - i >= MAX_FONTNAME )
	{
		ri.Con_Printf( PRINT_ALL, "Font with too long name\n" );
		return NULL;
	}

	f = &fonts[numFonts];
	strncpy( f->name, name+i, l-i );

	// check the font format (start with minimum 1x1 font)
	for ( sx = 8; sx < w; sx <<= 1 );
	if ( sx != w )
	{
		ri.Con_Printf( PRINT_ALL, "Font '%s' doesn't have a valid width\n", f->name );
		return NULL;
	}

	for ( sy = 8; sy < h; sy <<= 1 );
	if ( sy != h )
	{
		ri.Con_Printf( PRINT_ALL, "Font '%s' doesn't have a valid height\n", f->name );
		return NULL;
	}

	// get character count
	if ( h == w << 1 ) { f->rh = 0.0625; chars = 128; }
	else if ( h == w ) { f->rh = 0.125; chars = 64; }
	else
	{
		ri.Con_Printf( PRINT_ALL, "Font '%s' doesn't have a valid format (1/2 or 1/1)\n", f->name );
		return NULL;
	}

	// get size for single chars
	f->w = sx >>= 3;
	f->rw = 0.125;
	if ( f->rh == 0.125 ) f->h = sy >>= 3;
	else f->h = sy >>= 4;

	tx = 0;
	ty = 0;

	// analyze the width of the chars
	for ( i = 0; i < chars; i++ )
	{
		if ( i % 8 )
		{
			// same line
			tx += sx;
		}
		else
		{
			// new line
			tx = sx-1;
			ty = (i>>3)*sy;
		}

		// test pixels (+3 for alpha)
		tp = pic + ((tx + ty*w)<<2) + 3;

		for ( l = sx; l > 0; l-- )
		{
			// test a vertical line
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

	// return the font
	numFonts++;
	return f;
}


/*
================
Draw_GetFont
================
*/
font_t *Draw_GetFont( char *name )
{
	int		i;
	font_t	*f;

	for ( i = 0, f = fonts; i < numFonts; i++, f++ )
		if ( !strcmp( name, f->name ) )
			return f;

	return NULL;
}


/*
================
Draw_PropCharFont
texture NEEDS to be binded already
================
*/
int Draw_PropCharFont (font_t *f, int x, int y, char c)
{
	int			n, row, col;
	float		nx, ny, cx, cy;
	float		frow, fcol, sx, sy;

	// get the char
	n = (int)(c<32 ? 0 : c-32) & 127;
	if ( f->rh == 0.125 )
		while ( n >= 64 ) n -= 32;

	if ( n < 0 || !f->wc[n] ) return (float)f->w * CHAR_EMPTYWIDTH;

	// transform from 1024x768 coordinates for drawing
	nx = (float)x * vid.rx;
	ny = (float)y * vid.ry;
	cx = (float)f->wc[n] * vid.rx;
	cy = (float)f->h * vid.ry;

	// get texture coordinates
	row = n>>3;
	col = n&7;

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

	// draw the char
	GL_Bind (f->image->texnum);
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
	int		l, n;

	// get the font
	f = Draw_GetFont( font );
	if ( !f ) return 0;

	// parse the string
	for ( l = 0; *c; c++ )
	{
		n = (int)(*c<32 ? 0 : *c-32) & 127;
		if ( f->rh == 0.125 )
			while ( n >= 64 ) n -= 32;

		if ( n < 0 || !f->wc[n] ) l += (float)f->w * CHAR_EMPTYWIDTH;
		else l += f->wc[n]+1;
	}

	return l;
}

/*
================
Draw_PropString
================
*/
int Draw_PropString (char *font, int align, int x, int y, char *c)
{
	font_t	*f;
	int		l;

	// get the font
	f = Draw_GetFont( font );
	if ( !f ) return 0;

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
		l += Draw_PropCharFont( f, x + l, y, *c );

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
		qglColor4fv( rgba );
	else
		qglColor4f( 1, 1, 1, 1 );
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

	qglColor4fv( color );

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
	qglColor4f (1,1,1,1);
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
