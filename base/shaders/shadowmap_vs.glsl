

uniform int SHADOWMAP;
//uniform mat4 SHADOW_TRANSFORM;
varying vec4 shadowCoord;

void ShadowVertex (void)
{
	shadowCoord = gl_TextureMatrix[7] * gl_ModelViewMatrix * Vertex;
}
