/* NOTE: portions loosly based on D3D9 source from Jack Hoxley */

#define ATTENUATE_THRESH 0.05 

/**
 * The Cook-Torrance model for light reflection is a physics
 * based model which uses two "roughness" parameters to describe
 * the shape and distribution of micro-facets on the surface.
 */

uniform int SHADOWMAP;
varying vec4 shadowCoord;
varying vec4 Vertex;

//uniform sampler2D SAMPLER_SHADOW0;
uniform sampler2DShadow SAMPLER_SHADOW0;

const float xPixelOffset = 1.0 / 1024.0;
const float yPixelOffset = 1.0 / 768.0;

#define SAMPLES 16
#define INV_SAMPLES 1.0 / 16.0
vec2 poissondisk[SAMPLES] = vec2[SAMPLES]( 
		vec2(0.170017, 0.949476), 
		vec2(-0.856682, 0.834597), 
		vec2(0.884173, 0.407126), 
		vec2(0.564079, -0.647807), 
		vec2(-0.187066, -0.481096), 
		vec2(-0.737872, 0.151191), 
		vec2(0.301019, 0.258782), 
		vec2(-0.344032, 0.516536), 
		vec2(0.872586, 0.953879), 
		vec2(-0.958830, -0.575161), 
		vec2(-0.119198, -0.943774), 
		vec2(-0.215994, 0.034261), 
		vec2(0.902534, -0.281495), 
		vec2(0.909580, -0.972526), 
		vec2(-0.591030, -0.939582), 
		vec2(0.236135, -0.222607) );


float lookup( vec2 offSet)
{
	return shadow2DProj(SAMPLER_SHADOW0, shadowCoord + vec4(offSet.x * xPixelOffset * shadowCoord.w, offSet.y * yPixelOffset * shadowCoord.w, 0.05, 0.0) ).r;
}



float chebyshevUpperBound(vec4 shadow)
{
	//vec2 moments = texture2D(SAMPLER_SHADOW0, shadow.xy).rg;
	vec3 moments = shadow2DProj(SAMPLER_SHADOW0, shadow).rgb;
	//vec2 moments = vec2(0.0);

	//float shadowZ = (shadow.z / shadow.w);
	float shadowZ = (shadow.z / shadow.w) - 0.05;

	float dx = dFdx(moments.r);
	float dy = dFdy(moments.r);
	float grad = 100.0 * pow((dx*dx + dy*dy), 1.0);
	//float grad = 100000.0 * pow((dx*dx + dy*dy), 0.5);
	//return grad;

	//shadowZ -= 1.0*(dx*dx+dy*dy);


	//if (abs(dFdx(moments.g)) > 0.001 || abs(dFdy(moments.g)) > 0.001)
	//	return 1.0;
	
	// Surface is fully lit. as the current fragment is before the light occluder
	//if (shadow.z <= moments.x || grad > 0.5)
	if (shadowZ <= moments.x)
		return 1.0;

	//	return 0.0;


	// The fragment is either in shadow or penumbra. We now use chebyshev's upperBound to check
	// How likely this pixel is to be lit (p_max)
	float variance = moments.y - (moments.x * moments.x);
	//float variance = moments.y;
	variance = max(variance, 0.000002);
	//variance = max(variance, grad);

	float d = shadowZ - moments.x;
	float p_max = variance / (variance + d * d);

	//return moments.x;
	//return moments.y;
	return p_max * moments.z;
}


/** @todo does not compile on my ati x600 yet */
vec3 LightContribution(in gl_LightSourceParameters lightSource, in vec3 lightDir, in vec3 N, in vec3 V, float NdotV, float R_2, in vec4 roughness, in vec4 specular, in vec4 diffuse){

	/* calculate light attenuation due to distance (do this first so we can return early if possible) */
	/* @todo this assumes all lights are point sources; it should respect the gl_LightSource 
	 * settings for spot-light sources. */
	float attenuate = lightSource.constantAttenuation;

//#ifdef ATI
	/* HACK - for some reason, ATI cards return 0.0 for attenuation for directional sources */
	if (lightSource.position.w == 0.0) {
		attenuate = 1.0;
	}
//#endif

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
	//vec4 shadowCoordDivW = shadowCoord / shadowCoord.w;
	//if (abs(NdotL) < 0.001)
	//	return vec3(0.0, 0.0, 1.0);
	//return vec3(chebyshevUpperBound(shadowCoordDivW));

	//return vec3(shadow2DProj(SAMPLER_SHADOW0, shadowCoord).r);
	//return vec3( (lookup(vec2(0.0, 0.0)) );
	//return vec3(1.0 - (  (shadowCoord.z / shadowCoord.w) - (lookup(vec2(0.0, 0.0))) ));
	return vec3(chebyshevUpperBound(shadowCoord));
#endif

	float shadow = 1.0;
#if r_shadowmapping
	if (SHADOWMAP > 0) {
		//shadow = chebyshevUpperBound(shadowCoord2);
		//shadow = chebyshevUpperBound(shadowCoord);
		//return vec3(shadow);

		//vec4 shadowCoordDivW = shadowCoord / shadowCoord.w;
		//shadow = chebyshevUpperBound(shadowCoordDivW);
		/* if the fragment is completely shadowed, we don't need 
		 * to calculate anything but ambient */
		//if (shadow < ATTENUATE_THRESH) {
		//	return (0.2 * attenuate * ambientColor);
		//}

		shadow = 0.0;
		/*
		float x,y;
		for (y = -3.5 ; y <=3.5 ; y+=1.0) {
			for (x = -3.5 ; x <=3.5 ; x+=1.0) {
				//vec4 sampleCoord = shadowCoord + vec4(x * xPixelOffset * shadowCoord.w, y * yPixelOffset * shadowCoord.w, 0.0, 0.0);
				vec4 sampleCoord = shadowCoord + vec4(x * xPixelOffset * shadowCoord.w, y * yPixelOffset * shadowCoord.w, 0.0, 0.0);
				shadow += chebyshevUpperBound(sampleCoord);
				//shadow += ( shadowCoordDivW.z - lookup(vec2(x,y)) > 0.05 ? 0.0 : 1.0 ) ;
				//shadow += lookup(vec2(x,y));
			}
		}
		shadow /= 64.0 ;
		*/


		int i;
		for (i = 0; i < SAMPLES; i++) {
				vec4 sampleCoord = shadowCoord + vec4(poissondisk[i].x * xPixelOffset * shadowCoord.w, poissondisk[i].y * yPixelOffset * shadowCoord.w, 0.0, 0.0);
				shadow += chebyshevUpperBound(sampleCoord);
		}
		shadow *= INV_SAMPLES;

		//return vec3( shadow );
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

