/**
 * @file
 * @brief Bumpmap fragment shader.
 */

in_qualifier vec3 eyedir;

uniform float BUMP;
uniform float PARALLAX;
uniform float HARDNESS;
uniform float SPECULAR;

vec3 eye;

/**
 * @brief BumpTexcoord.
 */
vec2 BumpTexcoord(in float height) {
	eye = normalize(eyedir);

	return vec2(height * PARALLAX * 0.04 - 0.02) * eye.xy;
}
