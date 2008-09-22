// bumpmap fragment shader

varying vec3 eyedir;

uniform float BUMP;
uniform float SPECULAR;

/*
BumpFragment
*/
void BumpFragment(in vec3 deluxemap, in vec3 normalmap){

	float diffuse = dot(deluxemap, normalmap * vec3(BUMP));

	float spec = max(-dot(normalize(eyedir), reflect(deluxemap, normalmap * vec3(SPECULAR))), 0.0);

	float bump = diffuse + pow(spec, 4.0);

	gl_FragColor.rgb = gl_FragColor.rgb * vec3(bump);
}
