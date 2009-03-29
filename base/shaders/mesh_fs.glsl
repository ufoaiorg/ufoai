// mesh fragment shader

#include "light_fs.glsl"
#include "fog_fs.glsl"

uniform vec3 LIGHTPOS;

uniform sampler2D SAMPLER0;


/*
main
*/
void main(void){

	// sample the diffuse texture, honoring the parallax offset
	vec4 diffuse = texture2D(SAMPLER0, gl_TexCoord[0].st);

	// add any dynamic lighting and yield a base fragment color
	LightFragment(diffuse, gl_Color.rgb);

#if r_fog
	FogFragment();  // add fog
#endif
}
