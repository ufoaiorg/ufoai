//#version 130
/* shader for building variance shadowmaps */

uniform sampler2D SAMPLER_DIFFUSE;
in vec4 vPosScreen;
in vec4 texCoord;

void main (void) 
{
	float alpha = texture2D(SAMPLER_DIFFUSE, texCoord.st).a;
	if (alpha <= 0.0) {
		discard;
	}

	float depth = 0.5 * (vPosScreen.z / vPosScreen.w) + 0.5;
	float moment2 = depth * depth;

	/* bias moments using derivative */
	float dx = dFdx(depth);
	float dy = dFdy(depth);
	/* @note - this causes artifacts; possibly each model being drawn separately leads to issues */
	//moment2 += 0.25*(dx*dx+dy*dy) ;

	/* we use the diffuse texture alpha so transparent objects don't cast shadows */
	gl_FragColor = vec4(depth, moment2, 0.0, alpha);
}
