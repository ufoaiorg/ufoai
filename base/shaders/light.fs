// lighting fragment shader

varying vec3 vertex;
varying vec3 normal;


/*
LightFragment
*/
void LightFragment(vec4 diffuse, vec4 lightmap){
	vec3 light, l;
	vec3 delta, dir;
	float dist, d;
	float atten;
	int i;

	light = vec3(0.0, 0.0, 0.0);

#if defined(ATI)
	// ATI can't handle for loops and array access in fragment shaders - use at least two lights
	if (gl_LightSource[0].constantAttenuation > 0.0) {
		delta = gl_LightSource[0].position.xyz - vertex;
		dist = length(delta);

		if (dist <= gl_LightSource[0].constantAttenuation) {
			dir = normalize(delta);
			d = dot(normal, dir);

			if (d > 0.0) {
				atten = 0.5 / (dist / gl_LightSource[0].constantAttenuation);

				l = gl_LightSource[0].diffuse.rgb * d;
				l *= atten;
				l *= atten * atten;

				light += clamp(l, 0.0, 2.5);
			}
		}
	}
	if (gl_LightSource[1].constantAttenuation > 0.0) {
		delta = gl_LightSource[1].position.xyz - vertex;
		dist = length(delta);

		if (dist <= gl_LightSource[1].constantAttenuation) {
			dir = normalize(delta);
			d = dot(normal, dir);

			if (d > 0.0) {
				atten = 0.5 / (dist / gl_LightSource[1].constantAttenuation);

				l = gl_LightSource[1].diffuse.rgb * d;
				l *= atten;
				l *= atten * atten;

				light += clamp(l, 0.0, 2.5);
			}
		}
	}
#else
	// accumulate diffuse lighting for this fragment
	for(i = 0; i < 8; i++){

		// if the light is off, we're done
		if(gl_LightSource[i].constantAttenuation == 0.0)
			break;

		delta = gl_LightSource[i].position.xyz - vertex;
		dist = length(delta);

		if(dist > gl_LightSource[i].constantAttenuation)
			continue;

		dir = normalize(delta);
		d = dot(normal, dir);

		if(d <= 0.0)
			continue;

		atten = 0.5 / (dist / gl_LightSource[i].constantAttenuation);

		l = gl_LightSource[i].diffuse.rgb * d;
		l *= atten;
		l *= atten * atten;

		light += clamp(l, 0.0, 2.5);
	}
#endif

	// now modulate the diffuse sample with the modified lightmap
	gl_FragColor.rgb = diffuse.rgb * (lightmap.rgb + light);

	// lastly pass the alpha value through, unaffected
	gl_FragColor.a = diffuse.a * lightmap.a;
}
