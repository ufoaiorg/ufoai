

uniform int SHADOWMAP;
//uniform mat4 SHADOW_TRANSFORM;
varying vec4 shadowCoord;

void ShadowVertex (void)
{
	mat4 inverseCameraView;
	vec4 inverseXlate;
	/*
	inverseCameraView[0][0] = gl_NormalMatrix[0][0];
	inverseCameraView[1][0] = gl_NormalMatrix[1][0];
	inverseCameraView[2][0] = gl_NormalMatrix[2][0];

	inverseCameraView[0][1] = gl_NormalMatrix[0][1];
	inverseCameraView[1][1] = gl_NormalMatrix[1][1];
	inverseCameraView[2][1] = gl_NormalMatrix[2][1];

	inverseCameraView[0][2] = gl_NormalMatrix[0][2];
	inverseCameraView[1][2] = gl_NormalMatrix[1][2];
	inverseCameraView[2][2] = gl_NormalMatrix[2][2];

	inverseCameraView[0][3] = 0.0;
	inverseCameraView[1][3] = 0.0;
	inverseCameraView[2][3] = 0.0;
	*/

	inverseCameraView[0] = vec4(gl_NormalMatrix[0], 0.0);
	inverseCameraView[1] = vec4(gl_NormalMatrix[1], 0.0);
	inverseCameraView[2] = vec4(gl_NormalMatrix[2], 0.0);

	inverseCameraView[3] = vec4(0.0, 0.0, 0.0, 1.0);

/*
	inverseCameraView[3][0] = -gl_ModelViewMatrix[3][0];
	inverseCameraView[3][1] = -gl_ModelViewMatrix[3][1];
	inverseCameraView[3][2] = -gl_ModelViewMatrix[3][2];
	inverseCameraView[3][3] = -gl_ModelViewMatrix[3][3];
	*/

	inverseXlate = -gl_ModelViewMatrix[3];

	//shadowCoord = transform * gl_Vertex;
	//shadowCoord = SHADOW_TRANSFORM * inverseCameraView * gl_Vertex;
	//shadowCoord = SHADOW_TRANSFORM * gl_Vertex;
	//shadowCoord = gl_TextureMatrix[7] * Vertex;
	shadowCoord = gl_TextureMatrix[7] * gl_ModelViewMatrix * Vertex;
}
