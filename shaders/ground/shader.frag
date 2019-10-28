#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding=2) uniform sampler2D colorTexture;
layout(binding=3) uniform sampler2D noiseTexture;

layout(location=0) out vec4 outColor;

layout(location=0) in vec2 textureCoord;

const vec3 green = vec3(0.274509f, 0.537254f, 0.086274f);
const vec3 yellow = vec3(0.933333f, 0.862745f, 0.509803f);

void main() {
    outColor = texture(colorTexture, textureCoord);

	/* NOTE(jan): Vary texture colour by noise. */
	float noiseValue = texture(noiseTexture, textureCoord).x;
	vec3 noiseColor = mix(yellow, green, noiseValue);
	vec3 mixedColor = mix(outColor.xyz, noiseColor, 0.15f);

	outColor = vec4(mixedColor, outColor.w);
}
