// dynamic lighting fragment shader

varying vec3 point;
varying vec3 normal;

// constant + linear + quadratic attenuation
#define ATTENUATION (0.15 + 1.8 * dist + 3.5 * dist * dist)


vec3 LightContribution(in gl_LightSourceParameters lightSource, in vec4 diffuse, in vec3 lightmap){

	vec3 light = vec3(0.0);

	if(lightSource.constantAttenuation > 0.0){
		vec3 delta = lightSource.position.xyz - point;
		float dist = length(delta);
		vec3 dir = normalize(delta);
		float d = max(0.0, dot(normal, dir));
		if (lightSource.position.w == 0.0){
			light += lightSource.diffuse.rgb * d;
			light += lightSource.ambient.rgb;
		} else if(dist < lightSource.constantAttenuation){
			if(d > 0.0){
				dist = 1.0 - dist / lightSource.constantAttenuation;
				light += lightSource.diffuse.rgb * d * ATTENUATION;
				light += lightSource.ambient.rgb;
			}
		}
	}
	
	return light;
}

/*
 * LightFragment
 */
vec4 LightFragment(in vec4 diffuse, in vec3 lightmap){

	vec3 light = vec3(0.0);

#unroll r_dynamic_lights
	light += LightContribution(gl_LightSource[$], diffuse, lightmap);
#endunroll

	light = clamp(light, 0.0, 1.8);

	// now modulate the diffuse sample with the modified lightmap
	vec4 lightColor;
	lightColor.rgb = diffuse.rgb * (lightmap + light);
	// lastly modulate the alpha channel by the color
	lightColor.a = diffuse.a * gl_Color.a;

	return lightColor;
}
