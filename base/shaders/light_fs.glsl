// dynamic lighting fragment shader

varying vec3 point;
varying vec3 normal;

// constant + linear + quadratic attenuation
#define ATTENUATION (0.15 + 1.8 * dist + 3.5 * dist * dist)


vec3 LightContribution(int i, vec4 diffuse, vec3 lightmap)
{

	vec3 delta, dir, light = vec3(0.0);
	float dist, d;

	if(gl_LightSource[i].constantAttenuation > 0.0){
		delta = gl_LightSource[i].position.xyz - point;
		dist = length(delta);
		dir = normalize(delta);
		d = max(0.0, dot(normal, dir));
		if (gl_LightSource[i].position.w == 0.0){
			light += gl_LightSource[i].diffuse.rgb * d;
			light += gl_LightSource[i].ambient.rgb;
		} else if(dist < gl_LightSource[i].constantAttenuation){
			if(d > 0.0){
				dist = 1.0 - dist / gl_LightSource[i].constantAttenuation;
				light += gl_LightSource[i].diffuse.rgb * d * ATTENUATION;
				light += gl_LightSource[i].ambient.rgb;
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
	light += LightContribution($, diffuse, lightmap);
#endunroll

	light = clamp(light, 0.0, 1.8);

	// now modulate the diffuse sample with the modified lightmap
	vec4 LightColor;
	LightColor.rgb = diffuse.rgb * (lightmap + light);
	// lastly modulate the alpha channel by the color
	LightColor.a = diffuse.a * gl_Color.a;

	return LightColor;
}
