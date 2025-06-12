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

#include "ExtendedUtility.h"

#include "ModelH3D.h"

using namespace DirectX;
using namespace Math;

struct UniformGrid {

	Vector4 gridOrigin;
	Vector4 cellSize;
	Vector4 dimensions;
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
};


class SurfelGI
{
public:

	UINT _CELL_COUNT_;
	const UINT _DEBUG_SURFEL_NUM = 1000;

	//--- PIPELINE STATE OBJECTS --

	ComputePSO m_SurfelGenerationPSO = { (L"Surfel Generation Compute Shader Stage") };
	ComputePSO m_SurfelAccelerationPassPSO = { (L"Surfel Fill Acceleration Structure Pass Compute Shader Stage") };

	//--- ROOT SIGNATURES
	RootSignature m_SurfelGenerationRT;

	//--- DESCRIPTOR HEAPS ---
	DescriptorHeap srvHeap;
	DescriptorHeap uavHeap;

	__declspec(align(16)) struct SurfelGenCB
	{
		UINT   FrameIndex;
		float  DepthThreshold;
		float  NormalThreshold;
		float  ViewDistThreshold;

		UINT   MaxSurfels;
		// Makes sure the struct after is properly aligned
		UINT Padding0;
		UINT Padding1;
		UINT Padding2;

		UniformGrid UniformGrid;
	};


	__declspec(align(16)) struct ProjectionResources {

		XMMATRIX invViewProjeciton;
		float depthNear;
		float depthFar;
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

	//GPU->CPU readback buffer
	ReadbackBuffer m_SurfelDataReadback;

	//CPU->GPU data containers to hold data to pass 
	std::vector<SurfelData> m_SurfelDataArray;
	std::vector<UINT> m_SurfelListActual;
	std::vector<UINT> m_SurfelGridActual;
	std::vector<UINT> m_SurfelStackActual;

	GBufferPtrs m_GBuffer;

	void UpdateProjection(const Camera& camera);

	void Setup(GBufferPtrs gBuff);

	void SpawnSurfels(ComputeContext& gfxContext,const Camera& camera);
	void FillAccelerationStructures(ComputeContext& gfxContext);
	void ReadbackSurfelData(GraphicsContext& gfx);
	int GetClosestSurfelToPosition(Vector3 worldPos);

protected:
	void CopyCPUContainersToRespectiveGPUBuffers();
	void CreateHeaps();
	void InitializePSOs();
	void InitializeBuffers();
	void CreateRootSig();
	void SetDefaultCBData();
	void FillCPUContainers();
};

