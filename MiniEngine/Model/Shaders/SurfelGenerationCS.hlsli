// Constants and thresholds
#include "Common.hlsli"

struct SurfelData
{
    float4 position;
    float4 normal;
    float radius;
    float3 padding;
};
struct UniformGrid
{
    float3 gridOrigin;
    float3 cellSize;
    float3 dimensions;
};

cbuffer SurfelGenCB : register(b0)
{
    uint   FrameIndex;
    float  DepthThreshold;
    float  NormalThreshold;
    float  ViewDistThreshold;
    uint   MaxSurfels;
    UniformGrid Grid;
};

// G-buffer input textures
Texture2D<float4> gDepth : register(t0); // RGB = world pos
Texture2D<float4> gNormal   : register(t1); // RGB = world normal

RWStructuredBuffer<SurfelData> surfels : register(u0); // world position
// Stored pointer to a range of surfle IDs 
// SurfelList[from SurfelGrid[i] to SurfelGrid[i+1]] is all the surfel that occupy a cell with index I
RWStructuredBuffer<uint> surfelGrid : register(u1); 
RWStructuredBuffer<uint> surlfeList : register(u2); // Stored pointers (indices) to the appropriate surfel data


groupshared uint groupShareMinCoverage;
groupshared uint groupShareMaxContribution;


uint3 ComputeGridIndex(float3 position,float3 gridOrigin,float3 cellSize) {

    float3 relative = position - gridOrigin;
    uint3 res;
    return (uint3)floor(relative / cellSize);
}



uint HashGridIndex(uint3 gridIdx, uint3 cellSize) {
    return gridIdx.x  +
           gridIdx.y * cellSize.x   +
           gridIdx.z * cellSize.x * cellSize.y ;
}

uint UniqueHashGridIndex(uint3 gridIdx, uint3 cellSize) {
    const uint p1 = 73856093;
    const uint p2 = 19349663;
    const uint p3 = 83492791;
    return (gridIdx.x * p1) ^ (gridIdx.y * p2) ^ (gridIdx.z * p3);
}




// Returns an array of 8 dot products with the surfel plane
bool ComputeCornerDotProductsWithSurfelPlane(
    SurfelData surfel,
    UniformGrid grid,
    uint3 cellIndex)
{
    // Compute base corner of the grid cell
    float3 base = grid.gridOrigin + (cellIndex * grid.cellSize);

    // Precompute for convenience
    float3 size = grid.cellSize;
    float3 N = surfel.normal;
    float3 P0 = surfel.position;

    // 8 corners of the grid cell (relative to base)
    float3 corners[8] = {
        base,
        base + float3(size.x, 0, 0),
        base + float3(0, size.y, 0),
        base + float3(0, 0, size.z),
        base + float3(size.x, size.y, 0),
        base + float3(size.x, 0, size.z),
        base + float3(0, size.y, size.z),
        base + size
    };

    // Compute dot products: dot(corner - P0, N)
    float previousPointDot;


    //Test the first cell corner to avoid an if in the for loop

    float3 offset = corners[0] - P0;
    previousPointDot = dot(offset, N);





    

    //Test the rest of the poitns
    [unroll]
    for (int i = 1; i < 8; ++i)
    {
        float3 offset = corners[i] - P0;
        float currentPlaneDot = dot(offset, N);
        if (sign(currentPlaneDot) != previousPointDot)
        {
            //One cell corner is on one side of the plane while another is on the other 
            //therefore the surfel disc has to cut it
            return true;
        }
    }

    //All the corner of the box lie on one of the side of the plane formed by the surfle
    //the box does not overlap with the surfel
    return false;

}


//A funciton to overlap the surfel (disc) with the grid 
void SurfelCount(SurfelData surfel, UniformGrid grid)
{
    float3 minBounds = surfel.position - surfel.radius;
    float3 maxBounds = surfel.position + surfel.radius;

    // Convert world-space bounds to grid indices
    uint3 minCell = ComputeGridIndex(surfel.position, grid.gridOrigin, grid.cellSize);
    uint3 maxCell = ComputeGridIndex(surfel.position, grid.gridOrigin, grid.cellSize);


    // Iterate over overlapping cells
    for (uint z = minCell.z; z <= maxCell.z; ++z)
    {
        for (uint y = minCell.y; y <= maxCell.y; ++y)
        {
            for (uint x = minCell.x; x <= maxCell.x; ++x)
            {
                //uint linearIndex = x + y * gridDim.x + z * gridDim.x * gridDim.y;
                uint3 idx = uint3(x, y, z);
                uint linearIndex = HashGridIndex(idx,grid.cellSize);
                if (ComputeCornerDotProductsWithSurfelPlane(surfel, grid, idx))
                {
                    //Surfel Overlaps with the grid cell
                    InterlockedAdd(surfelGrid[linearIndex], 1);


                }
            }
        }
    }


}

