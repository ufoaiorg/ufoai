/**
 * @file light_fs.glsl
 * @brief Dynamic lighting fragment shader.
 */

in_qualifier vec3 point;
in_qualifier vec3 normal;

vec3 LightContribution(in gl_LightSourceParameters lightSource, in vec4 diffuse, in vec3 lightmap) {
	vec3 delta = lightSource.position.xyz - point;
	vec3 dir = normalize(delta);
	float NdotV = max(0.0, dot(normal, dir));
	float attenuation = 1.0;

	if (bool(lightSource.position.w) && NdotV > 0.0) {
		float dist = length(delta);
		float attenDiv = (lightSource.constantAttenuation +
			lightSource.linearAttenuation * dist +
			lightSource.quadraticAttenuation * dist * dist);
		/* If none of the attenuation parameters are set, we keep 1.0.*/
		if (bool(attenDiv)) {
			attenuation = 1.0 / attenDiv;
		}
	}

	return lightSource.diffuse.rgb * NdotV * attenuation + lightSource.ambient.rgb;
}

/**
 * @brief LightFragment.
 */
vec4 LightFragment(in vec4 diffuse, in vec3 lightmap) {
	vec3 light = vec3(0.0);

#unroll r_dynamic_lights
	light += LightContribution(gl_LightSource[$], diffuse, lightmap);
#endunroll

	light = clamp(light, 0.0, 1.8);

	/* Now modulate the diffuse sample with the modified lightmap.*/
	vec4 lightColor;
	lightColor.rgb = diffuse.rgb * (lightmap + light);
	/* Lastly modulate the alpha channel by the color.*/
	lightColor.a = diffuse.a * gl_Color.a;

	return lightColor;
}
