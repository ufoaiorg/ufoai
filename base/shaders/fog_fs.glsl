/**
 * @file fog_fs.glsl
 * @brief Fog fragment shader.
 */

in_qualifier float fog;

/**
 * @brief FogFragment.
 */
vec4 FogFragment(vec4 inColor) {
	return vec4(mix(inColor.rgb, gl_Fog.color.rgb, fog), inColor.a);
}
