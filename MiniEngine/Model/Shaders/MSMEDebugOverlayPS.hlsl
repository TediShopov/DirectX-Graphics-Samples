
#include "Common.hlsli"
#include "CommonSurfelRegisters.hlsli"
//#include "SurfelASAsserts.hlsli"
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};


static const float4 ColorArray[16] = {
    float4(0.0, 0.0, 0.0, 1.0),     // Black
    float4(1.0, 0.0, 0.0, 1.0),     // Red
    float4(0.0, 1.0, 0.0, 1.0),     // Green
    float4(0.0, 0.0, 1.0, 1.0),     // Blue
    float4(1.0, 1.0, 0.0, 1.0),     // Yellow
    float4(1.0, 0.0, 1.0, 1.0),     // Magenta
    float4(0.0, 1.0, 1.0, 1.0),     // Cyan
    float4(1.0, 0.5, 0.0, 1.0),     // Orange
    float4(0.5, 0.0, 0.5, 1.0),     // Purple
    float4(0.5, 0.5, 0.5, 1.0),     // Gray
    float4(1.0, 1.0, 1.0, 1.0),     // White
    float4(0.5, 0.25, 0.0, 1.0),    // Brown
    float4(0.0, 0.5, 0.0, 1.0),     // Dark Green
    float4(0.0, 0.0, 0.5, 1.0),     // Dark Blue
    float4(0.5, 0.0, 0.0, 1.0),     // Dark Red
    float4(0.5, 0.5, 1.0, 1.0)      // Light Blue
};


float3 computeRadianceForWorldPos(float3 worldPos, float3 worldNormal)
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
        float distance = length(worldPos - surfel.position);
        if(distance < surfel.radius / 2)
        {
            float4 colorFormVBBR = lerp(float4(0, 1, 0, 1), float4(1, 0, 0, 1), surfel.msme.vbbr);
            float4 colorFromInconsistency = lerp(float4(0, 1, 0, 1), float4(1, 0, 0, 1), surfel.msme.inconsistency);
            colorE = colorFormVBBR;
            //colorE = colorFromInconsistency;
            
        }


       


    }
    return colorE;

}


float4 main(PSInput input) : SV_TARGET
{
    uint2 gRes;
    uint numberOfLevels;
    gDepth.GetDimensions(0, gRes.x, gRes.y, numberOfLevels);

    float2 uv = input.position.xy / gRes; 
    //uv.y = 1.0f - uv.y;

    //Reconstruct world position
    float4 depthRaw = gDepth.Sample(defaultSampler, uv);
    float depthRange = 0.005;
    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);

    float3 worldNormal = gNormal.SampleLevel(defaultSampler, uv, 0);

    //Compute cell indices
    uint3 cellIndex = ComputeGridIndex(worldPos, Grid.gridOrigin,Grid.cellSize);
    uint flattenedIndex = HashGridIndex(cellIndex, Grid);

    uint numStructs, stride;
    surfelGridUAV.GetDimensions(numStructs, stride);
    if ((flattenedIndex + 1) >= numStructs)
        return float4(0, 0, 1, 1); // Debug blue

    uint surfelIdFrom = surfelGridUAV[flattenedIndex];
    uint surfelIdTo = surfelGridUAV[flattenedIndex + 1];
    uint surfelCount = surfelIdTo - surfelIdFrom;

    
    return float4(computeRadianceForWorldPos(worldPos, worldNormal), 1);
    //float4 finalColor = colorBasedOnSurfelCount; 
    //finalColor= float4(computeRadianceForWorldPos(worldPos, worldNormal), 1);
    //return finalColor;
}
