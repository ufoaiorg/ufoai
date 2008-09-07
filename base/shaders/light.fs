// lighting fragment shader

varying vec3 point;
varying vec3 normal;


/*
LightFragment
*/
void LightFragment(in vec4 diffuse, in vec3 lightmap){
	
	vec3 light = vec3(0.0, 0.0, 0.0);
	
	// accumulate diffuse lighting for this fragment
	for(int i = 0; i < 8; i++){
		
		// if the light is off, we're done
		if(gl_LightSource[i].constantAttenuation == 0.0)
			break;
		
		vec3 delta = gl_LightSource[i].position.xyz - point;
		float dist = length(delta);
		
		// if the light is too far away, skip it
		if(dist > gl_LightSource[i].constantAttenuation)
			continue;
		
		vec3 dir = normalize(delta);
		float d = dot(normal, dir);
		
		if(d <= 0.0)
			continue;
		
		float atten = gl_LightSource[i].constantAttenuation / dist - 1.0;
		light += gl_LightSource[i].diffuse.rgb * d * atten * atten;
	}
	
	// now modulate the diffuse sample with the modified lightmap
	gl_FragColor.rgb = diffuse.rgb * (lightmap + light);
	
	// lastly pass the alpha value through, unaffected
	gl_FragColor.a = diffuse.a;
}
