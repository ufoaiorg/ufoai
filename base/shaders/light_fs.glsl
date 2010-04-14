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

#if r_lights
	/*
	 * Any reasonable GLSL compiler could handle this as a for-loop
	 * and unroll it nicely.  But apparently some vendors are mental
	 * midgets.  So we do their job for them.
	 */

	if(gl_LightSource[0].constantAttenuation > 0.0){

		delta = gl_LightSource[0].position.xyz - point;
		dist = length(delta);

		if(dist < gl_LightSource[0].constantAttenuation){

			dir = normalize(delta);
			d = dot(normal, dir);

			if(d > 0.0){
				dist = 1.0 - dist / gl_LightSource[0].constantAttenuation;
				light += gl_LightSource[0].diffuse.rgb * d * ATTENUATION;
			}
		}
	}
	if(gl_LightSource[1].constantAttenuation > 0.0){

		delta = gl_LightSource[1].position.xyz - point;
		dist = length(delta);

		if(dist < gl_LightSource[1].constantAttenuation){

			dir = normalize(delta);
			d = dot(normal, dir);

			if(d > 0.0){
				dist = 1.0 - dist / gl_LightSource[1].constantAttenuation;
				light += gl_LightSource[1].diffuse.rgb * d * ATTENUATION;
			}
		}
	}
	if(gl_LightSource[2].constantAttenuation > 0.0){

		delta = gl_LightSource[2].position.xyz - point;
		dist = length(delta);

		if(dist < gl_LightSource[2].constantAttenuation){

			dir = normalize(delta);
			d = dot(normal, dir);

			if(d > 0.0){
				dist = 1.0 - dist / gl_LightSource[2].constantAttenuation;
				light += gl_LightSource[2].diffuse.rgb * d * ATTENUATION;
			}
		}
	}
	if(gl_LightSource[3].constantAttenuation > 0.0){

		delta = gl_LightSource[3].position.xyz - point;
		dist = length(delta);

		if(dist < gl_LightSource[3].constantAttenuation){

			dir = normalize(delta);
			d = dot(normal, dir);

			if(d > 0.0){
				dist = 1.0 - dist / gl_LightSource[3].constantAttenuation;
				light += gl_LightSource[3].diffuse.rgb * d * ATTENUATION;
			}
		}
	}
	if(gl_LightSource[4].constantAttenuation > 0.0){

		delta = gl_LightSource[4].position.xyz - point;
		dist = length(delta);

		if(dist < gl_LightSource[4].constantAttenuation){

			dir = normalize(delta);
			d = dot(normal, dir);

			if(d > 0.0){
				dist = 1.0 - dist / gl_LightSource[4].constantAttenuation;
				light += gl_LightSource[4].diffuse.rgb * d * ATTENUATION;
			}
		}
	}
	if(gl_LightSource[5].constantAttenuation > 0.0){

		delta = gl_LightSource[5].position.xyz - point;
		dist = length(delta);

		if(dist < gl_LightSource[5].constantAttenuation){

			dir = normalize(delta);
			d = dot(normal, dir);

			if(d > 0.0){
				dist = 1.0 - dist / gl_LightSource[5].constantAttenuation;
				light += gl_LightSource[5].diffuse.rgb * d * ATTENUATION;
			}
		}
	}
	if(gl_LightSource[6].constantAttenuation > 0.0){

		delta = gl_LightSource[6].position.xyz - point;
		dist = length(delta);

		if(dist < gl_LightSource[6].constantAttenuation){

			dir = normalize(delta);
			d = dot(normal, dir);

			if(d > 0.0){
				dist = 1.0 - dist / gl_LightSource[6].constantAttenuation;
				light += gl_LightSource[6].diffuse.rgb * d * ATTENUATION;
			}
		}
	}
	if(gl_LightSource[7].constantAttenuation > 0.0){

		delta = gl_LightSource[7].position.xyz - point;
		dist = length(delta);

		if(dist < gl_LightSource[7].constantAttenuation){

			dir = normalize(delta);
			d = dot(normal, dir);

			if(d > 0.0){
				dist = 1.0 - dist / gl_LightSource[7].constantAttenuation;
				light += gl_LightSource[7].diffuse.rgb * d * ATTENUATION;
			}
		}
	}

	light = clamp(light, 0.0, 1.8);

#endif

	// now modulate the diffuse sample with the modified lightmap
	gl_FragData[0].rgb = diffuse.rgb * (lightmap + light);

	// lastly modulate the alpha channel by the color
	gl_FragData[0].a = diffuse.a * gl_Color.a;

	//gl_FragData[1].r = gl_FragData[0].r;
	//gl_FragData[1].a = diffuse.a * gl_Color.a;
}
