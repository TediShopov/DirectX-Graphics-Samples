// Constants and thresholds
#include "Common.hlsli"
//#include "SurfelUniformGridAccelerationStructure.hlsli"
#include "SurfelASAsserts.hlsli"

#define M_PI 3.1415926




cbuffer SurfelGenCB : register(b0)
{
    uint   FrameIndex;
    float  DepthThreshold;
    float  NormalThreshold;
    float  ViewDistThreshold;
    uint   MaxSurfels;
    uint   CurrentSurfelCount;
    uint2   padding;
    UniformGrid Grid;
};

cbuffer ProjectionData : register(b1)
{
    matrix invViewProjectionMatrix;
    float depthNear;
    float depthFar;
    float fovY;
}

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


groupshared uint groupShareMinCoverage;
groupshared uint groupShareMaxContribution;


//Calculate the surfel contribution by using Mahalonobis distance and angular fallof.
//The actual inverse covariance matrix need be calculated. 
//A scalling factor based on the scene must be supplied for appropriate results.
float3 calculateSurfelsContribution_WActualCovarianceMatrix(SurfelData surfel, float3 worldPos, float sclalingFactor)
{
    
    float3 colorContribution = float3(0, 0, 0);
        
    float3 d = worldPos - surfel.position;
    float3 L = -normalize(d); // light dir toward surfel
    float NdotL = saturate(dot(surfel.normal, L));

    if (NdotL <= 0.0f)
    {
        return colorContribution;
        
    }

    float3x3 covarianceInverse = 0;
    covarianceInverse._m00_m01_m02 = surfel.co1;
    covarianceInverse._m10_m11_m12 = surfel.co2;
    covarianceInverse._m20_m21_m22 = surfel.co3;

    float3 dTransformed = mul(covarianceInverse, d);
    float D2 = dot(d, dTransformed); // Mahalanobis distance squared
    D2 /= sclalingFactor;
    float w = exp(-0.5 * D2);
    colorContribution = surfel.color * NdotL * w;;
    return colorContribution;
    
}


//Compute the surfel contributing color based on Mahalonobis-like metric and angular fallof.
// The mahalonobis metric does NOT use an inverse covariance matrix but instead the 
// distance is "squashed" by a factor in the surfels normal direction
float3 calculateSurfelsContribution_MahalonobisLikeMetric(SurfelData surfel, float3 worldPos, float squshFactor=2, float relScalingFactor = 2)
{
    float3 colorContribution = float3(0, 0, 0);
    float3 d = worldPos - surfel.position;

        //  Ellipsoidal distance without covariance matrix 
    float dDotN = dot(d, surfel.normal);
    float3 tangentOffset = d - dDotN * surfel.normal;
    float3 normalOffset = dDotN * surfel.normal;

    float squash = 2.0f; // faster falloff in normal direction
    float3 squashOffset = tangentOffset + squash * normalOffset;
    float D2 = dot(squashOffset, squashOffset);
    D2 /= pow(surfel.radius, relScalingFactor);
    float w = exp(-0.5 * D2); // Spatial weight

    colorContribution = surfel.color * w;

    return colorContribution;
        
}

//Compute the surfel contributing color based on Mahalonobis-like metric and angular fallof.
// The mahalonobis metric does NOT use an inverse covariance matrix but instead the 
// distance is "squashed" by a factor in the surfels normal direction
float3 calculateSurfelsContribution_MahalonobisLikeMetricAndAngularFallof(SurfelData surfel, float3 worldPos, float3 interpolatedNormal,float squshFactor=2, float relScalingFactor = 2)
{
    float3 colorContribution = float3(0, 0, 0);
    float3 d = worldPos - surfel.position;

        //  Ellipsoidal distance without covariance matrix 
    float dDotN = dot(d, surfel.normal);
    float3 tangentOffset = d - dDotN * surfel.normal;
    float3 normalOffset = dDotN * surfel.normal;

    float squash = 2.0f; // faster falloff in normal direction
    float3 squashOffset = tangentOffset + squash * normalOffset;
    float D2 = dot(squashOffset, squashOffset);
    D2 /= pow(surfel.radius, relScalingFactor);

    float w = exp(-0.5 * D2); // Spatial weight

        // Angular weighting (optional, like PICA PICA) 
    float NdotN = saturate(dot(interpolatedNormal, surfel.normal));
    w *= NdotN * NdotN;

    float3 L = -normalize(d); // light direction from surfel
    float NdotL = saturate(dot(surfel.normal, L));
    if (NdotL > 0.0f)
    {
        colorContribution = surfel.color * NdotL * w;;
    }
    return colorContribution;
        
}


float3 computeRadianceForWorldPos(float3 worldPos)
{
    
  //Get all for now
    //TODO make a bounding box around the surfel to only allow surfel in some range
    //TODO make a raycast toward surfels position to see if it contributes
    //uint3 index = ComputeGridIndex(worldPos, Grid.gridOrigin, Grid.cellSize);
    float3 colorE = float3(0, 0, 0);
    uint surfelNum = 0;
    uint surfelStride = 0;

    
    uint3 idx = ComputeGridIndex(worldPos, Grid.gridOrigin, Grid.cellSize);

    uint linearIndex = HashGridIndex(idx, Grid);
    uint surfelListIndexFrom = surfelGridUAV[linearIndex];
    uint surfelListIndexTo = surfelGridUAV[linearIndex + 1];

    

    
    

    for (uint i = surfelListIndexFrom; i < surfelListIndexTo; ++i)
    {

        uint index = surlfeListUAV[i];
        SurfelData surfel = surfelsUAV[index];
        float3 d = worldPos - surfel.position;
        float3 colorContribution = calculateSurfelsContribution_MahalonobisLikeMetric(surfel, worldPos);
        colorE += colorContribution;
    }
    return colorE;

}

[numthreads(32, 32, 1)]
void main(
    uint3 dispatchThreadId: SV_DispatchThreadID,
    uint groupIndex: SV_GroupIndex,
    uint3 groupThreadID: SV_GroupThreadID,
    uint3 groupdId: SV_GroupID
)
{

    // Initialize group shared values.
    if (groupIndex == 0)
    {
        groupShareMinCoverage = ~0;
        groupShareMaxContribution = 0;
    }

    GroupMemoryBarrierWithGroupSync();
    float3 gResolution;
    gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
    if (dispatchThreadId.x >= gResolution.x || dispatchThreadId.y >= gResolution.y)
        return;
    uint2 pixelPos = dispatchThreadId.xy;

    float2 uv = float2(dispatchThreadId.xy) / float2(gResolution.x - 1, gResolution.y - 1);
    float4 atNormal = gNormal.SampleLevel(defaultSampler, uv, 0);
    //Create a random "state" 
    
    //Reconstruct world-position
    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float depthRange = 0.005;
    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);

    //Compute the color for this pixel
    outputTexture[pixelPos] = float4(computeRadianceForWorldPos(worldPos), 1);




}



    








