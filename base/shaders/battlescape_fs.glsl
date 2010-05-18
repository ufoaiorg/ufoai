#version 130
/* battlescape fragment shader */

uniform int BUMPMAP;
uniform int ROUGHMAP;
uniform int SPECULARMAP;
uniform int LIGHTMAP;
uniform int BUILD_SHADOWMAP;
uniform int WARP;

uniform float GLOWSCALE;
uniform float BUMP;
uniform float PARALLAX;
uniform float HARDNESS;
uniform float SPECULAR;


uniform sampler2D SAMPLER_DIFFUSE;
uniform sampler2D SAMPLER_LIGHTMAP;
uniform sampler2D SAMPLER_DELUXEMAP;
uniform sampler2D SAMPLER_NORMALMAP;
uniform sampler2D SAMPLER_GLOWMAP;
uniform sampler2D SAMPLER_SPECMAP;
uniform sampler2D SAMPLER_ROUGHMAP;

uniform gl_FogParameters gl_Fog;

in vec4 Vertex;
in vec4 Tangent;
in vec4 vPosScreen;
in vec4 vPosCamera;
in vec3 vNormal;
in vec3 eyedir;
in vec3 lightDirs[];
in vec4 shadowCoord;
in float fog;
in vec4 gl_TexCoord[];

#if r_postprocess
out vec4 gl_FragData[];
#else
out vec4 gl_FragColor;
#endif

#include "illuminate_fs.glsl"

/**
 * main
 */
void main(void){
	vec4 outColor = vec4(0.0);
	vec4 glowColor = vec4(0.0);

	/* shadowmap generation */
	if (BUILD_SHADOWMAP > 0) {
		float depth = vPosScreen.z / vPosScreen.w ;

		/* move away from unit cube ([-1,1]) to [0,1] coordinate system used by textures */
		depth = depth * 0.5 + 0.5;			

		float moment1 = depth;
		float moment2 = depth * depth;

		/* bias moments using derivative */
		float dx = dFdx(depth);
		float dy = dFdy(depth);
		moment2 += 0.25*(dx*dx+dy*dy) ;

		outColor = vec4(moment1, moment2, 0.0, 1.0 );

	/* final rendering pass */
	} else {

		/* calculate dynamic lighting, including 
		 * the Cook-Torrance specularity model with the Phong
		 * model as a default if the roughness map isn't enabled */
		outColor = IlluminateFragment();

		/* for models with pre-computed lightmaps (ie. BSP models), use them */
		if (LIGHTMAP > 0) {
			vec2 offset = vec2(0.0);
			float NdotL = 1.0;
			float specular = 0.0;

#if r_bumpmap
			if(BUMPMAP > 0){
				vec4 normalmap = texture2D(SAMPLER_NORMALMAP, gl_TexCoord[0].st);
				normalmap.rgb = normalize(2.0 * (normalmap.rgb - vec3(0.5)));
				/* pre-computed incoming light vectors in object tangent space */
				vec3 deluxemap = texture2D(SAMPLER_DELUXEMAP, gl_TexCoord[1].st).rgb;
				deluxemap = normalize(2.0 * (deluxemap - vec3(0.5)));

				/* compute parallax offset and bump mapping reflection */
				vec3 V = normalize(eyedir);
				vec3 L = vec3(normalize(deluxemap).rgb);
				vec3 N = vec3(normalize(normalmap.rgb).rgb);
				N.xy *= BUMP;

				offset = vec2(normalmap.a * PARALLAX * 0.04 - 0.02) * V.xy;
				NdotL = dot(N, L);
				specular = HARDNESS * pow(max(-dot(V, reflect(L, N)), 0.0), 8.0 * SPECULAR);
			}
#endif
			/* sample the diffuse texture, using any parallax offset */
			vec4 diffuse = texture2D(SAMPLER_DIFFUSE, gl_TexCoord[0].st + offset);
			/* lightmap contains pre-computed incoming light color */
			vec3 lightmap = texture2D(SAMPLER_LIGHTMAP, gl_TexCoord[1].st).rgb;

			/* add light from lightmap */
			//outColor += diffuse * vec4(lightmap, 1.0) * (NdotL + specular);
		}

#if r_fog
		/* add fog */
		//outColor += vec4(mix(outColor.rgb, gl_Fog.color.rgb, fog), outColor.a);
#endif

		/* use glow-map */
		if(GLOWSCALE > 0.0){
			vec4 glow = texture2D(SAMPLER_GLOWMAP, gl_TexCoord[0].st);
			glowColor = vec4(glow.rgb * glow.a * GLOWSCALE, 1.0);
		} 

/* debuging tools */
#if r_debug_normals
		outColor = (1.0 + dot(vec3(0.0, 0.0, 1.0), normalize(-lightDirs[0]))) * 0.5 * vec4(1.0);
#endif

#if r_debug_tangents
		outColor.r = (1.0 + dot(vec3(1.0, 0.0, 0.0), normalize(-lightDirs[0]))) * 0.5;
		outColor.g = (1.0 + dot(vec3(0.0, 1.0, 0.0), normalize(-lightDirs[0]))) * 0.5;
		outColor.b = (1.0 + dot(vec3(0.0, 0.0, 1.0), normalize(-lightDirs[0]))) * 0.5;
		outColor.a = 1.0;
#endif


#if r_debug_normalmaps
		vec3 n = normalize(2.0 * (texture2D(SAMPLER_NORMALMAP, gl_TexCoord[0].st).rgb - 0.5));
		outColor = (1.0 + dot(n, normalize(-lightDirs[0]))) * 0.5 * vec4(1.0);
#endif

#if r_debug_lightmaps
		if(LIGHTMAP > 0)
			outColor = vec4(lightmap, 1.0);
#endif

#if r_debug_deluxemaps
		if(BUMPMAP > 0 && LIGHTMAP > 0)
			outColor = vec4(deluxemap, 1.0);
#endif
	}

/* return final fragment color */
#if r_postprocess
	gl_FragData[0] = outColor;
	gl_FragData[1] = glowColor;
#else
	gl_FragColor = outColor;
#endif

}
