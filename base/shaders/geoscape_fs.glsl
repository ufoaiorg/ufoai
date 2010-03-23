varying vec2 tex;
varying vec4 vertPos;
varying vec3 normal;
varying vec3 tangent;
varying vec3 binormal;

varying vec4 lightPos;
varying vec4 ambientLight;
varying vec4 diffuseLight;
varying vec4 specularLight;

varying vec4 lightPos2;
varying vec4 ambientLight2;
varying vec4 diffuseLight2;
varying vec4 specularLight2;

/* normal map */
uniform sampler2D SAMPLER0;
/* diffuse map (texture) */
uniform sampler2D SAMPLER1;
/* bump map */
uniform sampler2D SAMPLER2;
/* gloss map */
uniform sampler2D SAMPLER3;
/* night map */
uniform sampler2D SAMPLER4;

uniform float specularExp;
uniform float blendScale;
uniform vec4 defaultColor;
uniform vec3 viewVec;

void main()
{
	/* blend textures smoothly */
	vec4 diffuseColorA = texture2D(SAMPLER1, tex);
	vec4 diffuseColorB = texture2D(SAMPLER2, tex);
	vec4 diffuseColor = ((1.0 - blendScale) * diffuseColorA) + (blendScale * diffuseColorB);

	/* calculate diffuse reflections */
	vec3 N = texture2D(SAMPLER0, tex).rgb * 2.0 - 1.0;
	N = normalize(N.x * tangent + N.y * binormal + N.z * normal);
	float NdotL = clamp(dot(N, normalize(lightPos)), 0.0, 1.0);
	vec4 color = diffuseColor * diffuseLight * NdotL;

	/* calculate specular reflections */
	vec3 V = -normalize(vec3(vertPos.rgb) - viewVec);
	vec3 L = normalize(vec3(lightPos.rgb));
	vec3 R = reflect(N, V);
	float RdotL = clamp(dot(-R, L), 0.0, 1.0);
	vec4 gloss = texture2D(SAMPLER3, tex);
	color += gloss * specularLight * pow(RdotL, specularExp);

	/* calculate night illumination */
	vec4 diffuseNightColor = texture2D(SAMPLER4, tex);
	float NdotL2 = clamp(dot(N, normalize(lightPos2)), 0.0, 1.0);
	vec4 nightColor = diffuseNightColor * diffuseLight2 * NdotL2;

	/* calculate final color */
	gl_FragColor = defaultColor + (ambientLight * diffuseColor) + color + nightColor; 
}
