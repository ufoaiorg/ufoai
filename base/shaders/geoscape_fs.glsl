varying vec2 tex;
varying vec4 vert_pos;
varying vec3 normal;
varying vec3 tangent;
varying vec3 binormal;

varying vec4 light_pos;
varying vec4 ambientLight;
varying vec4 diffuseLight;
varying vec4 specularLight;

varying vec4 light_pos2;
varying vec4 ambientLight2;
varying vec4 diffuseLight2;
varying vec4 specularLight2;

uniform sampler2D sNormal;
uniform sampler2D sDiffuse;
uniform sampler2D sDiffuseB;\n
uniform sampler2D sGloss;\n
uniform sampler2D sNight;\n

uniform float specular_exp;
uniform float blend_scale;
uniform vec2 uvscale;
uniform vec4 default_color;
uniform vec3 viewVec;

void main()
{
	/* blend textures smoothly */
	vec4 diffuseColorA = texture2D(sDiffuse, tex);
	vec4 diffuseColorB = texture2D(sDiffuseB, tex);
	vec4 diffuseColor = ((1.0 - blend_scale) * diffuseColorA) + (blend_scale * diffuseColorB);
	/* calculate diffuse reflections */
	vec3 N = texture2D(sNormal, tex).rgb * 2.0 - 1.0;
	N = normalize(N.x * tangent + N.y * binormal + N.z * normal);
	float NdotL = clamp(dot(N, normalize(light_pos)), 0.0, 1.0);
	vec4 color = diffuseColor * diffuseLight * NdotL;
	/* calculate specular reflections */
	vec3 V = -normalize(vec3(vert_pos.rgb) - viewVec);
	vec3 L = normalize(vec3(light_pos.rgb) );
	vec3 R = reflect(N, V);
	float RdotL = clamp(dot(-R, L), 0.0, 1.0);
	vec4 gloss = texture2D(sGloss, tex);
	color += gloss * specularLight * pow(RdotL, specular_exp);
	/* calculate night illumination */
	vec4 diffuseNightColor = texture2D(sNight, tex);
	float NdotL2 = clamp(dot(N, normalize(light_pos2)), 0.0, 1.0);
	vec4 nightColor = diffuseNightColor * diffuseLight2 * NdotL2;
	/* calculate final color */
	gl_FragColor = default_color + (ambientLight * diffuseColor) + color + nightColor; 
}

