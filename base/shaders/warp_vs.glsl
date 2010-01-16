// warp vertex shader

#include "fog_vs.glsl"


/*
 * main
 */
void main(void){

	// mvp transform into clip space
	gl_Position = ftransform();

	// pass texcoords through
	gl_TexCoord[0] = gl_MultiTexCoord0;

	// and primary color
	gl_FrontColor = gl_Color;

#if r_fog
	FogVertex();
#endif
}
