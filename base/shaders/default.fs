// default fragment shader

#include "light.fs"

uniform int LIGHTMAP;

uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;

void main(void){

	// sample the diffuse texture
	vec4 diffuse = texture2D(SAMPLER0, gl_TexCoord[0].st);

	vec4 lightmap;

	// sample the lightmap
	if(LIGHTMAP > 0)
		lightmap = texture2D(SAMPLER1, gl_TexCoord[1].st);
	else  // or use primary color
		lightmap = gl_Color;

	LightFragment(diffuse, lightmap);  // add lighting
}
