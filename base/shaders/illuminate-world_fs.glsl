//#version 130
/* battlescape fragment shader */

/* flags to enable/disable rendering pathways */
uniform int BUMPMAP;
uniform int ROUGHMAP;
uniform int SPECULARMAP;
uniform int LIGHTMAP;
uniform int SHADOWMAP;
uniform int BUILD_SHADOWMAP;
uniform int DRAW_GLOW;

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
uniform sampler2D SAMPLER_SHADOW;

/* light source parameters */
uniform int NUM_ACTIVE_LIGHTS;
uniform vec4 LightLocation;
uniform vec4 LightAmbient;
uniform vec4 LightDiffuse;
uniform vec4 LightSpecular;
uniform vec3 LightAttenuation;

uniform vec4 FogColor;


/* per-fragment inputs from vertex shader */
in vec4 Vertex;
in vec4 Normal;
in vec4 Tangent;
in vec4 vPosScreen;
in vec4 vPos;
in vec3 vNormal;
in vec3 eyedir;
in vec3 lightDir;
in float fog;
in float alpha;
in vec4 shadowCoord;
in vec4 texCoord;

/* per-fragment output colors */
#if r_framebuffers
//out vec4 gl_FragData[];
#else
//out vec4 gl_FragColor;
#endif

#define ATTENUATE_THRESH 0.05 

const ivec2 offsets[] = ivec2[]( 
						  ivec2(-1, -1),
						  ivec2(-1,  0),
						  ivec2(-1,  1),
						  ivec2( 0, -1),
						  ivec2( 0,  0),
						  ivec2( 0,  1),
						  ivec2( 1, -1),
						  ivec2( 1,  0),
						  ivec2( 1,  1) );


/* Chebyshev's Upper Bound for the likelyhood a fragment is in shadow */
float chebyshevUpperBound (vec4 shadow)
{
	vec3 moments = textureProj(SAMPLER_SHADOW, shadow).rgb;

	//float shadowZ = (shadow.z / shadow.w) - 0.0001;
	float shadowZ = shadow.z / shadow.w;

	/* early return if fragment is fully lit */
	if (shadowZ <= moments.x)
		return 1.0;

	/* otherwise, the fragment is either in shadow or penumbra. We now use
	 * chebyshev's upper-bound to check How likely this pixel is to be lit */
	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, 0.0000001);

	float d = shadowZ - moments.x;
	float p_max = variance / (variance + d * d);

	return p_max;
}

float chebyshevUpperBoundOffset (vec4 shadow, const ivec2 offset)
{
	vec3 moments = textureProjOffset(SAMPLER_SHADOW, shadow, offset).rgb;

	float shadowZ = (shadow.z / shadow.w) - 0.0001;

	/* early return if fragment is fully lit */
	if (shadowZ <= moments.x)
		return 1.0;

	/* otherwise, the fragment is either in shadow or penumbra. We now use
	 * chebyshev's upper-bound to check How likely this pixel is to be lit */
	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, 0.0000001);

	float d = shadowZ - moments.x;
	float p_max = variance / (variance + d * d);

	return p_max;
}

float chebyshevUpperBoundMultisample (vec4 shadow)
{
	float sum = 0.0;

	sum += chebyshevUpperBoundOffset(shadow, offsets[0]);
	sum += chebyshevUpperBoundOffset(shadow, offsets[1]);
	sum += chebyshevUpperBoundOffset(shadow, offsets[2]);
	sum += chebyshevUpperBoundOffset(shadow, offsets[3]);
	sum += chebyshevUpperBoundOffset(shadow, offsets[4]);
	sum += chebyshevUpperBoundOffset(shadow, offsets[5]);
	sum += chebyshevUpperBoundOffset(shadow, offsets[6]);
	sum += chebyshevUpperBoundOffset(shadow, offsets[7]);
	sum += chebyshevUpperBoundOffset(shadow, offsets[8]);

	sum /= 9.0;
	return sum;
}

/* @todo - this on-the-fly dirtmap transparency generation
		   looks nice, but it's probably more expensive than
		   it needs to be...still cheaper than doing it
		   on the CPU, though */
float dirtNoise (in vec3 v) {
	float a = 0.0;
	float b = 0.0;
	float c = 0.0;
	float l;

	l = v.x;
	a += sin(l * 0.01);
	a += cos(l * 0.005);
	a += sin(l * 0.007);
	a += cos(l * 0.0212);

	l = v.y;
	a += sin(l * 0.01);
	a += cos(l * 0.005);
	a += sin(l * 0.007);
	a += cos(l * 0.0237);

	a *= 0.125;
	clamp(a, -0.3, 0.3);

	l = (v.x * v.x + v.y * v.x) * 0.0001;
	b += sin(l * 0.01);
	b += cos(l * 0.00513);
	b += sin(l * 0.00729);
	b += cos(l * 0.0527);
	b += cos(l * 0.1527);
	b += cos(l * 0.2367);
	b *= 0.166666666;
	clamp(b, -0.3, 0.3);

	l = (v.x * v.y) * 0.0007;
	c += sin(l * 0.01);
	clamp(c, -0.3, 0.3);

	float r = (((0.3 * a) + (0.4 * b) + (0.3 * c)) * 0.5) + 0.4;
	return clamp (r, 0.2, 0.7);
}

