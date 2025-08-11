
#include "MultiscaleMeanEstimator.hlsl"

struct SurfelData
{
    float4 position;
    float4 normal;
    float radius;
    float3 padding;
    uint2 tilePos;
    uint2 pixelPos;
    float4 randomValues;

    float4 color;
    uint4 raySamples; 

    float4 mean;
    uint4 contribution;

    MultiscaleMeanEstimatorData msme;

	uint isSurfelCap; //255 if it is 0 if not
	float height;
	float angle;
	float padSurfelCap;
};
struct UniformGrid
{
    float4 gridOrigin;
    float4 cellSize;
    float4 dimensions;
};

uint Hash(uint x)
{
    x ^= x >> 17;
    x *= 0xed5ad4bb;
    x ^= x >> 11;
    x *= 0xac4c1b51;
    x ^= x >> 15;
    x *= 0x31848bab;
    x ^= x >> 14;
    return x;
}

uint RandomUintInRange(inout uint seed, uint minVal, uint maxVal)
{
    uint range = maxVal - minVal;
    seed += 100;
    return minVal + (Hash(seed) % range);
}
float RandomFloat01(inout uint seed)
{
    seed += 100;
    return (float)(Hash(seed) & 0xFFFFFF) / 16777216.0f; // 2^24
}

uint GetThreadTemporalSeed(uint3 dispatchThreadID,  uint frameIndex)
{
    uint seed = dispatchThreadID.x * 73856093 ^ dispatchThreadID.y * 19349663 ^ dispatchThreadID.z * 83492791;
    seed ^= (frameIndex + 1) * 2654435761; // +1 prevents seed staying zero on frame 0
    return seed;
}

float LinearizeDepth(float z, float nearZ, float farZ)
{
    return (nearZ * farZ) / (farZ - z * (farZ - nearZ));
}

float RemapFloat(float value, float inMin, float inMax, float outMin, float outMax)
{
    float normalized = (value - inMin) / (inMax - inMin);
    normalized = saturate(normalized);
    return outMin + normalized * (outMax - outMin);
}

uint3 ComputeGridIndex(float3 position,float3 gridOrigin,float3 cellSize) {

    float3 relative = position - gridOrigin;
    uint3 res;
    return (uint3)floor(relative / cellSize);
}



uint HashGridIndex(uint3 gridIdx, UniformGrid grid) {

    uint3 dim = uint3(
    grid.dimensions.x / grid.cellSize.x,
    grid.dimensions.y / grid.cellSize.y,
    grid.dimensions.z / grid.cellSize.z
);
    return gridIdx.x  +
           gridIdx.y * dim.x   +
           gridIdx.z * dim.x * dim.y ;
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
    return true;
//    // Compute base corner of the grid cell
//    float3 base = grid.gridOrigin + (cellIndex * grid.cellSize);
//
//    // Precompute for convenience
//    float3 size = grid.cellSize;
//    float3 N = surfel.normal;
//    float3 P0 = surfel.position;
//
//    // 8 corners of the grid cell (relative to base)
//    float3 corners[8] = {
//        base,
//        base + float3(size.x, 0, 0),
//        base + float3(0, size.y, 0),
//        base + float3(0, 0, size.z),
//        base + float3(size.x, size.y, 0),
//        base + float3(size.x, 0, size.z),
//        base + float3(0, size.y, size.z),
//        base + size
//    };
//
//    // Compute dot products: dot(corner - P0, N)
//    float previousPointDot;
//
//
//    //Test the first cell corner to avoid an if in the for loop
//
//    float3 offset = corners[0] - P0;
//    previousPointDot = dot(offset, N);
//
//
//
//
//
//    
//
//    //Test the rest of the poitns
//    [unroll]
//    for (int i = 1; i < 8; ++i)
//    {
//        float3 offset = corners[i] - P0;
//        float currentPlaneDot = dot(offset, N);
//        if (sign(currentPlaneDot) != sign(previousPointDot))
//        {
//            //One cell corner is on one side of the plane while another is on the other 
//            //therefore the surfel disc has to cut it
//            return true;
//        }
//    }
//
//    //All the corner of the box lie on one of the side of the plane formed by the surfle
//    //the box does not overlap with the surfel
//    return false;

}

//Surfel Sphere of Influence bounding box;
void SurfelSOIBoundingCells(SurfelData surfel,UniformGrid grid, out uint3 bb[2])
{
    if(surfel.radius <= 0)
    {
        bb[0] = uint3(1, 1, 1);
        bb[1] = uint3(0, 0, 0);
    }
    


    float3 minBounds = surfel.position - surfel.radius;
    float3 maxBounds = surfel.position + surfel.radius;

    // Convert world-space bounds to grid indices
    uint3 minCell = ComputeGridIndex(minBounds, grid.gridOrigin, grid.cellSize);
    uint3 maxCell = ComputeGridIndex(maxBounds, grid.gridOrigin, grid.cellSize);

    bb[0] = minCell;
    bb[1] = maxCell;
}



