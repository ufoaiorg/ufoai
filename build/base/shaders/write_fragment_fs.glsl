/**
 * @file
 * @brief Writes given fragment color to the rendering buffer. Adds glow if needed.
 */

#ifndef glsl110
	#if r_postprocess
		/** After glsl1110 this need to be explicitly declared; used by fixed functionality at the end of the OpenGL pipeline.*/
		out vec4 gl_FragData[2];
	#else
		/** After glsl1110 this need to be explicitly declared; used by fixed functionality at the end of the OpenGL pipeline.*/
		out vec4 gl_FragColor;
	#endif
#endif

uniform float GLOWSCALE;

/** Glowmap.*/
uniform sampler2D SAMPLER_GLOWMAP;

void writeFragment(vec4 color) {
#if r_postprocess
	gl_FragData[0] = color;
	if (GLOWSCALE > 0.01) {
		vec4 glowcolor = texture2D(SAMPLER_GLOWMAP, gl_TexCoord[0].st);
		gl_FragData[1].rgb = glowcolor.rgb * glowcolor.a * GLOWSCALE;
		gl_FragData[1].a = 1.0;
	} else {
		gl_FragData[1] = vec4(0,0,0,1);
	}
#else
	if (GLOWSCALE > 0.01) {
		vec4 glowcolor = texture2D(SAMPLER_GLOWMAP, gl_TexCoord[0].st);
		color.rgb += glowcolor.rgb * glowcolor.a * GLOWSCALE;
	}
	gl_FragColor = color;
#endif
}
