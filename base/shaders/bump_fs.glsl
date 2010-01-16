// bumpmap fragment shader

varying vec3 eyedir;

uniform float BUMP;
uniform float PARALLAX;
uniform float HARDNESS;
uniform float SPECULAR;

vec3 eye;


/*
 * BumpTexcoord
 */
vec2 BumpTexcoord(in float height){

	eye = normalize(eyedir);

	return vec2(height * PARALLAX * 0.04 - 0.02) * eye.xy;
}


/*
 *BumpFragment
 */
vec3 BumpFragment(in vec3 deluxemap, in vec3 normalmap){

	float diffuse = dot(deluxemap,
			vec3(normalmap.x * BUMP, normalmap.y * BUMP, normalmap.z));

	float specular = HARDNESS * pow(max(-dot(eye,
			reflect(deluxemap, normalmap)), 0.0), 8.0 * SPECULAR);

	return vec3(diffuse + specular);
}
