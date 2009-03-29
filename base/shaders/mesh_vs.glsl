// mesh vertex shader

#include "light_vs.glsl"
#include "fog_vs.glsl"

uniform float OFFSET;


/*
main
*/
void main(void){

	// mvp transform into clip space
	gl_Position = ftransform();

	LightVertex();

	// pass texcoords through
	gl_TexCoord[0] = gl_MultiTexCoord0 + OFFSET;

#if r_fog
	FogVertex();
#endif
}
