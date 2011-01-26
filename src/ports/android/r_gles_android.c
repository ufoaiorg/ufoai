/**
 * @file r_gles_android.h
 * @brief OpenGL-ES to OpenGL compatibility layer
 */

/*
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
#include "r_gles_android.h"

/* Conversion functions from common 24-bit RGB to Android 16-bit RGB */

#define CONVERT_PIXEL_RGBA8888_RGBA4444( X ) ( ( ( (X) & 0xf0 ) / 0x10 ) | ( ((X) & 0xf000) / 0x100 ) | ( ((X) & 0xf00000) / 0x1000 ) | ( ((X) & 0xf0000000) / 0x10000 ) )

#define CONVERT_PIXEL_RGB888_RGB565( R, G, B ) ( ( B & 0xf8 ) / 0x8 ) | ( ( G & 0xfc ) / 0x4 * 0x20 ) | ( ( R & 0xf8 ) / 0x8 * 0x800 )


void R_TextureConvertNativePixelFormat(void * pixels, int w, int h, int alphaUsed)
{
	if(alphaUsed)
	{
		uint32_t * in = pixels;
		uint16_t * out = pixels;
		for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++)
		{
			out[y*w+x] = CONVERT_PIXEL_RGBA8888_RGBA4444(in[y*w+x]);
		}
	}
	else
	{
		uint8_t * in = pixels;
		uint16_t * out = pixels;
		for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++)
		{
			out[y*w+x] = CONVERT_PIXEL_RGB888_RGB565( in[(y*w+x)*3], in[(y*w+x)*3+1], in[(y*w+x)*3+2] );
		}
	}
}