void InclusivePrefixSum(UniformGrid grid)
{
    int surfelGridCellCount = 
    (grid.dimensions.x / grid.cellSize.x) * 
    (grid.dimensions.y / grid.cellSize.y) *
    (grid.dimensions.z / grid.cellSize.z);

    uint prefixSum = 0;

    for (uint i = 0; i < surfelGridCellCount; ++i)
    {
        //Inclusive Prefix Sum
        prefixSum += surfelGrid[i];
        surfelGrid[i] = prefixSum;
    }

}


void SurfelInsertion(uint surfelIndex, SurfelData surfel, UniformGrid grid)
{
    
    float3 minBounds = surfel.position - surfel.radius;
    float3 maxBounds = surfel.position + surfel.radius;

    // Convert world-space bounds to grid indices
    uint3 minCell = ComputeGridIndex(surfel.position, grid.gridOrigin, grid.cellSize);
    uint3 maxCell = ComputeGridIndex(surfel.position, grid.gridOrigin, grid.cellSize);


    // Iterate over overlapping cells
    for (uint z = minCell.z; z <= maxCell.z; ++z)
    {
        for (uint y = minCell.y; y <= maxCell.y; ++y)
        {
            for (uint x = minCell.x; x <= maxCell.x; ++x)
            {
                //uint linearIndex = x + y * gridDim.x + z * gridDim.x * gridDim.y;
                uint3 idx = uint3(x, y, z);
                uint linearIndex = HashGridIndex(idx,grid.cellSize);
                if (ComputeCornerDotProductsWithSurfelPlane(surfel, grid, idx))
                {
                    //Surfel Overlaps with the grid cell
                    //Decrement the stored index in the grid
                    uint originalValue;
                    InterlockedAdd(surfelGrid[linearIndex], -1, originalValue);
                    //Insert the ID/PTR/OFFSET of the current surfel in the sufle list at the offt
                    surlfeList[originalValue] = surfelIndex;


                }
            }
        }
    }


    
}

//Adapted from https://m4xc.dev/blog/surfel-maintenance/
void FillAccelerationStructure(UniformGrid grid)
{
    uint surfelNum, stride;
    surfels.GetDimensions(surfelNum,stride);

    //Pass 1. Surfel Counting
    for (uint i = 0; i < surfelNum; ++i)
    {
        SurfelCount(surfels[i], grid);
    }
    //Pass 2. Inclusive Prefix Sum on all the surfel grid buffer
    InclusivePrefixSum(grid);


    //Pass 3. Surfel Insertion 
    for (uint i = 0; i < surfelNum; ++i)
    {
        SurfelInsertion(i,surfels[i], grid);
    }
    
}


[numthreads(16, 16, 1)]
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

    uint2 tilePos = groupdId.xy;
    uint2 pixelPos = dispatchThreadId.xy;


    int index = pixelPos.x * gResolution.x + pixelPos.y;
    float2 uv = float2(dispatchThreadId.xy) / float2(gResolution.x - 1,gResolution.y -1);

    float4 atDepth = gDepth.SampleLevel(defaultSampler, uv,0);
    float4 atNormal = gNormal.SampleLevel(defaultSampler, uv,0);

    FillAccelerationStructure(Grid);

    

    GroupMemoryBarrierWithGroupSync();
}












    //surfelPosOut[index] =positionToOutput ;
    //surfelNormalOut[index] = normalToOutput;
    

    //RNG randomState;
    //randomState.init(pixelPos, gFrameIndex);

//    HitInfo hitInfo = HitInfo(gPackedHitInfo[pixelPos]);
//    if (!hitInfo.isValid())
//        return;
//
    //--- RETRIEVE POSITION OF TRIANGLE HIT ??? RAYTRACED TO SPAWN SURFEL POSITION ???
//    TriangleHit triangleHit = hitInfo.getTriangleHit();
//    VertexData v = gScene.getVertexData(triangleHit);
//    float4 curPosH = mul(gScene.camera.data.viewProjMatNoJitter, float4(v.posW, 1.f));
//    float depth = curPosH.z / curPosH.w;
//
    //--- GET SCREE SPACE CELL UNIT. UNIFORM GRID ? 
