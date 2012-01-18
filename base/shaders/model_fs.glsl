/**
 * @file world_fs.glsl
 * @brief Default battlescape fragment shader.
 */

#ifndef glsl110
	#if r_postprocess
		/*
		 * Indicates that gl_FragData is written to, not gl_FragColor.
		 * #extension needs to be placed before all non preprocessor code.
		 */
		#extension GL_ARB_draw_buffers : enable
		/** After glsl1110 this need to be explicitly declared; used by fixed functionality at the end of the OpenGL pipeline.*/
		out vec4 gl_FragData[2];
	#else
		/** After glsl1110 this need to be explicitly declared; used by fixed functionality at the end of the OpenGL pipeline.*/
		out vec4 gl_FragColor;
	#endif
#endif

uniform int BUMPMAP;
uniform int ROUGHMAP;
uniform int SPECULARMAP;
uniform int IS_A_MODEL;
uniform float GLOWSCALE;

/** Diffuse texture.*/
uniform sampler2D SAMPLER_DIFFUSE;
/** Specularmap.*/
uniform sampler2D SAMPLER_SPECULAR;
/** Roughnessmap.*/
uniform sampler2D SAMPLER_ROUGHMAP;
/** Normalmap.*/
uniform sampler2D SAMPLER_NORMALMAP;
/** Glowmap.*/
uniform sampler2D SAMPLER_GLOWMAP;

const vec3 two = vec3(2.0);
const vec3 negHalf = vec3(-0.5);

#define R_DYNAMIC_LIGHTS #replace r_dynamic_lights
#if r_dynamic_lights
in_qualifier vec4 lightDirs[R_DYNAMIC_LIGHTS];
in_qualifier vec4 lightParams[R_DYNAMIC_LIGHTS];
#endif

in_qualifier vec3 ambientLight;

#include "bump_fs.glsl"
#include "fog_fs.glsl"
#include "cook-torrance_fs.glsl"

/**
 * @brief main
 */
void main(void) {
	vec4 finalColor = vec4(0.0);

	/* use new dynamic lighing system, including
	 * the Cook-Torrance specularity model with the Phong
	 * model as a default if the roughness map isn't enabled
	 * ... but only for models for now
	 */
	finalColor = IlluminateFragment();

#if r_fog
	/* Add fog.*/
	finalColor = FogFragment(finalColor);
#endif

/* Developer tools */

#if r_debug_normals
	finalColor.rgb = finalColor.rgb * 0.01 + (1.0 - dot(vec3(0.0, 0.0, 1.0), normalize(lightDirs[0].xyz))) * 0.5 * vec3(1.0);
	finalColor.a = 1.0;
#endif

#if r_debug_tangents
	finalColor.rgb = finalColor.rgb * 0.01 + (1.0 - normalize(lightDirs[0].xyz)) * 0.5;
	finalColor.a = 1.0;
#endif

#if r_normalmap
	vec3 n = normalize(2.0 * (texture2D(SAMPLER_NORMALMAP, gl_TexCoord[0].st).rgb - 0.5));
	finalColor.rgb = finalColor.rgb * 0.01 + (1.0 - dot(n, normalize(lightDirs[0].xyz))) * 0.5 * vec3(1.0);
	finalColor.a = 1.0;
#endif

#if r_postprocess
	gl_FragData[0] = finalColor;
	if (GLOWSCALE > 0.01) {
		vec4 glowcolor = texture2D(SAMPLER_GLOWMAP, gl_TexCoord[0].st);
		gl_FragData[1].rgb = glowcolor.rgb * glowcolor.a * GLOWSCALE;
		gl_FragData[1].a = 1.0;
	} else {
		gl_FragData[1] = vec4(0,0,0,1);
	}
#else
	if (GLOWSCALE > 0.01) {
		vec4 glowcolor = texture2D(SAMPLER_GLOWMAP, gl_TexCoord[0].st);
		finalColor.rgb += glowcolor.rgb * glowcolor.a * GLOWSCALE;
	}
	gl_FragColor = finalColor;
#endif
}
