/* shadow-cubemap generation geometry shader */
#extension EXT_geometry_shader4 : enable 


in vec4 gl_PositionIn[];
in vec4 vTexCoord[];

out vec4 gl_Position;
out vec4 texCoord;
 
/* a simple geometry shader that projects each vertex onto the 6 faces of a cube */
void main() 
{ 
	int i, layer;
	for (layer = 0; layer < 6; layer++) {
		gl_Layer = layer;
		/*
		for (i = 0; i < 3; i++) {
			gl_Position = gl_PositionIn[i];
			texCoord = vTexCoord[i];
			EmitVertex();
		}
		*/

		gl_Position = gl_PositionIn[0];
		texCoord = vTexCoord[0];
		EmitVertex();
		EndPrimitive();
	}
} 
