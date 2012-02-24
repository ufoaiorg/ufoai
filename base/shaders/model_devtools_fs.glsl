/**
 * @file model_devtools_fs.glsl
 * @brief Developer tools for battlescape model fragment shader.
 */

vec4 ApplyDeveloperTools(vec4 color, vec3 sunDirection, vec3 normalmap) {
	vec4 finalColor = color;
	vec3 lightVec = normalize(sunDirection);

#if r_debug_normals
	finalColor.rgb = finalColor.rgb * 0.01 + dot(sunDirection, normalmap);
	finalColor.a = 1.0;
#endif

#if r_debug_tangents
	finalColor.rgb = finalColor.rgb * 0.01 + sunDirection;
	finalColor.a = 1.0;
#endif

	return finalColor;
}
