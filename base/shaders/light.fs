// lighting fragment shader

varying vec3 vertex;
varying vec3 normal;


/*
LightFragment
*/
void LightFragment(vec4 diffuse, vec4 lightmap){
	vec4 light, l;
	vec3 delta, dir;
	float dist, d;
	float atten;
	int i;

	light = vec4(0.0, 0.0, 0.0, 0.0);

	// accumulate diffuse lighting for this fragment

	// if the light is off, we're done
	if (gl_LightSource[0].constantAttenuation > 0.0) {
		delta = gl_LightSource[0].position.xyz - vertex;
		dist = length(delta);

		if (dist <= gl_LightSource[0].constantAttenuation) {
			dir = normalize(delta);
			d = dot(normal, dir);

			if (d > 0.0) {
				atten = 0.5 / (dist / gl_LightSource[0].constantAttenuation);

				l = gl_LightSource[0].diffuse * d;
				l *= atten;
				l *= atten * atten;

				light += clamp(l, 0.0, 2.5);
			}
		}
	}

	// now modulate the diffuse sample with the modified lightmap
	gl_FragColor = diffuse * (lightmap + light);
}
