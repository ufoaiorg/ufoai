//#version 130
/* battlescape fragment shader */

/* flags to enable/disable rendering pathways */
uniform int BUMPMAP;
uniform int ROUGHMAP;
uniform int SPECULARMAP;
uniform int LIGHTMAP;
uniform int SHADOWMAP;
uniform int BUILD_SHADOWMAP;

/* scaling factors */
uniform float BUMP;
uniform float PARALLAX;
uniform float HARDNESS;
uniform float SPECULAR;
uniform float GLOWSCALE;

/* material stage blending */
uniform float DIRT;
uniform float TAPE;
uniform float TERRAIN;
uniform float STAGE_MIN;
uniform float STAGE_MAX;

/* texture samplers */
uniform sampler2D SAMPLER_DIFFUSE;
uniform sampler2D SAMPLER_LIGHTMAP;
uniform sampler2D SAMPLER_DELUXEMAP;
uniform sampler2D SAMPLER_NORMALMAP;
uniform sampler2D SAMPLER_GLOWMAP;
uniform sampler2D SAMPLER_SPECMAP;
uniform sampler2D SAMPLER_ROUGHMAP;

/* light source parameters */
uniform int NUM_ACTIVE_LIGHTS;
uniform vec4 LightLocation[#replace r_dynamic_lights ];
uniform vec4 LightAmbient[#replace r_dynamic_lights ];
uniform vec4 LightDiffuse[#replace r_dynamic_lights ];
uniform vec4 LightSpecular[#replace r_dynamic_lights ];
uniform vec3 LightAttenuation[#replace r_dynamic_lights ];

uniform vec4 FogColor;


/* per-fragment inputs from vertex shader */
in vec4 Vertex;
in vec4 Normal;
in vec4 Tangent;
in vec4 vPosScreen;
in vec4 vPos;
in vec3 vNormal;
in vec3 eyedir;
in vec3 lightDirs[];
in vec4 shadowCoord;
in float fog;
in float alpha;
in vec4 gl_TexCoord[];

/* per-fragment output colors */
#if r_framebuffers
//out vec4 gl_FragData[];
#else
//out vec4 gl_FragColor;
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
		float depth = 0.5 * (vPosScreen.z / vPosScreen.w) + 0.5;
		float moment2 = depth * depth;

		/* bias moments using derivative */
		float dx = dFdx(depth);
		float dy = dFdy(depth);
		/* @note - this causes artifacts; possibly each model being drawn separately leads to issues */
		//moment2 += 0.25*(dx*dx+dy*dy) ;

		/* we use the diffuse texture alpha so transparent objects don't cast shadows */
		outColor = vec4(depth, moment2, 0.0, texture2D(SAMPLER_DIFFUSE, gl_TexCoord[0].st).a);

	/* standard rendering pass */
	} else {

		/* calculate dynamic lighting, including 
		 * the Cook-Torrance specularity model with the Phong
		 * model as a default if the roughness map isn't enabled */
		outColor = IlluminateFragment();

		/* for models with pre-computed lightmaps (ie. BSP models), use them */
		/* @todo - decide whether we actually want this or not, and then either
				   make new lightmaps to use with it or just remove the feature
				   entirely and use dynamic lights instead */
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
			//outColor.rgb += diffuse.rgb * lightmap * (NdotL + specular);
		}

#if r_fog
		/* add fog */
		outColor = vec4(mix(outColor.rgb, FogColor.rgb, fog), outColor.a);
#endif

		/* use glow-map */
		if(GLOWSCALE > 0.0){
			vec4 glow = texture2D(SAMPLER_GLOWMAP, gl_TexCoord[0].st);
			glowColor = vec4(glow.rgb * glow.a * GLOWSCALE, 1.0);
		} 

/* debuging tools */
#if r_debug_normals
		outColor = (1.0 + dot(vec3(0.0, 0.0, 1.0), normalize(-lightDirs[0]))) * 0.5 * vec4(1.0);
		//outColor = vec4(dot(normalize(vNormal), normalize(-vPos.rgb)));
#endif

#if r_debug_tangents
		outColor.r = (1.0 + dot(vec3(1.0, 0.0, 0.0), normalize(-eyedir))) * 0.5;
		outColor.g = (1.0 + dot(vec3(0.0, 1.0, 0.0), normalize(-eyedir))) * 0.5;
		outColor.b = (1.0 + dot(vec3(0.0, 0.0, 1.0), normalize(-eyedir))) * 0.5;
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
#if r_framebuffers
	gl_FragData[0] = outColor;
	gl_FragData[1] = glowColor;
#else
	gl_FragColor = outColor;
#endif

}
