//#version 130
/* battlescape vertex shader */

uniform vec4 LightLocation;

uniform float FogDensity;
uniform vec4 FogColor;

uniform float OFFSET;
uniform int BUMPMAP;
uniform int ANIMATE;
uniform float TIME;
uniform mat4 SHADOW_TRANSFORM;

in vec3 NEXT_FRAME_VERTS;
in vec3 NEXT_FRAME_NORMALS;
in vec4 TANGENTS;
in vec4 NEXT_FRAME_TANGENTS;

#ifndef ATI
in vec4 gl_Vertex;
in vec3 gl_Normal;
uniform mat4 gl_ModelViewMatrix;
uniform mat4 gl_ProjectionMatrix;
uniform mat3 gl_NormalMatrix;

in vec4 gl_MultiTexCoord0;
in vec4 gl_MultiTexCoord1;
#endif

uniform mat4 SHADOW_MATRIX;

out vec4 Vertex;
out vec3 Normal;
out vec4 Tangent;
out vec4 vPosScreen;
out vec4 vPos;
out vec3 vNormal;
out vec3 eyedir;
out vec3 lightDir;
out vec4 shadowCoord;
out float fog;
out vec4 texCoord;
out vec4 gl_Position;

/* @note - these values come from #defines in r_state.c */
const float fogStart =	300.0;
const float fogEnd 	 =	2500.0;
const float fogRange = fogEnd - fogStart;


/**
 * main
 */
void main (void) {
	if (ANIMATE > 0) {
		float lerp = 1.0 - TIME;
		Vertex = mix(gl_Vertex, vec4(NEXT_FRAME_VERTS, 1.0), lerp);
		Normal = mix(gl_Normal, NEXT_FRAME_NORMALS, lerp);
		Tangent = mix(TANGENTS, NEXT_FRAME_TANGENTS, lerp);
	} else {
		Vertex = gl_Vertex;
		Normal = gl_Normal;
		Tangent = TANGENTS;
	}

	/* transform vertex info to screen and camera coordinates */
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * Vertex;
	vPosScreen = gl_Position;
	vPos = gl_ModelViewMatrix * Vertex;
	vNormal = normalize(gl_NormalMatrix * Normal);


	/* pass texcoords and color through */
	texCoord = gl_MultiTexCoord0 + OFFSET;

	/* calculate vertex projection onto the shadowmap */ 
	shadowCoord = SHADOW_MATRIX * gl_ModelViewMatrix * Vertex;

#if r_fog
	/* calculate interpolated fog depth */
	fog = clamp((vPosScreen.z - fogStart) / fogRange, 0.0, 1.0) * FogDensity;
#endif

	/* setup tangent space */
	vec3 tangent = normalize(gl_NormalMatrix * Tangent.xyz);
	vec3 bitangent = normalize(cross(vNormal, tangent)) * Tangent.w;

	/* transform the eye direction into tangent space */
	vec3 v;
	v.x = dot(-vPos.xyz, tangent);
	v.y = dot(-vPos.xyz, bitangent);
	v.z = dot(-vPos.xyz, vNormal);
	eyedir = normalize(v);

	/* transform relative light positions into tangent space */
	vec3 lpos; 
	if(LightLocation.w > 0.0001) {
		lpos = normalize(gl_NormalMatrix * LightLocation.xyz - vPos.xyz);
	} else { // directional light source at "infinite" distance 
		lpos = normalize(gl_NormalMatrix * LightLocation.xyz);
	}

	lightDir.x = dot(lpos, tangent);
	lightDir.y = dot(lpos, bitangent);
	lightDir.z = dot(lpos, vNormal);

}
