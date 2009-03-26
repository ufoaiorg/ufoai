// default vertex shader

#include "light_vs.glsl"
#include "bump_vs.glsl"
#include "fog_vs.glsl"

uniform int LIGHTMAP;
uniform int BUMPMAP;
uniform int FOG;

uniform float OFFSET;


/*
main
*/
void main(void){

	// mvp transform into clip space
	gl_Position = ftransform();

	// pass any active texunits through
	gl_TexCoord[0] = gl_MultiTexCoord0 + OFFSET;

	if(LIGHTMAP > 0)
		gl_TexCoord[1] = gl_MultiTexCoord1;

	LightVertex();

	if(BUMPMAP > 0)
		BumpVertex();

	if(FOG > 0)
		FogVertex();
}
