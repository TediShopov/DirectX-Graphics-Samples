
#include "Common.hlsli"
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};


Texture2D<float4> rayOutput   : register(t18);

float4 main(PSInput input) : SV_TARGET
{
    float2 uv;
    //TEMPORARY RESOLUTION TO USE PIXEL POSITION FROM RAYTRACE OUTPUT
    uv.x = input.position.x / 1920;
    uv.y = input.position.y / 1080;
    float4 res3 = rayOutput.Sample(defaultSampler, uv);
    return res3;
	//return input.color;
}
