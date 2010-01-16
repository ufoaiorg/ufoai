// fog fragment shader

varying float fog;


/*
 * FogFragment
 */
void FogFragment(void){

	gl_FragColor.rgb = mix(gl_FragColor.rgb, gl_Fog.color.rgb, fog);
}

