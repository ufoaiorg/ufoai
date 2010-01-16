// mesh fragment shader

#include "light_fs.glsl"
#include "fog_fs.glsl"

uniform vec3 LIGHTPOS;

uniform sampler2D SAMPLER0;

varying vec3 lightpos;


/*
 * main
 */
void main(void){

	// sample the diffuse texture, honoring the parallax offset
	vec4 diffuse = texture2D(SAMPLER0, gl_TexCoord[0].st);

	// use the static lighting direction to resolve a shade
	vec3 lightdir = normalize(lightpos - point);
	float shade = max(0.5, pow(2.0 * dot(normal, lightdir), 2.0));

	// add any dynamic lighting and yield a base fragment color
	LightFragment(diffuse, gl_Color.rgb * shade);

#if r_fog
	FogFragment();  // add fog
#endif
}
