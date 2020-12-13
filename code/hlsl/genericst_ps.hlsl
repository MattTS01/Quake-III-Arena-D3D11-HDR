#include "pscommon.h"


Texture2D Diffuse;

SamplerState Sampler : register(s0);

struct VS_PS_Data
{
	float4 Position : SV_POSITION;
	float2 AlbedoTC : TEXCOORD0;
    float4 Color : COLOR;
    float4 ViewPos : TEXCOORD2;
};

float3 simple_reinhard(float3 x, float maxOutput) {
    x = x / (x + float3(1.0, 1.0, 1.0));
    return x *= maxOutput;
}

float4 main(VS_PS_Data input) : SV_TARGET
{
    //ClipToPlane(input.ViewPos);

    //return FinalColor(
    //    input.Color *
    //    Diffuse.Sample(Sampler, input.AlbedoTC));

    const float gamma = 1.6; // Set a gamma level but definitely not correct for HDR

    ClipToPlane(input.ViewPos);

    float4 outColour = input.Color * Diffuse.Sample(Sampler, input.AlbedoTC);
    float3 mapped;

    // Reinhard tone mapping
    mapped = simple_reinhard(outColour, maxOutput); // around 850 cd/m^2 converted to normalised value for ST 2084

    // Gamma correction
    mapped = pow(mapped, float3(1.0 / gamma, 1.0 / gamma, 1.0 / gamma));

    //mapped = float3(maxOutput, maxOutput, maxOutput);

    return FinalColor(float4(mapped, outColour.a));
}
