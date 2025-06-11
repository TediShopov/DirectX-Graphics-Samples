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
bool surfelMultipelCells_OverlappingSurfelPlaneOnly3SOI(RWStructuredBuffer<uint> tb)
{
    //Surfel at center of the cell (50,50,50)
    //Size small - 10
    SurfelData surfel;
    surfel.position = float4(150, 150, 150,1);
    surfel.normal = float4(0, 1, 0,0);
    surfel.radius = 100;

    
    uint3 overlappingIndices[128];
    uint overlappincCount;
    SurfelCountDebug(surfel, testGrid, overlappingIndices, overlappincCount);

    tb[0] = overlappingIndices[0];
    tb[1] = overlappingIndices[overlappincCount];

    bool whenNormalUp = overlappincCount == 9;

    surfel.normal = float4(0, 0, 1,0);
    SurfelCountDebug(surfel, testGrid, overlappingIndices, overlappincCount);
    tb[0] = overlappingIndices[0];
    tb[1] = overlappingIndices[overlappincCount];
    bool whenNormalForward =overlappincCount == 9;

    surfel.normal = float4(1, 0, 0,0);
    SurfelCountDebug(surfel, testGrid, overlappingIndices, overlappincCount);
    tb[0] = overlappingIndices[0];
    tb[1] = overlappingIndices[overlappincCount];
    bool whenNormalLeft =overlappincCount == 9;
    

    //SOI is 3 in each dimension -> 3x3x3
    //However the surfel is facing up so it will occupy only 3x3 in the middle 
    // the SOI area

    return whenNormalForward && whenNormalUp && whenNormalLeft;
}
bool surfelMultipelCells_OverlappingSurfelPlaneOnly5SOI(RWStructuredBuffer<uint> tb)
{
    //Surfel at center of the cell (50,50,50)
    //Size small - 10
    SurfelData surfel;
    surfel.position = float4(250, 250, 250,1);
    surfel.normal = float4(0, 1, 0,0);
    surfel.radius = 245;

    //SOI is 5 in each dimension -> 5x5x5
    //However the surfel is facing up so it will occupy only 5x5 in the middle 
    // the SOI area
    
    uint3 overlappingIndices[128];
    uint overlappincCount;
    SurfelCountDebug(surfel, testGrid, overlappingIndices, overlappincCount);

    tb[0] = overlappingIndices[0];
    tb[1] = overlappingIndices[overlappincCount];

    bool whenNormalUp = overlappincCount == 25;

    surfel.normal = float4(0, 0, 1,0);
    SurfelCountDebug(surfel, testGrid, overlappingIndices, overlappincCount);
    tb[0] = overlappingIndices[0];
    tb[1] = overlappingIndices[overlappincCount];
    bool whenNormalForward =overlappincCount == 25;

    surfel.normal = float4(1, 0, 0,0);
    SurfelCountDebug(surfel, testGrid, overlappingIndices, overlappincCount);
    tb[0] = overlappingIndices[0];
    tb[1] = overlappingIndices[overlappincCount];
    bool whenNormalLeft =overlappincCount == 25;

    return whenNormalForward && whenNormalUp && whenNormalLeft;

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
    testGrid.gridOrigin = float4(0, 0, 0,0);
    testGrid.cellSize = float4(100,100,100,100);
    testGrid.dimensions = float4(1000,1000,1000,1000);
    cellDimesions = (uint3) floor(testGrid.dimensions / testGrid.cellSize);

    results[0] = singleSufelExactlySizeOfCell_3X3COI(tb);
    results[1] = singleSufelExactlySizeOfCell_9X9COI(tb);
    results[2] = singleSufelExactlySizeOfCell_OneCellSOI();
    results[3] = singleSurfelInCentreOfCell_ExactlyOneCellSOI();
    results[4] = surfelMultipelCells_OverlappingSurfelPlaneOnly3SOI(tb);
    results[5] = surfelMultipelCells_OverlappingSurfelPlaneOnly5SOI(tb);
    
    
    

    
}
