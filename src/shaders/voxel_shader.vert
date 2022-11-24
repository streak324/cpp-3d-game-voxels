#version 460

layout(location = 0) in vec3 inPosition;

//layout(location = 0) out highp uint color;

layout (set=0, binding = 0) uniform UniformBuffer {
	mat4 view;
	mat4 projection;
} ub;

struct ObjectData {
	mat4 model;
};
struct RGBA8BitColor {
	highp ivec4 rgbaColor;
};

layout (std140,set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectBuffer;

layout (std140,set = 1, binding = 1) readonly buffer ColorBuffer{
	RGBA8BitColor colors[];
} colorBuffer;

void main() {
	gl_Position = ub.projection * ub.view * objectBuffer.objects[gl_InstanceIndex].model * vec4(inPosition, 1.0);
	//color = color;
}