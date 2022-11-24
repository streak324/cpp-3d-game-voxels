#version 460


layout(set = 2, binding = 0) uniform sampler samp;
layout(set = 2, binding = 1) uniform texture2D textures[4096];

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform object {
	int imageIndex;
} pc;

void main() {
	outColor = fragColor;// * texture(sampler2D(textures[pc.imageIndex], samp), fragTexCoord);
}