#ifdef GL_ES
precision highp float;
#endif
in vec2 vertex_position;
in vec2 vertex_texture1;
in vec2 vertex_texture2;
in vec4 vertex_color;
out vec2 out_texcoord;
out vec2 c_pos;
out vec4 blendColor;
uniform mat4 modelviewproj;

void main()
{
	gl_Position = modelviewproj * vec4(vertex_position, 0.0, 1.0);
	blendColor = vertex_color;
	out_texcoord = vertex_texture1;
  	c_pos = vertex_texture2;
}