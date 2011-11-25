/**
 * @file combine2_fs.glsl
 * @brief combine2 fragment shader.
 */

#ifndef glsl110
	/** After glsl1110 this need to be explicitly declared; used by fixed functionality at the end of the OpenGL pipeline.*/
	out vec4 gl_FragColor;
#endif

uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;
uniform vec4 DEFAULTCOLOR;

void main(void) {
	vec4 color1 = texture2D(SAMPLER0, gl_TexCoord[0].st);
	vec4 color2 = texture2D(SAMPLER1, gl_TexCoord[0].st);
	gl_FragColor = color1 + color2 + DEFAULTCOLOR;
}
