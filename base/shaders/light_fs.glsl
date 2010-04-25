// dynamic lighting fragment shader

varying vec3 point;
varying vec3 normal;

// constant + linear + quadratic attenuation
#define ATTENUATION (0.15 + 1.8 * dist + 3.5 * dist * dist)


/*
 * LightFragment
 */
void LightFragment(in vec4 diffuse, in vec3 lightmap){

	vec3 delta, dir, light = vec3(0.0);
	float dist, d;

#unroll r_dynamic_lights
	if(gl_LightSource[$].constantAttenuation > 0.0){

		delta = gl_LightSource[$].position.xyz - point;
		dist = length(delta);

		if(dist < gl_LightSource[$].constantAttenuation){

			dir = normalize(delta);
			d = dot(normal, dir);

			if(d > 0.0){
				dist = 1.0 - dist / gl_LightSource[$].constantAttenuation;
				light += gl_LightSource[$].diffuse.rgb * d * ATTENUATION;
			}
		}
	}

#endunroll

	light = clamp(light, 0.0, 1.8);

#if r_postprocess
	// now modulate the diffuse sample with the modified lightmap
	gl_FragData[0].rgb = diffuse.rgb * (lightmap + light);

	// lastly modulate the alpha channel by the color
	gl_FragData[0].a = diffuse.a * gl_Color.a;
#else 
	gl_FragColor.rgb = diffuse.rgb * (lightmap + light);
	gl_FragColor.a = diffuse.a * gl_Color.a;
#endif

}