vec4 IlluminateFragment (void)
{
	vec2 coords = texCoord.st;
	vec4 diffuse = texture2D(SAMPLER_DIFFUSE, coords);
	float alpha = diffuse.a;

	/* set alpha for material stages that use it */
	if (DIRT > 0.0) {
		alpha *= dirtNoise(Vertex.xyz) * DIRT;
	}

	if (TERRAIN > 0.0) {
		alpha *= clamp( (Vertex.z - STAGE_MIN) / STAGE_MAX, 0.0, 1.0);
	}

	if (TAPE > 0.0) {
		if (Vertex.z < STAGE_MAX && Vertex.z > STAGE_MIN) {
			if (Vertex.z > TAPE) {
				float delta = Vertex.z - TAPE;
				alpha *= clamp( 1.0 - (delta / STAGE_MAX), 0.0, 1.0);
			} else {
				float delta = TAPE - Vertex.z;
				alpha *= clamp( 1.0 - (delta / STAGE_MIN), 0.0, 1.0);
			}
		} else {
			alpha = 0.0;
		}
	}

	/* if alpha is zero, return early */
	if (alpha == 0.0) {
		return vec4(0.0);
	}

	/* calculate light attenuation due to distance */
	/* @todo this assumes all lights are point sources; it should respect 
	 * settings for spot-light sources. */
	float attenuate = LightAttenuation[0];

	if (attenuate > 0.0 && LightLocation.w != 0.0){ /* directional sources don't get attenuated */
		float dist = length((gl_NormalMatrix * LightLocation.xyz) - vPos.xyz);
		attenuate = 1.0 / (LightAttenuation[0] + 
				LightAttenuation[1] * dist +
				LightAttenuation[2] * dist * dist); 
	}

	/* if we're out of range, ignore the light and return early */
	if(attenuate < ATTENUATE_THRESH) {
		return vec4(0.0, 0.0, 0.0, alpha);
	}


	vec3 ambientColor = diffuse.rgb * diffuse.a * LightAmbient.rgb;

	vec3 V = -normalize(eyedir);
	vec3 N;
	if (BUMPMAP > 0) {
#if r_bumpmap
		vec4 normalMap = texture2D(SAMPLER_NORMALMAP, coords);
		N = vec3((normalize(normalMap.xyz) * 2.0 - vec3(1.0)));
		N.xy *= BUMP;
		if (PARALLAX > 0.0)
			coords += vec2(normalMap.a * PARALLAX * 0.04 - 0.02) * V.xy;
#else 
		N = vec3(0.0, 0.0, 1.0);
#endif
	} else {  /* just use the basic surface normal */
		N = vec3(0.0, 0.0, 1.0);
	}

	/* Normalize vectors and cache dot products */
	vec3 L = normalize(lightDir);
	float NdotL = dot(N, -L);
	if (NdotL < 0.0) {
		return vec4(attenuate * ambientColor, alpha);
	}


#if r_debug_shadows
	//return vec4(1.0 - ((1.0 - chebyshevUpperBound(shadowCoord)) * pow(NdotL, 0.5)));
	//return vec4(chebyshevUpperBound(shadowCoord));
	//return vec4(chebyshevUpperBoundOffset(shadowCoord, offsets[5]));
	//return vec4(chebyshevUpperBoundMultisample(shadowCoord));
	return vec4(textureProj(SAMPLER_SHADOW, shadowCoord).rgb, 1.0);
	//return vec4(textureProjOffset(SAMPLER_SHADOW, shadowCoord, const ivec2(5,5)).rgb, 1.0);
#endif

	float shadow = 1.0;
#if r_shadowmapping
	if (SHADOWMAP > 0) {
		//shadow = 1.0 - ((1.0 - chebyshevUpperBound(shadowCoord)) * NdotL);
		//shadow = chebyshevUpperBoundMultisample(shadowCoord);
		shadow = chebyshevUpperBound(shadowCoord);

		/* if the fragment is completely shadowed, we don't need 
		 * to calculate anything but ambient color */
		if (shadow < ATTENUATE_THRESH) {
			return vec4(attenuate * ambientColor, alpha);
		}
	}
#endif


	vec4 specular;
	if (SPECULARMAP > 0) {
		specular = texture2D(SAMPLER_SPECMAP, coords);
	} else {
		specular = vec4(HARDNESS, HARDNESS, HARDNESS, SPECULAR);
	}
	specular.a *= 512.0;

	vec4 roughness;
	float R_2 = 0.0;
	float NdotV = 0.0;
	if (ROUGHMAP > 0) {
		roughness = texture2D(SAMPLER_ROUGHMAP, coords);
		/* scale reflectance to a more useful range */
		roughness.r = clamp(roughness.r, 0.05, 0.95);
		roughness.g *= 3.0;
		R_2 = roughness.r * roughness.r;
		NdotV = dot(N, -V);
	} else {
		roughness = vec4(0.0);
	}



	/* Compute the final color contribution of the light */
	vec3 diffuseColor = diffuse.rgb * diffuse.a * LightDiffuse.rgb * NdotL;
	vec3 specularColor;

	/* Cook-Torrance shading */
	if (ROUGHMAP > 0) {
		vec3 H = normalize(L + V);
		float NdotH = dot(N, -H);
		float VdotH = dot(V, H);
		float NdotH_2 = NdotH * NdotH;

		/* Compute the geometric term for specularity */
		float G1 = (2.0 * NdotH * NdotV) / VdotH;
		float G2 = (2.0 * NdotH * NdotL) / VdotH;
		float G = clamp(min(G1, G2), 0.0, 1.0);

		/* Compute the roughness term for specularity */
		float A = 1.0 / (4.0 * R_2 * NdotH_2 * NdotH_2);
		float B = exp((NdotH_2 - 1.0) / (R_2 * NdotH_2));
		float R = A * B;

		/* Compute the fresnel term for specularity using Schlick's approximation*/
		float F = roughness.g + (1.0 - roughness.g) * pow(1.0 - VdotH, 5.0);

		specularColor = specular.rgb * LightSpecular.rgb * roughness.b * NdotL * (F * R * G) / (NdotV * NdotL);
	} else { /* Phong shading */
		specularColor = specular.rgb * LightSpecular.rgb * pow(max(dot(V, reflect(-L, N)), 0.0), specular.a);
	}

	/* @note We attenuate light here, but attenuation doesn't affect "directional" sources like the sun */
	vec3 totalColor = (attenuate * (max(ambientColor, 0.0) + shadow * max(diffuseColor, 0.0) + shadow * max(specularColor, 0.0)));

	return vec4(totalColor, alpha);
}

