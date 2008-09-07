// bumpmap fragment shader

varying vec3 eyedir;


/*
BumpFragment
*/
void BumpFragment(in vec3 deluxemap, in vec3 normalmap){

	float diffuse = dot(deluxemap, normalmap);

	float spec = max(-dot(eyedir, reflect(deluxemap, normalmap)), 0.0);

	gl_FragColor = gl_FragColor * diffuse + pow(spec, 6.0) * 0.001;
}
