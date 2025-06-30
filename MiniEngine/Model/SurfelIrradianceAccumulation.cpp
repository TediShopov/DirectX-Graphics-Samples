#include "SurfelIrradianceAccumulation.h"

#include "TestRaytracing.h"
#include "GraphicsCore.h"


#include "PipelineState.h"
#include "RootSignature.h"
#include "CommandContext.h"
#include "Camera.h"
#include "BufferManager.h"
#include "TemporalEffects.h"
#include <cstdint>
#include "Camera.h"
#include "Model.h"
#include "DescriptorHeapStack.h"
#include "ModelH3D.h"
#include "ExtendedUtility.h"
#include "Renderer.h"

namespace SurfelIrradianceAccumulation
{
	//ByteAddressBuffer	TestRaytracing::m_TestCB;
	UINT _RAYTRACE_MESH_COUNT_ = 33;
	ByteAddressBuffer	m_TestCB;

	std::unique_ptr<DescriptorHeapStack> g_pRaytracingDescriptorHeap = nullptr;

	// The names of the shaders. The actual shader code is found in the Raytracing.hlsl
	const wchar_t* c_hitGroupName = L"MyHitGroup";
	const wchar_t* c_raygenShaderName = L"MyRaygenShader";
	const wchar_t* c_closestHitShaderName = L"MyClosestHitShader";
	const wchar_t* c_missShaderName = L"MyMissShader";
	const wchar_t* c_shadowMissShaderName = L"ShadowMissShader";

	//DescriptorHeap m_materialDescriptorHeap;

	SunDirectionalLight directionalLightData;

	UINT m_descriptorSize;
	UINT m_descriptorsAllocated;
	ComPtr<ID3D12StateObject> m_dxrStateObject;
	//ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;

	ComPtr<ID3D12Resource> m_raytracingOutput;

	D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVCpuDescriptor;

	//Used for all the ray shaders
	ComPtr<ID3D12RootSignature> m_rtGlobalRootSignature;
	//Used by each individual shader and could be overwritten
	ComPtr<ID3D12RootSignature> m_rtRayGenLocalRootSignature;
	//Used specifically by the ray hit shader to pass flattened UVs and other material realated properties.
	ComPtr<ID3D12RootSignature> m_rtRayHitLocalRootSignate;

	//Stores the flattened indices as Buffer that can be passed on the GPU
	std::vector<StructuredBuffer> m_perInstanceFlattendUVs;

	//--- PER INSTANCE DATA ---

	//Per Instance Addtional Vertex Data
	std::vector<StructuredBuffer> m_perInstanceVertexData;
	//Per Instance Index Buffer
	std::vector<StructuredBuffer> m_perInstanceIndices;
	//Store the per instance diffuse textures
	std::vector<StructuredBuffer> m_perInstanceMaterial;

	ComPtr<ID3D12Resource> m_accelerationStructure;
	ComPtr<ID3D12Resource> m_bottomLevelAccelerationStructure;

	std::vector<ComPtr<ID3D12Resource>> m_blasArray;

	ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;

	//RayGenConstantBuffer m_rayGenCB;
	RayGet3DBuffer m_rayGenCB;
	ComPtr<ID3D12Resource> m_missShaderTable;
	ComPtr<ID3D12Resource> m_hitGroupShaderTable;
	ComPtr<ID3D12Resource> m_rayGenShaderTable;
	ColorBuffer m_raytracingColorBuffer;


	DescriptorHeap m_materialDescriptorHeap;

	float aspectRatio;
	UINT m_raytracingOutputResourceUAVDescriptorHeapIndex;

	 const UINT _GLOBAL_RAY_IRRADIANCE_BUDGET_ = 10000;
	 StructuredBuffer m_RayDispatchBuffer;
	std::vector<UINT> m_RayDispatchData;
	 UINT totalSamples = 0;
	 __declspec(align(16)) struct GridCB
	 {
		 UniformGrid Grid;
		 UINT frameIndex;
	 };


