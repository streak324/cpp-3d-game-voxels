#version 460

layout(location = 0) in vec3 inPosition;

layout (set=0, binding = 0) uniform UniformBuffer {
	mat4 view;
	mat4 projection;
} ub;

struct ObjectData {
	mat4 model;
};
struct RGBAColor {
	vec4 color;
};

layout (std140,set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectBuffer;

layout (std140,set = 1, binding = 1) readonly buffer ColorBuffer{
	RGBAColor colors[];
} colorBuffer;

layout(location = 0) out vec4 fragColor;

void main() {
	gl_Position = ub.projection * ub.view * objectBuffer.objects[gl_InstanceIndex].model * vec4(inPosition, 1.0);
	fragColor = colorBuffer.colors[gl_InstanceIndex].color;
}