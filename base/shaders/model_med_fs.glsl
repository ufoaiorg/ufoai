/**
 * @file model_med_fs.glsl
 * @brief Medium quality battlescape model fragment shader.
 */

#if r_postprocess
	/*
	 * Indicates that gl_FragData is written to, not gl_FragColor.
	 * #extension needs to be placed before all non preprocessor code.
	 */
	#extension GL_ARB_draw_buffers : enable
#endif

uniform int BUMPMAP;
uniform int SPECULARMAP;
uniform vec3 AMBIENT;
uniform vec3 SUNCOLOR;

in_qualifier vec3 sunDir; /** < Direction towards the sun */

/** Diffuse texture.*/
uniform sampler2D SAMPLER_DIFFUSE;
/** Specularmap.*/
uniform sampler2D SAMPLER_SPECULAR;
/** Normalmap.*/
uniform sampler2D SAMPLER_NORMALMAP;

#define R_DYNAMIC_LIGHTS #replace r_dynamic_lights
#if r_dynamic_lights
in_qualifier vec3 lightDirs[R_DYNAMIC_LIGHTS];
uniform vec4 LIGHTPARAMS[R_DYNAMIC_LIGHTS];
#endif

#include "light_fs.glsl"
#include "bump_fs.glsl"
#include "fog_fs.glsl"
#include "model_devtools_fs.glsl"
#include "write_fragment_fs.glsl"

/**
 * @brief main
 */
void main(void) {
	vec4 finalColor;
	vec3 specular;
	vec4 specularmap = vec4(HARDNESS, HARDNESS, HARDNESS, SPECULAR);

	vec3 sun = normalize(sunDir);
	vec3 normal = vec3(0.0, 0.0, 1.0);
	vec2 offset = vec2(0.0);

	if (BUMPMAP > 0) {
		vec4 normalmap = texture2D(SAMPLER_NORMALMAP, gl_TexCoord[0].st);
		normal = (normalmap.rgb - 0.5) * 2.0; /** < Expanded normal */
		normal.xy *= BUMP;
		normal = normalize(normal);

		/* Resolve parallax offset */
		offset = BumpTexcoord(normalmap.a);
	}

	/* Sample the diffuse texture, honoring the parallax offset.*/
	vec4 diffusemap = texture2D(SAMPLER_DIFFUSE, gl_TexCoord[0].st + offset);

	/* Sample specularity map, if any */
	if (SPECULARMAP > 0) {
		specularmap = texture2D(SAMPLER_SPECULAR, gl_TexCoord[0].st + offset);
	}

	/* Lambert illumination model */
	vec3 light = AMBIENT + SUNCOLOR * clamp(dot(sun, normal), 0.0, 1.0);

	/* Calculate specular component via Phong model */
	vec3 V = reflect(-normalize(eyedir), normal);
	float LdotV = dot(V, sun);
	specular = specularmap.rgb * pow(max(LdotV, 0.0), specularmap.a * 512.0);

	/* Calculate dynamic lights (if any) */
	light = clamp(light + LightFragment(normal), 0.0, 2.0);

	finalColor.rgb = diffusemap.rgb * light + specular;
	finalColor.a = diffusemap.a;

#if r_fog
	/* Add fog.*/
	finalColor = FogFragment(finalColor);
#endif

	/* Developer tools, if enabled */
	finalColor = ApplyDeveloperTools(finalColor, sunDir, normal);

	writeFragment(finalColor);
}