	void CreatePerInstanceCB(ModelH3D& model,UINT numMeshes) {
		m_perInstanceFlattendUVs.resize(numMeshes);

		for (UINT i = 0; i < numMeshes; i++)
		{
			auto& mesh = model.m_pMesh[i];
			auto uvAttribOffset = (UINT)mesh.attrib[ModelH3D::attrib_texcoord0].offset;
			D3D12_GPU_VIRTUAL_ADDRESS uvStartAddress =
				model.GetVertexBuffer().BufferLocation +
				mesh.vertexDataByteOffset +
				uvAttribOffset;

			uint8_t* basePtr = reinterpret_cast<uint8_t*>(model.m_pVertexData);
			basePtr += mesh.vertexDataByteOffset;
			std::vector<XMFLOAT2> uvData(mesh.vertexCount);

			//For loop to extract all the UV coordinates
			for (UINT j = 0; j < mesh.vertexCount; ++j)
			{
				//Append to an array
				float* uvPtr = reinterpret_cast<float*>(basePtr + j * mesh.vertexStride + uvAttribOffset);
				uvData[j] = XMFLOAT2(uvPtr[0], uvPtr[1]);
			}
			//Create in the appropriate per instance buffer
			m_perInstanceFlattendUVs[i].Create(L"Uv Buffer", mesh.vertexCount, sizeof(XMFLOAT2), uvData.data());
		}
	}

	void CreateRayDispatchData()
	{
		m_RayDispatchData.assign(_GLOBAL_RAY_IRRADIANCE_BUDGET_, 0);
		m_RayDispatchBuffer.Create(
			L"Ray Dispatch Data",
			_GLOBAL_RAY_IRRADIANCE_BUDGET_,
			sizeof(UINT),
			m_RayDispatchData.data());
	}

