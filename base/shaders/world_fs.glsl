#version 110
// default battlescape fragment shader

uniform int BUMPMAP;
uniform int ROUGHMAP;
uniform int SPECULARMAP;
uniform int LIGHTMAP;
uniform int BUILD_SHADOWMAP;
uniform float GLOWSCALE;

/* diffuse texture */
uniform sampler2D SAMPLER_DIFFUSE;
/* lightmap */
uniform sampler2D SAMPLER_LIGHTMAP;
/* deluxemap */
uniform sampler2D SAMPLER_DELUXEMAP;
/* normalmap */
uniform sampler2D SAMPLER_NORMALMAP;
/* glowmap */
uniform sampler2D SAMPLER_GLOWMAP;
/* specularity map */
uniform sampler2D SAMPLER_SPECMAP;
/* roughness map */
uniform sampler2D SAMPLER_ROUGHMAP;

const vec3 two = vec3(2.0);
const vec3 negHalf = vec3(-0.5);

varying vec4 vertPos;
varying vec3 lightDirs[#replace r_dynamic_lights ];

#include "light_fs.glsl"
#include "bump_fs.glsl"
#include "fog_fs.glsl"
#include "cook-torrance_fs.glsl"

/**
 * main
 */
void main(void){

	if (BUILD_SHADOWMAP > 0) {
		float depth = vertPos.z / vertPos.w ;

		/* move away from unit cube ([-1,1]) to [0,1] coordinate system used by textures */
		depth = depth * 0.5 + 0.5;			

		float moment1 = depth;
		float moment2 = depth * depth;

		/* bias moments using derivative */
		float dx = dFdx(depth);
		float dy = dFdy(depth);
		moment2 += 0.25*(dx*dx+dy*dy);
		//moment2 += 20.0 * (dx*dx+dy*dy) ;
		//moment2 = 1000.0 * (abs(dx)+abs(dy));

		//moment2 = 100.0 * sqrt(dx*dx + dy*dy);

#if r_postprocess
		gl_FragData[0] = vec4(moment1, moment2, 0.0, 1.0 );
#else 
		gl_FragColor = vec4(moment1, moment2, 0.0, 1.0 );
#endif
		return;
	}

	/* calculate dynamic lighting, including 
	 * the Cook-Torrance specularity model with the Phong
	 * model as a default if the roughness map isn't enabled */
	vec4 finalColor = IlluminateFragment();

	/* for models with pre-computed lightmaps (ie. BSP models), use them */
	if (LIGHTMAP > 0) {
		/* use static lighting (ie. legacy rendering code) */
		vec2 offset = vec2(0.0);
		vec3 bump = vec3(1.0);

		/* lightmap contains pre-computed incoming light color */
		vec3 lightmap = texture2D(SAMPLER_LIGHTMAP, gl_TexCoord[1].st).rgb;

#if r_bumpmap
		if(BUMPMAP > 0){  // sample deluxemap and normalmap
			vec4 normalmap = texture2D(SAMPLER_NORMALMAP, gl_TexCoord[0].st);
			normalmap.rgb = normalize(two * (normalmap.rgb + negHalf));

			/* deluxemap contains pre-computed incoming light vectors in object tangent space */
			vec3 deluxemap = texture2D(SAMPLER_DELUXEMAP, gl_TexCoord[1].st).rgb;
			deluxemap = normalize(two * (deluxemap + negHalf));

			// resolve parallax offset and bump mapping
			offset = BumpTexcoord(normalmap.a);
			bump = BumpFragment(deluxemap, normalmap.rgb);
		}
#endif

		// sample the diffuse texture, honoring the parallax offset
		vec4 diffuse = texture2D(SAMPLER_DIFFUSE, gl_TexCoord[0].st + offset);
		// add light from lightmap
		finalColor += 0.4 * vec4(diffuse.rgb * lightmap * bump, diffuse.a * gl_Color.a);
	}

#if r_fog
	finalColor = FogFragment(finalColor);  // add fog
#endif

#if r_postprocess
	gl_FragData[0] = finalColor;
	if(GLOWSCALE > 0.0){
		 vec4 glowcolor = texture2D(SAMPLER_GLOWMAP, gl_TexCoord[0].st);
		 gl_FragData[1].rgb = glowcolor.rgb * glowcolor.a * GLOWSCALE;
		 gl_FragData[1].a = 1.0;
	} 
#else 
	gl_FragColor = finalColor;
#endif

// developer tools

#if r_debug_normals
	gl_FragData[0] = (1.0 + dot(vec3(0.0, 0.0, 1.0), normalize(-lightDirs[0]))) * 0.5 * vec4(1.0);
	gl_FragData[1] = vec4(0.0);
#endif

#if r_debug_tangents
	gl_FragData[0].r = (1.0 + dot(vec3(1.0, 0.0, 0.0), normalize(-lightDirs[0]))) * 0.5;
	gl_FragData[0].g = (1.0 + dot(vec3(0.0, 1.0, 0.0), normalize(-lightDirs[0]))) * 0.5;
	gl_FragData[0].b = (1.0 + dot(vec3(0.0, 0.0, 1.0), normalize(-lightDirs[0]))) * 0.5;
	gl_FragData[0].a = 1.0;
	gl_FragData[1] = vec4(0.0);
#endif


#if r_debug_normalmaps
	vec3 n = normalize(2.0 * (texture2D(SAMPLER_NORMALMAP, gl_TexCoord[0].st).rgb - 0.5));
	gl_FragData[0] = (1.0 + dot(n, normalize(-lightDirs[0]))) * 0.5 * vec4(1.0);
	gl_FragData[1] = vec4(0.0);
#endif

#if r_debug_lightmaps
	gl_FragData[0].rgb = lightmap;
	gl_FragData[0].a = 1.0;
	gl_FragData[1] = vec4(0.0);
#endif

#if r_debug_deluxemaps
	if(BUMPMAP > 0){
		gl_FragData[0].rgb = deluxemap;
		gl_FragData[0].a = 1.0;
		gl_FragData[1] = vec4(0.0);
	}
#endif

}
