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
uniform float GLOWSCALE;

/** Diffuse texture.*/
uniform sampler2D SAMPLER0;
/** Lightmap.*/
uniform sampler2D SAMPLER1;
/** Deluxemap.*/
uniform sampler2D SAMPLER2;
/** Normalmap.*/
uniform sampler2D SAMPLER3;
/** Glowmap.*/
uniform sampler2D SAMPLER4;

const vec3 two = vec3(2.0);
const vec3 negHalf = vec3(-0.5);

#define R_DYNAMIC_LIGHTS #replace r_dynamic_lights
#if r_dynamic_lights
in_qualifier vec4 lightDirs[R_DYNAMIC_LIGHTS];
in_qualifier vec4 lightParams[R_DYNAMIC_LIGHTS];
#endif

in_qualifier vec3 ambientLight;

#include "light_fs.glsl"
#include "bump_fs.glsl"
#include "fog_fs.glsl"

/**
 * @brief main
 */
void main(void) {
	vec4 finalColor = vec4(0.0);
	vec3 lightmap = vec3(0.0);
	/* These two should be declared in this scope for developer tools to work */
	vec3 deluxemap = vec3(0.0);
	vec4 normalmap = vec4(0.0);

	/* use Phong lighting (ie. legacy rendering code) */
	vec2 offset = vec2(0.0);
	vec3 bump = vec3(1.0);

	/* lightmap contains pre-computed incoming light color */
	lightmap = texture2D(SAMPLER1, gl_TexCoord[1].st).rgb;

#if r_bumpmap
	if (BUMPMAP > 0) {
		/* Sample deluxemap and normalmap.*/
		normalmap = texture2D(SAMPLER3, gl_TexCoord[0].st);
		normalmap.rgb = normalize(two * (normalmap.rgb + negHalf));

		/* deluxemap contains pre-computed incoming light vectors in object tangent space */
		deluxemap = texture2D(SAMPLER2, gl_TexCoord[1].st).rgb;
		deluxemap = normalize(two * (deluxemap + negHalf));

		/* Resolve parallax offset and bump mapping.*/
		offset = BumpTexcoord(normalmap.a);
		bump = BumpFragment(deluxemap, normalmap.rgb);
	}
#endif

	/* Sample the diffuse texture, honoring the parallax offset.*/
	vec4 diffuse = texture2D(SAMPLER0, gl_TexCoord[0].st + offset);

	/* Factor in bump mapping.*/
	diffuse.rgb *= bump;

	/* Otherwise, add to lightmap.*/
	finalColor = LightFragment(diffuse, lightmap);

#if r_fog
	/* Add fog.*/
	finalColor = FogFragment(finalColor);
#endif

/* Developer tools */

#if r_normalmap
	vec3 n = normalize(2.0 * (texture2D(SAMPLER3, gl_TexCoord[0].st).rgb - 0.5));
	finalColor.rgb = finalColor.rgb * 0.01 + (1.0 - dot(n, normalize(lightDirs[0].xyz))) * 0.5 * vec3(1.0);
	finalColor.a = 1.0;
#endif

#if r_lightmap
	finalColor.rgb = finalColor.rgb * 0.01 + lightmap;
	finalColor.a = 1.0;
#endif

#if r_deluxemap
	if (BUMPMAP > 0) {
		finalColor.rgb = finalColor.rgb * 0.01 + deluxemap;
		finalColor.a = 1.0;
	}
#endif

#if r_postprocess
	gl_FragData[0] = finalColor;
	if (GLOWSCALE > 0.01) {
		vec4 glowcolor = texture2D(SAMPLER4, gl_TexCoord[0].st);
		gl_FragData[1].rgb = glowcolor.rgb * glowcolor.a * GLOWSCALE;
		gl_FragData[1].a = 1.0;
	} else {
		gl_FragData[1] = vec4(0,0,0,0);
	}
#else
	gl_FragColor = finalColor;
#endif
}
