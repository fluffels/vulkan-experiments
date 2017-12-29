#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location=0) in vec2 v_pos;
layout (location=1) in vec3 v_col;

layout (location=0) out vec3 f_col;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = vec4(v_pos, 0.0, 1.0);
    f_col = v_col;
}
