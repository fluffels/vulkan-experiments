#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (binding=0) uniform Uniforms {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

layout (location=0) out vec2 texCoord;

layout (location=0) in vec3 pos;
layout (location=1) in vec3 normal;
layout (location=2) in vec2 tex;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = u.proj * u.view * u.model * vec4(pos, 1.0);
	texCoord = tex;
}
