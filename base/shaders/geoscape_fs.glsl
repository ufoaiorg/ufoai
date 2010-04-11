varying in vec2 tex;

varying in vec4 ambientLight;
varying in vec4 diffuseLight;
varying in vec4 specularLight;
varying in vec4 diffuseLight2;

varying in vec3 lightVec;
varying in vec3 lightVec2;
varying in vec3 eyeVec;

/* diffuse */
uniform sampler2D SAMPLER0;
/* blend */
uniform sampler2D SAMPLER1;
/* normalmap */
uniform sampler2D SAMPLER2;

uniform float BLENDSCALE;
uniform float GLOWSCALE;
uniform vec4 DEFAULTCOLOR;
uniform vec4 CITYLIGHTCOLOR;

void main()
{
	/* blend textures smoothly */
	vec3 diffuseColorA = texture2D(SAMPLER0, tex).rgb;
	vec3 diffuseColorB = texture2D(SAMPLER1, tex).rgb;
	vec4 diffuseColor;
	diffuseColor.rgb = ((1.0 - BLENDSCALE) * diffuseColorA) + (BLENDSCALE * diffuseColorB);
	diffuseColor.a = 1.0;

	/* calculate diffuse reflections */
	vec3 V = normalize(eyeVec);
	vec3 L = normalize(lightVec);
	vec3 N = normalize(texture2D(SAMPLER2, tex).rgb * 2.0 - 1.0);
	float NdotL = clamp(dot(N, L), 0.0, 1.0);
	vec4 reflectColor = diffuseColor * diffuseLight * NdotL;
	
	/* calculate specular reflections */
	float RdotL = clamp(dot(reflect(-L, N), V), 0.0, 1.0);
	float gloss = texture2D(SAMPLER2, tex).a;

	const float specularExp = 32.0;
	vec4 specularColor = specularLight * gloss * pow(RdotL, specularExp);

	/* calculate night illumination */
	float diffuseNightColor = texture2D(SAMPLER0, tex).a;
	float NdotL2 = clamp(dot(N, normalize(lightVec2)), 0.0, 1.0);
	vec4 nightColor = diffuseLight2 * CITYLIGHTCOLOR * diffuseNightColor * NdotL2;

	vec4 color = DEFAULTCOLOR + (ambientLight * diffuseColor) + reflectColor + nightColor; 
	vec4 hdrColor = GLOWSCALE * (
					clamp((reflectColor - vec4(0.9, 0.9, 0.9, 0)), 0.0, GLOWSCALE) +
					//clamp((0.4 * specularColor), 0.0, 1.0) +
					clamp((nightColor ), 0.0, 1.0));
	hdrColor.a = 1.0;

	/* calculate final color */
	gl_FragData[0] = color;
	gl_FragData[1] = hdrColor;
}
