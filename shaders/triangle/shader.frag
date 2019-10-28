#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding=1) uniform sampler2D colorTexture;
layout(binding=3) uniform sampler2D noiseTexture;

layout(location=0) out vec4 outColor;

layout(location=0) in vec2 geometryTexCoord;
layout(location=1) in flat vec2 gridCoord;
layout(location=2) in flat uint geometryType;
layout(location=3) in float distanceFromCamera;

const vec3 green = vec3(0.274509f, 0.537254f, 0.086274f);
const vec3 yellow = vec3(0.933333f, 0.862745f, 0.509803f);

void main() {
	vec2 textureCoord = geometryTexCoord;
	textureCoord.x += (geometryType % 4) * 0.25f;
	textureCoord.y += (geometryType / 4) * 0.25f;
	outColor = texture(colorTexture, textureCoord);

	/* NOTE(jan): Discard fully transparent fragments so transparency is
	order-independent. */
	if (outColor.w < 0.1) { discard; }

	/* NOTE(jan): Dissolve effect prevents popping. */
	float dissolveNoiseValue = texture(noiseTexture, textureCoord).x;
	if (dissolveNoiseValue < distanceFromCamera) { discard; }

	/* NOTE(jan): Vary texture-color. */
	float variationNoiseValue = texture(noiseTexture, gridCoord).x;
	vec3 noiseColor = mix(yellow, green, variationNoiseValue);
	vec3 mixedColor = mix(outColor.xyz, noiseColor, 0.7f);

	outColor = vec4(mixedColor.xyz, outColor.w);
}
