// Constants and thresholds
#include "Common.hlsli"
//#include "SurfelUniformGridAccelerationStructure.hlsli"
#include "CommonSurfelRegisters.hlsli"
#include "SurfelSpawningUtility.hlsli"




float3 CalculateSurfelContriubution(SurfelData surfel, float3 worldPos, float3 interpolatedNormal)
{
    float3 colorContribution = float3(0, 0, 0);
    float3 d = length(worldPos - surfel.position);

     float contribution = 1.f;
    float dotN = dot(interpolatedNormal, normalize(surfel.normal));

    contribution *= saturate(dotN);
    contribution *= saturate(1 - d / surfel.radius);
    contribution = smoothstep(0, 1, contribution);



    colorContribution = surfel.color * contribution;
    return colorContribution;
        
}

float3 computeRadianceForWorldPos(float3 worldPos, float3 worldNormal)
{
    float3 colorE = float3(0, 0, 0);
    uint surfelNum = 0;
    uint surfelStride = 0;

    
    uint3 idx = ComputeGridIndex(worldPos, Grid.gridOrigin, Grid.cellSize);

    uint linearIndex = HashGridIndex(idx, Grid);
    uint surfelListIndexFrom = surfelGridUAV[linearIndex];
    uint surfelListIndexTo = surfelGridUAV[linearIndex + 1];


    //[unroll(120)] \ aroudn a ms performance gain
    for (uint i = surfelListIndexFrom; i < surfelListIndexTo; ++i)
    {
        uint index = surlfeListUAV[i];
        SurfelData surfel = surfelsUAV[index];
        float3 d = worldPos - surfel.position;
        float3 colorContribution ;
        colorContribution = CalculateSurfelContriubution(surfel, worldPos, worldNormal);
        colorE += colorContribution;
    }
    return colorE;

}

[numthreads(16, 16, 1)]
void main(
    uint3 dispatchThreadId: SV_DispatchThreadID,
    uint groupIndex: SV_GroupIndex,
    uint3 groupThreadID: SV_GroupThreadID,
    uint3 groupdId: SV_GroupID
)
{

    uint2 pixelPos = dispatchThreadId.xy;
    float2 uv = float2(dispatchThreadId.xy) / float2(resolution.x - 1, resolution.y - 1);
    
    //Reconstruct world-position
    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float depthRange = 0.005;
    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);
    float3 worldNormal = gNormal.SampleLevel(defaultSampler, uv, 0);
    //Compute the color for this pixel
    outputTexture[pixelPos] = float4(computeRadianceForWorldPos(worldPos, worldNormal), 1);
}



    








