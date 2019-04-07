#version 330 core

out vec4 FragColor;

in vec2 texCoord;

uniform sampler2D vid_tex;

void main() {
	FragColor = texture(vid_tex, texCoord);
} 