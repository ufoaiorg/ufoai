#version 110
// world vertex shader

uniform float OFFSET;
uniform int BUMPMAP;
uniform int ANIMATE;

/* from includes:
varying vec3 point;
varying vec3 normal;

uniform mat4 SHADOW_TRANSFORM;

attribute vec4 TANGENT;

attribute vec4 NEXT_FRAME_VERTS;
attribute vec4 NEXT_FRAME_NORMALS;
attribute vec4 NEXT_FRAME_TANGENTS;
uniform float TIME;
varying vec4 Tangent;

varying vec3 eyedir;
varying vec3 lightDirs[];
varying vec3 staticLightDir;

varying float fog; 
*/


attribute vec3 NEXT_FRAME_VERTS;
attribute vec3 NEXT_FRAME_NORMALS;
attribute vec4 TANGENTS;
attribute vec4 NEXT_FRAME_TANGENTS;
uniform float TIME;

varying vec4 vertPos;
varying vec3 Normal;
varying vec4 Vertex;
varying vec4 Tangent;

#include "light_vs.glsl"
#include "bump_vs.glsl"
#include "fog_vs.glsl"
#include "shadowmap_vs.glsl"

/**
 * main
 */
void main(void){

/*
	if (ANIMATE > 0) {
		lerpVertex();
	} else {
		Vertex = gl_Vertex;
		Normal = gl_Normal;
		Tangent = TANGENTS;
	}
	*/


	float lerp = (1.0 - TIME) * float(ANIMATE);
	Vertex = mix(gl_Vertex, vec4(NEXT_FRAME_VERTS, 1.0), lerp);
	Normal = mix(gl_Normal, NEXT_FRAME_NORMALS, lerp);
	Tangent = mix(TANGENTS, NEXT_FRAME_TANGENTS, lerp);


	// mvp transform into clip space
	//gl_Position = ftransform();
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * Vertex;
	vertPos = gl_Position;


	// pass texcoords through
	gl_TexCoord[0] = gl_MultiTexCoord0 + OFFSET;
	gl_TexCoord[1] = gl_MultiTexCoord1 + OFFSET;

	LightVertex();

#if r_bumpmap
	BumpVertex();
#endif

#if r_shadowmapping
	if(SHADOWMAP > 0)
		ShadowVertex();
#endif

#if r_fog
	FogVertex();
#endif
}
