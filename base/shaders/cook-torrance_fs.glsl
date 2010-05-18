/* NOTE: portions loosly based on D3D9 source from Jack Hoxley */

#define ATTENUATE_THRESH 0.05 

/**
 * The Cook-Torrance model for light reflection is a physics
 * based model which uses two "roughness" parameters to describe
 * the shape and distribution of micro-facets on the surface.
 */

uniform int SHADOWMAP;
varying vec4 shadowCoord;

uniform sampler2D SAMPLER_SHADOW0;


float chebyshevUpperBound(vec4 shadow)
{
	//if (shadow.x < 0.0 || shadow.x > 1.0 ||shadow.x < 0.0 || shadow.x > 1.0){
	//	return 0.0;
	//}

	//shadow.z += 0.001;

	vec2 moments = texture2D(SAMPLER_SHADOW0, shadow.xy).rg;

	float dx = dFdx(moments.r);
	float dy = dFdy(moments.r);
	float grad = 10000.0 * pow((dx*dx + dy*dy), 1.0);
	//float grad = 100000.0 * pow((dx*dx + dy*dy), 0.5);
	//return grad;


	//if (abs(dFdx(moments.g)) > 0.001 || abs(dFdy(moments.g)) > 0.001)
	//	return 1.0;
	
	// Surface is fully lit. as the current fragment is before the light occluder
	//if (shadow.z <= moments.x || grad > 0.5)
	if (shadow.z <= moments.x)
		return 1.0 ;

	//	return 0.0;


	// The fragment is either in shadow or penumbra. We now use chebyshev's upperBound to check
	// How likely this pixel is to be lit (p_max)
	float variance = moments.y - (moments.x * moments.x);
	//float variance = moments.y;
	variance = max(variance, 0.0002);
	//variance = max(variance, grad);

	float d = shadow.z - moments.x;
	float p_max = variance / (variance + d * d);

	//return moments.x;
	//return moments.y;
	return p_max;
}


/** @todo does not compile on my ati x600 yet */
vec3 LightContribution(in gl_LightSourceParameters lightSource, in vec3 lightDir, in vec3 N, in vec3 V, float NdotV, float R_2, in vec4 roughness, in vec4 specular, in vec4 diffuse){

	/* calculate light attenuation due to distance (do this first so we can return early if possible) */
	/* @todo this assumes all lights are point sources; it should respect the gl_LightSource 
	 * settings for spot-light sources. */
	float attenuate = lightSource.constantAttenuation;
	if (attenuate > 0.0 && lightSource.position.w != 0.0){ /* directional sources don't get attenuated */
		float dist = length((lightSource.position).xyz - point);
		attenuate = 1.0 / (lightSource.constantAttenuation + 
				lightSource.linearAttenuation * dist +
				lightSource.quadraticAttenuation * dist * dist); 
	}

	/* if we're out of range, ignore the light; else calculate its contribution */
	if(attenuate < ATTENUATE_THRESH) {
		return vec3(0.0);
	}

	vec3 ambientColor = diffuse.rgb * diffuse.a * lightSource.ambient.rgb;
	/* Normalize vectors and cache dot products */
	vec3 L = normalize(lightDir);
	float NdotL = clamp(dot(N, -L), 0.0, 1.0);

#if r_debug_shadows
	vec4 shadowCoordDivW = shadowCoord / shadowCoord.w;
	//if (abs(NdotL) < 0.001)
	//	return vec3(0.0, 0.0, 1.0);
	return vec3(chebyshevUpperBound(shadowCoordDivW));
#endif

	float shadow = 1.0;
#if r_shadowmapping
	if (SHADOWMAP > 0) {
		vec4 shadowCoordDivW = shadowCoord / shadowCoord.w;
		shadow = chebyshevUpperBound(shadowCoordDivW);
		/* if the fragment is completely shadowed, we don't need 
		 * to calculate anything but ambient */
		if (shadow < ATTENUATE_THRESH) {
			return (0.2 * attenuate * ambientColor);
		}
	}
#endif


	/* Compute the final color contribution of the light */
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
	return (attenuate * (max(shadow, 0.2) * max(ambientColor, 0.0) + shadow * max(diffuseColor, 0.0) + shadow * max(specularColor, 0.0)));
}


vec4 IlluminateFragment(void){

	vec3 totalColor= vec3(0.0);

	/* sample the relevant textures */
	vec2 coords = gl_TexCoord[0].st;
	vec2 offset = vec2(0.0);

	/* do per-fragment calculations */
	vec3 V = -normalize(eyedir);
	vec3 N;
	if (BUMPMAP > 0) {
#if r_bumpmap
		vec4 normalMap = texture2D(SAMPLER_NORMALMAP, coords);
		N = vec3((normalize(normalMap.xyz) * 2.0 - vec3(1.0)));
		N.xy *= BUMP;
		if (PARALLAX > 0.0){
			offset = BumpTexcoord(normalMap.a);
			coords += offset;
		}
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
	totalColor += LightContribution(gl_LightSource[$], lightDirs[$], N, V, NdotV, R_2, roughness, specular, diffuse);
#endunroll

	return vec4(totalColor, diffuse.a);
}

