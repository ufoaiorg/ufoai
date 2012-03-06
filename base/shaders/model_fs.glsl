/**
 * @file world_fs.glsl
 * @brief Default battlescape model fragment shader.
 */

#if r_postprocess
	/*
	 * Indicates that gl_FragData is written to, not gl_FragColor.
	 * #extension needs to be placed before all non preprocessor code.
	 */
	#extension GL_ARB_draw_buffers : enable
#endif

uniform int BUMPMAP;
uniform int ROUGHMAP;
uniform int SPECULARMAP;
uniform vec3 AMBIENT;

uniform vec3 SUNCOLOR;

in_qualifier vec3 sunDir; /** < Direction towards the sun */

/** Diffuse texture.*/
uniform sampler2D SAMPLER_DIFFUSE;
/** Specularmap.*/
uniform sampler2D SAMPLER_SPECULAR;
/** Roughnessmap.*/
uniform sampler2D SAMPLER_ROUGHMAP;
/** Normalmap.*/
uniform sampler2D SAMPLER_NORMALMAP;

const vec3 two = vec3(2.0);
const vec3 negHalf = vec3(-0.5);

#define R_DYNAMIC_LIGHTS #replace r_dynamic_lights
#if r_dynamic_lights
in_qualifier vec3 lightDirs[R_DYNAMIC_LIGHTS];
in_qualifier vec4 lightParams[R_DYNAMIC_LIGHTS];
#endif

#include "bump_fs.glsl"
#include "fog_fs.glsl"
#include "cook-torrance_fs.glsl"
#include "model_devtools_fs.glsl"
#include "write_fragment_fs.glsl"

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

	/* Developer tools, if enabled */
	finalColor = ApplyDeveloperTools(finalColor, sunDir, texture2D(SAMPLER_NORMALMAP, gl_TexCoord[0].st).rgb);

	writeFragment(finalColor);
}
