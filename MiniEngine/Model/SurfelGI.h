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
#include "IParameterBlock.h"

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
	UINT isSurfelCap; //255 if it is 0 if not
	float height;
	float angle;
	float padSurfelCap;
};
__declspec(align(16)) struct SurfelGenCB
{
	UINT	FrameIndex;
	float	DepthThreshold;
	float	NormalThreshold;
	float	MinRadius;

	UINT	MaxSurfels;
	int		SurfelCount;
	int		SurfelCellLimit;
	float	PlacementThreshold;

	float	RemovalTreshold;
	float	SpawnChancePower;
	float	SpawnChanceMultiplier;
	float	MaxRadius;

	Vector3 CameraPosition; // Vector3 still occupy 16 bytes
	XMFLOAT4 AOVariables; //Threshold and padding

	Vector4 Resolution; 
	UniformGrid UniformGrid;
};
__declspec(align(16)) struct SurfelDebugData
{
	UINT   PointedCellX;
	UINT   PointedCellY;
	UINT   PointedCellZ;
	UINT   PointedCellW;
};

//A Utility class for grouping togheter common buffers used in Surfel GI Solutoin.
//For each different surfel collectoin there is usually a
// 1. StrcuturedBuffer that is to be passed to the GPU
// 2. A Vector<T> that hold the actual that usualy initialized on CPU side
// 3. A readback buffer that read from (1) and write to (2)
// This is a single collection combinign this operations
template<typename T>
class MultiElementCommunicationBuffer
{
public:

	std::wstring m_name;
	std::vector<T> m_Actual;
	StructuredBuffer m_GPUBuffer;
	ReadbackBuffer m_Readback;
	UINT getByteSize()
	{
		return m_Actual.size() * sizeof(T);
	}
	MultiElementCommunicationBuffer<T>()
	{
		m_name = L"Default Multi Element Buffer";
		m_Actual = std::vector<T>();
	}
	MultiElementCommunicationBuffer<T>(const std::wstring& name, UINT elementCount)
	{
		m_name = name;
		m_Actual = std::vector<T>(elementCount);
	}

	void operator=(const MultiElementCommunicationBuffer<T> rhs)
	{
		this->m_name = rhs.m_name;
		this->m_Actual = rhs.m_Actual;
	}

	//Actually create the buffers with m_Actual as initial data
	void Create()
	{
		m_GPUBuffer.Create(m_name, m_Actual.size(), sizeof(T), m_Actual.data());
		m_Readback.Create(m_name, m_Actual.size(), sizeof(T));
	}
	//Write from CPU TO GPU
	void Write(GraphicsContext& gfx, D3D12_RESOURCE_STATES endState)
	{
		gfx.TransitionResource(m_GPUBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
		gfx.WriteBuffer(m_GPUBuffer, 0, m_Actual.data(), getByteSize());
		gfx.TransitionResource(m_GPUBuffer, endState, true);
	}
	void WriteFromExternal(GraphicsContext& gfx, D3D12_RESOURCE_STATES endState, std::vector<T> external, bool flushImmediate = false)
	{
		if (external.size() != m_Actual.size())
		{
			//TODO throw a warning
		}

		gfx.TransitionResource(m_GPUBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, flushImmediate);
		gfx.WriteBuffer(m_GPUBuffer, 0, external.data(), getByteSize());
		gfx.TransitionResource(m_GPUBuffer, endState, flushImmediate);
	}

	//Read form GPU TO CPU
	void Read(GraphicsContext& gfx, D3D12_RESOURCE_STATES endState, bool flushImmediate = false)
	{
		gfx.TransitionResource(m_GPUBuffer, D3D12_RESOURCE_STATE_COPY_DEST, flushImmediate);
		gfx.CopyBuffer(m_Readback, m_GPUBuffer);
		void* mappedData = m_Readback.Map();
		memcpy(m_Actual.data(), mappedData, getByteSize());
		m_Readback.Unmap();
		gfx.TransitionResource(m_GPUBuffer, endState, flushImmediate);
	}
};

class SurfelGI : public IParameterBlock
{
public:


	UINT _CELL_COUNT_;
	static const int _SURFEL_MAX_COUNT_ = 100000;
	static const int _SURFEL_PER_CELL = 250;
	//static const int _SURFEL_PER_CELL = 500;
	//static const int _SURFEL_PER_CELL = 1;

