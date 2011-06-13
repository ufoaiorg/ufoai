/**
 * @file atmosphere_fs.glsl
 * @brief Atmosphere fragment shader.
 */

/*
 * Indicates that gl_FragData is written to, not gl_FragColor.
 * #extension needs to be placed before all non preprocessor code.
 */
#extension GL_ARB_draw_buffers : enable

#ifndef glsl110
	/** Linkage into a shader from a previous stage, variable is copied in.*/
	#define in_qualifier in
#else
	/** Deprecated after glsl110; linkage between a vertex shader and a fragment shader for interpolated data.*/
	#define in_qualifier varying
#endif

in_qualifier vec2 tex;

in_qualifier vec4 ambientLight;
in_qualifier vec4 diffuseLight;
in_qualifier vec4 specularLight;

in_qualifier vec3 lightVec;
in_qualifier vec3 eyeVec;

#ifndef glsl110
	/** After glsl1110 this need to be explicitly declared; used by fixed functionality at the end of the OpenGL pipeline.*/
	out vec4 gl_FragData[2];
#endif

/** Diffuse.*/
uniform sampler2D SAMPLER0;
/** Normalmap.*/
uniform sampler2D SAMPLER2;

uniform float GLOWSCALE;
uniform vec4 DEFAULTCOLOR;

const float specularExp = 32.0;

/**
 * @brief Fresnel's equations for reflection and refraction between different density media.
 */
void fresnelRefract(vec3 L, vec3 N, float n1, float n2,
	out vec3 reflection, out vec3 refraction,
	out float reflectance, out float transmittance) {

	float eta = n1/n2;
	float cos_theta1 = dot(L, N);
	float cos_theta2 = sqrt(1.0 - ((eta * eta) * ( 1.0 - (cos_theta1 * cos_theta1))));
	reflection = L - 2.0 * cos_theta1 * N;
	refraction = (eta * L) + (cos_theta2 - eta * cos_theta1) * N;
	float rs = (n1 * cos_theta1 - n2 * cos_theta2 ) / (n1 * cos_theta1 + n2 * cos_theta2);
	float rp = (n1 * cos_theta2 - n2 * cos_theta1 ) / (n1 * cos_theta2 + n2 * cos_theta1);
	reflectance = (rs * rs + rp * rp) / 2.0;
	transmittance = ((1.0-rs) * (1.0-rs) + (1.0-rp) * (1.0-rp)) / 2.0;
}

void main() {
	vec4 diffuseColor = texture2D(SAMPLER0, tex);
	vec3 V = vec3(normalize(eyeVec).rgb);
	vec3 L = vec3(normalize(lightVec).rgb);
	vec3 N = vec3(normalize(texture2D(SAMPLER2, tex).rgb * 2.0 - 1.0).rgb);
	/* calculate reflections/refractions */
	vec3 Rvec;
	vec3 Tvec;
	float R;
	float T;
	fresnelRefract(L, N, 1.0, 1.133, Rvec, Tvec, R, T);

	float RdotV = clamp(R * dot(Rvec, -V), 0.0, 1.0);
	float TdotV = clamp(T * ((dot(Tvec, -V) + 1.0) / 2.0), 0.0, 1.0);
	float MTdotV = clamp(T * ((dot(-Tvec, -V) + 1.0) / 2.0), 0.0, 1.0);
	float NdotL = clamp(((dot(N, L) + 1.0) / 2.0), 0.0, 1.0);
	float LNdotV = clamp(dot(reflect(-L, N), V), 0.0, 1.0);
	float VdotL = clamp(((dot(L, -V) + 1.0) / 2.0), 0.0, 1.0);

	vec4 ambient = vec4(0.05, 0.05, 0.05, 0.05);

	/* calculate reflections */
	vec4 reflectColor = diffuseColor * (ambient + pow(NdotL, 4.0) * (TdotV + MTdotV) + 0.2 * pow(VdotL, 16.0));

	float d = clamp(pow(1.0 + dot(V, L), 0.4), 0.0, 1.0);
	vec4 specularColor = d * RdotV * pow(LNdotV, specularExp) * specularLight;

	vec4 hdrColor = GLOWSCALE * (0.4 * reflectColor + 1.0 * specularColor);
	hdrColor.a = 1.0;

	/* calculate final color */
	gl_FragData[0] = DEFAULTCOLOR;
	gl_FragData[1] = hdrColor;
}
