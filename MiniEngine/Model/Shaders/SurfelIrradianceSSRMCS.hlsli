// Constants and thresholds
#include "Common.hlsli"
#include "ScreenSpaceReflections.hlsli"
#include "SurfelLightTransferUtility.hlsl"

//#define M_PI 3.1415926

cbuffer SurfelGenCB : register(b0)
{
    uint   FrameIndex;
    float  DepthThreshold;
    float  NormalThreshold;
    float  minRadius;

    uint   MaxSurfels;
    int   CurrentSurfelCount;
    int kPerCellSurfelLimit = 20;
    int gPlacementThreshold = 2;

    int gRemovalThreshold = 0;
    float gChancePower = 1.1;
    float gChanceMultiply = 15;

    float maxRadius;
    
    
    
    
    UniformGrid Grid;
};

cbuffer ProjectionData : register(b1)
{
    matrix invViewProjectionMatrix;
    float depthNear;
    float depthFar;
    float fovY;
}
struct SurfelDebugData
{
    uint4 pointedCell;
};

// G-buffer input textures
Texture2D<float4> gDepth : register(t0); // RGB = world pos
Texture2D<float4> gNormal   : register(t1); // RGB = world normal

RWStructuredBuffer<SurfelData> surfelsUAV : register(u0); // world position
RWStructuredBuffer<uint> surlfeListUAV : register(u1); // Stored pointers (indices) to the appropriate surfel data
// Stored pointer to a range of surfle IDs 
// SurfelList[from SurfelGrid[i] to SurfelGrid[i+1]] is all the surfel that occupy a cell with index I
RWStructuredBuffer<uint> surfelGridUAV : register(u2); 
//The first index of this structure is the stack pointer
RWStructuredBuffer<uint> surfleStackUAV : register(u3); // Stored pointers (indices) to the appropriate surfel data
RWTexture2D<float4> outputTexture   : register(u4); // RGB = outputTexture
RWStructuredBuffer<SurfelDebugData> debugUAV : register(u0,space1); 
Texture2D<float4> gColor   : register(t2); // RGB = world normal

cbuffer ScreenSpaceCBV : register(b2)
{
    SSRCameraData cameraData;
    SSRParameters params;
};

//#include "SurfelUniformGridAccelerationStructure.hlsli"
[numthreads(32, 1, 1)]
void main(
    uint3 dispatchThreadId: SV_DispatchThreadID,
    uint groupIndex: SV_GroupIndex,
    uint3 groupThreadID: SV_GroupThreadID,
    uint3 groupdId: SV_GroupID
)
{

    SurfelData s = surfelsUAV[dispatchThreadId.x];
    float3 accumulatedIrradiance = float3(0, 0, 0);
    float3 meanRayDir = float3(0, 0, 0);
    float3x3 sumOuter = 0;

    float variance = s.msme.inconsistency;
    float minRays = 1;
    float maxRays = 30;
    float N = lerp(minRays, maxRays, variance);
    surfelsUAV[dispatchThreadId.x].raySamples.x = N;
    if(s.radius > 0)
    {
        uint3 index3 = dispatchThreadId;
        uint seed = GetThreadTemporalSeed(index3, FrameIndex);
        float2 rnd;
        rnd.x = RandomFloat01(seed);
        rnd.y = RandomFloat01(seed);
        float3 rayDir = CosineSampleHemisphere(rnd, s.normal);

        float3 Li = 
            worldSpaceRayMarchCS(s.position, rayDir, cameraData, params, gDepth, gColor, defaultSampler, float4(0, 0, 0, 1)); // returned radiance
        float cosTheta = saturate(dot(rayDir, surfelsUAV[dispatchThreadId.x].normal));
        float3 radiance = Li * M_PI;
        accumulatedIrradiance += radiance;
        
    }
    uint globalIndex = dispatchThreadId.x;

    meanRayDir /= N;
    surfelsUAV[globalIndex].mean = float4(meanRayDir, 0);
    accumulatedIrradiance /= surfelsUAV[globalIndex].raySamples.x;

    InterlockedAdd(surfelsUAV[globalIndex].raySamples.y, surfelsUAV[globalIndex].raySamples.x);

    //First time irradiance is accumulated
    if( surfelsUAV[globalIndex].raySamples.y <= surfelsUAV[globalIndex].raySamples.x)
    {
        surfelsUAV[globalIndex].msme.mean = float4(accumulatedIrradiance, 1);
        surfelsUAV[globalIndex].msme.shortMean = accumulatedIrradiance;
    }
    else
    {
        surfelsUAV[globalIndex].color = float4(MultiscaleMeanEstimator(accumulatedIrradiance, surfelsUAV[globalIndex].msme, 0.08), 1);
    }
    
//    float3 gResolution;
//    gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
//    if (dispatchThreadId.x >= gResolution.x || dispatchThreadId.y >= gResolution.y)
//        return;
//    uint2 pixelPos = dispatchThreadId.xy;
//    float2 uv = float2(dispatchThreadId.xy) / float2(gResolution.x - 1, gResolution.y - 1);

//    surfelGridUAV[0] = gColor.SampleLevel(defaultSampler, uv, 1);
//    surfelsUAV[0].radius = gDepth.SampleLevel(defaultSampler, uv, 1);
    return;

}








