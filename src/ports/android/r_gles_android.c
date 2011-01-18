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

#define CONVERT_PIXEL_RGBA8888_RGBA4444( X ) ( ( (X) & 0xf ) | ( ((X) & 0xf00) >> 4 ) | ( ((X) & 0xf0000) >> 8 ) | ( ((X) & 0xf000000) >> 12 ) )

#define CONVERT_RGBA8888_RGBA4444(data, length) { \
	GLuint length2 = (length); \
	for(int ixx = 0; ixx != length2; ixx++) {  \
		GLuint pixel = ((GLuint *)(data))[ixx]; \
		((GLushort *)(data))[ixx] = CONVERT_PIXEL_RGBA8888_RGBA4444( pixel ); \
	} \
}

#define CONVERT_PIXEL_RGB888_RGB565( X ) ( ( (X) & 0x1f ) | ( ((X) & 0x3f00) >> 3 ) | ( ((X) & 0x1f0000) >> 5 ) )

#define CONVERT_RGB888_RGB565(data, length) { \
	GLuint length2 = (length); \
	for(int ixx = 0; ixx != length2; ixx++) {  \
		GLuint pixel = ((GLuint)((unsigned char *)(data))[ixx*3]) | (((GLuint)((unsigned char *)(data))[ixx*3+1]) << 8) | (((GLuint)((unsigned char *)(data))[ixx*3+2]) << 16); \
		((GLushort *)(data))[ixx] = CONVERT_PIXEL_RGB888_RGB565( pixel ); \
	} \
}

#define GL_CONVERT_NATIVE_PIXEL_FORMAT_ALPHA(pixels, w, h) CONVERT_RGBA8888_RGBA4444(pixels, (w)*(h))
#define GL_CONVERT_NATIVE_PIXEL_FORMAT_SOLID(pixels, w, h) CONVERT_RGB888_RGB565(pixels, (w)*(h))

void R_TextureConvertNativePixelFormat(void * pixels, int w, int h, int alphaUsed)
{
	if(alphaUsed) {
		GL_CONVERT_NATIVE_PIXEL_FORMAT_ALPHA(pixels, w, h);
	} else {
		GL_CONVERT_NATIVE_PIXEL_FORMAT_SOLID(pixels, w, h);
	}
}
