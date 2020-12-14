cbuffer SkyBoxDataPS : register(b0)
{
    float4 ColorTint;
};


Texture2D Diffuse;

SamplerState Sampler : register(s0);

struct VS_PS_Data
{
	float4 Position : SV_POSITION;
	float2 AlbedoTC : TEXCOORD0;
};

float3 simple_reinhard(float3 x, float maxOutput) {
    x = x / (x + float3(1.0, 1.0, 1.0));
    return x *= maxOutput;
}

float4 main(VS_PS_Data input) : SV_TARGET
{
	const float gamma = 1.6; // Set a gamma level but definitely not correct for HDR

	float4 outColour = ColorTint* Diffuse.Sample(Sampler, input.AlbedoTC);

    float3 mapped;

    // Reinhard tone mapping
    mapped = simple_reinhard(outColour, 1.0); 

    // Gamma correction
    mapped = pow(mapped, float3(1.0 / gamma, 1.0 / gamma, 1.0 / gamma));

    mapped = float3(1.0, 0.0, 1.0);

    // doesn't seem this shader is actually used for Skyboxes?

    return float4(mapped, outColour.a);
}

    //return ColorTint * Diffuse.Sample(Sampler, input.AlbedoTC );