//    int3 cellPos = getCellPos(v.posW, gScene.camera.getPosition(), kCellUnit);
//    if (!isCellValid(cellPos))
//        return;
//
//    //--- RETRIEVE THE INFO RELATED TO THE CELL ITSELF
//    uint flattenIndex = getFlattenCellIndex(cellPos);
//    CellInfo cellInfo = gCellInfoBuffer[flattenIndex];

    // Evaluate min coverage value and pixel position.
    // Also evaluate max contribution and surfel index (for handling over-coverage).
    // Also evalute weighted color output (indrect lighting).
//    float4 indirectLighting = float4(0.f);
//    {
//        float coverage = 0.f;
//        float varianceEx = 0.f;
//        float rayCountEx = 0.f;
//        uint refCount = 0;
//        uint life = 0;
//
//        float maxVariance = 0.f;
//        float maxContribution = 0.f;
//        uint maxContributionSurfelIndex = randomState.next_uint(cellInfo.surfelCount);
//
//        for (uint i = 0; i < cellInfo.surfelCount; ++i)
//        {
//            uint surfelIndex = gCellToSurfelBuffer[cellInfo.cellToSurfelBufferOffset + i];
//            Surfel surfel = gSurfelBuffer[surfelIndex];
//
//            float3 bias = v.posW - surfel.position;
//            float dist2 = dot(bias, bias);
//
//            if (dist2 < surfel.radius * surfel.radius)
//            {
//                float3 normal = normalize(surfel.normal);
//
//                float dotN = dot(v.normalW, normal);
//                if (dotN > 0)
//                {
//                    const SurfelRecycleInfo surfelRecycleInfo = gSurfelRecycleInfoBuffer[surfelIndex];
//                    bool isSleeping = surfelRecycleInfo.status & 0x0001;
//                    bool lastSeen = surfelRecycleInfo.status & 0x0002;
//
//                    if (!isSleeping)
//                    {
//                        float dist = sqrt(dist2);
//                        float contribution = 1.f;
//
//                        contribution *= saturate(dotN);
//                        contribution *= saturate(1 - dist / surfel.radius);
//                        contribution = smoothstep(0, 1, contribution);
//
//                        coverage += contribution;
//
//                        #ifdef USE_SURFEL_DEPTH
//                        {
//                            float2 uv = getSurfelDepthUV(surfelIndex, bias / dist, normal);
//                            float2 surfelDepth = gSurfelDepth.SampleLevel(gSurfelDepthSampler, uv, 0u);
//
//                            float mean = surfelDepth.x;
//                            float sqrMean = surfelDepth.y;
//
//                            if (dist > mean)
//                            {
//                                float variance = sqrMean - pow(mean, 2);
//                                contribution *= variance / (variance + pow(dist - mean, 2));
//                            }
//                        }
//                        #else  // USE_SURFEL_DEPTH
//                        #endif // USE_SURFEL_DEPTH
//
//                        // Delay blending if not sufficient sample is accumulated.
//                        // Because samples are updated per frame, so do not use sample count directly.
//                        indirectLighting += float4(surfel.radiance, 1.f) * contribution * smoothstep(0, gBlendingDelay, surfelRecycleInfo.frame);
//
//                        varianceEx += length(surfel.msmeData.variance) * contribution;
//                        rayCountEx += surfel.rayCount * contribution;
//
//                        refCount = max(refCount, gSurfelRefCounter.Load(surfelIndex));
//                        life = max(life, surfelRecycleInfo.life);
//
//                        if (maxContribution < contribution)
//                        {
//                            maxContribution = contribution;
//                            maxContributionSurfelIndex = i;
//                        }
//
//                        maxVariance = max(maxVariance, length(surfel.msmeData.variance));
//                    }
//
//                    if (!lastSeen)
//                        gSurfelRecycleInfoBuffer[surfelIndex].status |= 0x0002;
//                }
//            }
//        }