	//Compute an array of surfel indices
	//The array lenght would be corsesponding to the total number of ray budget
	//-> Each ray can access this data to determine which surfel it would be casted from
	void ComputeRayIndexing(std::vector<SurfelData>& data) {

		 totalSamples = 0;
		int surfelID = 0;
		int rayDispatchIndex = 0;
		for each (SurfelData s in data)
		{

			totalSamples += s.raySamples;
			
			for (size_t i = 0; i < s.raySamples; i++)
			{
				m_RayDispatchData[rayDispatchIndex] = surfelID;
				rayDispatchIndex++;

			}
			surfelID++;

		}
		int a = 3;

	}
	void CreateRaytracingInterfaces()
	{
		auto device = Graphics::g_Device;

		//Start a command context
		GraphicsContext& gfxContext = GraphicsContext::Begin();
		ID3D12GraphicsCommandList* pCmdList = gfxContext.GetCommandList();
		auto commandList = pCmdList;


		ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&device)), L"Couldn't get DirectX Raytracing interface for the device.\n");
		ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&commandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");

		gfxContext.Finish();
	}
	void ThrowIfFailed(HRESULT hr, const wchar_t* msg)
	{
		if (FAILED(hr))
		{
			OutputDebugString(msg);
			throw HrException(hr);
		}
	}
	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
	{
		//auto device = m_deviceResources->GetD3DDevice();
		ComPtr<ID3DBlob> blob;
		ComPtr<ID3DBlob> error;

		ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
		ThrowIfFailed(Graphics::g_Device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))), L"Creating Root Signature Failed");
	}
	void CreateRootSignatures()
	{
		// Global Root Signature
		// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
		{
			CD3DX12_DESCRIPTOR_RANGE UAVDescriptor;
			UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0);

			CD3DX12_DESCRIPTOR_RANGE MaterialDescriptor;
			MaterialDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 85, 0, 1);

			//CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams::Count];
			//rootParameters[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &UAVDescriptor);
			//rootParameters[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
			CD3DX12_ROOT_PARAMETER rootParameters[6];
			rootParameters[0].InitAsDescriptorTable(1, &UAVDescriptor);
			rootParameters[1].InitAsShaderResourceView(0);
			rootParameters[2].InitAsConstantBufferView(0);
			rootParameters[3].InitAsConstantBufferView(1);
			rootParameters[4].InitAsDescriptorTable(1,&MaterialDescriptor);
			rootParameters[5].InitAsConstantBufferView(2);


			CD3DX12_STATIC_SAMPLER_DESC staticSampler(
				0, // Shader Register (s0)
				D3D12_FILTER_MIN_MAG_MIP_LINEAR, // Filter
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // AddressU
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // AddressV
				D3D12_TEXTURE_ADDRESS_MODE_WRAP   // AddressW
			);

			staticSampler.Filter = D3D12_FILTER_ANISOTROPIC;
			staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSampler.MipLODBias = 0.0f;
			staticSampler.MaxAnisotropy = 16;
			staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			staticSampler.MinLOD = 0.0f;
			staticSampler.MaxLOD = D3D12_FLOAT32_MAX;

			CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters, 1, &staticSampler);
			SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &m_rtGlobalRootSignature);
		}

		//Empty Ray Gen Local Root Signature
		{

			CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(0, 0);
			localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_rtRayGenLocalRootSignature);
		}
		//Ray Hit Local Root Signature
		{
			CD3DX12_ROOT_PARAMETER rootParameters[3];
			rootParameters[0].InitAsConstantBufferView(3);
			rootParameters[1].InitAsUnorderedAccessView(5);
			rootParameters[2].InitAsUnorderedAccessView(6);

			CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
			localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_rtRayHitLocalRootSignate);
		}
	}

	// Local root signature and shader association
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
	{
		// Hit group and miss shaders in this sample are not using a local root signature and thus one is not associated with them.

		// Local root signature to be used in a ray gen shader.
		{
			auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
			localRootSignature->SetRootSignature(m_rtRayGenLocalRootSignature.Get());
			// Shader association
			auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
			rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
			rootSignatureAssociation->AddExport(c_raygenShaderName);
		}
		{
			auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
			localRootSignature->SetRootSignature(m_rtRayHitLocalRootSignate.Get());
			// Shader association
			auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
			rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
			rootSignatureAssociation->AddExport(c_closestHitShaderName);
		}
	}
	void CreateRaytracingPipelineStateObject()
	{
		// Create 7 subobjects that combine into a RTPSO:
		// Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
		// Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
		// This simple sample utilizes default shader association except for local root signature subobject
		// which has an explicit association specified purely for demonstration purposes.
		// 1 - DXIL library
		// 1 - Triangle hit group
		// 1 - Shader config
		// 2 - Local root signature and association
		// 1 - Global root signature
		// 1 - Pipeline config
		CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };


		// DXIL library
		// This contains the shaders and their entrypoints for the state object.
		// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
		auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
		D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(
			(void*)g_pSurfelIrradianceRayTracing, ARRAYSIZE(g_pSurfelIrradianceRayTracing));
		lib->SetDXILLibrary(&libdxil);
		// Define which shader exports to surface from the library.
		// If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
		// In this sample, this could be omitted for convenience since the sample uses all shaders in the library. 
		{
			lib->DefineExport(c_raygenShaderName);
			lib->DefineExport(c_closestHitShaderName);
			//lib->DefineExport(c_shadowHitShaderName);
			lib->DefineExport(c_missShaderName);
			lib->DefineExport(c_shadowMissShaderName);
		}

		// Triangle hit group
		// A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
		// In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
		auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
		hitGroup->SetClosestHitShaderImport(c_closestHitShaderName);
		hitGroup->SetHitGroupExport(c_hitGroupName);
		hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

		// Shader config
		// Defines the maximum sizes in bytes for the ray payload and attribute structure.
		auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
		UINT payloadSize = 4 * sizeof(float);   // float4 color
		UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
		shaderConfig->Config(payloadSize, attributeSize);

		// Local root signature and shader association
		CreateLocalRootSignatureSubobjects(&raytracingPipeline);
		// This is a root signature that enables a shader to have unique arguments that come from shader tables.

		// Global root signature
		// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
		auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
		globalRootSignature->SetRootSignature(m_rtGlobalRootSignature.Get());

		// Pipeline config
		// Defines the maximum raceRay() recursion depth.
		auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
		// PERFOMANCE TIP: Set max recursion depth as low as needed 
		// as drivers may apply optimization strategies for low recursion depths. 
		UINT maxRecursionDepth = 2; // ~ primary rays only. 
		pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
		PrintStateObjectDesc(raytracingPipeline);
