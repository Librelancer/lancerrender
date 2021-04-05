@vertex
in vec3 vertex_position;
in vec3 vertex_normal;
in vec2 vertex_texture1;

out vec2 texcoord;
out vec3 normal;
out vec3 fragPos;

uniform mat4x4 World;
uniform mat4x4 Normal;
uniform mat4x4 ViewProjection;

void main()
{
	vec4 pos = (ViewProjection * World) * vec4(vertex_position, 1.0);
	fragPos = (World * vec4(vertex_position, 1.0)).xyz;
	gl_Position = pos;

	texcoord = vertex_texture1;
	normal = (Normal * vec4(vertex_normal, 0.0)).xyz;
}

@fragment
out vec4 out_color;
in vec2 texcoord;
in vec3 normal;
in vec3 fragPos;

uniform sampler2D texsampler;

layout (std140) uniform Lighting {
	float lightEnabled; //vec4 1
	float ambientR;
	float ambientG;
	float ambientB;
	float lightR; //vec4 2
	float lightG;
	float lightB;
	float lightX;
	float lightY; //vec4 3
	float lightZ;
};

void main()
{
	vec4 objColor = texture(texsampler, vec2(texcoord.x, 1.0 - texcoord.y));

	vec3 litColor;

	if(lightEnabled > 0.0) {
		vec3 lightPos = vec3(lightX, lightY, lightZ);
		vec3 lightColor = vec3(lightR, lightG, lightB);
		vec3 n = normalize(normal);
		vec3 lightDir = normalize(lightPos - fragPos);
		float diff = max(dot(n, lightDir), 0.0);
		vec3 color = vec3(ambientR, ambientG, ambientB) + (diff * lightColor);
		color = clamp(color, 0.0, 1.0);
		litColor =  color * objColor.xyz;
	} else {
		litColor = objColor.xyz;
	}

	out_color = vec4(litColor, objColor.a);
}
