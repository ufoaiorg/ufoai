varying out vec2 tex;

varying out vec4 ambientLight;
varying out vec4 diffuseLight;
varying out vec4 specularLight;

varying out vec3 lightVec;
varying out vec3 eyeVec;

uniform vec2 UVSCALE;

void main()
{
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
	vec4 view = {0.0, 0.0, 100.0, 1.0};
	eyeVec.x = dot(view, t);
	eyeVec.y = dot(view, b);
	eyeVec.z = dot(view, n);
}
