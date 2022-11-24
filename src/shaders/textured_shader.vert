
#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout (set=0, binding = 0) uniform UniformBuffer {
	mat4 view;
	mat4 projection;
} ub;

struct ObjectData {
	mat4 model;
};

layout (std140,set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectBuffer;

void main() {
	gl_Position = ub.projection * ub.view * objectBuffer.objects[gl_InstanceIndex].model * vec4(inPosition, 1.0);
	fragColor = inColor;
	fragTexCoord = inTexCoord;
}