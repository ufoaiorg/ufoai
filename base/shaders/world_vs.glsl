//#version 130
/* battlescape vertex shader */

uniform vec4 LightLocation[#replace r_dynamic_lights ];

uniform float FogDensity;
uniform vec4 FogColor;

uniform float OFFSET;
uniform int BUMPMAP;
uniform int ANIMATE;
uniform float TIME;
uniform mat4 SHADOW_TRANSFORM;
uniform int NUM_ACTIVE_LIGHTS;
uniform int BUILD_SHADOWMAP;

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
out vec3 lightDirs[#replace r_dynamic_lights ];
out vec4 shadowCoord;
out float fog;
/* @todo replace static "8" with something appropriate from a Cvar */
out vec4 gl_TexCoord[8];

/* @note - these values come from #defines in r_state.c */
const float fogStart =	300.0;
const float fogEnd 	 =	2500.0;
const float fogRange = fogEnd - fogStart;


/**
 * main
 */
void main (void) {
	float lerp = (1.0 - TIME) * float(ANIMATE);
	Vertex = mix(gl_Vertex, vec4(NEXT_FRAME_VERTS, 1.0), lerp);
	Normal = mix(gl_Normal, NEXT_FRAME_NORMALS, lerp);
	Tangent = mix(TANGENTS, NEXT_FRAME_TANGENTS, lerp);

	/* transform vertex info to screen and camera coordinates */
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * Vertex;
	vPosScreen = gl_Position;
	vPos = gl_ModelViewMatrix * Vertex;
	vNormal = normalize(gl_NormalMatrix * Normal);


	/* pass texcoords and color through */
	gl_TexCoord[0] = gl_MultiTexCoord0 + OFFSET;
	gl_TexCoord[1] = gl_MultiTexCoord1 + OFFSET;

	/* if we're just building shadowmaps, we don't need to set up stuff for lighting */
	if (BUILD_SHADOWMAP == 0) {
#if r_shadowmapping
		/* calculate vertex projection onto the shadowmap */ 
		shadowCoord = SHADOW_MATRIX * gl_ModelViewMatrix * Vertex;
#endif

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
#unroll r_dynamic_lights
		if ($ < NUM_ACTIVE_LIGHTS) {
			vec3 lpos; 
			if(LightLocation[$].w > 0.0001) {
				lpos = normalize(gl_NormalMatrix * LightLocation[$].xyz - vPos.xyz);
			} else { /* directional light source at "infinite" distance */
				lpos = normalize(gl_NormalMatrix * LightLocation[$].xyz);
			}

			lightDirs[$].x = dot(lpos, tangent);
			lightDirs[$].y = dot(lpos, bitangent);
			lightDirs[$].z = dot(lpos, vNormal);
		}
#endunroll
	}

}
