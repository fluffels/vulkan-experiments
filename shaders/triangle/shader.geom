#version 450

layout (binding=0) uniform Uniforms {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(binding=3) uniform sampler2D noiseTexture;

layout(location=0) in uint[] vertexType;

layout(location=0) out vec2 texCoord;
layout(location=1) out flat vec2 gridCoord;
layout(location=2) out flat uint geometryType;
layout(location=3) out float distanceFromCamera;

void main() {
	vec4 inPos = gl_in[0].gl_Position;

	const vec2 GRID_COORD = vec2(inPos.x, inPos.z) / 100.f;
	const float NOISE_OFFSET = texture(noiseTexture, GRID_COORD).x - 0.5f;
	inPos.x = inPos.x + NOISE_OFFSET;
	inPos.z = inPos.z - NOISE_OFFSET;
   
	vec4 ORIGIN = u.view * u.model * inPos;
	const float DISTANCE_FROM_CAMERA = (
		ORIGIN.x * ORIGIN.x +
		ORIGIN.y * ORIGIN.y +
		ORIGIN.z * ORIGIN.z
	) / 1000.f;

    vec4 pos;
	pos = ORIGIN + vec4(0.0, -1.0, 0.0, 0.0);
	gl_Position = u.proj * pos;
	texCoord = vec2(0.0f, 0.0f);
	gridCoord = GRID_COORD;
	geometryType = vertexType[0];
	distanceFromCamera = DISTANCE_FROM_CAMERA;
	EmitVertex();

	pos = ORIGIN + vec4(0.0, 0.0, 0.0, 0.0);
	gl_Position = u.proj * pos;
	texCoord = vec2(0.0f, 0.25f);
	gridCoord = GRID_COORD;
	geometryType = vertexType[0];
	distanceFromCamera = DISTANCE_FROM_CAMERA;
	EmitVertex();

	pos = ORIGIN + vec4(1.0, -1.0, 0.0, 0.0);
	gl_Position = u.proj * pos;
	texCoord = vec2(0.25f, 0.0f);
	gridCoord = GRID_COORD;
	geometryType = vertexType[0];
	distanceFromCamera = DISTANCE_FROM_CAMERA;
	EmitVertex();

    pos = ORIGIN + vec4(1.0, 0.0, 0.0, 0.0);
	gl_Position = u.proj * pos;
	texCoord = vec2(0.25f, 0.25f);
	gridCoord = GRID_COORD;
	geometryType = vertexType[0];
	distanceFromCamera = DISTANCE_FROM_CAMERA;
    EmitVertex();
}
