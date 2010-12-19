/**
 * @file fog_fs.glsl
 * @brief Fog fragment shader.
 */

#ifndef glsl110
	/** Linkage into a shader from a previous stage, variable is copied in.*/
        #define in_qualifier in
#else
        /** Deprecated after glsl110; linkage between a vertex shader and a fragment shader for interpolated data.*/
        #define in_qualifier varying
#endif

in_qualifier float fog;

/**
 * @brief FogFragment.
 */
vec4 FogFragment(vec4 inColor){

	return vec4(mix(inColor.rgb, gl_Fog.color.rgb, fog), inColor.a);
}
