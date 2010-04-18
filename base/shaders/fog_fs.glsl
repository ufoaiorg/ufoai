// fog fragment shader

varying float fog;


/*
 * FogFragment
 */
void FogFragment(void){

	gl_FragData[0].rgb = mix(gl_FragData[0].rgb, gl_Fog.color.rgb, fog);
}

