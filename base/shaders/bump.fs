// bumpmap fragment shader

varying vec3 eyedir;

uniform float SPECULAR;

/*
BumpFragment
*/
void BumpFragment(in vec3 deluxemap, in vec3 normalmap){

	float diffuse = dot(deluxemap, normalmap);

	float spec = max(-dot(eyedir, reflect(deluxemap, normalmap)), 0.0);

	float bump = diffuse + clamp(pow(spec, SPECULAR), 0.0, 8.0);

	gl_FragColor.rgb = gl_FragColor.rgb * vec3(bump, bump, bump);
}
