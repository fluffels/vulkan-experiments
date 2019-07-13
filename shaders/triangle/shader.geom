#version 450

layout (binding=0) uniform Uniforms {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(location=0) in uint[] vertexType;

layout(location=0) out vec2 texCoord;
layout(location=1) out flat vec2 gridCoord;
layout(location=2) out flat uint geometryType;

void main() {
    vec4 origin;
    vec4 pos;

    origin = u.view * u.model * gl_in[0].gl_Position;

	pos = origin + vec4(0.0, -1.0, 0.0, 0.0);
	gl_Position = u.proj * pos;
	texCoord = vec2(0.0f, 0.0f);
	gridCoord = vec2(gl_in[0].gl_Position.x, gl_in[0].gl_Position.z) / 100.0f;
	geometryType = vertexType[0];
	EmitVertex();

	pos = origin + vec4(0.0, 0.0, 0.0, 0.0);
	gl_Position = u.proj * pos;
	texCoord = vec2(0.0f, 1.0f);
	gridCoord = vec2(gl_in[0].gl_Position.x, gl_in[0].gl_Position.z) / 100.0f;
	geometryType = vertexType[0];
	EmitVertex();

	pos = origin + vec4(1.0, -1.0, 0.0, 0.0);
	gl_Position = u.proj * pos;
	texCoord = vec2(1.0f, 0.0f);
	gridCoord = vec2(gl_in[0].gl_Position.x, gl_in[0].gl_Position.z) / 100.0f;
	geometryType = vertexType[0];
	EmitVertex();

    pos = origin + vec4(1.0, 0.0, 0.0, 0.0);
	gl_Position = u.proj * pos;
	texCoord = vec2(1.0f, 1.0f);
	gridCoord = vec2(gl_in[0].gl_Position.x, gl_in[0].gl_Position.z) / 100.0f;
	geometryType = vertexType[0];
    EmitVertex();
}
