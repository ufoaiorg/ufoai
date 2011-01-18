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

#define APIENTRY
typedef char GLchar;
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

#define glOrtho												glOrthof

#endif
