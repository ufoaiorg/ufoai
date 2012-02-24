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

/** Diffuse. */
uniform sampler2D SAMPLER_DIFFUSE;

#include "write_fragment_fs.glsl"

void main(void) {
	vec4 diffuse_tex = texture2D(SAMPLER_DIFFUSE, gl_TexCoord[0].st);

	/* glowmap will be added automatically */
	writeFragment(diffuse_tex);
}
