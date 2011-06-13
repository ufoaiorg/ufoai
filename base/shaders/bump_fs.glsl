/**
 * @file bump_fs.glsl
 * @brief Bumpmap fragment shader.
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

in_qualifier vec3 eyedir;

uniform float BUMP;
uniform float PARALLAX;
uniform float HARDNESS;
uniform float SPECULAR;

vec3 V;

/**
 * @brief BumpTexcoord.
 */
vec2 BumpTexcoord(in float height) {
	V = normalize(eyedir);

	return vec2(height * PARALLAX * 0.04 - 0.02) * V.xy;
}


/**
 * @brief BumpFragment.
 */
vec3 BumpFragment(in vec3 lightVec, in vec3 normalVec) {
	V = normalize(eyedir);
	vec3 L = vec3(normalize(lightVec).rgb);
	vec3 N = vec3(normalize(normalVec).rgb);
	N.xy *= BUMP;

	float diffuse = max(dot(N, L), 0.5);

	float specular = HARDNESS * pow(max(-dot(V, reflect(L, N)), 0.0),
									8.0 * SPECULAR);

	return vec3(diffuse + specular);
}
