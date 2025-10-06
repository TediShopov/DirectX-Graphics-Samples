
#include "Common.hlsli"
#include "CommonSurfelRegisters.hlsli"
#include "SurfelSpawningUtility.hlsli"
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

// Returns true if intersection occurs between rayStart and rayEnd
bool SegmentDiscIntersection(
    float3 rayStart,
    float3 rayEnd,
    float3 discCenter,
    float3 discNormal,  // Should be normalized
    float  discRadius,
    out float t,        // 0.0 to 1.0 along segment
    out float3 hitPoint)
{
    float3 rayDir = rayEnd - rayStart;
    float  segLength = length(rayDir);

    // Avoid division by zero
    if (segLength < 1e-6f)
        return false;

    rayDir /= segLength; // Normalize direction

    // Ray-plane intersection
    float denom = dot(rayDir, discNormal);
    if (abs(denom) < 1e-6f)
        return false;

    float distToPlane = dot(discCenter - rayStart, discNormal) / denom;

    // If intersection is outside the segment
    if (distToPlane < 0.0f || distToPlane > segLength)
        return false;

    // Compute hit point
    hitPoint = rayStart + rayDir * distToPlane;

    // Check disc radius
    float distSq = dot(hitPoint - discCenter, hitPoint - discCenter);
    if (distSq > (discRadius * discRadius))
        return false;

    // Convert intersection distance to normalized segment parameter [0,1]
    t = distToPlane / segLength;
    return true;
}

float ContributionFromBentCone(float3 worldPos, float2 uv, out float3 bentNormal, out float radius, out float cosAngle,out float height)

{
    float4 sampleBentConeTexture = bentCones.SampleLevel(defaultSampler, uv, 0);
     bentNormal = sampleBentConeTexture.xyz;
    float AO = ambientOcclusion.SampleLevel(defaultSampler, uv, 0);
    float cosHalfAngle = (1 - AO);
    cosAngle = (1 - AO);

    //Static for now
     height = sampleBentConeTexture.w;
						 
    float sinHalfAngle = sqrt(1.0 - cos(cosHalfAngle));
    float tanHalfAngle = sinHalfAngle / cosHalfAngle;
     radius = abs(height) * tanHalfAngle * 2;

    //radius = min(maxRadius, radius);
    
    return  cosHalfAngle;

}

float EstimateSurfelCapSurfaceAreaCoverage( float2 uv,float depthRaw)
{

    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);
    float3 bentNormal;
    float radius;
    float cosAngle;
    float height;
    ContributionFromBentCone(worldPos, uv, bentNormal, radius, cosAngle, height);
    return 1-RemapFloat(radius, minRadius, maxRadius, 0.0, 1.0f);


}

float EstimateSurfelCapCoverage( float2 uv,float depthRaw)
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
        if(surfel.isSurfelCap == false)
            continue;

        float3 rayToWorldPosition = normalize(worldPos - cameraPosition.xyz);
        float t = 0.0f;
        float3 hitPoint ;
        float contribution = 1.f;
        if(SegmentDiscIntersection(cameraPosition, worldPos, surfel.position, surfel.normal, surfel.radius, t, hitPoint))
        {
            //If hit use the distance from the hit point to the world position
            float bias = length(hitPoint - worldPos);
            //contribution *= saturate(bias / (surfel.radius * 2));
            contribution *= 1;
        }
        else
        {
            contribution *= 0;
            
        }
        
        if(contribution > maxContribution)
        {
            maxContribution = contribution;
            
        }


        //coverage = contribution;

        //contribution = smoothstep(0, 1, contribution);
        //coverage += contribution;
    }
    coverage = maxContribution;
    return coverage;
}
float EstimateRecycleChance(float coverage)
{
    // If coverage is upper removal threshold,
    // remove surfel that most contribute to coverage probabilistically.
    if (coverage > gRemovalThreshold)
    {

        //float chanceRemove = pow(depthRaw, gChancePower) * gChanceMultiply;
        float chanceRemove = 1;
        return chanceRemove;


    }
    return 0;
    
}

