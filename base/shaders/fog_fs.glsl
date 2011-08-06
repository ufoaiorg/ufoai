/**
 * @file fog_fs.glsl
 * @brief Fog fragment shader.
 */

in_qualifier float fog;
uniform vec3 FOGCOLOR;

/**
 * @brief FogFragment.
 */
vec4 FogFragment(vec4 inColor) {
	return vec4(mix(inColor.rgb, FOGCOLOR, fog), inColor.a);
}
