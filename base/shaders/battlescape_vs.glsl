#version 130
/* battlescape vertex shader */

struct gl_LightSourceParameters {
	vec4	ambient;
	vec4	diffuse;
	vec4	specular;
	vec4	position;
	vec4	halfVector;
	vec3	spotDirection;
	float	spotExponent;
	float	spotCutoff;
	float	spotCosCutoff;
	float	constentAttenuation;
	float	linearAttenuation;
	float	quadraticAttenuation;
};

uniform gl_LightSourceParameters gl_LightSource[#replace r_dynamic_lights ];

struct gl_FogParameters {
	vec4 color;
	float density;
	float start;
	float end;
	float scale;
};
uniform gl_FogParameters gl_Fog;

uniform float OFFSET;
uniform int BUMPMAP;
uniform int ANIMATE;
uniform float TIME;
uniform mat4 SHADOW_TRANSFORM;

in vec3 NEXT_FRAME_VERTS;
in vec3 NEXT_FRAME_NORMALS;
in vec4 TANGENTS;
in vec4 NEXT_FRAME_TANGENTS;

in vec4 gl_Vertex;
in vec3 gl_Normal;
uniform mat4 gl_ModelViewMatrix;
uniform mat4 gl_ProjectionMatrix;
uniform mat3 gl_NormalMatrix;
uniform mat4 gl_TextureMatrix[];

in vec4 gl_MultiTexCoord0;
in vec4 gl_MultiTexCoord1;
//uniform gl_LightSourceParameters gl_LightSource[];
//uniform gl_FogParameters gl_Fog;

//out vec4 Vertex;
//out vec4 Normal;
//out vec4 Tangent;
out vec4 vPosScreen;
out vec4 vPos;
out vec3 vNormal;
out vec3 eyedir;
out vec3 lightDirs[#replace r_dynamic_lights ];
out vec4 shadowCoord;
out float fog;
/* @todo replace static value */
out vec4 gl_TexCoord[8];

/**
 * main
 */
void main (void) {

	/* lerp vertex info for animated models */
	/*
	if (ANIMATE > 0) {
		Vertex = mix(vec4(NEXT_FRAME_VERTS, 1.0), gl_Vertex, TIME);
		Normal = mix(NEXT_FRAME_NORMALS, gl_Normal, TIME);
		Tangent = mix(NEXT_FRAME_TANGENTS, TANGENTS, TIME);
	} else {
		Vertex = gl_Vertex;
		Normal = gl_Normal;
		Tangent = TANGENTS;
	}
	*/

	//float lerp = TIME;
	//vec4 Vertex = mix(vec4(NEXT_FRAME_VERTS, 1.0), gl_Vertex, lerp);
	//vec3 Normal = mix(NEXT_FRAME_NORMALS, gl_Normal, lerp);
	//vec4 Tangent = mix(NEXT_FRAME_TANGENTS, TANGENTS, lerp);

	float lerp = (1.0 - TIME) * float(ANIMATE);
	vec4 Vertex = mix(gl_Vertex, vec4(NEXT_FRAME_VERTS, 1.0), lerp);
	vec3 Normal = mix(gl_Normal, NEXT_FRAME_NORMALS, lerp);
	vec4 Tangent = mix(TANGENTS, NEXT_FRAME_TANGENTS, lerp);


	/* transform vertex info to screen and camera coordinates */
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * Vertex;
	vPosScreen = gl_Position;
	vPos = gl_ModelViewMatrix * Vertex;
	vNormal = normalize(gl_NormalMatrix * Normal);


	/* pass texcoords and color through */
	gl_TexCoord[0] = gl_MultiTexCoord0 + OFFSET;
	gl_TexCoord[1] = gl_MultiTexCoord1 + OFFSET;
	//gl_FrontColor = gl_Color;


#if r_shadowmapping
	/* calculate vertex projection onto the shadowmap */ 
	shadowCoord = gl_TextureMatrix[7] * gl_ModelViewMatrix * Vertex;
#endif

#if r_fog
	/* calculate interpolated fog depth */
	//fog = (gl_Position.z - gl_Fog.start) / (gl_Fog.end - gl_Fog.start);
	//fog = clamp(fog, 0.0, 1.0) * gl_Fog.density;
#endif


#if r_bumpmap
	//if (BUMPMAP > 0) 
	{
		/* setup tangent space */
		vec3 tangent = normalize(gl_NormalMatrix * Tangent.xyz);
		vec3 bitangent = normalize(cross(vNormal, tangent)) * Tangent.w;

		/* transform the eye direction into tangent space */
		vec3 v;
		v.x = dot(-vPos.xyz, tangent);
		v.y = dot(-vPos.xyz, bitangent);
		v.z = dot(-vPos.xyz, vNormal);
		eyedir = normalize(v);

		/* transform relative light positions into tangent space */
#unroll r_dynamic_lights
		{
			vec3 lpos;
			if(gl_LightSource[$].position.a != 0.0) 
				lpos = normalize(gl_LightSource[$].position.xyz - vPos.xyz);
			else /* directional light source at "infinite" distance */
				lpos = normalize(gl_LightSource[$].position.xyz);

			lightDirs[$].x = dot(lpos, tangent);
			lightDirs[$].y = dot(lpos, bitangent);
			lightDirs[$].z = dot(lpos, vNormal);
		}
#endunroll
	}
#endif
}
