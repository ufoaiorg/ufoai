// lighting fragment shader

varying vec3 point;
varying vec3 normal;

// NOTE about OpenGL3:
// gl_FragColor is now out_Color
// gl_LightSource doesn't exist anymore

/*
LightFragment
*/
void LightFragment(in vec4 diffuse, in vec3 lightmap){

	vec3 light = vec3(0.0);
#if defined(ATI)
	// ATI can't handle for loops and array access in fragment shaders - use at least two lights
	if(gl_LightSource[0].constantAttenuation > 0.0){
		vec3 delta = gl_LightSource[0].position.xyz - point;
		float dist = length(delta);

		// if the light is too far away, skip it
		if(dist <= gl_LightSource[0].constantAttenuation){
			vec3 dir = normalize(delta);
			float d = dot(normal, dir);

			if(d > 0.0){
				float atten = gl_LightSource[0].constantAttenuation / dist - 1.0;
				light += gl_LightSource[0].diffuse.rgb * d * atten * atten;
			}
		}
	}
	if(gl_LightSource[1].constantAttenuation > 0.0){
		vec3 delta = gl_LightSource[1].position.xyz - point;
		float dist = length(delta);

		// if the light is too far away, skip it
		if(dist <= gl_LightSource[1].constantAttenuation){
			vec3 dir = normalize(delta);
			float d = dot(normal, dir);

			if(d > 0.0){
				float atten = gl_LightSource[1].constantAttenuation / dist - 1.0;
				light += gl_LightSource[1].diffuse.rgb * d * atten * atten;
			}
		}
	}
#else
	// accumulate diffuse lighting for this fragment
	for(int i = 0; i < 8; i++){

		// if the light is off, we're done
		if(gl_LightSource[i].constantAttenuation == 0.0)
			break;

		vec3 delta = gl_LightSource[i].position.xyz - point;
		float dist = length(delta);

		// if the light is too far away, skip it
		if(dist <= gl_LightSource[i].constantAttenuation){

			vec3 dir = normalize(delta);
			float d = dot(normal, dir);

			// if the light is not facing us, skip it
			if(d > 0.0){
				float atten = gl_LightSource[i].constantAttenuation / dist - 1.0;
				light += gl_LightSource[i].diffuse.rgb * d * atten * atten;
			}
		}
	}
#endif

	light = clamp(light, 0.0, 1.8);

	// now modulate the diffuse sample with the modified lightmap
	gl_FragColor.rgb = diffuse.rgb * (lightmap + light);

	// lastly pass the alpha value through, unaffected
	gl_FragColor.a = diffuse.a;
}
