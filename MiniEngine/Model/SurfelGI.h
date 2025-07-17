#pragma once
#include <d3d12.h>
#include <dxcapi.h>

#include <vector>
#include <initializer_list>

#include "RootSignature.h"
#include "Math/Vector.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "ReadbackBuffer.h"
#include "Camera.h"
#include "UniformGrid.h"

#include "ExtendedUtility.h"

#include "ModelH3D.h"

using namespace DirectX;
using namespace Math;

__declspec(align(16)) struct MSME
{
  Vector4 mean;

  Vector3 shortMean;
  float vbbr;

  Vector3 variance;
  float inconsistency;

};

__declspec(align(16)) struct SurfelData
{
	Vector4 position;
	Vector4 normal;
	Vector4 radius;

	UINT tilePosX;
	UINT tilePosY;
	UINT pixelPosX;
	UINT pixelPosY;
	

	Vector4 randomFloats;
	Vector4 color;

	UINT raySamples;
	UINT pad;
	UINT pad2;
	UINT pad3;

	Vector4 meanOne;
	//16

	UINT contribution0;
	UINT contribution1;
	UINT contribution2;
	UINT contribution3;

	Vector4 mean;
	Vector4 shortMeanAndVBBR;
	Vector4 varianceAndInconsistency;
	//16

};
	__declspec(align(16)) struct SurfelGenCB
	{
		UINT   FrameIndex;
		float  DepthThreshold;
		float  NormalThreshold;
		float  minRadius;

		UINT   MaxSurfels;
		// Makes sure the struct after is properly aligned
		int CurrentSurfelCount;
		int kPerCellSurfelLimit = 20;
		int gPlacementThreshold = 2;

		int gRemovalThreshold = 0;
		float gChancePower = 1.1;
		float gChanceMultiply = 15;

		float maxRadius;

		UniformGrid UniformGrid;
	};
	__declspec(align(16)) struct SurfelDebugData
	{
		UINT   PointedCellX;
		UINT   PointedCellY;
		UINT   PointedCellZ;
		UINT   PointedCellW;
	};


class SurfelGI
{
public:

	UINT _CELL_COUNT_;
	static const int _SURFEL_MAX_COUNT_ = 100000;
	static const int _SURFEL_PER_CELL = 135;

	//--- PIPELINE STATE OBJECTS --

	ComputePSO m_GenerationPassPSO = { (L"Surfel Generation Compute Shader Stage") };
	ComputePSO m_ApplicationPassPSO = { (L"Surfel Application Compute Shader Stage") };

	ComputePSO m_AccelerationPassPSO = { (L"Surfel Fill Acceleration Structure Pass Compute Shader Stage") };
	ComputePSO m_AccelerationPassSurfelCountPSO= { (L"Surfel Fill Acceleration Structure Pass 1") };

	ComputePSO m_AccelerationPassPrefixSumReducePSO = { (L"Surfel Fill Acceleration Structure Pass 2 - Reduce ") };
	ComputePSO m_AccelerationPassPrefixSumScanPSO = { (L"Surfel Fill Acceleration Structure Pass 2 - Scan") };
	ComputePSO m_AccelerationPassPrefixSumInclusivePropagatePSO = { (L"Surfel Fill Acceleration Structure Pass 2 - Inclusive Propagate") };

	ComputePSO m_AccelerationPassSurfelInsertionPSO = { (L"Surfel Fill Acceleration Structure Pass 3") };

	ComputePSO m_RecyclingPassPSO = { (L"Surfel Recycling Pass PSO") };

	//--- ROOT SIGNATURES
	RootSignature m_SurfelGenerationRT;
	RootSignature m_ReduceThenScanRT;
	//RootSignature m_SurfelApplicationRT;

	//--- DESCRIPTOR HEAPS ---
	DescriptorHeap descriptorHeap;
	DescriptorHeap reduceThenScanPSHeap;
	DescriptorHeap nonShaderVisibleHeap;
	DescriptorHeap uavHeap;



	__declspec(align(16)) struct ProjectionResources {

		XMMATRIX invViewProjeciton;
		float depthNear;
		float depthFar;
		float fovY;
	};

	ProjectionResources m_ProjectionData;

	SurfelGenCB m_SurfelGen;

	ByteAddressBuffer m_SufelSettingBuffer;
	ByteAddressBuffer m_ProjectoinBuffer;
	//Adapted from https://m4xc.dev/blog/surfel-maintenance/
	StructuredBuffer m_SurfelData;
	StructuredBuffer m_SurfelList;
	StructuredBuffer m_SurfelGrid;
	StructuredBuffer m_SurfelStack; //A stack holding unique surfel IDs.Used for spawning and recycling surfels.
	StructuredBuffer m_SurfelDebug; 


