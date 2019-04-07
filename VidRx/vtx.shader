#version 330 core
layout (location = 0) in vec2 aPos;

out vec2 texCoord;

const vec2 verts[4] = vec2[] (
	vec2(-1, -1),
	vec2(-1, 1),
	vec2(1, -1),
	vec2(1, 1)
);

const vec2 texcoords[4] = vec2[] (
	vec2(0.0, 1.0),
	vec2(0.0, 0.0),
	vec2(1.0, 1.0),
	vec2(1.0, 0.0)
);

void main() {
	vec2 vert = verts[gl_VertexID];
	texCoord = texcoords[gl_VertexID];
	
	gl_Position = vec4(vert, 0.0, 1.0);
}