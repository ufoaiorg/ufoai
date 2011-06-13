/**
 * @file warp_fs.glsl
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

uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;
uniform sampler2D SAMPLER4;

/**
 * @brief Main.
 */
void main(void) {
	vec4 finalColor = vec4(0.0);

	/* Sample the warp texture at a time-varied offset,*/
	vec4 warp = texture2D(SAMPLER1, gl_TexCoord[0].xy + OFFSET.xy);

	/* and derive a diffuse texcoord based on the warp data,*/
	vec2 coord = vec2(gl_TexCoord[0].x + warp.z, gl_TexCoord[0].y + warp.w);

	/* sample the diffuse texture, factoring in primary color as well.*/
	finalColor = gl_Color * texture2D(SAMPLER0, coord);

#if r_fog
	/* Add fog.*/
	finalColor = FogFragment(finalColor);
#endif

#if r_postprocess
	gl_FragData[0] = finalColor;
	if (GLOWSCALE > 0.0) {
		gl_FragData[1] = gl_Color * texture2D(SAMPLER4, coord) * GLOWSCALE;
	}
#else
	gl_FragColor = finalColor;
#endif
}
