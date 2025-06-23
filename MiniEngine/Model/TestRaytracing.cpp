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
#include "Renderer.h"
#include "ExtendedUtility.h"

	__declspec(align(16)) 
		struct PerInstanceCB {

		UINT materialIndex;
		UINT b;
		UINT c;
		UINT d;

        Color diffuse;
        Color specular;
        Color ambient;
        Color emissive;
        Color transparent; // light passing through a transparent surface is multiplied by this filter color
        float opacity;
        float shininess; // specular exponent
        float specularStrength; // multiplier on top of specular color
		float padding;


	};
struct RayGet3DBuffer {

	XMMATRIX invViewProject;
	XMMATRIX viewToWorld;
	Viewport viewport;
	Viewport stencil;
};
namespace TestRaytracing
{
	//ByteAddressBuffer	TestRaytracing::m_TestCB;
	ByteAddressBuffer	m_TestCB;

	std::unique_ptr<DescriptorHeapStack> g_pRaytracingDescriptorHeap = nullptr;

	// The names of the shaders. The actual shader code is found in the Raytracing.hlsl
	const wchar_t* c_hitGroupName = L"MyHitGroup";
	const wchar_t* c_raygenShaderName = L"MyRaygenShader";
	const wchar_t* c_closestHitShaderName = L"MyClosestHitShader";
	const wchar_t* c_missShaderName = L"MyMissShader";


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
	ComPtr<ID3D12RootSignature> m_rtLocalRayGenRB;
	//A different locla root signature for the hit shader
	ComPtr<ID3D12RootSignature> m_rtLocalHitRB;

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
	//Stores the flattened indices as Buffer that can be passed on the GPU
	//Per Instance Addtional Vertex Data
	__declspec(align(16)) struct AdditionalVertexData
	{
		//$VECTOR2 uv
		//float uvX;
		//float uvY;
		XMFLOAT4 uv;
		XMFLOAT4 normal;
		XMFLOAT4 tangent;
		XMFLOAT4 bitanget;

	};


