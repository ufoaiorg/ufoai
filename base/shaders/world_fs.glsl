// default fragment shader

#include "light_fs.glsl"
#include "bump_fs.glsl"
#include "fog_fs.glsl"

uniform int BUMPMAP;
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
varying vec3 lightDirs[];

varying vec3 tangent;

/**
 * main
 */
void main(void){

	vec2 offset = vec2(0.0);
	vec3 bump = vec3(1.0);

	/* lightmap contains pre-computed incoming light color */
	vec3 lightmap = texture2D(SAMPLER1, gl_TexCoord[1].st).rgb;

#if r_bumpmap
	if(BUMPMAP > 0){  // sample deluxemap and normalmap

		vec4 normalmap = texture2D(SAMPLER3, gl_TexCoord[0].st);
		normalmap.rgb = normalize(two * (normalmap.rgb + negHalf));

		if(STATICLIGHT > 0){
			offset = BumpTexcoord(normalmap.a);
			/* note: clamp() is a hack we need because we don't actually want unlit surfaces to be totally dark */
			bump = clamp(BumpFragment(staticLightDir, normalmap.rgb), 0.5, 1.0);
		} else {
			/* deluxemap contains pre-computed incoming light vectors in object tangent space */
			vec3 deluxemap = texture2D(SAMPLER2, gl_TexCoord[1].st).rgb;
			deluxemap = normalize(two * (deluxemap + negHalf));


			// resolve parallax offset and bump mapping
			offset = BumpTexcoord(normalmap.a);
			bump = BumpFragment(deluxemap, normalmap.rgb);
		} 
	}

	if(DYNAMICLIGHTS > 0){
		vec4 normalmap = texture2D(SAMPLER3, gl_TexCoord[0].st);	
		offset = BumpTexcoord(normalmap.a);
#unroll r_dynamic_lights
		bump += BumpFragment(lightDirs[$], normalmap.rgb);
#endunroll
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
		LightFragment(diffuse, color * shade);
	} else {
		// otherwise, add any dynamic lighting and yield a base fragment color
		LightFragment(diffuse, lightmap);
	}

#if r_fog
	FogFragment();  // add fog
#endif

// developer tools
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

#if r_postprocess
	if(GLOWSCALE > 0.01){
		 vec4 glowcolor = texture2D(SAMPLER4, gl_TexCoord[0].st);
		 gl_FragData[1].rgb = glowcolor.rgb * glowcolor.a * GLOWSCALE;
		 gl_FragData[1].a = 1.0;
	}
#endif
}
