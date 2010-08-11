uniform sampler2D SAMPLER0;
uniform sampler2D SAMPLER1;

void main(void){
	vec4 color1 = texture2D(SAMPLER0, gl_TexCoord[0].st);
	vec4 color2 = texture2D(SAMPLER1, gl_TexCoord[0].st);
	gl_FragColor.rgb = (color1.rgb * color1.a) + (color2.rgb * color2.a);
	gl_FragColor.a = color1.a + color2.a;
}