	std::vector<StructuredBuffer> m_perInstanceVertexData;
	std::vector<StructuredBuffer> m_perInstanceIndices;
	//Store the per-intance diffuse textures
	std::vector<StructuredBuffer> m_perInstanceCB;
	std::vector<D3D12_GPU_VIRTUAL_ADDRESS> m_perInstanceVirtualAdress;
	std::vector<PerInstanceCB> m_perInstanceData;

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
			UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0);

			CD3DX12_DESCRIPTOR_RANGE MaterialDescriptor;
			MaterialDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 85, 0, 1);

			CD3DX12_ROOT_PARAMETER rootParameters[5];
			rootParameters[0].InitAsDescriptorTable(1, &UAVDescriptor);
			rootParameters[1].InitAsShaderResourceView(0);
			rootParameters[2].InitAsConstantBufferView(0);
			rootParameters[3].InitAsConstantBufferView(1);
			rootParameters[4].InitAsDescriptorTable(1, &MaterialDescriptor);


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
//			staticSampler.BorderColor[0] = 1.0f;
//			staticSampler.BorderColor[1] = 1.0f;
//			staticSampler.BorderColor[2] = 1.0f;
//			staticSampler.BorderColor[3] = 1.0f;
			staticSampler.MinLOD = 0.0f;
			staticSampler.MaxLOD = D3D12_FLOAT32_MAX;





			CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters, 1, &staticSampler);
			SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &m_rtGlobalRootSignature);
		}

		// Local Root Signature
		// This is a root signature that enables a shader to have unique arguments that come from shader tables.
		{
			CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(0,0);
			localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_rtLocalRayGenRB);
		}
		// HIT SHADER - Local Root Signature
		// This is a root signature that enables a shader to have unique arguments that come from shader tables.
		{
			CD3DX12_ROOT_PARAMETER rootParameters[3];
			rootParameters[0].InitAsConstantBufferView(2);
			rootParameters[1].InitAsUnorderedAccessView(4);
			rootParameters[2].InitAsUnorderedAccessView(5);
			CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
			localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_rtLocalHitRB);
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
			localRootSignature->SetRootSignature(m_rtLocalRayGenRB.Get());
			// Shader association
			auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
			rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
			rootSignatureAssociation->AddExport(c_raygenShaderName);
		}
		// Local root signature to be used in a ray gen shader.
		{
			auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
			localRootSignature->SetRootSignature(m_rtLocalHitRB.Get());
			// Shader association
			auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
			rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
			rootSignatureAssociation->AddExport(c_hitGroupName);
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
		D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)g_pRaytracing, ARRAYSIZE(g_pRaytracing));
		lib->SetDXILLibrary(&libdxil);
		// Define which shader exports to surface from the library.
		// If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
		// In this sample, this could be omitted for convenience since the sample uses all shaders in the library. 
		{
			lib->DefineExport(c_raygenShaderName);
			lib->DefineExport(c_closestHitShaderName);
			lib->DefineExport(c_missShaderName);
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
		UINT maxRecursionDepth = 1; // ~ primary rays only. 
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

	void BuildAccelerationStructures(Transform transform, D3D12_VERTEX_BUFFER_VIEW vertexBV, D3D12_INDEX_BUFFER_VIEW indexBV)
	{
		//TODO Build Acceleration Structures The MiniEngine Way

		GraphicsContext& gfxContextn = GraphicsContext::Begin(L"Build Acceleratoin Structures");
		ID3D12GraphicsCommandList4* pCmdList = static_cast<ID3D12GraphicsCommandList4*>(gfxContextn.GetCommandList());


		assert(indexBV.BufferLocation != 0);
		assert((indexBV.BufferLocation % 4) == 0); // R32_UINT requires 4-byte alignment


		// Setup geometry description
		D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
		//geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc.Triangles.IndexBuffer = indexBV.BufferLocation;
		// CHANGED TO 32 UINT

		//Calculate the index count from size in bytes / stride in bytes properties of the generated 
		//Index Buffer View of the triangle 
		if (indexBV.Format == DXGI_FORMAT_R16_UINT)
			geometryDesc.Triangles.IndexCount = indexBV.SizeInBytes / sizeof(uint16_t);
		else if (indexBV.Format == DXGI_FORMAT_R32_UINT)
			geometryDesc.Triangles.IndexCount = indexBV.SizeInBytes / sizeof(uint32_t);
		else
			assert(false && "Unsupported index format");
		geometryDesc.Triangles.IndexFormat = indexBV.Format;
		geometryDesc.Triangles.Transform3x4 = 0;
		geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		//Vertex Buffer View of the triangle 
		geometryDesc.Triangles.VertexCount = static_cast<UINT>(vertexBV.SizeInBytes) / vertexBV.StrideInBytes;
		geometryDesc.Triangles.VertexBuffer.StartAddress = vertexBV.BufferLocation;
		geometryDesc.Triangles.VertexBuffer.StrideInBytes = vertexBV.StrideInBytes;
		geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

		// Get required sizes for an acceleration structure.
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
		topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		topLevelInputs.Flags = buildFlags;
		topLevelInputs.NumDescs = 1;
		topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		auto m_dxrDevice = static_cast<ID3D12Device5*>(Graphics::g_Device);

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
		m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
		ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = {};
		bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomLevelInputs.Flags = buildFlags;
		bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bottomLevelInputs.NumDescs = 1;
		bottomLevelInputs.pGeometryDescs = &geometryDesc;

		m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
		ThrowIfFalse(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

		ComPtr<ID3D12Resource> scratchResource;
		AllocateUAVBuffer(Graphics::g_Device,
			std::max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes),
			&scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

		// Allocate resources for acceleration structures.
		// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
		// Default heap is OK since the application doesn’t need CPU read/write access to them. 
		// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
		// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
		//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
		//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
		{
			D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

			AllocateUAVBuffer(Graphics::g_Device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_bottomLevelAccelerationStructure, initialResourceState, L"BottomLevelAccelerationStructure");
			AllocateUAVBuffer(Graphics::g_Device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_topLevelAccelerationStructure, initialResourceState, L"TopLevelAccelerationStructure");
		}

		ComPtr<ID3D12Resource> instanceDescs;
		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};

		//IDENTITY
		SetInstanceTransform(XMMatrixIdentity(), instanceDesc);

		instanceDesc.InstanceMask = 1;
		instanceDesc.AccelerationStructure = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
		AllocateUploadBuffer(Graphics::g_Device, &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");

		topLevelInputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress();
		// Bottom Level Acceleration Structure desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		{
			bottomLevelBuildDesc.Inputs = bottomLevelInputs;
			bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
			bottomLevelBuildDesc.DestAccelerationStructureData = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
		}

		// Top Level Acceleration Structure desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
		{
			topLevelInputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress();
			topLevelBuildDesc.Inputs = topLevelInputs;
			topLevelBuildDesc.DestAccelerationStructureData = m_topLevelAccelerationStructure->GetGPUVirtualAddress();
			topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
		}

		auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
			{
				raytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
				//TODO make sure that this is the passed parameter
				raytracingCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_bottomLevelAccelerationStructure.Get()));
				raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
			};
		// Build acceleration structure.
		BuildAccelerationStructure(static_cast<ID3D12GraphicsCommandList4*>(gfxContextn.GetCommandList()));
		// Wait for finish by passing true
		gfxContextn.Finish(true);
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

		//g_pRaytracingDescriptorHeap->AllocateDescriptor(uavHandle, uavDescriptorIndex);
		//Graphics::g_Device->CopyDescriptorsSimple(1, uavHandle, g_SceneColorBuffer.GetUAV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		//g_OutputUAV = g_pRaytracingDescriptorHeap->GetGpuHandle(uavDescriptorIndex);



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
			//instanceDesc.InstanceContributionToHitGroupIndex = 0;
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
		void* hitGroupShaderIdentifier;

		auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
			{
				rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);
				missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_missShaderName);
				hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_hitGroupName);
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
			UINT numShaderRecords = 1;
			UINT shaderRecordSize = shaderIdentifierSize;
			ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
			missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
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

			for (UINT i = 0; i < numShaderRecords; ++i)
			{
				// Root arguments are packed together
				struct RootArgs rootArgs;
				rootArgs.cb1 = m_perInstanceCB[i].GetGpuVirtualAddress();
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

	void CreateRaytracingOutputResourceNew(ColorBuffer* outputBuffer, DescriptorHeap surfelUAVHeap, ModelH3D& model)
	{
		UINT textureCount = model.GetMaterialCount() * 6;
		UINT b = model.m_TextureReferences.size();

		//Foreach texture referene found in the model
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;
		handles.push_back(m_raytracingColorBuffer.GetUAV());
		handles.push_back(surfelUAVHeap[2]);
		handles.push_back(surfelUAVHeap[3]);
		handles.push_back(surfelUAVHeap[4]);



		//Only diffuse textures 
		//UINT diffuseTextureSize = model.m_TextureReferences.size() / 3;
		UINT texturesSize = model.GetMaterialCount() * 3;
		auto mat0=model.GetMaterial(0);
		auto mat1=model.GetMaterial(1);
		auto mat2=model.GetMaterial(2);
		auto mat3=model.GetMaterial(3);
		auto mat4=model.GetMaterial(4);

		for (size_t i = 0; i < model.GetMaterialCount(); i++ )
		{
			//auto t = model.m_TextureReferences[i];

			TextureRef* diffuseRef =  model.m_TextureReferences.data() + i * 3;
			TextureRef* specularRef =  model.m_TextureReferences.data() + i * 3 +1;
			TextureRef* normalRef =  model.m_TextureReferences.data() + i * 3+2;
			handles.push_back(diffuseRef->GetSRV());
			handles.push_back(specularRef->GetSRV());
			handles.push_back(normalRef->GetSRV());

		}
		UINT textureCountFromHeap = Renderer::s_TextureHeap.GetDescriptorSize();
		m_materialDescriptorHeap.Create(L"HeapName", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, texturesSize+4);
		ExtendedUtility::CopyDescriptorsToHeap(m_materialDescriptorHeap, handles);

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




	void CreatePerInstanceCB(ModelH3D& model,UINT numMeshes) {

		m_perInstanceVertexData.resize(numMeshes);
		m_perInstanceIndices.resize(numMeshes);
		m_perInstanceCB.resize(numMeshes);
		m_perInstanceData.resize(numMeshes);
		m_perInstanceVirtualAdress.resize(numMeshes);

		for (UINT i = 0; i < numMeshes; i++)
		{
			auto& mesh = model.m_pMesh[i];
			auto uvAttribOffset = (UINT)mesh.attrib[ModelH3D::attrib_texcoord0].offset;
			auto normalAttribOffset = (UINT)mesh.attrib[ModelH3D::attrib_normal].offset;
			auto tangetAttribOffset = (UINT)mesh.attrib[ModelH3D::attrib_tangent].offset;
			auto bitangentAttribOffset = (UINT)mesh.attrib[ModelH3D::attrib_bitangent].offset;
			D3D12_GPU_VIRTUAL_ADDRESS uvStartAddress =
				model.GetVertexBuffer().BufferLocation +
				mesh.vertexDataByteOffset +
				uvAttribOffset;



			//-- Copy Vertice UVs --
			uint8_t* basePtr = reinterpret_cast<uint8_t*>(model.m_pVertexData);
			basePtr += mesh.vertexDataByteOffset;

			//std::vector<XMFLOAT2> uvData(mesh.vertexCount);
			std::vector<AdditionalVertexData> additionalVertexDataTemp(mesh.vertexCount);

			//For loop to extract all the UV coordinates
			for (UINT j = 0; j < mesh.vertexCount; ++j)
			{
				//Append to an array
				float* uvPtr = reinterpret_cast<float*>(basePtr + j * mesh.vertexStride + uvAttribOffset);
				float* normalPtr = reinterpret_cast<float*>(basePtr + j * mesh.vertexStride + normalAttribOffset);
				float* tangentPtr = reinterpret_cast<float*>(basePtr + j * mesh.vertexStride + tangetAttribOffset);
				float* bitangentPtr = reinterpret_cast<float*>(basePtr + j * mesh.vertexStride + bitangentAttribOffset);

				additionalVertexDataTemp[j].uv = XMFLOAT4(uvPtr[0], uvPtr[1],uvPtr[0], uvPtr[1]);
				additionalVertexDataTemp[j].normal = XMFLOAT4(normalPtr[0], normalPtr[1], normalPtr[2],0);
				additionalVertexDataTemp[j].tangent = XMFLOAT4(tangentPtr[0], tangentPtr[1], tangentPtr[2],0);
				additionalVertexDataTemp[j].bitanget = XMFLOAT4(bitangentPtr[0], bitangentPtr[1], bitangentPtr[2],0);
			}
			//Create in the appropriate per instance buffer
			m_perInstanceVertexData[i].Create(L"Uv Buffer", mesh.vertexCount, sizeof(AdditionalVertexData), additionalVertexDataTemp.data());






			uint8_t* meshIndexBufferPtr = reinterpret_cast<uint8_t*>(model.m_pIndexData);
			meshIndexBufferPtr += mesh.indexDataByteOffset;
			std::vector<UINT> indexTemps(mesh.indexCount);

			//For loop to extract all the UV coordinates
			for (UINT j = 0; j < mesh.indexCount; ++j)
			{
				USHORT* index = reinterpret_cast<USHORT*>(meshIndexBufferPtr + j * sizeof(USHORT));

				indexTemps[j] = static_cast<UINT>(*index);
				//indexTemps[j] = meshIndexBufferPtr[j];
			}




			//Passing a raw pointer only based on per index data byte offset does not guarantee 16 byte alignment
			m_perInstanceIndices[i].Create(L"Index Buffer", mesh.indexCount, sizeof(UINT), indexTemps.data());


			auto mat =model.GetMaterial(mesh.materialIndex);


//        Color diffuse;
//        Color specular;
//        Color ambient;
//        Color emissive;
//        Color transparent; // light passing through a transparent surface is multiplied by this filter color
//        float opacity;
//        float shininess; // specular exponent
//        float specularStrength; // multiplier on top of specular color
//		float padding;

			//PerInstanceCB a = { mesh.materialIndex,mesh.materialIndex,mesh.materialIndex,mesh.materialIndex};
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

			m_perInstanceCB[i].Create(L"PerInstance CB", 1,sizeof(PerInstanceCB),&materialTemp);
		}
	}

	void CreateDeviceDependentResources(Transform transform, ModelH3D& model, ColorBuffer* outputBuffer, DescriptorHeap surfelSRVHeap)
	{
		m_rayGenCB.viewport = { -1.0f, -1.0f, 1.0f, 1.0f };

		m_TestCB.Create(L"Ray Tracing CBV", 1, static_cast<uint32_t>(sizeof(m_rayGenCB)));



		CreateRaytracingOutputResourceNew(outputBuffer,surfelSRVHeap,model);
		//Extract fllatened UV for per instance parameters
		CreatePerInstanceCB(model, 20);


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
		BuildAccelerationStructures(transform, model, 20);

		// Build shader tables, which define shaders and their local root arguments.
		BuildShaderTables();
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
	void DoRaytracing(const Math::Camera& camera,DescriptorHeap surfelUAVHeap, UniformGrid grid)
	{

		XMMATRIX viewMatrix = XMMatrixLookAtLH(camera.GetPosition(), camera.GetPosition() + camera.GetForwardVec(), camera.GetUpVec());
		XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(camera.GetFOV(), 1 / aspectRatio, camera.GetNearClip(), camera.GetFarClip());
		XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewMatrix * projMatrix);
		XMMATRIX viewToWorld = XMMatrixInverse(nullptr, viewMatrix);

		m_rayGenCB.invViewProject = XMMatrixTranspose(invViewProj);
		m_rayGenCB.viewToWorld = XMMatrixTranspose(viewToWorld);


		ComputeContext& gfxContext = ComputeContext::Begin(L"RayTracing");
		ID3D12GraphicsCommandList4* pCmdList = static_cast<ID3D12GraphicsCommandList4*>(gfxContext.GetCommandList());
		auto commandList = pCmdList;

		gfxContext.TransitionResource(TestRaytracing::GetOutputBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);


		commandList->SetComputeRootSignature(m_rtGlobalRootSignature.Get());
		auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
			{
				dispatchDesc->HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
				dispatchDesc->HitGroupTable.SizeInBytes = 20*64;
				dispatchDesc->HitGroupTable.StrideInBytes = 64;
				dispatchDesc->MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
				dispatchDesc->MissShaderTable.SizeInBytes = m_missShaderTable->GetDesc().Width;
				dispatchDesc->MissShaderTable.StrideInBytes = dispatchDesc->MissShaderTable.SizeInBytes;
				dispatchDesc->RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
				dispatchDesc->RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;
				dispatchDesc->Width = Graphics::g_SceneColorBuffer.GetWidth();
				dispatchDesc->Height = Graphics::g_SceneColorBuffer.GetHeight();
				dispatchDesc->Depth = 1;
				commandList->SetPipelineState1(stateObject);
				commandList->DispatchRays(dispatchDesc);
			};

		D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
		gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_materialDescriptorHeap.GetHeapPointer());

		commandList->SetComputeRootDescriptorTable(0, m_materialDescriptorHeap[0]);
		commandList->SetComputeRootDescriptorTable(4, m_materialDescriptorHeap[4]);
		commandList->SetComputeRootShaderResourceView(1, m_topLevelAccelerationStructure->GetGPUVirtualAddress());
		gfxContext.SetDynamicConstantBufferView(2, sizeof(RayGet3DBuffer), &m_rayGenCB);
		gfxContext.SetDynamicConstantBufferView(3, sizeof(UniformGrid), &grid);
		DispatchRays(commandList, m_dxrStateObject.Get(), &dispatchDesc);
		gfxContext.Finish(true);
	}
}
