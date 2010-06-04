#define ATTENUATE_THRESH 0.05 

/* @todo - this on-the-fly dirtmap transparency generation
		   looks nice, but it's probably more expensive than
		   it needs to be...still cheaper than doing it
		   on the CPU, though */
float dirtNoise(in vec3 v) {
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


/**
 * The Cook-Torrance model for light reflection is a physics
 * based model which uses two "roughness" parameters to describe
 * the shape and distribution of micro-facets on the surface.
 */

uniform sampler2D SAMPLER_SHADOW0;

/* Chebyshev's Upper Bound for the likelyhood a fragment is in shadow */
float chebyshevUpperBound(vec4 shadow)
{
	vec3 moments = textureProj(SAMPLER_SHADOW0, shadow).rgb;

	float shadowZ = (shadow.z / shadow.w) - 0.05;

	/* early return if fragment is fully lit */
	if (shadowZ <= moments.x)
		return 1.0;

	/* otherwise, the fragment is either in shadow or penumbra. We now use
	 * chebyshev's upper-bound to check How likely this pixel is to be lit */
	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, 0.0000002);

	float d = shadowZ - moments.x;
	float p_max = variance / (variance + d * d);

	return p_max;
}


/** @todo does not compile on my ati x600 yet */
vec3 LightContribution(in vec4 location, 
					   in vec4 ambientLight, 
					   in vec4 diffuseLight, 
					   in vec4 specularLight, 
					   in vec3 attenuation, 
					   in vec3 lightDir, 
					   in vec3 N, 
					   in vec3 V, 
					   in float NdotV, 
					   in float R_2, 
					   in vec4 roughness, 
					   in vec4 specular, 
					   in vec4 diffuse){

	/* calculate light attenuation due to distance (do this first so we can return early if possible) */
	/* @todo this assumes all lights are point sources; it should respect 
	 * settings for spot-light sources. */
	float attenuate = attenuation[0];

	if (attenuate > 0.0 && location.w != 0.0){ /* directional sources don't get attenuated */
		float dist = length((gl_NormalMatrix * location.xyz) - vPosCamera.xyz);
		attenuate = 1.0 / (attenuation[0] + 
				attenuation[1] * dist +
				attenuation[2] * dist * dist); 
	}

	/* if we're out of range, ignore the light; else calculate its contribution */
	if(attenuate < ATTENUATE_THRESH) {
		return vec3(0.0);
	}

	vec3 ambientColor = diffuse.rgb * diffuse.a * ambientLight.rgb;
	/* Normalize vectors and cache dot products */
	vec3 L = normalize(lightDir);
	//float NdotL = clamp(dot(N, -L), 0.0, 1.0);
	float NdotL = dot(N, -L);
	if (NdotL < 0.0) {
		//return attenuate * ambientColor;
	}

#if r_debug_shadows
	//return vec3(textureProj(SAMPLER_SHADOW0, shadowCoord).r * NdotL);
	return textureProj(SAMPLER_SHADOW0, shadowCoord).rgb;
	//return vec3(1.0 - ((1.0 - chebyshevUpperBound(shadowCoord)) * NdotL));
	//return vec3(1.0);
	//return vec3(shadowCoord.a );
#endif

	float shadow = 1.0;
#if r_shadowmapping
	if (SHADOWMAP > 0) {
		shadow = 1.0 - ((1.0 - chebyshevUpperBound(shadowCoord)) * NdotL);

		/* if the fragment is completely shadowed, we don't need 
		 * to calculate anything but ambient */
		if (shadow < ATTENUATE_THRESH) {
			return attenuate * ambientColor;
		}
	}
#endif


	/* Compute the final color contribution of the light */
	vec3 diffuseColor = diffuse.rgb * diffuse.a * diffuseLight.rgb * NdotL;
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

		specularColor = specular.rgb * specularLight.rgb * roughness.b * NdotL * (F * R * G) / (NdotV * NdotL);
	} else { /* Phong shading */
		specularColor = specular.rgb * specularLight.rgb * pow(max(dot(V, reflect(-L, N)), 0.0), specular.a);
	}

	/* @note We attenuate light here, but attenuation doesn't affect "directional" sources like the sun */
	return (attenuate * (max(ambientColor, 0.0) + shadow * max(diffuseColor, 0.0) + shadow * max(specularColor, 0.0)));
}


vec4 IlluminateFragment(void){

	vec3 totalColor= vec3(0.0);
	vec2 coords = gl_TexCoord[0].st;

	/* do per-fragment calculations */
	vec3 V = -normalize(eyedir);
	vec3 N;
	if (BUMPMAP > 0) {
#if r_bumpmap
		vec4 normalMap = texture2D(SAMPLER_NORMALMAP, coords);
		N = vec3((normalize(normalMap.xyz) * 2.0 - vec3(1.0)));
		N.xy *= BUMP;
		if (PARALLAX > 0.0)
			coords += vec2(normalMap.a * PARALLAX * 0.04 - 0.02) * V.xy;
#endif
	} else {  /* just use the basic surface normal */
		N = vec3(0.0, 0.0, 1.0);
	}

	vec4 diffuse = texture2D(SAMPLER_DIFFUSE, coords);
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

	/* do per-light calculations */
#unroll r_dynamic_lights
	if ($ < NUM_ACTIVE_LIGHTS) {
		//totalColor += LightContribution(gl_LightSource[$], lightDirs[$], N, V, NdotV, R_2, roughness, specular, diffuse);
		totalColor += LightContribution(LightLocation[$],
										LightAmbient[$],
										LightDiffuse[$],
										LightSpecular[$],
										LightAttenuation[$],
										lightDirs[$], 
										N, V, NdotV, R_2, roughness, specular, diffuse);
	}
#endunroll

	float alpha = 1.0;
	/* set alpha for material stages that use it */
	if (DIRT > 0.0) {
		alpha = dirtNoise(Vertex.xyz) * DIRT;
	}

	if (TERRAIN > 0.0) {
		alpha = clamp( (Vertex.z - STAGE_MIN) / STAGE_MAX, 0.0, 1.0);
	}

	if (TAPE > 0.0) {
		if (Vertex.z < STAGE_MAX && Vertex.z > STAGE_MIN) {
			if (Vertex.z > TAPE) {
				float delta = Vertex.z - TAPE;
				alpha = clamp( 1.0 - (delta / STAGE_MAX), 0.0, 1.0);
			} else {
				float delta = TAPE - Vertex.z;
				alpha = clamp( 1.0 - (delta / STAGE_MIN), 0.0, 1.0);
			}
		} else {
			alpha = 0.0;
		}
	}

	return vec4(totalColor, diffuse.a * alpha);
}


