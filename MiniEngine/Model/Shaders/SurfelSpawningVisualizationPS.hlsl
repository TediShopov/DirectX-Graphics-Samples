
#include "Common.hlsli"
#include "CommonSurfelRegisters.hlsli"
//#include "SurfelASAsserts.hlsli"
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};


Texture2D bentCones : register(t2);
Texture2D ambientOcclusion : register(t3);



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

float EstimateSurfelCoverage( float2 uv,float depthRaw)
{

    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);

    uint2 surfelFromTo = ComputeRelevantSurfelRange(worldPos);
    uint surfelCount = surfelFromTo.y - surfelFromTo.x;
    float coverage = 0;
    float maxContribution = 0.0f;

    //For each surfel into the current Surfle Acceleration Structure Cell
    //In this case is the uniform grid
    for (uint i = surfelFromTo.x; i < surfelFromTo.y; ++i)
    {
        uint surfelIndex = surlfeListUAV[i];
        SurfelData surfel = surfelsUAV[surfelIndex];
        float dotN = 1;

        //Bias is relative position from surfel world to the current reconstructed world 
        float3 bias = worldPos - (float3) surfel.position;

        float dist = length(bias);
        float contribution = 1.f;

        contribution *= saturate(1 - dist / surfel.radius);
        contribution = smoothstep(0, 1, contribution);

        coverage += contribution;
    }
    return coverage;
}
float EstimateSpawnChance(float coverage,float depthRaw)
{
    if (coverage < gPlacementThreshold)
    {
        float chanceSpawn = 1;
         chanceSpawn = pow(depthRaw, gChancePower) * gChanceMultiply;
        chanceSpawn *= (1 - coverage);
        return float4(chanceSpawn, chanceSpawn, chanceSpawn, 1);
    }
    else
    {
        return 0;
    }

}

float4 debugOutputSpawnChance(PSInput input)
{
    float3 gResolution;
    gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
    uint2 pixelPos = input.position;

    float2 uv = input.position.xy / float2(gResolution.x , gResolution.y );
    float4 sampledNormal = gNormal.SampleLevel(defaultSampler, uv, 0);

    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float coverage = EstimateSurfelCoverage(uv,depthRaw.x);
    float spawnChance = EstimateSpawnChance(coverage, depthRaw.x);
    
    return float4(spawnChance, spawnChance, spawnChance, 1);
}



float4 main(PSInput input) : SV_TARGET
{
    if(debugModeIndex.x == 0)
    {
        return debugOutputSpawnChance(input);
    }
    else if(debugModeIndex.x == 1)
    {
        //Unified debugging overlay ( Informed Spawn Chance In Green / Defaul spawn chance in red)
        float3 gResolution;
        gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
        uint2 pixelPos = input.position;
        float2 uv = input.position.xy / float2(gResolution.x, gResolution.y);
        float AO =  ambientOcclusion.Sample(defaultSampler, uv);

        //Possibly modify this by the augmented depth value to 
        //make it easier to react in AO in the distance
        float AOThreshold = 0.4f;
        float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);

        if(AO < AOThreshold)
        {
            //Ambient Occlussion is larger --> Resort to Surfel Cap spawning
            return float4(0, 1, 0, 1);
            
        }
        else
        {
            float coverage = EstimateSurfelCoverage(uv, depthRaw.x);
            float spawnChance = EstimateSpawnChance(coverage, depthRaw.x);
            return float4(spawnChance, 0, 0, 1);
            
        }






    }

    else if(debugModeIndex.x == 2)
    {
        float3 gResolution;
        gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
        uint2 pixelPos = input.position;
        float2 uv = input.position.xy / float2(gResolution.x, gResolution.y);
        return ambientOcclusion.Sample(defaultSampler, uv);

    }
    return float4(1, 0, 1, 1);
}
