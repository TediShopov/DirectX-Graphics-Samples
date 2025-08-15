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

float EstimateSurfelCoverage( float2 uv,float depthRaw, inout float maxContribution, inout int maxContributionSurfelIndex)
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

        //Bias is relative position from surfel world to the current reconstructed world 
        float3 bias = worldPos - (float3) surfel.position;

        float dist = length(bias);
        float contribution = 1.f;

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
