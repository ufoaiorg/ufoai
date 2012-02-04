/**
 * @file light_fs.glsl
 * @brief Dynamic lighting fragment shader.
 */

in_qualifier vec3 point;
in_qualifier vec3 normal;

vec3 LightContribution(vec4 lightParams, vec4 lightDir) {
	vec3 delta = lightDir.xyz;
	vec3 dir = normalize(delta);
	float NdotL = max(0.0, dot(normal, dir));
	float attenuation = 1.0;

	if (bool(lightDir.w) && NdotL > 0.0) {
		float dist = length(delta);
		float attenDiv = lightParams.a * dist * dist;
		/* If none of the attenuation parameters are set, we keep 1.0.*/
		if (bool(attenDiv)) {
			attenuation = 1.0 / attenDiv;
		}
	}

	return lightParams.rgb * NdotL * attenuation;
}

/**
 * @brief LightFragment.
 */
vec3 LightFragment() {
	vec3 light = vec3(0.0);

#unroll r_dynamic_lights
	light += LightContribution(lightParams[$], lightDirs[$]);
#endunroll

	return light;
}
