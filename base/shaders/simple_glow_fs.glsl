/**
 * @file simple_glow_fs.glsl
 * @brief This is the glow shader that is applied if no other shader is
 * active but the glow map should get rendered.
 */
#if r_postprocess
	/*
	 * Indicates that gl_FragData is written to, not gl_FragColor.
	 * #extension needs to be placed before all non preprocessor code.
	 */
	#extension GL_ARB_draw_buffers : enable
#endif

#ifndef glsl110
	#if r_postprocess
		/** After glsl1110 this need to be explicitly declared; used by fixed functionality at the end of the OpenGL pipeline.*/
		out vec4 gl_FragData[2];
	#else
		/** After glsl1110 this need to be explicitly declared; used by fixed functionality at the end of the OpenGL pipeline.*/
		out vec4 gl_FragColor;
	#endif
#endif

/** Diffuse. */
uniform sampler2D SAMPLER_DIFFUSE;
/** Glowmap. */
uniform sampler2D SAMPLER_GLOWMAP;
uniform float GLOWSCALE;

void main(void) {
	vec4 diffuse_tex = texture2D(SAMPLER_DIFFUSE, gl_TexCoord[0].st);
	vec4 glow_tex = texture2D(SAMPLER_GLOWMAP, gl_TexCoord[0].st);

	vec3 glowcolor = glow_tex.rgb * glow_tex.a * GLOWSCALE;

#if r_postprocess
	gl_FragData[0] = diffuse_tex;
	if (GLOWSCALE > 0.01) {
		gl_FragData[1].rgb = glowcolor;
		gl_FragData[1].a = 1.0;
	} else {
		gl_FragData[1] = vec4(0,0,0,1);
	}
#else
	gl_FragColor.rgb = diffuse_tex.rgb + glowcolor;
	gl_FragColor.a = diffuse_tex.a;
#endif
}
