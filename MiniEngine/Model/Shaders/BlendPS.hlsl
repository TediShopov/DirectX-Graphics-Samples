
#include "Common.hlsli"

Texture2D textureToBlend : register(t0);

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};
float4 main(PSInput input) : SV_TARGET
{
    uint2 res;
    uint numberOfLevels;
    textureToBlend.GetDimensions(0, res.x, res.y, numberOfLevels);
    float2 uv = input.position.xy / res; 
    return textureToBlend.Sample(defaultSampler, uv);
}
