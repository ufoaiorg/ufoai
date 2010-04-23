#version 110
// mesh vertex shader

#include "light_vs.glsl"
#include "fog_vs.glsl"

uniform float OFFSET;

uniform vec3 LIGHTPOS;

varying vec3 lightpos;

/*
 * main
 */
void main(void){

	// mvp transform into clip space
	gl_Position = ftransform();

	// transform the static lighting direction into model space
	lightpos = vec3(gl_ModelViewMatrix * vec4(LIGHTPOS, 1.0));

	LightVertex();

	// pass texcoords through
	gl_TexCoord[0] = gl_MultiTexCoord0 + OFFSET;

#if r_fog
	FogVertex();
#endif
}