float4 debugRecycleChance(PSInput input)
{
    float3 gResolution;
    gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
    uint2 pixelPos = input.position;

    float2 uv = input.position.xy / float2(gResolution.x , gResolution.y );
    float4 sampledNormal = gNormal.SampleLevel(defaultSampler, uv, 0);

    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float mC;
    uint mcIndex;
    float coverage = EstimateSurfelCoverage(uv,depthRaw.x,sampledNormal,mC,mcIndex);
    float recycleChance = EstimateRecycleChance(coverage);
    
    return float4(recycleChance, recycleChance, recycleChance, 1);
    
}

float4 debugOutputSpawnChance(PSInput input)
{
    float3 gResolution;
    gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
    uint2 pixelPos = input.position;

    float2 uv = input.position.xy / float2(gResolution.x , gResolution.y );
    float4 sampledNormal = gNormal.SampleLevel(defaultSampler, uv, 0);

    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float mC;
    uint mcIndex;
    float coverage = EstimateSurfelCoverage(uv,depthRaw.x,sampledNormal,mC,mcIndex);
    float spawnChance = EstimateSpawnChance(coverage, depthRaw.x);
    
    return float4(spawnChance, spawnChance, spawnChance, 1);
}
float4 debugSurfelCapOrNot(PSInput input)
{
    float3 gResolution;
    gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
    uint2 pixelPos = input.position;

    float2 uv = input.position.xy / float2(gResolution.x , gResolution.y );
    float4 sampledNormal = gNormal.SampleLevel(defaultSampler, uv, 0);

    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);

    uint2 surfelFromTo = ComputeRelevantSurfelRange(worldPos);
    uint surfelCount = surfelFromTo.y - surfelFromTo.x;

    //For each surfel into the current Surfle Acceleration Structure Cell
    //In this case is the uniform grid
    for (uint i = surfelFromTo.x; i < surfelFromTo.y; ++i)
    {
        uint surfelIndex = surlfeListUAV[i];
        SurfelData surfel = surfelsUAV[surfelIndex];



        //Bias is relative position from surfel world to the current reconstructed world 
        float3 bias = worldPos - (float3) surfel.position;
        if (length(bias) < surfel.radius/2.0f)
        {
            if (surfel.isSurfelCap)
            {


                
                
                return float4(0, 1, 0, 1);
            }
            else
            {
                return float4(1, 0, 0, 1);
                
            }
        }

    }
    return float4(0, 0, 0, 1);
}



float4 main(PSInput input) : SV_TARGET
{
    if(debugModeIndex.x == 0)
    {
        //Unified debugging overlay ( Informed Spawn Chance In Green / Defaul spawn chance in red)
        float3 gResolution;
        gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
        uint2 pixelPos = input.position;
        float2 uv = input.position.xy / float2(gResolution.x, gResolution.y);
        float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
        float AO = ambientOcclusion.Sample(defaultSampler, uv);
        float inverseAO = 1-AO;

        float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);
        float3 bentNormal;
        float radius;
        float cosAngle;
        float height;
        ContributionFromBentCone(worldPos, uv, bentNormal, radius, cosAngle, height);
        float RContribution = 1 - RemapFloat(radius, 0, AOVariables.y, 0, 1);
        float contribution = lerp(AO, RContribution, AOVariables.w);
        if (contribution < AOVariables.x)
        {
            float spawnChance = 1 - RemapFloat(radius, 0, AOVariables.z, 0, 1);
            return float4(0, 1-spawnChance, 0, 1);
        }
        else
        {
            return float4(1, 0, 0, 1);
            
        }
    }
    else if(debugModeIndex.x == 1)
    {
        //Render surfel based on if they have surfel cap flag
        return debugOutputSpawnChance(input);
        //return debugRecycleChance(input);

    }

    else if(debugModeIndex.x == 2)
    {
        return debugSurfelCapOrNot(input);
    }
    return float4(1, 0, 1, 1);
}
