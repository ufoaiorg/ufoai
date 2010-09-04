// fog fragment shader

varying float fog;


/*
 * FogFragment
 */
vec4 FogFragment(vec4 inColor){

	return vec4(mix(inColor.rgb, gl_Fog.color.rgb, fog), inColor.a);
}
