/**
 * @file r_error.h
 * @brief Error checking function
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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


#define R_CheckError() R_CheckErrorDebug(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#define GL_ERROR_TRANSLATE(e) case e: return #e;

static inline const char* R_TranslateError (GLenum error)
{
	switch (error) {
	GL_ERROR_TRANSLATE(GL_INVALID_ENUM)
	GL_ERROR_TRANSLATE(GL_INVALID_VALUE)
	GL_ERROR_TRANSLATE(GL_INVALID_OPERATION)
	GL_ERROR_TRANSLATE(GL_OUT_OF_MEMORY)
	GL_ERROR_TRANSLATE(GL_STACK_OVERFLOW)
	GL_ERROR_TRANSLATE(GL_STACK_UNDERFLOW)
	GL_ERROR_TRANSLATE(GL_INVALID_FRAMEBUFFER_OPERATION_EXT)
	default:
		return "UNKNOWN";
	}
}

/**
 * @brief Checks for opengl errors
 */
static inline void R_CheckErrorDebug (const char *file, int line, const char *function)
{
	if (r_checkerror && r_checkerror->integer) {
		/* can return multiple errors */
		for (;;) {
			GLenum error = glGetError();
			if (error != GL_NO_ERROR)
				Com_Printf("OpenGL err: %s (%d): %s %s (0x%X)\n", file, line, function, R_TranslateError(error), error);
			else
				break;
		}
	} else
		glGetError();
}
