//#version 130
/* shader for building variance shadowmaps */
uniform float OFFSET;
uniform int ANIMATE;
uniform float TIME;

in vec3 NEXT_FRAME_VERTS;

#ifndef ATI
in vec4 gl_MultiTexCoord0;
#endif

out vec4 vPosScreen;
out vec4 texCoord;


void main (void)
{
	float lerp = (1.0 - TIME) * float(ANIMATE);
	vec4 Vertex = mix(gl_Vertex, vec4(NEXT_FRAME_VERTS, 1.0), lerp);

	/* transform vertex info to screen and camera coordinates */
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * Vertex;
	vPosScreen = gl_Position;

	texCoord = gl_MultiTexCoord0 + OFFSET;
}
