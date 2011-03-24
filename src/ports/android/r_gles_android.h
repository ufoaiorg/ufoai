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

#ifndef __R_GLES_ANDROID_H__
#define __R_GLES_ANDROID_H__

#include <GLES/gl.h>
#include <GLES/glext.h>

#define GL_NATIVE_TEXTURE_PIXELFORMAT_SOLID GL_UNSIGNED_SHORT_5_6_5
/* #define GL_NATIVE_TEXTURE_PIXELFORMAT_ALPHA GL_UNSIGNED_SHORT_4_4_4_4 */
#define GL_NATIVE_TEXTURE_PIXELFORMAT_ALPHA GL_UNSIGNED_SHORT_5_5_5_1

typedef char GLchar;

#ifndef APIENTRY
#define APIENTRY
#endif

#define GL_FRAMEBUFFER_EXT									GL_FRAMEBUFFER_OES
#define GL_FRAMEBUFFER_COMPLETE_EXT							GL_FRAMEBUFFER_COMPLETE_OES
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT			GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT	GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT			GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT				GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES
#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT				GL_INVALID_FRAMEBUFFER_OPERATION_OES
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT						GL_FRAMEBUFFER_UNSUPPORTED_OES
#define GL_FRAMEBUFFER_BINDING_EXT							GL_FRAMEBUFFER_BINDING_OES
#define GL_RENDERBUFFER_EXT									GL_RENDERBUFFER_OES
#define GL_MAX_RENDERBUFFER_SIZE_EXT						GL_MAX_RENDERBUFFER_SIZE_OES
#define GL_COLOR_ATTACHMENT0_EXT							GL_COLOR_ATTACHMENT0_OES
#define GL_DEPTH_ATTACHMENT_EXT								GL_DEPTH_ATTACHMENT_OES
#define GL_DEPTH_COMPONENT									GL_DEPTH_COMPONENT16_OES

#ifndef GL_CLAMP
#define GL_CLAMP											GL_CLAMP_TO_EDGE /* Not exactly GL_CLAMP but very close */
#endif

#define glOrtho												glOrthof
#define glFrustum											glFrustumf
#define glFogi												glFogx
#define glDepthRange										glDepthRangef

#define glTranslated( X, Y, Z )								glTranslatex( (X)*0x10000, (Y)*0x10000, (Z)*0x10000 )

/* GLES 2 defines for shaders */
#define GL_COMPILE_STATUS                   0x8B81
#define GL_LINK_STATUS                      0x8B82
#define GL_FRAGMENT_SHADER                  0x8B30
#define GL_VERTEX_SHADER                    0x8B31
#define GL_SHADING_LANGUAGE_VERSION         0x8B8C
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS   0x8B4C
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS
#define GL_MAX_VERTEX_ATTRIBS               0x8869
#define GL_MAX_TEXTURE_COORDS               GL_MAX_TEXTURE_UNITS
#define GL_MAX_VARYING_VECTORS              0x8DFC
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS     0x8DFD
#define GL_MAX_VERTEX_UNIFORM_VECTORS       0x8DFB

#endif