//        if (indirectLighting.w > 0)
//        {
//            indirectLighting.xyz /= indirectLighting.w;
//            indirectLighting.w = saturate(indirectLighting.w);
//
//            varianceEx /= indirectLighting.w;
//            rayCountEx /= indirectLighting.w;
//
//            // Write texture by overlay mode.
//            if (gOverlayMode == 0)
//            {
//                gOutput[pixelPos] = indirectLighting;
//            }
//            else if (gOverlayMode == 1)
//            {
//                gOutput[pixelPos] = float4(stepColor(maxVariance * gVarianceSensitivity, 0.8f, 0.5f), 1);
//            }
//            else if (gOverlayMode == 2)
//            {
//                gOutput[pixelPos] = float4(stepColor(rayCountEx, 48.f, 32.f), 1);
//            }
//            else if (gOverlayMode == 3)
//            {
//                gOutput[pixelPos] = float4(lerpColor(refCount / 256.f), 1);
//            }
//            else if (gOverlayMode == 4)
//            {
//                gOutput[pixelPos] = float4(step(life, 0u), step(1u, life), 0, 1);
//            }
//            else if (gOverlayMode == 5)
//            {
//                gOutput[pixelPos] = float4(lerpColor(smoothstep(gPlacementThreshold, gRemovalThreshold, coverage)), 1);
//            }
//        }
//
//        uint coverageData = 0;
//        coverageData |= ((f32tof16(coverage) & 0x0000FFFF) << 16);
//        coverageData |= ((randomState.next_uint(255) & 0x000000FF) << 8);
//        coverageData |= ((groupThreadID.x & 0x0000000F) << 4);
//        coverageData |= ((groupThreadID.y & 0x0000000F) << 0);
//
//        InterlockedMin(groupShareMinCoverage, coverageData);
//
//        uint contributionData = 0;
//        contributionData |= ((f32tof16(maxContribution) & 0x0000FFFF) << 16);
//        contributionData |= ((maxContributionSurfelIndex & 0x0000FFFF) << 0);
//
        //InterlockedMax(groupShareMaxContribution, contributionData);


//    uint coverageData = groupShareMinCoverage;
//    float coverage = f16tof32((coverageData & 0xFFFF0000) >> 16);
//    uint x = (coverageData & 0x000000F0) >> 4;
//    uint y = (coverageData & 0x0000000F) >> 0;
//
//    if (cellInfo.surfelCount < kPerCellSurfelLimit)
//    {
//        if (groupThreadID.x == x && groupThreadID.y == y)
//        {
//            // If seat for surfel in current cell avaliable and coverage is under threshold,
//            // genearte new surfel probabilistically.
//            if (coverage <= gPlacementThreshold)
//            {
//                const float chance = pow(depth, gChancePower);
//                if (randomState.next_float() < chance * gChanceMultiply)
//                {
//                    int freeSurfelCount;
//                    gSurfelCounter.InterlockedAdd((int)SurfelCounterOffset::FreeSurfel, -1, freeSurfelCount);
//
//                    if (0 < freeSurfelCount && freeSurfelCount <= kTotalSurfelLimit)
//                    {
//                        uint validSurfelCount;
//                        gSurfelCounter.InterlockedAdd((int)SurfelCounterOffset::ValidSurfel, 1, validSurfelCount);
//
//                        if (validSurfelCount < kTotalSurfelLimit)
//                        {
//                            uint newIndex = gSurfelFreeIndexBuffer[freeSurfelCount - 1];
//
//                            float varRadius = calcSurfelRadius(
//                                distance(gScene.camera.getPosition(), v.posW),
//                                gFOVy,
//                                gResolution,
//                                kSurfelTargetArea,
//                                kCellUnit
//                            );
//
//                            Surfel newSurfel = Surfel(v.posW, v.normalW, varRadius);
//
//                            newSurfel.radiance = indirectLighting.xyz;
//                            newSurfel.msmeData.mean = indirectLighting.xyz;
//                            newSurfel.msmeData.shortMean = indirectLighting.xyz;
//
//                            gSurfelValidIndexBuffer[validSurfelCount] = newIndex;
//                            gSurfelBuffer[newIndex] = newSurfel;
//                            gSurfelRecycleInfoBuffer[newIndex] = { kMaxLife, 0u, 0u };
//                            gSurfelGeometryBuffer[newIndex] = hitInfo.data;
//                            gSurfelRefCounter.Store(newIndex, 0);
//                        }
//                    }
//                }
//            }
//        }
//    }
//
//    if (cellInfo.surfelCount > 0)
//    {
//        if (groupThreadID.x == x && groupThreadID.y == y)
//        {
//            // If coverage is upper removal threshold,
//            // remove surfel that most contribute to coverage probabilistically.
//            if (coverage > gRemovalThreshold)
//            {
//                const float chance = pow(depth, gChancePower);
//                if (randomState.next_float() < chance * gChanceMultiply)
//                {
//                    uint contributionData = groupShareMaxContribution;
//                    float maxContribution = f16tof32((contributionData & 0xFFFF0000) >> 16);
//                    uint maxContributionSurfelIndex = (contributionData & 0x0000FFFF) >> 0;
//
//                    uint toDestroySurfelIndex = gCellToSurfelBuffer[cellInfo.cellToSurfelBufferOffset + maxContributionSurfelIndex];
//                    Surfel toDestroySurfel = gSurfelBuffer[toDestroySurfelIndex];
//
//                    toDestroySurfel.radius = 0;
//                    gSurfelBuffer[toDestroySurfelIndex] = toDestroySurfel;
//                }
//            }
//        }
//    }
