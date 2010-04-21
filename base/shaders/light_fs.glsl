// dynamic lighting fragment shader

varying vec3 point;
varying vec3 normal;
// amount of lights add to the scene
uniform int LIGHTS;

// constant + linear + quadratic attenuation
#define ATTENUATION (0.15 + 1.8 * dist + 3.5 * dist * dist)

#if r_lights
void LightContribution(gl_LightSourceParameters param, out vec3 light){

	float attenuation = param.constantAttenuation;
	if(attenuation > 0.0){
		vec3 lightPos = param.position.xyz;
		vec3 lightColor = param.diffuse.rgb;
		vec3 delta = lightPos - point;
		float dist = length(delta);
		if(dist < attenuation){
			vec3 dir = normalize(delta);
			float d = dot(normal, dir);
			if(d > 0.0)
				light += lightColor * d * ATTENUATION;
		}
	}
}
#endif


/*
 * LightFragment
 */
void LightFragment(in vec4 diffuse, in vec3 lightmap){

	vec3 light = vec3(0.0);

#if r_lights

	for (int i = 0; i < LIGHTS; i++) {
		gl_LightSourceParameters param = gl_LightSource[i];
		LightContribution(param, light);
	}

	light = clamp(light, 0.0, 1.8);

#endif

	// now modulate the diffuse sample with the modified lightmap
	gl_FragData[0].rgb = diffuse.rgb * (lightmap + light);

	// lastly modulate the alpha channel by the color
	gl_FragData[0].a = diffuse.a * gl_Color.a;

}
