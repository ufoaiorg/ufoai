/**
 * @file light_fs.glsl
 * @brief Dynamic lighting fragment shader.
 */

vec3 LightContribution(vec4 lightParams, vec3 lightDir, vec3 normal) {
	vec3 delta = lightDir;
	vec3 dir = normalize(delta);
	float NdotL = clamp(dot(normal, dir), 0.0, 1.0);

	float dist = length(delta);
	float attenDiv = max(lightParams.a * dist * dist, 0.5);
	float attenuation = 1.0 / attenDiv;

	return lightParams.rgb * NdotL * attenuation;
}

/**
 * @brief LightFragment.
 */
vec3 LightFragment(vec3 normal) {
	vec3 light = vec3(0.0);

#unroll r_dynamic_lights
	light += LightContribution(lightParams[$], lightDirs[$], normal);
#endunroll

	return light;
}
