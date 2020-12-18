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

// Improved Reinhard tone mapping based on https://www.shadertoy.com/view/lslGzl
float3 whitePreservingLumaBasedReinhardToneMapping(float3 color, float maxOutput)
{
	float white = 2.;
	float luma = dot(color, float3(0.2126, 0.7152, 0.0722)); // calc luma based on RGB
	float toneMappedLuma = luma * (1. + luma / (white * white)) / (1. + luma);
	color *= toneMappedLuma / luma;
	return color * maxOutput;
}

// Filmic tone mapping based on https://www.shadertoy.com/view/lslGzl
float3 filmicToneMapping(float3 color, float maxOutput)
{
	color = max(float3(0.0,0.0,0.0), color - float3(0.004,0.004,0.004));
	color = (color * (6.2 * color + .5)) / (color * (6.2 * color + 1.7) + 0.06);
	return color * maxOutput;
}

// RomBinDaHouse tone mapping based on https://www.shadertoy.com/view/lslGzl
float3 RomBinDaHouseToneMapping(float3 color, float maxOutput)
{
	float gamma = 2.2;
	color = exp(-1.0 / (2.72 * color + 0.15));
	color = pow(color, float3(1. / gamma, 1./gamma, 1./gamma));
	return color * maxOutput;
}

float4 main(VS_PS_Data input) : SV_TARGET
{
	const float gamma = 2.2; // Set a gamma level but definitely not correct for HDR

	float4 outColour = Diffuse.Sample(Sampler, input.TexCoord);

	float3 mapped;

	// Reinhard tone mapping
	//mapped = simple_reinhard(outColour, maxOutput);

	//mapped = pow(mapped, float3(1.0 / gamma, 1.0 / gamma, 1.0 / gamma));

	// current favourite tonemapping method 
	mapped = RomBinDaHouseToneMapping(outColour, maxOutput);


	return float4(mapped, 1.0);
}
