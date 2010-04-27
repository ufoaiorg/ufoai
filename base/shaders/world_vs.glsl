// world vertex shader

uniform float OFFSET;
uniform int BUMPMAP;
uniform vec3 LIGHTPOS;

varying vec3 lightpos;

/* from includes:
varying vec3 point;
varying vec3 normal;

attribute vec4 TANGENT;
uniform int DYNAMICLIGHTS;

varying vec3 eyedir;
varying vec3 lightDirs[8];
varying vec3 staticLightDir;
varying vec3 tangent;

varying float fog; 
*/


#include "light_vs.glsl"
#include "bump_vs.glsl"
#include "fog_vs.glsl"

/**
 * main
 */
void main(void){

	// mvp transform into clip space
	gl_Position = ftransform();

	// pass texcoords through
	gl_TexCoord[0] = gl_MultiTexCoord0 + OFFSET;
	gl_TexCoord[1] = gl_MultiTexCoord1 + OFFSET;

	// transform the static lighting direction into model space
	lightpos = vec3(gl_ModelViewMatrix * vec4(LIGHTPOS, 1.0));

	LightVertex();

#if r_bumpmap
	if(BUMPMAP > 0)
		BumpVertex();
#endif

#if r_fog
	FogVertex();
#endif
}