/**
 * main
 */
void main (void)
{
	

	/* calculate dynamic lighting, including 
	 * the Cook-Torrance specularity model with the Phong
	 * model as a default if the roughness map isn't enabled */
	vec4 outColor = IlluminateFragment();

	/* use glow-map */
	vec4 glowColor = vec4(0.0);
	if (GLOWSCALE > 0.0) {
		vec4 glow = texture2D(SAMPLER_GLOWMAP, texCoord.st);
		glowColor = vec4(glow.rgb * GLOWSCALE, glow.a);
	}

	/* discard completely transparent pixels */
	if (outColor.a <= 0.0 && glowColor.a <= 0.0) {
		discard;
	}

#if r_fog
	/* add fog */
	outColor = vec4(mix(outColor.rgb, FogColor.rgb, fog), outColor.a);
#endif

	
	/* debuging tools */
#if r_debug_normals
	outColor = (1.0 + dot(vec3(0.0, 0.0, 1.0), normalize(-lightDirs[0]))) * 0.5 * vec4(1.0);
	//outColor = vec4(dot(normalize(vNormal), normalize(-vPos.rgb)));
#endif
#if r_debug_normalmaps
	vec3 n = normalize(2.0 * (texture2D(SAMPLER_NORMALMAP, texCoord.st).rgb - 0.5));
	outColor = (1.0 + dot(n, normalize(-lightDirs[0]))) * 0.5 * vec4(1.0);
#endif
#if r_debug_tangents
	outColor.r = (1.0 + dot(vec3(1.0, 0.0, 0.0), normalize(-eyedir))) * 0.5;
	outColor.g = (1.0 + dot(vec3(0.0, 1.0, 0.0), normalize(-eyedir))) * 0.5;
	outColor.b = (1.0 + dot(vec3(0.0, 0.0, 1.0), normalize(-eyedir))) * 0.5;
	outColor.a = 1.0;
#endif


	/* return final fragment color */
#if r_framebuffers
	//gl_FragData[0] = outColor;
	//gl_FragData[1] = glowColor;
	if (DRAW_GLOW > 0) {
		gl_FragData[0] = vec4(0.0);;
		gl_FragData[1] = glowColor;
	} else {
		gl_FragData[0] = outColor;
		gl_FragData[1] = vec4(0.0);;
	}
#else
	if (DRAW_GLOW > 0) {
		gl_FragColor = glowColor;
	} else {
		gl_FragColor = outColor;
	}
#endif

}
