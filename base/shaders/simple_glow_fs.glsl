/**
 * @file simple_glow_fs.glsl
 * @brief This is the glow shader that is applied if no other shader is
 * active but the glow map should get rendered.
 */
/*
 * Indicates that gl_FragData is written to, not gl_FragColor.
 * #extension needs to be placed before all non preprocessor code.
 */
#extension GL_ARB_draw_buffers : enable

#ifndef glsl110
	/** After glsl1110 this need to be explicitly declared; used by fixed functionality at the end of the OpenGL pipeline.*/
	out vec4 gl_FragData[2];
#endif

/** Diffuse. */
uniform sampler2D SAMPLER0;
/** Glowmap. */
uniform sampler2D SAMPLER1;
uniform float GLOWSCALE;

void main(void) {
	vec4 color;
	color.rgba = texture2D(SAMPLER0, gl_TexCoord[0].st);
	gl_FragData[0].rgb = color.rgb * color.a;
	gl_FragData[0].a = 1.0;

	color.rgba = texture2D(SAMPLER1, gl_TexCoord[0].st);
	gl_FragData[1].rgb = color.rgb * color.a * GLOWSCALE;
	gl_FragData[1].a = 1.0;
}
