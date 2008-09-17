// default fragment shader

#include "light.fs"
#include "bump.fs"
uniform int LIGHTMAP;
uniform int BUMPMAP;

uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;
uniform sampler2D SAMPLER2;
uniform sampler2D SAMPLER3;

const vec3 two = vec3(2.0);
const vec3 negOne = vec3(-1.0);
const vec3 negHalf = vec3(-0.5);

/*
main
*/
void main(void){

	vec4 diffuse = texture2D(SAMPLER0, gl_TexCoord[0].st);

	vec3 lightmap;

	if(LIGHTMAP > 0)
		lightmap = texture2D(SAMPLER1, gl_TexCoord[1].st).rgb;
	else  // use primary color
		lightmap = gl_Color.rgb;

	LightFragment(diffuse, lightmap);

	if(BUMPMAP > 0){
		vec3 deluxemap = texture2D(SAMPLER2, gl_TexCoord[1].st).rgb * two + negOne;
		vec3 normalmap = normalize(texture2D(SAMPLER3, gl_TexCoord[0].st).rgb + negHalf);

		BumpFragment(deluxemap, normalmap);
	}
}
