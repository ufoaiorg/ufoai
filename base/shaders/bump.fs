// bumpmap fragment shader

varying vec3 eyedir;

uniform float BUMP;
uniform float SPECULAR;

/*
BumpFragment
*/
void BumpFragment(in vec3 deluxemap, in vec3 normalmap){

	float diffuse = dot(deluxemap, normalmap) / BUMP;

	float spec = max(-dot(eyedir, reflect(deluxemap, normalmap)), 0.0);

	float bump = diffuse + pow(spec, SPECULAR * 4.0);

	gl_FragColor.rgb = gl_FragColor.rgb * vec3(bump, bump, bump);
}