	//--- PIPELINE STATE OBJECTS --
	ComputePSO m_AccelerationPassPrefixSumInclusivePropagatePSO = { (L"Surfel Fill Acceleration Structure Pass 2 - Inclusive Propagate") };
	ComputePSO m_AccelerationPassPrefixSumReducePSO = { (L"Surfel Fill Acceleration Structure Pass 2 - Reduce ") };
	ComputePSO m_AccelerationPassPrefixSumScanPSO = { (L"Surfel Fill Acceleration Structure Pass 2 - Scan") };
	ComputePSO m_AccelerationPassPSO = { (L"Surfel Fill Acceleration Structure Pass Compute Shader Stage") };
	ComputePSO m_AccelerationPassSurfelCountPSO = { (L"Surfel Fill Acceleration Structure Pass 1") };
	ComputePSO m_AccelerationPassSurfelInsertionPSO = { (L"Surfel Fill Acceleration Structure Pass 3") };
	ComputePSO m_ApplicationPassPSO = { (L"Surfel Application Compute Shader Stage") };
	ComputePSO m_GenerationPassPSO = { (L"Surfel Generation Compute Shader Stage") };
	ComputePSO m_InformedGenerationPassPSO = { (L"Surfel Informed Generation Compute Shader Stage") };

	ComputePSO m_RecyclingPassPSO = { (L"Surfel Recycling Pass PSO") };

	//--- DESCRIPTOR HEAPS ---
	//--- ROOT SIGNATURES
	//RootSignature m_SurfelApplicationRT;
	DescriptorHeap descriptorHeap;
	DescriptorHeap nonShaderVisibleHeap;
	DescriptorHeap reduceThenScanPSHeap;
	DescriptorHeap uavHeap;
	RootSignature m_ReduceThenScanRT;
	RootSignature m_SurfelGenerationRT;
	RootSignature m_SurfelInformedGenerationRT;

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

	MultiElementCommunicationBuffer<UINT> m_SurfelList;
	MultiElementCommunicationBuffer<UINT> m_SurfelGrid;
	MultiElementCommunicationBuffer<SurfelData> m_SurfelData;

	MultiElementCommunicationBuffer<UINT> m_SurfelStack;

	//Fully initialized with zeroes
	std::vector<UINT> m_SurfelGridResetValues;

	//Adapted from https://m4xc.dev/blog/surfel-maintenance/
	//StructuredBuffer m_SurfelData;
	//StructuredBuffer m_SurfelStack; //A stack holding unique surfel IDs.Used for spawning and recycling surfels.
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

	ReadbackBuffer m_SurfelDebugReadback;

	//std::vector<UINT> m_SurfelStackActual;

	GBufferPtrs m_GBuffer;
	ColorBuffer* m_bentCones;
	ColorBuffer* m_AO;
	DescriptorHeap m_informedSpawningDescriptorHear;

	void UpdateProjection(const Camera& camera);

	void Setup(GBufferPtrs gBuff);
	void SetupInformed(ColorBuffer* m_bentCondesInput, ColorBuffer* AO);

	void SendParametersInformed(ComputeContext& gfxContext);
	void SendParametersInformedGraphics(GraphicsContext& gfxContext);

	void SpawnSurfels(ComputeContext& gfxContext, const Camera& camera);

	//Additionally uses the Ambient Occlusion data and Horizon-Angles from it
	//to inform the generation algorithm
	void SpawnSurfelsInformed(ComputeContext& gfxContext, const Camera& camera);

	//void SendParameters(ComputeContext& gfxContext,const Camera& camera);
	//void SendParametersGraphics(GraphicsContext& gfxContext, const Camera& camera);

	void SendParameters(ComputeContext& gfxContext);
	void SendParametersGraphics(GraphicsContext& gfxContext);
	//-FILLING ACCELERATION STRUCTURES -
	void FillAccelerationStructures(ComputeContext& gfxContext);

	//--FILLING ACCELERATIONG STRUCTURES WITH REDUCEN THEN SCAN -
	void FillAccelerationStructuresReduceThenScan(ComputeContext& gfxContext);
	void CopyPrefixInput(ComputeContext& gfxContext, StructuredBuffer* srcBuffer, StructuredBuffer* destBuffer);

	void FASSurfelCount(ComputeContext& gfxContext);
	void FASInclusivePrefixSum(ComputeContext& gfxContext);
	void FASSurfelInsertion(ComputeContext& gfxContext);

	void ReadbackSurfelData(GraphicsContext& gfx);
	void ReadbakcSurfelDebugData(GraphicsContext& gfx);
	void ReadbackSurfelAccelerationStructure(GraphicsContext& gfx);

	void ApplySurfels(ComputeContext& gfxContext, const Camera& camera);
	void RecycleSurfels(ComputeContext& gfxContext, const Camera& camera);
	int GetClosestSurfelToPosition(Vector3 worldPos);
	void RecreateSurfelListBuffers(int newNum);
	void TransitionResourcesTo();

	//Reset Surfels
	void ResetSurfels(GraphicsContext& gfx);
	void ResetSurfelsIrradiance(GraphicsContext& gfx);

