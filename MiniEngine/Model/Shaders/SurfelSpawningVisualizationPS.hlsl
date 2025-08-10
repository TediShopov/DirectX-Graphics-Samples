
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
            //colorE = colorFormVBBR;
            colorE = colorFromInconsistency;
            
        }


       


    }
    return colorE;

}

uint2 ComputeRelevantSurfelRange(float3 worldPos)
{
    uint2 fromTo;
    //Compute cell indices
    uint3 cellIndex = ComputeGridIndex(worldPos, Grid.gridOrigin, Grid.cellSize);
    uint flattenedIndex = HashGridIndex(cellIndex, Grid);

    fromTo.x = surfelGridUAV[flattenedIndex];
    fromTo.y = surfelGridUAV[flattenedIndex + 1];
    return fromTo;
}

bool IsInSurfelInfluence(float3 relativePosition, SurfelData surfel)
{
    float dist2 = dot(relativePosition, relativePosition);
    return dist2 < surfel.radius * surfel.radius;
}
bool IsInSurfelGeneralDirection(float3 relativePosition, SurfelData surfel, out float dotN)
{
    float3 normal = normalize(surfel.normal);
     dotN = dot(normalize(relativePosition), normal);
    return dotN > 0;

}

float EstimateSurfelCoverage(float3 worldPos)
{
    return 1;
}

float4 debugOutputSpawnChance(PSInput input)
{
    float3 gResolution;
    gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
    uint2 pixelPos = input.position;

    //uint threadRandomnessSeed = GetThreadTemporalSeed(dispatchThreadId,FrameIndex);

    //int index = pixelPos.x * gResolution.x + pixelPos.y;
    //float2 uv = float2(dispatchThreadId.xy) / float2(gResolution.x - 1, gResolution.y - 1);
    //float2 uv = float2(dispatchThreadId.xy) / float2(gResolution.x , gResolution.y );
    float2 uv = input.position.xy / float2(gResolution.x , gResolution.y );
    float4 sampledNormal = gNormal.SampleLevel(defaultSampler, uv, 0);

    //Create a random "state" 
    //Reconstruct world-position
    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float depthRange = 0.005;
    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);

    uint2 surfelFromTo = ComputeRelevantSurfelRange(worldPos);
    uint surfelCount = surfelFromTo.y - surfelFromTo.x;
    float coverage = 0;


     // Evaluate min coverage value and pixel position.
    // Also evaluate max contribution and surfel index (for handling over-coverage).
    // Also evalute weighted color output (indrect lighting).
    //if (surfelCount < kPerCellSurfelLimit-2)
    //{
        float maxContribution = 0.0f;

        //uint maxContributionSurfelIndex = RandomUintInRange(threadRandomnessSeed, 0, surfelCount);

        //For each surfel into the current Surfle Acceleration Structure Cell
        //In this case is the uniform grid
        for (uint i = surfelFromTo.x; i < surfelFromTo.y; ++i)
        {
            //uint surfelIndex = surlfeListUAV[index];
            uint surfelIndex = surlfeListUAV[i];
            SurfelData surfel = surfelsUAV[surfelIndex];
            float dotN = 1;

            //Bias is relative position from surfel world to the current reconstructed world 
            float3 bias = worldPos - (float3) surfel.position;
//            if (IsInSurfelInfluence(bias, surfel) == false)
//                continue;

            float dist = length(bias);
            float contribution = 1.f;

            //contribution *= saturate(dotN);
            //contribution *= saturate(1 - dist / surfel.radius);
            //Max Radius would be half the length of the uniform grid cell
            
            contribution *= saturate(1-dist / surfel.radius);
            contribution = smoothstep(0, 1, contribution);

            coverage += contribution;
    }
    //Normalize the coverage 
//    if(surfelCount != 0)
//        coverage /= surfelCount;
    //}

    if (coverage < gPlacementThreshold)
    //if (coverage < gPlacementThreshold)
    {
        float chanceSpawn = 1;
         chanceSpawn = pow(depthRaw, gChancePower) * gChanceMultiply;
        //float chanceSpawn = 1-RemapFloat(LinearizeDepth(depthRaw, depthFar, depthNear),depthNear,depthFar,0,1);
        //float chanceSpawn = 1-RemapFloat(LinearizeDepth(depthRaw, depthFar, depthNear),depthNear,depthFar,0,1);
        //chanceSpawn = pow(chanceSpawn, gChancePower) * gChanceMultiply;
        //float chanceSpawn = depthRaw;
        //float chanceSpawn = 1;
        chanceSpawn *= (1 - coverage);

        return float4(chanceSpawn, chanceSpawn, chanceSpawn, 1);
    }
    else
    {
        return float4(1, 0, 0, 1);
        
    }


    

}



float4 main(PSInput input) : SV_TARGET
{
    if(debugModeIndex.x == 0)
    {
        return debugOutputSpawnChance(input);
    }
    else if(debugModeIndex.x == 1)
    {
        return 1-debugOutputSpawnChance(input);
    }
    else
    {
        return float4(1, 0, 1, 1);
    }
}
