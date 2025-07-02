
#include "Common.hlsli"

Texture2D<float4> giOnlyResult : register(t0); 
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};



float4 main(PSInput input) : SV_TARGET
{

    uint2 gRes;
    uint numberOfLevels;
    giOnlyResult.GetDimensions(0, gRes.x, gRes.y, numberOfLevels);
    float2 uv = input.position.xy / gRes; 
    return giOnlyResult.Sample(defaultSampler, uv);
}
