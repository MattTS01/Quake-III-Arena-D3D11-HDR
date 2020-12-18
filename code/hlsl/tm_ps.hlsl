#include "pscommon.h"

cbuffer CBuf : register(b1)
{
    float3 Color;
};

Texture2D Diffuse;

SamplerState Sampler : register(s0);

struct VS_PS_Data
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD;
};

float3 simple_reinhard(float3 x, float maxOutput) {
	x = x / (x + float3(1.0, 1.0, 1.0));
	return x *= maxOutput;
}

float4 main(VS_PS_Data input) : SV_TARGET
{
	const float gamma = 2.2; // Set a gamma level but definitely not correct for HDR

	float4 outColour = Diffuse.Sample(Sampler, input.TexCoord);

	float3 mapped;

	// Reinhard tone mapping
	mapped = simple_reinhard(outColour, maxOutput);

	// Gamma correction
	// Raises the mid-tones, looks very dark without it. Probably a smarter way to do this. 
	mapped = pow(mapped, float3(1.0 / gamma, 1.0 / gamma, 1.0 / gamma));



	return float4(mapped, 1.0);
}
