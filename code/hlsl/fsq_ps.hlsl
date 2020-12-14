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
	const float gamma = 1.6; // Set a gamma level but definitely not correct for HDR

	float4 outColour = float4(Color, 1) * Diffuse.Sample(Sampler, input.TexCoord);

	float3 mapped;

	// Reinhard tone mapping
	mapped = simple_reinhard(outColour, maxOutput); 

	// Gamma correction
	mapped = pow(mapped, float3(1.0 / gamma, 1.0 / gamma, 1.0 / gamma));

	//mapped = float3(maxOutput, maxOutput, maxOutput);

	return FinalColor(float4(mapped, outColour.a));
    //return FinalColor(float4(Color, 1) * Diffuse.Sample(Sampler, input.TexCoord));
}
