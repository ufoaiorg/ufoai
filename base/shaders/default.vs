// default vertex shader

#include "light.vs"

uniform int LIGHTMAP;

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
}
