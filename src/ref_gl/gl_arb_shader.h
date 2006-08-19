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

#ifdef SHADERS
#ifndef __GL_ARB_SHADER_H__
#define __GL_ARB_SHADER_H__

shader_t* GL_GetShaderForImage(char* image);
void GL_ShutdownShaders(void);
void GL_ShaderInit(void);
void SH_UseShader(shader_t * shader);
#endif

#endif							/* SHADERS */
