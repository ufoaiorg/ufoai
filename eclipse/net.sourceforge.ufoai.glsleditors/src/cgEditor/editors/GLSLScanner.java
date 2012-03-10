/**
 *
 */
package cgEditor.editors;

/**
 * @author Martinez
 */
public class GLSLScanner extends ShaderFileScanner {
	static char escChar[] = { '\n', ' ', '.', ';', ',', '(', ')', '[', ']' };

	static final String language[] = { "break", "const", "continue", "discard",
			"do", "else", "false", "for", "if", "in", "inout", "out", "return",
			"true", "uniform", "varying", "void", "while", "#define", "#else",
			"#elif", "#endif", "#error", "#if", "#ifdef", "#ifndef", "#line",
			"#undef", "#pragma", "class", "enum", "typdef", "union" };

	static final String types[] = { "float", "vec2", "vec3", "vec4",
			"sampler1D", "sampler1DShadow", "sampler2D", "sampler2DShadow",
			"sampler3D", "samplerCube", "mat2", "mat3", "mat4", "ivec2",
			"ivec3", "ivec4", "bvec2", "bvec3", "bvec4", "attribute", "bool",
			"gl_Color", "gl_FragCoord", "gl_FogCoord", "gl_FrontFacing",
			"gl_MultiTexCoord0", "gl_MultiTexCoord1", "gl_MultiTexCoord2",
			"gl_MultiTexCoord3", "gl_MultiTexCoord4", "gl_MultiTexCoord5",
			"gl_MultiTexCoord6", "gl_MultiTexCoord7", "gl_MultiTexCoord8",
			"gl_MultiTexCoord9", "gl_MultiTexCoord10", "gl_MultiTexCoord11",
			"gl_MultiTexCoord12", "gl_MultiTexCoord13", "gl_MultiTexCoord14",
			"gl_MultiTexCoord15", "gl_MultiTexCoord16", "gl_MultiTexCoord17",
			"gl_MultiTexCoord18", "gl_MultiTexCoord19", "gl_MultiTexCoord20",
			"gl_MultiTexCoord21", "gl_MultiTexCoord22", "gl_MultiTexCoord23",
			"gl_MultiTexCoord24", "gl_MultiTexCoord25", "gl_MultiTexCoord26",
			"gl_MultiTexCoord27", "gl_MultiTexCoord28", "gl_MultiTexCoord29",
			"gl_MultiTexCoord30", "gl_MultiTexCoord31", "gl_SecondaryColor",
			"gl_Normal", "gl_Vertex", "gl_PointSize", "gl_Position",
			"gl_ClipVertex", "gl_FragColor", "gl_FragDepth", "gl_FrontColor",
			"gl_BackColor", "gl_FrontSecondaryColor", "gl_BackSecondaryColor",
			"gl_TexCoord", "gl_FogFragCoord" };

	static final String functions[] = { "radians", "degrees", "sin", "cos",
			"tan", "asin", "acos", "atan", "pow", "exp2", "log2", "sqrt",
			"inversesqrt", "abs", "sign", "floor", "cei", "fract", "mod",
			"min", "max", "clamp", "mix", "step", "smoothstep", "length",
			"distance", "dot", "cross", "normalize", "ftransform",
			"faceforward", "reflect", "matrixcompmult", "lessThan",
			"lessThanEqual", "greaterThan", "greaterThanEqual", "equal",
			"notEqual", "any", "all", "not", "texture1D", "texture1DProj",
			"texture1DLod", "texture1DProjLod", "texture2D", "texture2DProj",
			"texture2DLod", "texture2DProjLod", "texture3D", "texture3DProj",
			"texture3DLod", "texture3DProjLod", "textureCube",
			"textureCubeLod", "shadow1D", "shadow1DProj", "shadow1DLod",
			"shadow1DProjLod", "shadow2D", "shadow2DProj", "shadow2DLod",
			"shadow2DProjLod", "dFdx", "dFdy", "fwidth", "noise1", "noise2",
			"noise3", "noise4" };

	static final String semantics[] = { "gl_ModelViewMatrix",
			"gl_ProjectionMatrix", "gl_ModelViewProjectionMatrix",
			"gl_NormalMatrix", "gl_TextureMatrix", "gl_NormalScale",
			"gl_DepthRangeParameters", "gl_DepthRange", "gl_ClipPlane",
			"gl_PointParameters", "gl_Point", "gl_MaterialParameters",
			"gl_FrontMaterial", "gl_BackMaterial", "gl_LightSourceParameters",
			"gl_LightSource", "gl_LightModelParamters", "gl_LightModel",
			"gl_LightModelProducts", "gl_FrontLightModelProduct",
			"gl_BackLightModelProduct", "gl_LightProducts",
			"gl_FrontLightProduct", "gl_BackLightProduct",
			"gl_TextureEnvColor", "gl_EyePlaneS", "gl_EyePlaneT",
			"gl_EyePlaneR", "gl_EyePlaneQ", "gl_ObjectPlaneS",
			"gl_ObjectPlaneT", "gl_ObjectPlaneR", "gl_ObjectPlaneQ",
			"gl_FogParameters", "gl_Fog", "gl_MaxLights", "gl_MaxClipPlanes",
			"gl_MaxTextureUnits", "gl_MaxTextureCoords", "gl_MaxVertexAttribs",
			"gl_MaxVertexUniformComponents", "gl_MaxVaryingFloats",
			"gl_MaxVectexTextureImageUnits", "gl_MaxTextureImageUnits",
			"gl_MaxFragmentUniformComponents",
			"gl_MaxCombinedTextureImageUnits" };

	public GLSLScanner() {
		super();

		super.language = GLSLScanner.language;
		super.types = GLSLScanner.types;
		super.functions = GLSLScanner.functions;
		super.semantics = GLSLScanner.semantics;
	}
}