	__declspec(align(16))
		struct  PrefixSum {
		UINT e_vectorizedSize;
		UINT e_threadBlocks;
		UINT e_isPartial;
		UINT e_fullDispatches;
	};
	StructuredBuffer m_ReductionBuffer; 

	StructuredBuffer m_PrefixSumBuffer; 
	StructuredBuffer m_PrefixSumInput; 

	PrefixSum m_PrefixSum; 

	ColorBuffer m_OutputTexture; 

	SurfelDebugData m_SurfelDebugActual;

	//GPU->CPU readback buffer
	ReadbackBuffer m_SurfelDataReadback;
	ReadbackBuffer m_SurfelGridReadback;
	ReadbackBuffer m_SurfelListReadback;
	ReadbackBuffer m_SurfelDebugReadback;

	//CPU->GPU data containers to hold data to pass 
	std::vector<SurfelData> m_SurfelDataArray;
	std::vector<UINT> m_SurfelListActual;
	std::vector<UINT> m_SurfelGridActual;
	std::vector<UINT> m_SurfelStackActual;

	GBufferPtrs m_GBuffer;

	void UpdateProjection(const Camera& camera);

	void Setup(GBufferPtrs gBuff);

	void SpawnSurfels(ComputeContext& gfxContext,const Camera& camera);

	//void SendParameters(ComputeContext& gfxContext,const Camera& camera);
	//void SendParametersGraphics(GraphicsContext& gfxContext, const Camera& camera);

	void SendParameters(ComputeContext& gfxContext);
	void SendParametersGraphics(GraphicsContext& gfxContext);
	//-FILLING ACCELERATION STRUCTURES -
	void FillAccelerationStructures(ComputeContext& gfxContext);

	//--FILLING ACCELERATIONG STRUCTURES WITH REDUCEN THEN SCAN -
	void FillAccelerationStructuresReduceThenScan(ComputeContext& gfxContext);
	void CopyPrefixInput(ComputeContext& gfxContext, StructuredBuffer* srcBuffer,StructuredBuffer* destBuffer);

	void FASSurfelCount(ComputeContext& gfxContext);
	void FASInclusivePrefixSum(ComputeContext& gfxContext);
	void FASSurfelInsertion(ComputeContext& gfxContext);




	void ReadbackSurfelData(GraphicsContext& gfx);
	void ReadbakcSurfelDebugData(GraphicsContext& gfx);
	void ReadbackSurfelAccelerationStructure(GraphicsContext& gfx);
	void ApplySurfels(ComputeContext& gfxContext,const Camera& camera);
	void RecycleSurfels(ComputeContext& gfxContext,const Camera& camera);
	int GetClosestSurfelToPosition(Vector3 worldPos);
	void RecreateSurfelListBuffers(int newNum);
	void TransitionResourcesTo();

protected:
	template<typename T>
void CopyReadbackBuffer(GraphicsContext& gfx, ReadbackBuffer& dstReadbackBuffer,  StructuredBuffer& srcBuffer, T& outData)
{
    gfx.CopyBuffer(dstReadbackBuffer, srcBuffer);
    void* mappedData = dstReadbackBuffer.Map();
    memcpy(&outData, mappedData, sizeof(T));
    dstReadbackBuffer.Unmap();
}
	template<typename T>
void CopyReadbackBufferMany(GraphicsContext& gfx, ReadbackBuffer& dstReadbackBuffer,  StructuredBuffer& srcBuffer, std::vector<T>& outData, int size = 1)
{
	gfx.InsertUAVBarrier(srcBuffer);
    gfx.CopyBuffer(dstReadbackBuffer, srcBuffer);
    void* mappedData = dstReadbackBuffer.Map();
    memcpy(outData.data(), mappedData, sizeof(T) * size);
    dstReadbackBuffer.Unmap();
}
	void CopyCPUContainersToRespectiveGPUBuffers();
	void CreateHeaps();
	void InitializePSOs();
	void InitializeBuffers();
	void CreateRootSig();
	void SetDefaultCBData();
	void FillCPUContainers();
	void CreateOutputTexture(ColorBuffer* ouputBuffer);


	UINT GetVectorizedSize(UINT origSize, UINT vectorLen=4)
	{
		return (origSize + 3) / 4;
	}

};

