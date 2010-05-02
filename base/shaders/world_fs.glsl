// default fragment shader

#include "light_fs.glsl"
#include "bump_fs.glsl"
#include "fog_fs.glsl"

uniform int BUMPMAP;
uniform int ROUGHMAP;
uniform int SPECULARMAP;
uniform int STATICLIGHT;
uniform int DYNAMICLIGHTS;
uniform float GLOWSCALE;

uniform sampler2D SAMPLER0;
/* lightmap */
uniform sampler2D SAMPLER1;
/* deluxemap */
uniform sampler2D SAMPLER2;
/* normalmap */
uniform sampler2D SAMPLER3;
/* glowmap */
uniform sampler2D SAMPLER4;


const vec3 two = vec3(2.0);
const vec3 negHalf = vec3(-0.5);

varying vec3 lightpos;
varying vec3 staticLightDir;
varying vec3 lightDirs[1];

varying vec3 tangent;

#include "cook-torrance_fs.glsl"

/**
 * main
 */
void main(void){

	vec4 finalColor = vec4(0.0);

	/* use new dynamic lighing system, including 
	 * the Cook-Torrance specularity model with the Phong
	 * model as a default if the roughness map isn't enabled */
	if (DYNAMICLIGHTS > 0) {
		finalColor = IlluminateFragment();
	} else {
		/* use static lighting (ie. legacy rendering code) */
		vec2 offset = vec2(0.0);
		vec3 bump = vec3(1.0);

		/* lightmap contains pre-computed incoming light color */
		vec3 lightmap = texture2D(SAMPLER1, gl_TexCoord[1].st).rgb;

#if r_bumpmap
		if(BUMPMAP > 0){  // sample deluxemap and normalmap
			vec4 normalmap = texture2D(SAMPLER3, gl_TexCoord[0].st);
			normalmap.rgb = normalize(two * (normalmap.rgb + negHalf));

			/* note: clamp() is a hack we need because we don't actually 
			 * want unlit surfaces to be totally dark */
			if(STATICLIGHT > 0){
				bump = clamp(BumpFragment(staticLightDir, normalmap.rgb), 0.5, 1.0);
			} else if (DYNAMICLIGHTS == 0){
				/* deluxemap contains pre-computed incoming light vectors in object tangent space */
				vec3 deluxemap = texture2D(SAMPLER2, gl_TexCoord[1].st).rgb;
				deluxemap = normalize(two * (deluxemap + negHalf));

				// resolve parallax offset and bump mapping
				offset = BumpTexcoord(normalmap.a);
				bump = BumpFragment(deluxemap, normalmap.rgb);
			}
		}
#endif

		// sample the diffuse texture, honoring the parallax offset
		vec4 diffuse = texture2D(SAMPLER0, gl_TexCoord[0].st + offset);

		// factor in bump mapping
		diffuse.rgb *= bump;

		// use static lighting if enabled
		if (STATICLIGHT > 0) {
			vec3 lightdir = normalize(lightpos - point);
			float shade = max(0.5, pow(2.0 * dot(normal, lightdir), 2.0));

			vec3 color = vec3(1.0);
			if (gl_Color.r > 0.0 || gl_Color.g > 0.0 || gl_Color.b > 0.0) {
				color = gl_Color.rgb;
			} 
			finalColor = LightFragment(diffuse, color * shade);
		} else {
			// otherwise, add to lightmap
			finalColor = LightFragment(diffuse, lightmap);
		}
	}

#if r_fog
	finalColor = FogFragment(finalColor);  // add fog
#endif

#if r_postprocess
	gl_FragData[0] = finalColor;
	if(GLOWSCALE > 0.0){
		 vec4 glowcolor = texture2D(SAMPLER4, gl_TexCoord[0].st);
		 gl_FragData[1].rgb = glowcolor.rgb * glowcolor.a * GLOWSCALE;
		 gl_FragData[1].a = 1.0;
	} 
#else 
	gl_FragColor = finalColor;
#endif

// developer tools
/*
#if r_normalmap
	gl_FragData[0] = (1.0 + dot(vec3(0.0, 0.0, 1.0), normalize(-lightDirs[0]))) * 0.5 * vec4(1.0);
#endif

#if r_lightmap
	gl_FragData[0].rgb = lightmap;
	gl_FragData[0].a = 1.0;
#endif

#if r_deluxemap
	if(BUMPMAP > 0){
		gl_FragData[0].rgb = deluxemap;
		gl_FragData[0].a = 1.0;
	}
#endif
*/
}
