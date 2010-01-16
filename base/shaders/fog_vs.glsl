// fog vertex shader

varying float fog;


/*
 * FogVertex
 */
void FogVertex(void){

	// calculate interpolated fog depth
	fog = (gl_Position.z - gl_Fog.start) / (gl_Fog.end - gl_Fog.start);
	fog = clamp(fog, 0.0, 1.0) * gl_Fog.density;
}
