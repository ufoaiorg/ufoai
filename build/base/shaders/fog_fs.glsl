/**
 * @file
 * @brief Fog fragment shader.
 */

#if r_fog
in_qualifier float fog;
uniform vec3 FOGCOLOR;

/**
 * @brief FogFragment.
 */
vec4 FogFragment(vec4 inColor) {
	return vec4(mix(inColor.rgb, FOGCOLOR, fog), inColor.a);
}
#endif
