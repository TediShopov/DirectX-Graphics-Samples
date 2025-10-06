//#include "SurfelASAsserts.hlsli"

#define M_PI 3.1415926
float3 ReconstructWorldPosition(float2 uv, float depth, float4x4 invViewProj)
{
    float4 ndc;
    // Convert from UV to NDC [-1, 1]
    ndc.xy = uv * 2.0 - 1.0;
    // For some reason Y has to be flipped
    ndc.y = -ndc.y;
    // Using Raw Depth
    ndc.z =  depth;
    ndc.w = 1.0;
    float4 worldPos = mul( invViewProj,ndc);
    return worldPos.xyz / worldPos.w;
}

float calcProjectArea(float radius, float distance, float fovy, uint2 resolution)
{
    float projRadius = atan(radius / distance) * max(resolution.x, resolution.y) / fovy;
    return M_PI * projRadius * projRadius;
}

float calcRadiusApprox(float area, float distance, float fovy, uint2 resolution)
{
    return distance * tan(sqrt(area / M_PI) * fovy / max(resolution.x, resolution.y));
}

float calcRadius(float area, float distance, float fovy, uint2 resolution)
{
    float cosTheta = 1 - (area * (1 - cos(fovy / 2)) / M_PI) / (resolution.x * resolution.y);
    float sinTheta = sqrt(1 - pow(cosTheta, 2));
    return distance * sinTheta;
}

float calcSurfelRadius(float distance, float fovy, uint2 resolution, float area, float cellUnit)
{
    return min(calcRadiusApprox(area, distance, fovy, resolution), cellUnit * 2);
}

uint PackCoverageData(float coverage, uint threadRandomnessSeed,uint3 groupThreadID)
{
    uint coverageData = 0;
    coverageData |= ((f32tof16(coverage) & 0x0000FFFF) << 16);
    coverageData |= ((RandomUintInRange(threadRandomnessSeed, 0, 255) & 0x000000FF) << 8);
    coverageData |= ((groupThreadID.x & 0x0000000F) << 4);
    coverageData |= ((groupThreadID.y & 0x0000000F) << 0);
    return coverageData;
}
void UnpackCoverageData(uint coverageData,out float coverage, out uint threadRandomnessSeed,out uint3 groupThreadID)
{
    groupThreadID.x  = coverageData & 0xF; // bits 0–3
    groupThreadID.y = (coverageData >> 4) & 0xF; // bits 4–7
    threadRandomnessSeed = (coverageData >> 8) & 0xFF; // bits 8–15
    coverage = f16tof32((coverageData >> 16) & 0xFFFF); // bits 16–31
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
float EstimateSurfelCoverageInformed( float2 uv,float depthRaw,float3 normal, inout float maxContribution, inout int maxContributionSurfelIndex)
{

    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);

    uint2 surfelFromTo = ComputeRelevantSurfelRange(worldPos);
    uint surfelCount = surfelFromTo.y - surfelFromTo.x;
    float coverage = 0;

    //For each surfel into the current Surfle Acceleration Structure Cell
    //In this case is the uniform grid
    for (uint i = surfelFromTo.x; i < surfelFromTo.y; ++i)
    {
        uint surfelIndex = surlfeListUAV[i];
        SurfelData surfel = surfelsUAV[surfelIndex];
        float dotN = 1;

        if(surfel.isSurfelCap == false)
        {
            dotN = dot(normal, surfel.normal);
        }

        //Bias is relative position from surfel world to the current reconstructed world 
        float3 bias = worldPos - (float3) surfel.position;

        float dist = length(bias);
        float contribution = 0.f;

        if(dotN > 0.0f)
        {
            contribution = 1.f;
            contribution *= dotN;
            contribution *= saturate(1 - dist / surfel.radius);
            contribution = smoothstep(0, 1, contribution);
            
        }

        coverage += contribution;

        if (maxContribution < contribution)
        {
            maxContribution = contribution;
            maxContributionSurfelIndex = i;
        }
    }
    return coverage;
}