	void RenderImGui() override {
		static bool spawnThresholdsCollapsingHeader = true;
		if (ImGui::CollapsingHeader("Spawning Thresholds", &spawnThresholdsCollapsingHeader))
		{
			ImGui::DragInt("Per Cell Surfel Limit", &m_SurfelGen.SurfelCellLimit);
			ImGui::DragFloat("Placement Threshold", &m_SurfelGen.PlacementThreshold,0.1f,0.0f,10.0f);
			ImGui::DragFloat("Removal Threshold", &m_SurfelGen.RemovalTreshold,0.1f,0.0f,10.0f);
		}

		static bool spawnChancesCollapsingHeader = true;
		if(ImGui::CollapsingHeader("Spawn Chances", &spawnChancesCollapsingHeader))
		{
			//Used for altering the 0-1 range chance
			ImGui::DragFloat("Chance Power", &m_SurfelGen.SpawnChancePower,0.01f,0.01f,1.2f);
			ImGui::DragFloat("Chance Mulitply", &m_SurfelGen.SpawnChanceMultiplier,1,1,150);
			ImGui::DragFloat("AO Threhold", &m_SurfelGen.AOVariables.x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Surfel Cap Min Radius", &m_SurfelGen.AOVariables.y, 1.0f, 1.0f, 300.0f);
			ImGui::DragFloat("Surlfe Cap Max Radius", &m_SurfelGen.AOVariables.z, 1.0f, 1.0f, 300.0f);
			ImGui::DragFloat("Lerp ", &m_SurfelGen.AOVariables.w, 0.01f, 0.0f, 1.0f);
		}
	}

    // Populates a JSON object with parameter data
	void ToJson(nlohmann::json& outJson) const {
		outJson["m_SurfelGen.SurfelCellLimit"] = m_SurfelGen.SurfelCellLimit;
		outJson["m_SurfelGen.PlacementThreshold"] = m_SurfelGen.PlacementThreshold;
		outJson["m_SurfelGen.RemovalTreshold"] = m_SurfelGen.RemovalTreshold;

		outJson["m_SurfelGen.SpawnChancePower"] = m_SurfelGen.SpawnChancePower;
		outJson["m_SurfelGen.SpawnChanceMultiplier"] = m_SurfelGen.SpawnChanceMultiplier;
		outJson["m_SurfelGen.AOVariables"] = m_SurfelGen.AOVariables.x ;
		outJson["m_SurfelGen.AOVariables.y"] = m_SurfelGen.AOVariables.y ;
		outJson["m_SurfelGen.AOVariables.z"] = m_SurfelGen.AOVariables.z ;
		outJson["m_SurfelGen.AOVariables.w"] = m_SurfelGen.AOVariables.w ;
	}

    // Loads parameter data from a JSON object
	void FromJson(const nlohmann::json& inJson) {
		 m_SurfelGen.SurfelCellLimit = inJson.value("m_SurfelGen.SurfelCellLimit",150);
		 m_SurfelGen.PlacementThreshold = inJson.value("m_SurfelGen.PlacementThreshold",1);
		 m_SurfelGen.RemovalTreshold = inJson.value("m_SurfelGen.RemovalTreshold",3);

		 m_SurfelGen.SpawnChancePower = inJson.value("m_SurfelGen.SpawnChancePower", 0.5);
		 m_SurfelGen.SpawnChanceMultiplier = inJson.value("m_SurfelGen.SpawnChanceMultiplier",2);
		 m_SurfelGen.AOVariables.x  = inJson.value("m_SurfelGen.AOVariables.x",0.5);
		 m_SurfelGen.AOVariables.y  = inJson.value("m_SurfelGen.AOVariables.y",20);
		 m_SurfelGen.AOVariables.z  = inJson.value("m_SurfelGen.AOVariables.z",30);
		 m_SurfelGen.AOVariables.w  = inJson.value("m_SurfelGen.AOVariables.w",0);


	}

protected:
	template<typename T>
	void CopyReadbackBuffer(GraphicsContext& gfx, ReadbackBuffer& dstReadbackBuffer, StructuredBuffer& srcBuffer, T& outData)
	{
		gfx.CopyBuffer(dstReadbackBuffer, srcBuffer);
		void* mappedData = dstReadbackBuffer.Map();
		memcpy(&outData, mappedData, sizeof(T));
		dstReadbackBuffer.Unmap();
	}
	template<typename T>
	void CopyReadbackBufferMany(GraphicsContext& gfx, ReadbackBuffer& dstReadbackBuffer, StructuredBuffer& srcBuffer, std::vector<T>& outData, int size = 1)
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


	UINT GetVectorizedSize(UINT origSize, UINT vectorLen = 4)
	{
		return (origSize + 3) / 4;
	}
};