#endif

		ID3D12Device5* device5 = static_cast<ID3D12Device5*>(Graphics::g_Device);
		// Create the state object.
		ThrowIfFailed(device5->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
	}

	inline void SetInstanceTransform(const DirectX::XMMATRIX& worldMatrix, D3D12_RAYTRACING_INSTANCE_DESC& instanceDesc)
	{
		// DXR expects row-major 3x4 matrix in float[3][4]
		// Strip last row (for affine transform) and store in Transform
		DirectX::XMMATRIX m = DirectX::XMMatrixTranspose(worldMatrix); // DXR expects row-major
		memcpy(instanceDesc.Transform, &m, sizeof(instanceDesc.Transform));
	}

	void BuildAccelerationStructures(Transform transform, ModelH3D& model, UINT numMeshes)
	{
		GraphicsContext& gfxContextn = GraphicsContext::Begin(L"Build Acceleratoin Structures");
		ID3D12GraphicsCommandList4* pCmdList = static_cast<ID3D12GraphicsCommandList4*>(gfxContextn.GetCommandList());
		auto m_dxrDevice = static_cast<ID3D12Device5*>(Graphics::g_Device);


		D3D12_CPU_DESCRIPTOR_HANDLE uavHandle;
		UINT uavDescriptorIndex;
		g_pRaytracingDescriptorHeap = std::unique_ptr<DescriptorHeapStack>(
			new DescriptorHeapStack(*Graphics::g_Device, 200, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 0));


		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo;
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelAccelerationStructureDesc = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelAccelerationStructureDesc.Inputs;
		topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		topLevelInputs.NumDescs = numMeshes;
		topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		topLevelInputs.pGeometryDescs = nullptr;
		topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

		m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);

		const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlag = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs(model.m_Header.meshCount);
		UINT64 scratchBufferSizeNeeded = topLevelPrebuildInfo.ScratchDataSizeInBytes;

		ByteAddressBuffer tlasScratchBuffer;
		tlasScratchBuffer.Create(L"Acceleration Structure Scratch Buffer", (UINT)scratchBufferSizeNeeded, 1);

		D3D12_HEAP_PROPERTIES defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto tlasBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(topLevelPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		m_dxrDevice->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&tlasBufferDesc,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
			nullptr,
			IID_PPV_ARGS(&m_topLevelAccelerationStructure));

		topLevelAccelerationStructureDesc.DestAccelerationStructureData = m_topLevelAccelerationStructure->GetGPUVirtualAddress();
		topLevelAccelerationStructureDesc.ScratchAccelerationStructureData = tlasScratchBuffer.GetGpuVirtualAddress();

		//
		// Define the bottom level acceleration structures
		//

		for (UINT i = 0; i < numMeshes; i++)
		{
			auto& mesh = model.m_pMesh[i];

			D3D12_RAYTRACING_GEOMETRY_DESC& desc = geometryDescs[i];
			desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;



			D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC& trianglesDesc = desc.Triangles;
			trianglesDesc.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			trianglesDesc.VertexCount = mesh.vertexCount;
			trianglesDesc.VertexBuffer.StartAddress = model.GetVertexBuffer().BufferLocation + (mesh.vertexDataByteOffset + (UINT)mesh.attrib[ModelH3D::attrib_position].offset);
			trianglesDesc.IndexBuffer = model.GetIndexBuffer().BufferLocation + mesh.indexDataByteOffset;
			trianglesDesc.VertexBuffer.StrideInBytes = mesh.vertexStride;
			trianglesDesc.IndexCount = mesh.indexCount;
			trianglesDesc.IndexFormat = DXGI_FORMAT_R16_UINT;
			trianglesDesc.Transform3x4 = 0;
		}

		m_blasArray.resize(numMeshes);
		std::vector<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC> blasDescs(numMeshes);
		std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs(numMeshes);
		std::vector<ByteAddressBuffer> blasScratchBuffers(numMeshes);

		for (UINT i = 0; i < numMeshes; i++)
		{
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& blasDesc = blasDescs[i];
			blasDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			blasDesc.Inputs.NumDescs = (numMeshes == numMeshes) ? 1 : numMeshes;
			blasDesc.Inputs.pGeometryDescs = &geometryDescs[i];
			blasDesc.Inputs.Flags = buildFlag;
			blasDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo;
			m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&blasDesc.Inputs, &bottomLevelPrebuildInfo);

			auto& blas = m_blasArray[i];

			auto blasBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			m_dxrDevice->CreateCommittedResource(
				&defaultHeapProps,
				D3D12_HEAP_FLAG_NONE,
				&blasBufferDesc,
				D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
				nullptr,
				IID_PPV_ARGS(&blas));

			blasDesc.DestAccelerationStructureData = blas->GetGPUVirtualAddress();

			blasScratchBuffers[i].Create(L"BLAS build scratch buffer", (UINT)bottomLevelPrebuildInfo.ScratchDataSizeInBytes, 1);
			blasDesc.ScratchAccelerationStructureData = blasScratchBuffers[i].GetGpuVirtualAddress();

			D3D12_RAYTRACING_INSTANCE_DESC& instanceDesc = instanceDescs[i];
			g_pRaytracingDescriptorHeap->AllocateBufferUav(*blas.Get());

			// Identity matrix
			ZeroMemory(instanceDesc.Transform, sizeof(instanceDesc.Transform));
			instanceDesc.Transform[0][0] = 1.0f;
			instanceDesc.Transform[1][1] = 1.0f;
			instanceDesc.Transform[2][2] = 1.0f;

			instanceDesc.AccelerationStructure = blas->GetGPUVirtualAddress();
			instanceDesc.Flags = 0;
			instanceDesc.InstanceID = i;
			instanceDesc.InstanceMask = 1;
			instanceDesc.InstanceContributionToHitGroupIndex = i;
		}

		//
		// Upload the instance data
		//
		ByteAddressBuffer instanceDataBuffer;
		instanceDataBuffer.Create(L"Instance Data Buffer", numMeshes, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), instanceDescs.data());

		topLevelInputs.InstanceDescs = instanceDataBuffer.GetGpuVirtualAddress();
		topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

		//
		// Build the acceleration structures
		//
		GraphicsContext& gfxContext = GraphicsContext::Begin(L"Build Acceleration Structures");

		ComPtr<ID3D12GraphicsCommandList4> pRaytracingCommandList;
		gfxContext.GetCommandList()->QueryInterface(IID_PPV_ARGS(&pRaytracingCommandList));

		ID3D12DescriptorHeap* descriptorHeaps[] = { &g_pRaytracingDescriptorHeap->GetDescriptorHeap() };
		pRaytracingCommandList->SetDescriptorHeaps(ARRAYSIZE(descriptorHeaps), descriptorHeaps);

		auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
		for (UINT i = 0; i < blasDescs.size(); i++)
		{
			pRaytracingCommandList->BuildRaytracingAccelerationStructure(&blasDescs[i], 0, nullptr);

			// If each BLAS build reuses the scratch buffer, you would need a UAV barrier between each. But without
			// barriers, the driver may be able to batch these BLAS builds together. This maximizes GPU utilization
			// and should execute more quickly.
		}
		pRaytracingCommandList->ResourceBarrier(1, &uavBarrier);
		pRaytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelAccelerationStructureDesc, 0, nullptr);

		gfxContext.Finish(true);

	}

	void BuildShaderTables()
	{
		auto device = Graphics::g_Device;

		void* rayGenShaderIdentifier;
		void* missShaderIdentifier;
		void* shadowMissShaderIdentifier;
		void* hitGroupShaderIdentifier;
		void* shadowHitShaderIdentifier;

		auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
			{
				rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);
				missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_missShaderName);
				shadowMissShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_shadowMissShaderName);
				hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_hitGroupName);
				//shadowHitShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_shadowHitShaderName);
			};

		// Get shader identifiers.
		UINT shaderIdentifierSize;
		{
			ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
			ThrowIfFailed(m_dxrStateObject.As(&stateObjectProperties), L"");
			GetShaderIdentifiers(stateObjectProperties.Get());
			shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		}

		// Ray gen shader table
		{
			struct RootArguments {
				//RayGenConstantBuffer cb;
				RayGet3DBuffer cb;
			} rootArguments;
			rootArguments.cb = m_rayGenCB;

			UINT numShaderRecords = 1;
			UINT shaderRecordSize = shaderIdentifierSize + sizeof(rootArguments);
			ShaderTable rayGenShaderTable(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
			rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize, &rootArguments, sizeof(rootArguments)));
			m_rayGenShaderTable = rayGenShaderTable.GetResource();
		}

		// Miss shader table
		{
			UINT numShaderRecords = 2;
			UINT shaderRecordSize = shaderIdentifierSize;
			ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
			missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
			missShaderTable.push_back(ShaderRecord(shadowMissShaderIdentifier, shaderIdentifierSize));
			m_missShaderTable = missShaderTable.GetResource();
		}


		{
			struct RootArgs
			{
				D3D12_GPU_VIRTUAL_ADDRESS cb1;
				D3D12_GPU_VIRTUAL_ADDRESS uav0;
				D3D12_GPU_VIRTUAL_ADDRESS uavIndices;
			};
			//auto rootArgSize = sizeof(D3D12_GPU_VIRTUAL_ADDRESS) * 2;
			auto rootArgSize = sizeof(RootArgs);
			UINT numShaderRecords = static_cast<UINT>(m_perInstanceVertexData.size());
			UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

			// Each shader record must contain the shader identifier + local root arguments
			UINT shaderRecordSize = AlignUp(shaderIdentifierSize + rootArgSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
			//UINT shaderRecordSize = AlignUp(shaderIdentifierSize + sizeof(D3D12_GPU_VIRTUAL_ADDRESS), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);


			ShaderTable hitGroupShaderTable(Graphics::g_Device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");

			for (UINT i = 0; i < m_perInstanceVertexData.size(); ++i)
			{
				// Root arguments are packed together
				struct RootArgs rootArgs;
				rootArgs.cb1 = m_perInstanceMaterial[i].GetGpuVirtualAddress();
				rootArgs.uav0 = m_perInstanceVertexData[i].GetGpuVirtualAddress();
				rootArgs.uavIndices = m_perInstanceIndices[i].GetGpuVirtualAddress();

				ShaderRecord record(
					hitGroupShaderIdentifier,             // Pointer to the shader identifier
					shaderIdentifierSize,                 // Size of the shader identifier
					&rootArgs,                           // Pointer to root arguments (e.g., CBV GPU VA)
					rootArgSize
				);
				hitGroupShaderTable.push_back(record);
			}

			// Store the resource for later use in DispatchRays
			m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
		}
	}
	UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
	{
		//	auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		//	if (descriptorIndexToUse >= m_descriptorHeap->GetDesc().NumDescriptors)
		//	{
		//		descriptorIndexToUse = m_descriptorsAllocated++;
		//	}
		//	*cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_descriptorSize);
		//	return descriptorIndexToUse;
		return 0;
	}


	void CreateOutputTextureUAV(ColorBuffer* outputBuffer) {


		m_raytracingColorBuffer.Create(L"RayTracingOutput", outputBuffer->GetWidth(), outputBuffer->GetHeight(), 1, DXGI_FORMAT_R8G8B8A8_UNORM);

	}


	void UpdateCBForSizeChange(UINT width, UINT height)
	{
		float border = 0.1f;
		aspectRatio = (float)height / (float)width;
		if (width < height)
		{
			m_rayGenCB.stencil =
			{
				-1 + border, -1 + border * aspectRatio,
				1.0f - border, 1 - border * aspectRatio
			};
		}
		else
		{
			m_rayGenCB.stencil =
			{
				-1 + border / aspectRatio, -1 + border,
				 1 - border / aspectRatio, 1.0f - border
			};

		}
	}
	// Create resources that depend on the device.
	void CreateOutputAndPerInstanceTexturesHeap(ColorBuffer* outputBuffer, DescriptorHeap surfelUAVHeap, ModelH3D& model)
	{

		//Foreach texture referene found in the model
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;
		handles.push_back(m_raytracingColorBuffer.GetUAV());
		handles.push_back(surfelUAVHeap[2]);
		handles.push_back(surfelUAVHeap[3]);
		handles.push_back(surfelUAVHeap[4]);
		handles.push_back(m_RayDispatchBuffer.GetUAV());
		//handles.push_back(m_RayDispatchBuffer.GetUAV());
		UINT texturesSize = model.GetMaterialCount() * 3;
		for (size_t i = 0; i < model.GetMaterialCount(); i++)
		{
			TextureRef* diffuseRef = model.m_TextureReferences.data() + i * 3;
			TextureRef* specularRef = model.m_TextureReferences.data() + i * 3 + 1;
			TextureRef* normalRef = model.m_TextureReferences.data() + i * 3 + 2;
			handles.push_back(diffuseRef->GetSRV());
			handles.push_back(specularRef->GetSRV());
			handles.push_back(normalRef->GetSRV());

		}
		UINT textureCountFromHeap = Renderer::s_TextureHeap.GetDescriptorSize();
		m_materialDescriptorHeap.Create(L"Surfel Accumulation Heap", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, texturesSize + 5);
		ExtendedUtility::CopyDescriptorsToHeap(m_materialDescriptorHeap, handles);
	}

	void CreatePerInstanceCBs(ModelH3D& model, UINT numMeshes) {

		m_perInstanceVertexData.resize(numMeshes);
		m_perInstanceIndices.resize(numMeshes);
		m_perInstanceMaterial.resize(numMeshes);

		for (UINT i = 0; i < numMeshes; i++)
		{
			//The current mesh
			auto& mesh = model.m_pMesh[i];

			//The vertex data as layed out in the model h3d
			struct ActualVD {
				XMFLOAT2 uv;
				XMFLOAT3 normal;
				XMFLOAT3 tangent;
				XMFLOAT3 bitangent;
			};


			//-- Copy Vertice UVs --
			uint8_t* vertexStartPtr = reinterpret_cast<uint8_t*>(model.m_pVertexData) + mesh.vertexDataByteOffset;
			std::vector<AdditionalVertexData> additionalVertexDataTemp(mesh.vertexCount);

			//For loop to extract all the additional data from interleaved buffer
			for (UINT j = 0; j < mesh.vertexCount; ++j)
			{
				//Read the per-vertex data at and after UVs
				ActualVD testVD = ExtendedUtility::ReadAttribute<ActualVD>(
					vertexStartPtr, mesh.vertexStride, j, (UINT)mesh.attrib[ModelH3D::attrib_texcoord0].offset);

				additionalVertexDataTemp[j].uv = XMFLOAT4(testVD.uv.x, testVD.uv.y, testVD.uv.x, testVD.uv.y);
				additionalVertexDataTemp[j].normal = ExtendedUtility::PadToXMFLOAT4(testVD.normal, 0);
				additionalVertexDataTemp[j].tangent = ExtendedUtility::PadToXMFLOAT4(testVD.tangent, 0);
				additionalVertexDataTemp[j].bitanget = ExtendedUtility::PadToXMFLOAT4(testVD.bitangent, 0);
			}
			//Create in the appropriate per instance buffer
			m_perInstanceVertexData[i].Create(L"Add V Data", mesh.vertexCount, sizeof(AdditionalVertexData), additionalVertexDataTemp.data());


			uint8_t* indexStartPtr = reinterpret_cast<uint8_t*>(model.m_pIndexData) + mesh.indexDataByteOffset;
			std::vector<UINT> indexTemps(mesh.indexCount);
			//For loop to extract all the UV coordinates
			for (UINT j = 0; j < mesh.indexCount; ++j)
			{
				indexTemps[j] = static_cast<UINT>(ExtendedUtility::ReadAttribute<USHORT>(indexStartPtr, j, 0));
			}

			//Passing a raw pointer only based on per index data byte offset does not guarantee 16 byte alignment
			m_perInstanceIndices[i].Create(L"Index Buffer", mesh.indexCount, sizeof(UINT), indexTemps.data());


			auto mat = model.GetMaterial(mesh.materialIndex);

			PerInstanceCB materialTemp;
			materialTemp.materialIndex = mesh.materialIndex;
			materialTemp.b = mesh.materialIndex;
			materialTemp.c = mesh.materialIndex;
			materialTemp.d = mesh.materialIndex;
			materialTemp.diffuse = mat.diffuse;
			materialTemp.specular = mat.specular;
			materialTemp.ambient = mat.ambient;
			materialTemp.emissive = mat.emissive;
			materialTemp.transparent = mat.transparent;
			materialTemp.opacity = mat.opacity;
			materialTemp.shininess = mat.shininess;
			materialTemp.specularStrength = mat.specularStrength;
			materialTemp.padding = 5;

			m_perInstanceMaterial[i].Create(L"PerInstance CB", 1, sizeof(PerInstanceCB), &materialTemp);
		}
	}


	void CreateDeviceDependentResources(Transform transform, ModelH3D& model, ColorBuffer* outputBuffer, DescriptorHeap surfelSRVHeap)
	{
		m_rayGenCB.viewport = { -1.0f, -1.0f, 1.0f, 1.0f };

		m_TestCB.Create(L"Ray Tracing CBV", 1, static_cast<uint32_t>(sizeof(m_rayGenCB)));
		CreateRayDispatchData();

		CreateOutputAndPerInstanceTexturesHeap(outputBuffer, surfelSRVHeap, model);

		CreatePerInstanceCBs(model, _RAYTRACE_MESH_COUNT_);

		UpdateCBForSizeChange(outputBuffer->GetWidth(), outputBuffer->GetHeight());

		// Initialize raytracing pipeline.

		// Create raytracing interfaces: raytracing device and commandlist.
		CreateRaytracingInterfaces();

		// Create root signatures for the shaders.
		CreateRootSignatures();

		// Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
		CreateRaytracingPipelineStateObject();

		//	
		// Build raytracing acceleration structures from the generated geometry.
		BuildAccelerationStructures(transform, model, _RAYTRACE_MESH_COUNT_);
		//BuildAccelerationStructures(transform,model);
		//	
		// Build shader tables, which define shaders and their local root arguments.
		BuildShaderTables();

		//CreateRaytracingOutputResourceNew(outputBuffer,surfelSRVHeap);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle()
	{
		return m_materialDescriptorHeap[0];
		//return m_raytracingOutputResourceUAVGpuDescriptor;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle()
	{
		return m_materialDescriptorHeap[0];
		//return m_raytracingOutputResourceUAVCpuDescriptor;
	}
	ColorBuffer GetOutputBuffer()
	{

		//return ColorBuffer();
		return m_raytracingColorBuffer;
	}

	UINT frameIndex;

	void DoRaytracing(const Math::Camera& camera,DescriptorHeap surfelUAVHeap, UniformGrid grid,std::vector<SurfelData>& surfels)
	{

		XMMATRIX viewMatrix = XMMatrixLookAtLH(camera.GetPosition(), camera.GetPosition() + camera.GetForwardVec(), camera.GetUpVec());
		XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(camera.GetFOV(), 1 / aspectRatio, camera.GetNearClip(), camera.GetFarClip());
		XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewMatrix * projMatrix);
		XMMATRIX viewToWorld = XMMatrixInverse(nullptr, viewMatrix);


		m_rayGenCB.invViewProject = XMMatrixTranspose(invViewProj);
		m_rayGenCB.viewToWorld = XMMatrixTranspose(viewToWorld);

		//m_TestCB.Create(L"Ray Tracing CBV", 1, static_cast<uint32_t>(sizeof(m_rayGenCB)), &m_rayGenCB);

		ComputeContext& gfxContext = ComputeContext::Begin(L"Surfel Irradiance Ray Tracing");
		ID3D12GraphicsCommandList4* pCmdList = static_cast<ID3D12GraphicsCommandList4*>(gfxContext.GetCommandList());
		auto commandList = pCmdList;

		//Update the m_RayDispatch data
		ComputeRayIndexing(surfels);

		gfxContext.WriteBuffer(m_RayDispatchBuffer, 0, m_RayDispatchData.data(), sizeof(UINT) * m_RayDispatchData.size());

		commandList->SetComputeRootSignature(m_rtGlobalRootSignature.Get());
		static const UINT surfelCount = 1000;
		auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
			{
				dispatchDesc->HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
				dispatchDesc->HitGroupTable.SizeInBytes = _RAYTRACE_MESH_COUNT_ * 64;
				dispatchDesc->HitGroupTable.StrideInBytes = 64;
				dispatchDesc->MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
				dispatchDesc->MissShaderTable.SizeInBytes = 64;
				dispatchDesc->MissShaderTable.StrideInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
				dispatchDesc->RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
				dispatchDesc->RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;
//$				dispatchDesc->Width = Graphics::g_SceneColorBuffer.GetWidth();
//$				dispatchDesc->Height = Graphics::g_SceneColorBuffer.GetHeight();
				dispatchDesc->Width = 1000;
				dispatchDesc->Height = 1;
				dispatchDesc->Depth = 1;
				commandList->SetPipelineState1(stateObject);
				commandList->DispatchRays(dispatchDesc);
			};

		D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
		gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_materialDescriptorHeap.GetHeapPointer());

		commandList->SetComputeRootDescriptorTable(0, m_materialDescriptorHeap[0]);
		commandList->SetComputeRootDescriptorTable(4, m_materialDescriptorHeap[5]);
		commandList->SetComputeRootShaderResourceView(1, m_topLevelAccelerationStructure->GetGPUVirtualAddress());

		GridCB cb = {
			grid,
			frameIndex
		};

		frameIndex++;
		gfxContext.SetDynamicConstantBufferView(2, sizeof(RayGet3DBuffer), &m_rayGenCB);
		gfxContext.SetDynamicConstantBufferView(3, sizeof(GridCB), &cb);
		gfxContext.SetDynamicConstantBufferView(5, sizeof(SunDirectionalLight), &directionalLightData);
		DispatchRays(commandList, m_dxrStateObject.Get(), &dispatchDesc);
		gfxContext.Finish(true);
	}
}
