/**
 * @file
 * @brief Medium quality battlescape fragment shader.
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
/*uniform float SPECULAR; specular exponent divided by 512  */
/*uniform float HARDNESS; specular component brightness (0 - no specular) */
uniform vec3 AMBIENT;

/** Diffuse texture.*/
uniform sampler2D SAMPLER_DIFFUSE;
/** Specularmap.*/
uniform sampler2D SAMPLER_SPECULAR;
/** Lightmap.*/
uniform sampler2D SAMPLER_LIGHTMAP;
/** Deluxemap.*/
uniform sampler2D SAMPLER_DELUXEMAP;
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
#include "world_devtools_fs.glsl"
#include "write_fragment_fs.glsl"

in_qualifier vec4 blendColor;

/**
 * @brief main
 */
void main(void) {
	vec4 finalColor = vec4(0.0);
	vec3 light = vec3(0.0);
	vec3 specular;
	vec3 deluxemap;
	/* normalmap should be declared in this scope for developer tools to work */
	vec4 normalmap = vec4(0.0, 0.0, 1.0, 0.5);
	vec4 specularmap = vec4(HARDNESS, HARDNESS, HARDNESS, SPECULAR);

	vec2 offset = vec2(0.0);

	/* lightmap contains pre-computed incoming light color */
	light = texture2D(SAMPLER_LIGHTMAP, gl_TexCoord[1].st).rgb;

	/* deluxemap contains pre-computed incoming light vectors in object tangent space */
	deluxemap = texture2D(SAMPLER_DELUXEMAP, gl_TexCoord[1].st).rgb;
	deluxemap = normalize(deluxemap * 2.0 - 1.0);

#if r_bumpmap
	if (BUMPMAP > 0) {
		/* Sample normalmap.*/
		normalmap = texture2D(SAMPLER_NORMALMAP, gl_TexCoord[0].st);
		normalmap.rgb = normalize(normalmap.rgb * 2.0 - 1.0);

		/* Resolve parallax offset */
		offset = BumpTexcoord(normalmap.a);
	}
#endif

	/* Modulate incoming light by cos(angle_of_incidence); should be done even bump mapping is disabled, to avoid having flat shading as a result.*/
	light *= clamp(dot(deluxemap, vec3(normalmap.x * BUMP, normalmap.y * BUMP, normalmap.z)), 0.0, 1.0);

	/* Sample the diffuse texture, honoring the parallax offset.*/
	vec4 diffusemap = texture2D(SAMPLER_DIFFUSE, gl_TexCoord[0].st + offset);

	/* Sample specularity map, if any */
	if (SPECULARMAP > 0) {
		specularmap = texture2D(SAMPLER_SPECULAR, gl_TexCoord[0].st + offset);
	}

	/* Calculate specular component via Phong model */
	vec3 V = reflect(-normalize(eyedir), normalmap.rgb);
	float LdotV = dot(V, deluxemap);
	specular = clamp(light - AMBIENT, 0.0, 1.0) * specularmap.rgb * pow(max(LdotV, 0.0), specularmap.a * 512.0);

	/* Calculate dynamic lights (if any) */
	light = clamp(light + LightFragment(normalmap.rgb), 0.0, 2.0);

	finalColor.rgb = diffusemap.rgb * light + specular;
	finalColor.a = diffusemap.a;

	finalColor *= blendColor;

#if r_fog
	/* Add fog.*/
	finalColor = FogFragment(finalColor);
#endif

	/* Developer tools, if enabled */
	finalColor = ApplyDeveloperTools(finalColor, normalmap.rgb, light, deluxemap);

	writeFragment(finalColor);
}
