varying in vec2 tex;

varying in vec4 ambientLight;
varying in vec4 diffuseLight;
varying in vec4 specularLight;

varying in vec3 lightVec;
varying in vec3 eyeVec;

/* diffuse */
uniform sampler2D SAMPLER0;
/* normalmap */
uniform sampler2D SAMPLER2;

uniform float GLOWSCALE;
uniform vec4 DEFAULTCOLOR;

const float specularExp = 32.0;

/* Fresnel's equations for reflection and refraction between different density media */
void fresnelRefract(in vec3 L, in vec3 N, in float n1, in float n2, 
	out vec3 reflection, out vec3 refraction, 
	out float reflectance, out float transmittance) 
{
	float eta = n1/n2;
	float cos_theta1 = dot(L, N);
	float cos_theta2 = sqrt(1.0 - ((eta * eta) * ( 1.0 - (cos_theta1 * cos_theta1))));
	reflection = L - 2.0 * cos_theta1 * N;
	refraction = (eta * L) + (cos_theta2 - eta * cos_theta1) * N;
	float rs = (n1 * cos_theta1 - n2 * cos_theta2 ) / (n1 * cos_theta1 + n2 * cos_theta2);
	float rp = (n1 * cos_theta2 - n2 * cos_theta1 ) / (n1 * cos_theta2 + n2 * cos_theta1);
	reflectance = (rs * rs + rp * rp) / 2.0;
	transmittance =((1.0-rs) * (1.0-rs) + (1.0-rp) * (1.0-rp)) / 2.0;
}

void main()
{
	vec4 diffuseColor = texture2D(SAMPLER0, tex);
	vec3 V = normalize(eyeVec);
	vec3 L = normalize(lightVec);
	vec3 N = normalize(texture2D(SAMPLER2, tex).rgb * 2.0 - 1.0);
	/* calculate reflections/refractions */
	vec3 Rvec;
	vec3 Tvec;
	float R;
	float T;
	fresnelRefract(L, N, 1.0, 1.133, Rvec, Tvec, R, T);

	float RdotV = clamp(R * dot(Rvec, -V), 0.0, 1.0);
	float TdotV = clamp(T * ((dot(Tvec, -V) + 1.0) / 2.0), 0.0, 1.0);
	float NdotL = clamp(((dot(N, L) + 1.0) / 2.0), 0.0, 1.0);
	float LNdotV = clamp(dot(reflect(-L, N), V), 0.0, 1.0);
	float VdotL = clamp(((dot(L, -V) + 1.0) / 2.0), 0.0, 1.0);

	vec4 ambient = {0.05, 0.05, 0.05, 0.05};

	/* calculate reflections */
	vec4 reflectColor = diffuseColor * (ambient + pow(NdotL, 4.0) * TdotV + 0.2 * pow(VdotL, 16.0));

	float d = clamp(pow(1.0 + dot(V, L), 0.4), 0.0, 1.0);
	vec4 specularColor = d * RdotV * pow(LNdotV, specularExp) * specularLight;

	vec4 color = DEFAULTCOLOR;
	vec4 hdrColor = GLOWSCALE * (0.5 * reflectColor + 1.0 * specularColor);
	hdrColor.a = 1.0;

	/* calculate final color */
	gl_FragData[0] = color;
	gl_FragData[1] = hdrColor;
}
