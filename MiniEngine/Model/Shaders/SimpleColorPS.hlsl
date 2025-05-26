
#include "Common.hlsli"
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};


Texture2D<float4> t1   : register(t16);
Texture2D<float4> t2   : register(t17);
Texture2D<float4> t3   : register(t18);
Texture2D<float4> t4   : register(t19);

float4 main(PSInput input) : SV_TARGET
{
    float4 res1 = t1.Sample(defaultSampler, input.position);
    float4 res2 = t2.Sample(defaultSampler, input.position);
    float4 res3 = t3.Sample(defaultSampler, input.position);
    float4 res4 = t4.Sample(defaultSampler, input.position);
    return res1 * res2 * res3 * res4;
	//return input.color;
}
