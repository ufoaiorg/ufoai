

uniform int SHADOWMAP;
//uniform mat4 SHADOW_TRANSFORM;
varying vec4 shadowCoord;

void ShadowVertex (void)
{
	//shadowCoord = gl_TextureMatrix[7] *  Vertex;
	//shadowCoord = gl_ModelViewMatrix * Vertex;
	//shadowCoord.a *= 1000.0;
	//shadowCoord = gl_TextureMatrix[7] * vertPos;
	shadowCoord = gl_TextureMatrix[7] * gl_ModelViewMatrix * Vertex;
}
