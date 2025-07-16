// Constants and thresholds
#include "Common.hlsli"
#include "CommonSurfelRegisters.hlsli"

Texture2D<float4> gColor   : register(t2); // RGB = world normal
//#include "SurfelUniformGridAccelerationStructure.hlsli"
[numthreads(32, 1, 1)]
void main(
    uint3 dispatchThreadId: SV_DispatchThreadID,
    uint groupIndex: SV_GroupIndex,
    uint3 groupThreadID: SV_GroupThreadID,
    uint3 groupdId: SV_GroupID
)
{
    
    float3 gResolution;
    gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
    if (dispatchThreadId.x >= gResolution.x || dispatchThreadId.y >= gResolution.y)
        return;
    uint2 pixelPos = dispatchThreadId.xy;
    float2 uv = float2(dispatchThreadId.xy) / float2(gResolution.x - 1, gResolution.y - 1);

    surfelGridUAV[0] = gColor.SampleLevel(defaultSampler, uv, 1);
    surfelsUAV[0].radius = gDepth.SampleLevel(defaultSampler, uv, 1);
    return;

}








