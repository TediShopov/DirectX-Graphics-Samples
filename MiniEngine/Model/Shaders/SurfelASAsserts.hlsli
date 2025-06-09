#include "SurfelUniformGridAccelerationStructure.hlsli"
//SOI - Sphere Of Influence determined from surfel radius

static UniformGrid testGrid;
static uint3 cellDimesions;

bool singleSufelExactlySizeOfCell_3X3COI(RWStructuredBuffer<uint> tb)
{
    //Surfel at center of the cell (50,50,50)
    //Size small - 10
    SurfelData surfel;
    surfel.position = float4(150, 150, 150,1);
    surfel.normal = float4(0, 1, 0,0);
    surfel.radius = 145;

    uint3 bb[2];
    SurfelSOIBoundingCells(surfel,testGrid,bb);

    //Spanning from 0th cell to 2th cell size should be 2-0+1=3
    uint3 bbSize = (bb[1] - bb[0]) + 1;

    tb[0] = bbSize;

    bool expectedSize =  bbSize.x == bbSize.y && bbSize.y == bbSize.z && bbSize.x == 3;
    return expectedSize;
}
bool singleSufelExactlySizeOfCell_9X9COI(RWStructuredBuffer<uint> tb)
{
    SurfelData surfel;
    surfel.position = float4(450, 450, 450,1);
    surfel.normal = float4(0, 1, 0,0);
    surfel.radius = 400;

    uint3 bb[2];
    SurfelSOIBoundingCells(surfel,testGrid,bb);
    uint3 bbSize = (bb[1] - bb[0]) + 1;

    bool expectedSize =  bbSize.x == bbSize.y && bbSize.y == bbSize.z && bbSize.x == 9;
    return expectedSize;
}

bool singleSurfelInCentreOfCell_ExactlyOneCellSOI()
{
    //Surfel at center of the cell (50,50,50)
    //Size small - 10
    SurfelData surfel;
    surfel.position = float4(50, 50, 50,1);
    surfel.normal = float4(0, 1, 0,0);
    surfel.radius = 10;

    
    uint3 overlappingIndices[128];
    uint overlappincCount;
    SurfelCountDebug(surfel, testGrid, overlappingIndices, overlappincCount);

    bool countIsOne = overlappincCount == 1;
    bool cellIsOOO = overlappingIndices[0] == uint3(0, 0, 0);
    return countIsOne && cellIsOOO;
}

bool singleSufelExactlySizeOfCell_OneCellSOI()
{
    //Surfel at center of the cell (50,50,50)
    //Size small - 10
    SurfelData surfel;
    surfel.position = float4(50, 50, 50,1);
    surfel.normal = float4(0, 1, 0,0);
    surfel.radius = 50;

    
    uint3 overlappingIndices[128];
    uint overlappincCount;
    SurfelCountDebug(surfel, testGrid, overlappingIndices, overlappincCount);

    bool countIsOne = overlappincCount == 1;
    bool cellIsOOO = overlappingIndices[0] == uint3(0, 0, 0);
    return countIsOne && cellIsOOO;
}


// Setup the test grid shared by the asserts
void runAsserts(out bool  results[8],RWStructuredBuffer<uint> tb)
{
    testGrid.gridOrigin = float3(0, 0, 0);
    testGrid.cellSize = float3(100,100,100);
    testGrid.dimensions = float3(1000,1000,1000);
    cellDimesions = (uint3) floor(testGrid.dimensions / testGrid.cellSize);

    results[0] = singleSufelExactlySizeOfCell_3X3COI(tb);
    results[1] = singleSufelExactlySizeOfCell_9X9COI(tb);
    results[2] = singleSufelExactlySizeOfCell_OneCellSOI();
    results[3] = singleSurfelInCentreOfCell_ExactlyOneCellSOI();
    
    
    

    
}
