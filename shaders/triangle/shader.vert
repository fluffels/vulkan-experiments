#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location=0) in vec3 pos;
layout (location=1) in uint typeIn;

layout (location=0) out uint vertexType;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = vec4(pos, 1.0);
	vertexType = typeIn;
}
