// default vertex shader

#include "light.vs"
#include "bump.vs"

uniform int LIGHTMAP;
uniform int BUMPMAP;


/*
main
*/
void main(void){

	// mvp transform into clip space
	gl_Position = ftransform();

	// pass any active texunits through
	gl_TexCoord[0] = gl_MultiTexCoord0;

	if(LIGHTMAP > 0)
		gl_TexCoord[1] = gl_MultiTexCoord1;
	else
		gl_FrontColor = gl_Color;

	LightVertex();

	if(BUMPMAP > 0)
		BumpVertex();
}
