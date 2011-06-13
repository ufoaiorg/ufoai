/**
 * @file fog_fs.glsl
 * @brief Fog fragment shader.
 */

#ifndef glsl110
	#ifndef in_qualifier
		/** Linkage into a shader from a previous stage, variable is copied in.*/
		#define in_qualifier in
	#endif
#else
	#ifndef in_qualifier
		/** Deprecated after glsl110; linkage between a vertex shader and a fragment shader for interpolated data.*/
		#define in_qualifier varying
	#endif
#endif

in_qualifier float fog;

/**
 * @brief FogFragment.
 */
vec4 FogFragment(vec4 inColor) {
	return vec4(mix(inColor.rgb, gl_Fog.color.rgb, fog), inColor.a);
}
