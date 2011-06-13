/**
 * @file cook-torrance_fs.glsl
 * @brief A fragment shader model for light reflection.
 *
 * The Cook-Torrance model for light reflection is a physics
 * based model which uses two "roughness" parameters to describe
 * the shape and distribution of micro-facets on the surface.
 * NOTE: portions based on D3D9 source from Jack Hoxley
 */

#define ATTENUATE_THRESH 0.05

/**
 * @todo does not compile on my ati x600 yet
 */
vec3 LightContribution(in gl_LightSourceParameters lightSource, in vec3 lightDir, in vec3 N, in vec3 V, float NdotV, float R_2, in vec4 roughness, in vec4 specular, in vec4 diffuse) {
	/* calculate light attenuation due to distance (do this first so we can return early if possible) */
	float attenuate = 1.0;

	if (bool(lightSource.position.w)) { /* directional sources don't get attenuated */
		float dist = length((lightSource.position).xyz - point);
		float attenDiv = (lightSource.constantAttenuation +
				lightSource.linearAttenuation * dist +
				lightSource.quadraticAttenuation * dist * dist);
		/* If none of the attenuation parameters are set, we keep 1.0.*/
		if (bool(attenDiv)) {
			attenuate = 1.0 / attenDiv;
		}
	}

	/* if we're out of range, ignore the light; else calculate its contribution */
	if (attenuate < ATTENUATE_THRESH) {
		return vec3(0.0);
	}

	/* Normalize vectors and cache dot products */
	vec3 L = normalize(lightDir);
	float NdotL = clamp(dot(N, -L), 0.0, 1.0);

	/* Compute the final color contribution of the light */
	vec3 ambientColor = diffuse.rgb * diffuse.a * lightSource.ambient.rgb;
	vec3 diffuseColor = diffuse.rgb * diffuse.a * lightSource.diffuse.rgb * NdotL;
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

		specularColor = lightSource.specular.rgb * specular.rgb * roughness.b * NdotL * (F * R * G) / (NdotV * NdotL);
	} else { /* Phong shading */
		specularColor = lightSource.specular.rgb * specular.rgb * pow(max(dot(V, reflect(-L, N)), 0.0), specular.a);
	}

	/* @note We attenuate light here, but attenuation doesn't affect "directional" sources like the sun */
	return (attenuate * (max(ambientColor, 0.0) + max(diffuseColor, 0.0) + max(specularColor, 0.0)));
}


vec4 IlluminateFragment(void) {
	vec3 totalColor= vec3(0.0);

	/* sample the relevant textures */
	vec2 coords = gl_TexCoord[0].st;
	vec2 offset = vec2(0.0);

	/* do per-fragment calculations */
	vec3 V = -normalize(eyedir);
	vec3 N;
	if (BUMPMAP > 0) {
#if r_bumpmap
		vec4 normalMap = texture2D(SAMPLER3, coords);
		N = vec3((normalize(normalMap.xyz) * 2.0 - vec3(1.0)));
		N.xy *= BUMP;
		if (PARALLAX > 0.0) {
			offset = BumpTexcoord(normalMap.a);
			coords += offset;
		}
#endif
	} else {  /* just use the basic surface normal */
		N = vec3(0.0, 0.0, 1.0);
	}

	vec4 diffuse = texture2D(SAMPLER0, coords);
	vec4 specular;
	if (SPECULARMAP > 0) {
		specular = texture2D(SAMPLER1, coords);
	} else {
		specular = vec4(HARDNESS, HARDNESS, HARDNESS, SPECULAR);
	}
	specular.a *= 512.0;

	vec4 roughness;
	float R_2 = 0.0;
	float NdotV = 0.0;
	if (ROUGHMAP > 0) {
		roughness = texture2D(SAMPLER2, coords);
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
	totalColor += LightContribution(gl_LightSource[$], lightDirs[$], N, V, NdotV, R_2, roughness, specular, diffuse);
#endunroll

	return vec4(totalColor, diffuse.a);
}
