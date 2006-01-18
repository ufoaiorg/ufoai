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

/*
 * Text-mode renderer using AA-lib
 *
 * version 0.1
 *
 * Jacek Fedorynski <jfedor@jfedor.org>
 *
 * http://www.jfedor.org/aaquake2/
 *
 * This file is a modified rw_svgalib.c.
 *
 * Input stuff is in rw_in_aa.c.
 *
 * Changelog:
 *
 * 	2001-12-30	0.1	Initial release
 *
 */

/*
** RW_AA.C
**
** This file contains ALL Linux specific stuff having to do with the
** software refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** SWimp_EndFrame
** SWimp_Init
** SWimp_InitGraphics
** SWimp_SetPalette
** SWimp_Shutdown
** SWimp_SwitchFullscreen
*/

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <sys/mman.h>
#include <dlfcn.h>
#if defined (__linux__)
#include <sys/vt.h>
#include <asm/io.h>
#endif
#include <aalib.h>

#include "../ref_soft/r_local.h"
#include "../client/keys.h"
#include "../linux/rw_linux.h"

/*****************************************************************************/

/* 
 * The following function (fastscale) is taken from Greag Alexander's aavga
 * library. Jan Hubicka <hubicka@freesoft.cz> is the current maintainer of
 * aavga.
 *
 * http://aa-project.sourceforge.net/aavga/
 * 
 */

void fastscale (register char *b1, register char *b2, int x1, int x2, int y1, int y2)
{
  register int ex, spx = 0, ddx, ddx1;
  int ddy1, ddy, spy = 0, ey;
  int x;
  char *bb1 = b1;
  if (!x1 || !x2 || !y1 || !y2)
    return;
  ddx = x1 + x1;
  ddx1 = x2 + x2;
  if (ddx1 < ddx)
    spx = ddx / ddx1, ddx %= ddx1;
  ddy = y1 + y1;
  ddy1 = y2 + y2;
  if (ddy1 < ddy)
    spy = (ddy / ddy1) * x1, ddy %= ddy1;
  ey = -ddy1;
  for (; y2; y2--)
    {
      ex = -ddx1;
      for (x = x2; x; x--)
        {
          *b2 = *b1;
          b2++;
          b1 += spx;
          ex += ddx;
          if (ex > 0)
            {
              b1++;
              ex -= ddx1;
            }
        }
      bb1 += spy;
      ey += ddy;
      if (ey > 0)
        {
          bb1 += x1;
          ey -= ddy1;
        }
      b1 = bb1;
    }
}

/* end of fastscale */

aa_context *aac = NULL;

aa_palette mypalette;

/*
** SWimp_Init
**
** This routine is responsible for initializing the implementation
** specific stuff in a software rendering subsystem.
*/
int SWimp_Init( void *hInstance, void *wndProc )
{
  if (!dlopen("libaa.so", RTLD_LAZY))
    fprintf(stderr, dlerror());
  return true;
}

/*
** SWimp_InitGraphics
**
** This initializes the software refresh's implementation specific
** graphics subsystem.  In the case of Windows it creates DIB or
** DDRAW surfaces.
**
** The necessary width and height parameters are grabbed from
** vid.width and vid.height.
*/
static qboolean SWimp_InitGraphics( qboolean fullscreen )
{
	SWimp_Shutdown();

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow (vid.width, vid.height);

//	Cvar_SetValue ("vid_mode", (float)modenum);
	
	vid.rowbytes = vid.width;

// get goin'
	aa_parseoptions(NULL, NULL, NULL, NULL);
	aa_defparams.supported = (AA_DIM_MASK | AA_BOLD_MASK | AA_NORMAL_MASK
				  | AA_EXTENDED);
	aac = aa_autoinit(&aa_defparams);
	aa_defrenderparams.bright=10;
	aa_defrenderparams.dither = AA_FLOYD_S;
	aa_defparams.dimmul = 2.5;
	aa_defparams.boldmul = 2.5;
	if (!aac)
		Sys_Error("aa_autoinit() failed\n");

	if (!aa_image(aac))
		Sys_Error("This mode isn't hapnin'\n");

	ri.Con_Printf (PRINT_ALL, "AA driver: %s\n", aac->driver->name);
	ri.Con_Printf (PRINT_ALL, "AA resolution: %d %d\n", aa_imgwidth(aac), aa_imgheight(aac));

	vid.buffer = malloc(vid.rowbytes * vid.height);
	if (!vid.buffer)
		Sys_Error("Unabled to alloc vid.buffer!\n");

	aa_resizehandler(aac, (void *) aa_resize);

	return true;
}

/*
** SWimp_EndFrame
**
** This does an implementation specific copy from the backbuffer to the
** front buffer.  In the Win32 case it uses BitBlt or BltFast depending
** on whether we're using DIB sections/GDI or DDRAW.
*/
void SWimp_EndFrame (void)
{
	fastscale(vid.buffer, aa_image(aac), vid.width, aa_imgwidth(aac), vid.height, aa_imgheight(aac));
	aa_renderpalette(aac, mypalette, &aa_defrenderparams, 0, 0, aa_scrwidth (aac), aa_scrheight (aac));
	aa_flush(aac);
}

/*
** SWimp_SetMode
*/
rserr_t SWimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	rserr_t retval = rserr_ok;

	ri.Con_Printf (PRINT_ALL, "setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( pwidth, pheight, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d\n", *pwidth, *pheight);

	if ( !SWimp_InitGraphics( false ) ) {
		// failed to set a valid mode in windowed mode
		return rserr_invalid_mode;
	}

	R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );

	return retval;
}

/*
** SWimp_SetPalette
**
** System specific palette setting routine.  A NULL palette means
** to use the existing palette.  The palette is expected to be in
** a padded 4-byte xRGB format.
*/
void SWimp_SetPalette( const unsigned char *palette )
{
	const unsigned char *pal;
	int i;

    if ( !palette )
        palette = ( const unsigned char * ) sw_state.currentpalette;

	pal = palette;
 
	for (i=0 ; i < 256 ; i++, pal += 4) {
//		aa_setpalette(mypalette, i, pal[0] >> 2, pal[1] >> 2, pal[2] >> 2);
		aa_setpalette(mypalette, i, pal[0], pal[1], pal[2]);
	}
}

/*
** SWimp_Shutdown
**
** System specific graphics subsystem shutdown routine.  Destroys
** DIBs or DDRAW surfaces as appropriate.
*/
void SWimp_Shutdown( void )
{
	if (vid.buffer) {
		free(vid.buffer);
		vid.buffer = NULL;
	}

	if (aac)
		aa_close(aac);
	aac = NULL;
}

/*
** SWimp_AppActivate
*/
void SWimp_AppActivate( qboolean active )
{
}

//===============================================================================

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{

	int r;
	unsigned long addr;
	int psize = getpagesize();

	addr = (startaddr & ~(psize-1)) - psize;

//	fprintf(stderr, "writable code %lx(%lx)-%lx, length=%lx\n", startaddr,
//			addr, startaddr+length, length);

	r = mprotect((char*)addr, length + startaddr - addr + psize, 7);

	if (r < 0)
    		Sys_Error("Protection change failed\n");
}

/*
 * from aavga
 */

