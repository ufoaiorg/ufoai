/* gcc gldummy.c -shared -fpic -ldl -o libGL.so */
#include <GL/gl.h>

void glBindTexture (GLenum target, GLuint id)
{
}

GLenum glGetError (void)
{
}

void glTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
}

void glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
		GLenum format, GLenum type, const GLvoid *texels)
{
}
