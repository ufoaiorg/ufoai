// world vertex shader

#include "light_vs.glsl"
#include "bump_vs.glsl"
#include "fog_vs.glsl"

uniform int BUMPMAP;


/*
 * main
 */
void main(void){

	// mvp transform into clip space
	gl_Position = ftransform();

	// pass texcoords through
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_TexCoord[1] = gl_MultiTexCoord1;

	LightVertex();

#if r_bumpmap
	if(BUMPMAP > 0)
		BumpVertex();
#endif

#if r_fog
	FogVertex();
#endif
}
