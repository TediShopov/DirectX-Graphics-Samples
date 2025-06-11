
struct SurfelData
{
    float4 position;
    float4 normal;
    float radius;
    float3 padding;
    uint2 tilePos;
    uint2 pixelPos;
};
struct UniformGrid
{
    float4 gridOrigin;
    float4 cellSize;
    float4 dimensions;
};


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
        if (sign(currentPlaneDot) != sign(previousPointDot))
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

//Surfel Sphere of Influence bounding box;
void SurfelSOIBoundingCells(SurfelData surfel,UniformGrid grid, out uint3 bb[2])
{
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
                if (ComputeCornerDotProductsWithSurfelPlane(surfel, grid, idx))
                {
                    overlappingIndices[overlappingCount] = idx;
                    overlappingCount++;
                }
            }
        }
    }


    
}


//A funciton to overlap the surfel (disc) with the grid 
void SurfelCount(SurfelData surfel, UniformGrid grid, RWStructuredBuffer<uint> surfelGrid)
{
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
                if (ComputeCornerDotProductsWithSurfelPlane(surfel, grid, idx))
                {
                    //Surfel Overlaps with the grid cell
                    InterlockedAdd(surfelGrid[linearIndex], 1);


                }
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

