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

uniform vec2 uvscale;

void main()
{
	gl_Position = ftransform();
	tex = gl_MultiTexCoord0.xy * uvscale;
	vert_pos = gl_ModelViewMatrix * gl_Vertex;

	light_pos =  gl_LightSource[0].position;
	ambientLight = gl_LightSource[0].ambient;
	diffuseLight = gl_LightSource[0].diffuse;
	specularLight = gl_LightSource[0].specular;

	light_pos2 =  gl_LightSource[1].position;
	ambientLight2 = gl_LightSource[1].ambient;
	diffuseLight2 = gl_LightSource[1].diffuse;
	specularLight2 = gl_LightSource[1].specular;

	tangent  = gl_NormalMatrix * gl_MultiTexCoord3.xyz;
	binormal = gl_NormalMatrix * gl_MultiTexCoord4.xyz;
	normal   = gl_NormalMatrix * gl_Normal;
}

