/**
 * @file
 * @brief Developer tools for battlescape world fragment shader.
 */

vec4 ApplyDeveloperTools(vec4 color, vec3 normalmap, vec3 lightmap, vec3 deluxemap) {
	vec4 finalColor = color;

#if r_debug_normals
	if (BUMPMAP > 0) {
		finalColor.rgb = finalColor.rgb * 0.01 + dot(normalmap, deluxemap);
	} else {
		finalColor.rgb = vec3(0.0, 0.0, 1.0);
	}
	finalColor.a = 1.0;
#endif

#if r_lightmap
	finalColor.rgb = finalColor.rgb * 0.01 + lightmap;
	finalColor.a = 1.0;
#endif

#if r_deluxemap
	finalColor.rgb = finalColor.rgb * 0.01 + (deluxemap + 1.0) * 0.5;
	finalColor.a = 1.0;
#endif

	return finalColor;
}
