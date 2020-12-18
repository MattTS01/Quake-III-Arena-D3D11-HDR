#include "vscommon.h"

struct VS_Data
{
	float2 Position : POSITION;
	float2 TexCoord : TEXCOORD;
};

struct VS_PS_Data
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD;
};

VS_PS_Data main(VS_Data input)
{
	VS_PS_Data output;

	output.Position = float4(input.Position.xy, 0, 1);
	output.TexCoord = input.TexCoord;

	return output;
}