float EstimateSurfelCoverage( float2 uv,float depthRaw,float3 normal, inout float maxContribution, inout int maxContributionSurfelIndex)
{

    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);

    uint2 surfelFromTo = ComputeRelevantSurfelRange(worldPos);
    uint surfelCount = surfelFromTo.y - surfelFromTo.x;
    float coverage = 0;

    //For each surfel into the current Surfle Acceleration Structure Cell
    //In this case is the uniform grid
    for (uint i = surfelFromTo.x; i < surfelFromTo.y; ++i)
    {
        uint surfelIndex = surlfeListUAV[i];
        SurfelData surfel = surfelsUAV[surfelIndex];


        //Bias is relative position from surfel world to the current reconstructed world 
        float3 bias = worldPos - (float3) surfel.position;
        float dotN = dot(normal, surfel.normal);

        float dist = length(bias);
        float contribution = 1.f;

        contribution *= dotN;
        contribution *= saturate(1 - dist / surfel.radius);
        contribution = smoothstep(0, 1, contribution);

        coverage += contribution;

        if (maxContribution < contribution)
        {
            maxContribution = contribution;
            maxContributionSurfelIndex = i;
        }
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

float ContributionFromBentCone(float3 worldPos, float2 uv, out float3 bentNormal, out float radius, out float cosAngle,out float height,Texture2D bentCones, Texture2D ambientOcclusion)

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
    //radius *= 2;

    //radius = min(maxRadius, radius);
    
    return  cosHalfAngle;

}

void AttemptSpawnSurfel(SurfelData newSurfel)
{
    uint prevStackPointer, prevSurfelCount;
    InterlockedAdd(surfleStackUAV[0], 1, prevStackPointer);
    InterlockedAdd(surfleStackUAV[1], 1, prevSurfelCount);
    uint surfelStackPointer = prevStackPointer + 1;

    if (surfelStackPointer < MaxSurfels + 2)
    {
        uint surfelID = surfleStackUAV[prevStackPointer];
        surfelsUAV[surfelID] = newSurfel;
    }
    else
    {
        InterlockedAdd(surfleStackUAV[0], -1);
        InterlockedAdd(surfleStackUAV[1], -1);

    }

}

float EstimateSurfelCapSurfaceAreaCoverage( float2 uv,float depthRaw, Texture2D bentCones,Texture2D ambientOcclusion)
{

    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);
    float3 bentNormal;
    float radius;
    float cosAngle;
    float height;
    ContributionFromBentCone(worldPos, uv, bentNormal, radius, cosAngle, height,bentCones,ambientOcclusion);
    return 1-RemapFloat(radius, minRadius, maxRadius, 0.0, 1.0f);


}
SurfelData SurfelPrototype(float3 worldPos,float depthRaw, float4 sampledNormal, float2 gResolution)
{
    float linearDepth = LinearizeDepth(depthRaw, depthFar, depthNear);
    float v = linearDepth;

    SurfelData newSurfel;
    float calcProjArea = calcProjectArea(10, 250, fovY, gResolution.xy);
    float varRadius = clamp(calcSurfelRadius(v, fovY, gResolution.xy, calcProjArea, 100000), minRadius, maxRadius);

    newSurfel.position = float4(worldPos, 1) + sampledNormal * 1.0f;
    newSurfel.randomValues = float4(0, 0, 0, 1);
    newSurfel.color = float4(0, 0, 0, 1);
    newSurfel.contribution = uint4(0, FrameIndex, 0, 0);
    newSurfel.mean = float4(0, 0, 0, 0);
    newSurfel.raySamples = float4(10, 0, 0, 0);
    newSurfel.padding = float3(FrameIndex, FrameIndex, FrameIndex);
    newSurfel.normal = sampledNormal;
    newSurfel.radius = varRadius;
//    newSurfel.tilePos = tilePos;
//    newSurfel.pixelPos = pixelPos;
    newSurfel.tilePos = float2(0, 0);
    newSurfel.pixelPos = float2(0, 0);

    newSurfel.msme.mean = float4(0, 0, 0, 0);
    newSurfel.msme.shortMean = float3(0, 0, 0);
    newSurfel.msme.variance = float3(1e-4, 1e-4, 1e-4);
    newSurfel.msme.inconsistency = 1;
    newSurfel.msme.vbbr = 1;

                    //Not a surfel cap by default
    newSurfel.isSurfelCap = 0;
    newSurfel.height = 0;
    newSurfel.angle = 0;
    newSurfel.padSurfelCap = 0;
    return newSurfel;
}
