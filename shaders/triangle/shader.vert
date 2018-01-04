#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (binding=0) uniform Uniforms {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

layout (location=0) in vec3 v_pos;
layout (location=1) in vec3 v_col;
layout (location=2) in vec2 v_tex;

layout (location=0) out vec3 f_col;
layout (location=1) out vec2 f_tex;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = u.proj * u.view * u.model * vec4(v_pos, 1.0);
    f_col = v_col;
    f_tex = v_tex;
}