//Max expected overlapp is expected to 128 cells
void SurfelCountDebug(SurfelData surfel, UniformGrid grid,out uint3 overlappingIndices[128], out uint overlappingCount)
{
    if(surfel.radius <= 0)
    {
        return;
    }
    uint3 bb[2];
    SurfelSOIBoundingCells(surfel, grid, bb);
    uint3 minCell = bb[0];
    uint3 maxCell = bb[1];

    overlappingCount = 0;

    // Iterate over overlapping cells
    for (uint z = minCell.z; z <= maxCell.z; ++z)
    {
        for (uint y = minCell.y; y <= maxCell.y; ++y)
        {
            for (uint x = minCell.x; x <= maxCell.x; ++x)
            {
                //uint linearIndex = x + y * gridDim.x + z * gridDim.x * gridDim.y;
                uint3 idx = uint3(x, y, z);
                uint linearIndex = HashGridIndex(idx,grid);
                    overlappingIndices[overlappingCount] = idx;
                    overlappingCount++;
//                if (ComputeCornerDotProductsWithSurfelPlane(surfel, grid, idx))
//                {
//                    overlappingIndices[overlappingCount] = idx;
//                    overlappingCount++;
//                }
            }
        }
    }


    
}


//A funciton to overlap the surfel (disc) with the grid 
void SurfelCount(SurfelData surfel, UniformGrid grid, RWStructuredBuffer<uint> surfelGrid)
{
    if(surfel.radius <= 0)
    {
        return;
    }
    uint3 bb[2];
    SurfelSOIBoundingCells(surfel, grid, bb);
    uint3 minCell = bb[0];
    uint3 maxCell = bb[1];


    // Iterate over overlapping cells
    for (uint z = minCell.z; z <= maxCell.z; ++z)
    {
        for (uint y = minCell.y; y <= maxCell.y; ++y)
        {
            for (uint x = minCell.x; x <= maxCell.x; ++x)
            {
                //uint linearIndex = x + y * gridDim.x + z * gridDim.x * gridDim.y;
                uint3 idx = uint3(x, y, z);

                uint linearIndex = HashGridIndex(idx,grid);
                InterlockedAdd(surfelGrid[linearIndex], 1);
//                if (ComputeCornerDotProductsWithSurfelPlane(surfel, grid, idx))
//                {
//                    //Surfel Overlaps with the grid cell
//                    InterlockedAdd(surfelGrid[linearIndex], 1);
//
//
//                }
            }
        }
    }


}

void InclusivePrefixSum(UniformGrid grid,RWStructuredBuffer<uint> surfelGrid )

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


void SurfelInsertion(uint surfelIndex, SurfelData surfel, UniformGrid grid, 
RWStructuredBuffer<uint> surlfeList,
RWStructuredBuffer<uint> surfelGrid
)
{
    if(surfel.radius <= 0)
    {
        return;
    }
    
    uint3 bb[2];
    SurfelSOIBoundingCells(surfel, grid, bb);
    uint3 minCell = bb[0];
    uint3 maxCell = bb[1];


    // Iterate over overlapping cells
    for (uint z = minCell.z; z <= maxCell.z; ++z)
    {
        for (uint y = minCell.y; y <= maxCell.y; ++y)
        {
            for (uint x = minCell.x; x <= maxCell.x; ++x)
            {
                //uint linearIndex = x + y * gridDim.x + z * gridDim.x * gridDim.y;
                uint3 idx = uint3(x, y, z);

                uint linearIndex = HashGridIndex(idx,grid);
                    //Surfel Overlaps with the grid cell
                    //Decrement the stored index in the grid
                    uint originalValue;
                    InterlockedAdd(surfelGrid[linearIndex], -1, originalValue);
                    //Insert the ID/PTR/OFFSET of the current surfel in the sufle list at the offt
                    surlfeList[originalValue] = surfelIndex;
//                if (ComputeCornerDotProductsWithSurfelPlane(surfel, grid, idx))
//                {
//                    //Surfel Overlaps with the grid cell
//                    //Decrement the stored index in the grid
//                    uint originalValue;
//                    InterlockedAdd(surfelGrid[linearIndex], -1, originalValue);
//                    //Insert the ID/PTR/OFFSET of the current surfel in the sufle list at the offt
//                    surlfeList[originalValue] = surfelIndex;
//
//
//                }
            }
        }
    }


    
}






//Adapted from https://m4xc.dev/blog/surfel-maintenance/
void FillAccelerationStructure(UniformGrid grid,
RWStructuredBuffer<SurfelData> surfels,
RWStructuredBuffer<uint> slist,
RWStructuredBuffer<uint> sgrid
)
{

    uint surfelNum, stride;
    surfels.GetDimensions(surfelNum,stride);

    //Pass 1. Surfel Counting
    for (uint i = 0; i < surfelNum; ++i)
    {
        SurfelCount(surfels[i], grid,sgrid);
    }
//    //Pass 2. Inclusive Prefix Sum on all the surfel grid buffer
    InclusivePrefixSum(grid,sgrid);
//
//
//    //Pass 3. Surfel Insertion 
    for (uint i = 0; i < surfelNum; ++i)
    {
        SurfelInsertion(i,surfels[i],grid,slist,sgrid);
    }
    
}
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

