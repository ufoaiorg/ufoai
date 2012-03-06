/**
 * @file model_low_fs.glsl
 * @brief Low quality battlescape model fragment shader.
 */

#if r_postprocess
	/*
	 * Indicates that gl_FragData is written to, not gl_FragColor.
	 * #extension needs to be placed before all non preprocessor code.
	 */
	#extension GL_ARB_draw_buffers : enable
#endif

uniform int BUMPMAP;
uniform vec3 AMBIENT;
uniform vec3 SUNCOLOR;

in_qualifier vec3 sunDir; /** < Direction towards the sun */

/** Diffuse texture.*/
uniform sampler2D SAMPLER_DIFFUSE;
/** Normalmap.*/
uniform sampler2D SAMPLER_NORMALMAP;

#include "fog_fs.glsl"
#include "model_devtools_fs.glsl"
#include "write_fragment_fs.glsl"

/**
 * @brief main
 */
void main(void) {
	vec4 finalColor;

	vec4 diffuse = texture2D(SAMPLER_DIFFUSE, gl_TexCoord[0].st);
	vec3 normal = vec3(0.0, 0.0, 1.0);

	if (BUMPMAP > 0) {
		normal = normalize((texture2D(SAMPLER_NORMALMAP, gl_TexCoord[0].st).rgb - 0.5) * 2.0); /** < Expanded normal */
	}

	/* Lambert illumination model */
	vec3 light = AMBIENT + SUNCOLOR * clamp(dot(sunDir, normal), 0.0, 1.0);

	finalColor.rgb = diffuse.rgb * light;
	finalColor.a = diffuse.a;

#if r_fog
	/* Add fog.*/
	finalColor = FogFragment(finalColor);
#endif

	/* Developer tools, if enabled */
	finalColor = ApplyDeveloperTools(finalColor, sunDir, normal);

	writeFragment(finalColor);
}
