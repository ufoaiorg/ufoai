/**
 * @file
 * @brief Warp fragment shader.
 */

#if r_postprocess
	/*
	 * Indicates that gl_FragData is written to, not gl_FragColor.
	 * #extension needs to be placed before all non preprocessor code.
	 */
	#extension GL_ARB_draw_buffers : enable
#endif

#include "fog_fs.glsl"

#ifndef glsl110
	#if r_postprocess
		/** After glsl1110 this need to be explicitly declared; used by fixed functionality at the end of the OpenGL pipeline.*/
		out vec4 gl_FragData[2];
	#else
		/** After glsl1110 this need to be explicitly declared; used by fixed functionality at the end of the OpenGL pipeline.*/
		out vec4 gl_FragColor;
	#endif
#endif

uniform vec4 OFFSET;
uniform float GLOWSCALE;

uniform sampler2D SAMPLER_DIFFUSE;
uniform sampler2D SAMPLER_WARP;
uniform sampler2D SAMPLER_GLOWMAP;

/**
 * @brief Main.
 */
void main(void) {
	vec4 finalColor = vec4(0.0);

	/* Sample glowmap and calculate final glow color. */
	vec4 glow_tex = texture2D(SAMPLER_GLOWMAP, gl_TexCoord[0].st);
	vec3 glowcolor = glow_tex.rgb * glow_tex.a * GLOWSCALE;

	/* Sample the warp texture at a time-varied offset, */
	vec4 warp = texture2D(SAMPLER_WARP, gl_TexCoord[0].xy + OFFSET.xy);

	/* and derive a diffuse texcoord based on the warp data, */
	vec2 coord = vec2(gl_TexCoord[0].x + warp.z, gl_TexCoord[0].y + warp.w);

	/* sample the diffuse texture, factoring in primary color as well. */
	finalColor = gl_Color * texture2D(SAMPLER_DIFFUSE, coord);

#if r_fog
	/* Add fog.*/
	finalColor = FogFragment(finalColor);
#endif

#if r_postprocess
	gl_FragData[0] = finalColor;
	if (GLOWSCALE > 0.01) {
		gl_FragData[1].rgb = glowcolor;
		gl_FragData[1].a = 1.0;
	} else {
		gl_FragData[1] = vec4(0,0,0,1);
	}
#else
	gl_FragColor.rgb = finalColor.rgb + glowcolor;
	gl_FragColor.a = finalColor.a;
#endif
}
