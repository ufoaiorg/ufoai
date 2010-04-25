// fog fragment shader

varying float fog;


/*
 * FogFragment
 */
void FogFragment(void){

#if r_postprocess
	gl_FragData[0].rgb = mix(gl_FragData[0].rgb, gl_Fog.color.rgb, fog);
#else
	gl_FragColor.rgb = mix(gl_FragData[0].rgb, gl_Fog.color.rgb, fog);
#endif
}

