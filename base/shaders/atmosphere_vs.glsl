varying vec2 tex;

varying vec4 ambientLight;
varying vec4 diffuseLight;
varying vec4 specularLight;

varying vec3 lightVec;
varying vec3 eyeVec;

uniform vec2 UVSCALE;

void main(){

	gl_Position = ftransform();
	tex = gl_MultiTexCoord0.xy * UVSCALE;

	vec4 lightPos = gl_LightSource[0].position;
	ambientLight = gl_LightSource[0].ambient;
	diffuseLight = gl_LightSource[0].diffuse;
	specularLight = gl_LightSource[0].specular;

	vec3 t, b, n;
	n = normalize(gl_NormalMatrix * gl_Normal);
	t = normalize(cross(n, vec3(1.0, 0.0, 0.0)));
	b = normalize(cross(t, n));

	lightVec.x = dot(lightPos.rgb, t);
	lightVec.y = dot(lightPos.rgb, b);
	lightVec.z = dot(lightPos.rgb, n);

	/* estimate view vector (orthographic projection means we don't really have one) */
	vec4 view = vec4(0.0, 0.0, 100.0, 1.0);
	eyeVec.x = dot(view.xyz, t);
	eyeVec.y = dot(view.xyz, b);
	eyeVec.z = dot(view.xyz, n);
}
