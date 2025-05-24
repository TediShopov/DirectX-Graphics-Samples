#include "TestRaytracing.h"

namespace TestRaytracing
{

	// The names of the shaders. The actual shader code is found in the Raytracing.hlsl
	const wchar_t* c_hitGroupName = L"MyHitGroup";
	const wchar_t* c_raygenShaderName = L"MyRaygenShader";
	const wchar_t* c_closestHitShaderName = L"MyClosestHitShader";
	const wchar_t* c_missShaderName = L"MyMissShader";


	UINT m_descriptorSize;
	UINT m_descriptorsAllocated;
	ComPtr<ID3D12StateObject> m_dxrStateObject;
	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;

	//Used for all the ray shaders
	ComPtr<ID3D12RootSignature> m_rtGlobalRootSignature;
	//Used by each individual shader and could be overwritten
	ComPtr<ID3D12RootSignature> m_rtLocalRootSignature;

	ComPtr<ID3D12Resource> m_accelerationStructure;
	ComPtr<ID3D12Resource> m_bottomLevelAccelerationStructure;
	ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;

	RayGenConstantBuffer m_rayGenCB;
	ComPtr<ID3D12Resource> m_missShaderTable;
	ComPtr<ID3D12Resource> m_hitGroupShaderTable;
	ComPtr<ID3D12Resource> m_rayGenShaderTable;

	ComPtr<ID3D12Resource> m_raytracingOutput;
	D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;

	UINT m_raytracingOutputResourceUAVDescriptorHeapIndex;
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
			UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
			//CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams::Count];
			//rootParameters[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &UAVDescriptor);
			//rootParameters[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
			CD3DX12_ROOT_PARAMETER rootParameters[2];
			rootParameters[0].InitAsDescriptorTable(1, &UAVDescriptor);
			rootParameters[1].InitAsShaderResourceView(0);
			CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
			SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &m_rtGlobalRootSignature);
		}

		// Local Root Signature
		// This is a root signature that enables a shader to have unique arguments that come from shader tables.
		{
			//CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams::Count];
			//rootParameters[LocalRootSignatureParams::ViewportConstantSlot].InitAsConstants(SizeOfInUint32(m_rayGenCB), 0, 0);
			CD3DX12_ROOT_PARAMETER rootParameters[1];
			rootParameters[0].InitAsConstants(SizeOfInUint32(m_rayGenCB), 0, 0);
			//---TODO TEST IF CORRECT SIZE IF SUPPLIED

			CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
			localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_rtLocalRootSignature);
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
			localRootSignature->SetRootSignature(m_rtLocalRootSignature.Get());
			// Shader association
			auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
			rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
			rootSignatureAssociation->AddExport(c_raygenShaderName);
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
		// Defines the maximum TraceRay() recursion depth.
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
	void CreateDescriptorHeap()
	{
		auto device = Graphics::g_Device;

		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
		// Allocate a heap for a single descriptor:
		// 1 - raytracing output texture UAV
		descriptorHeapDesc.NumDescriptors = 1;
		descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		descriptorHeapDesc.NodeMask = 0;
		device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
		// m_descriptorHeap->SetName(L"Ray-Tracing Heap Descriptor");
		NAME_D3D12_OBJECT(m_descriptorHeap);

		m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	void BuildAccelerationStructures(D3D12_VERTEX_BUFFER_VIEW vertexBV, D3D12_INDEX_BUFFER_VIEW indexBV)
	{
		//TODO Build Acceleration Structures The MiniEngine Way

		GraphicsContext& gfxContextn = GraphicsContext::Begin(L"Build Acceleratoin Structures");
		ID3D12GraphicsCommandList4* pCmdList = static_cast<ID3D12GraphicsCommandList4*>(gfxContextn.GetCommandList());



		// Setup geometry description
		D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
		geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc.Triangles.IndexBuffer = indexBV.BufferLocation;
		// CHANGED TO 32 UINT

		//Calculate the index count from size in bytes / stride in bytes properties of the generated 
		//Index Buffer View of the triangle 
		geometryDesc.Triangles.IndexCount = static_cast<UINT>(indexBV.SizeInBytes) / sizeof(uint32_t);
		geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		geometryDesc.Triangles.Transform3x4 = 0;
		geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		//Calculate the vertex count from size in bytes / stride in bytes properties of the generated 
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
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = topLevelInputs;
		bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
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
		instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
		instanceDesc.InstanceMask = 1;
		instanceDesc.AccelerationStructure = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
		AllocateUploadBuffer(Graphics::g_Device, &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");

		topLevelInputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress();


		// Create an instance desc for the bottom-level acceleration structure.
		//    ComPtr<ID3D12Resource> instanceDescs;
		//    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
		//    instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
		//    instanceDesc.InstanceMask = 1;
		//    instanceDesc.AccelerationStructure = m_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
		//    AllocateUploadBuffer(Graphics::g_Device, &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");

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
				RayGenConstantBuffer cb;
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

		// Hit group shader table
		{
			UINT numShaderRecords = 1;
			UINT shaderRecordSize = shaderIdentifierSize;
			ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
			hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize));
			m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
		}
	}
	UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
	{
		auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		if (descriptorIndexToUse >= m_descriptorHeap->GetDesc().NumDescriptors)
		{
			descriptorIndexToUse = m_descriptorsAllocated++;
		}
		*cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_descriptorSize);
		return descriptorIndexToUse;
	}
	void CreateRaytracingOutputResource(ColorBuffer* outputBuffer)
	{
		auto device = Graphics::g_Device;

		auto backbufferFormat = outputBuffer->GetFormat();

		// Create the output resource. The dimensions and format should match the swap-chain.
		auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat,
			outputBuffer->GetWidth(), outputBuffer->GetHeight(), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(
			&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_raytracingOutput)), L"");
		NAME_D3D12_OBJECT(m_raytracingOutput);

		D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
		m_raytracingOutputResourceUAVDescriptorHeapIndex = AllocateDescriptor(&uavDescriptorHandle, m_raytracingOutputResourceUAVDescriptorHeapIndex);
		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		device->CreateUnorderedAccessView(m_raytracingOutput.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
		m_raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, m_descriptorSize);
	}

	// Create resources that depend on the device.
	void CreateDeviceDependentResources(D3D12_VERTEX_BUFFER_VIEW vertexBV, D3D12_INDEX_BUFFER_VIEW indexBV, ColorBuffer* outputBuffer)
	{
		// Initialize raytracing pipeline.

		// Create raytracing interfaces: raytracing device and commandlist.
		CreateRaytracingInterfaces();

		// Create root signatures for the shaders.
		CreateRootSignatures();

		// Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
		CreateRaytracingPipelineStateObject();
		//	
		// Create a heap for descriptors.
		CreateDescriptorHeap();
		//	
		// Build raytracing acceleration structures from the generated geometry.
		BuildAccelerationStructures(vertexBV, indexBV);
		//	
		// Build shader tables, which define shaders and their local root arguments.
		BuildShaderTables();
		//	
		//	    // Create an output 2D texture to store the raytracing result to.
		CreateRaytracingOutputResource(outputBuffer);
	}
	void DoRaytracing()
	{
		GraphicsContext& gfxContext = GraphicsContext::Begin(L"RayTracing");
		ID3D12GraphicsCommandList4* pCmdList = static_cast<ID3D12GraphicsCommandList4*>(gfxContext.GetCommandList());
		auto commandList = pCmdList;

		auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
			{
				// Since each shader table has only one shader record, the stride is same as the size.
				dispatchDesc->HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
				dispatchDesc->HitGroupTable.SizeInBytes = m_hitGroupShaderTable->GetDesc().Width;
				dispatchDesc->HitGroupTable.StrideInBytes = dispatchDesc->HitGroupTable.SizeInBytes;
				dispatchDesc->MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
				dispatchDesc->MissShaderTable.SizeInBytes = m_missShaderTable->GetDesc().Width;
				dispatchDesc->MissShaderTable.StrideInBytes = dispatchDesc->MissShaderTable.SizeInBytes;
				dispatchDesc->RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
				dispatchDesc->RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;
				dispatchDesc->Width = Graphics::g_SceneColorBuffer.GetWidth();
				dispatchDesc->Height = Graphics::g_SceneColorBuffer.GetHeight();
				dispatchDesc->Depth = 1;
				//commandList->SetPipelineState1(stateObject);
				commandList->SetPipelineState1(stateObject);
				commandList->DispatchRays(dispatchDesc);
			};

		commandList->SetComputeRootSignature(m_rtGlobalRootSignature.Get());

		// Bind the heaps, acceleration structure and dispatch rays.    
		D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
		commandList->SetDescriptorHeaps(1, m_descriptorHeap.GetAddressOf());
		commandList->SetComputeRootDescriptorTable(0, m_raytracingOutputResourceUAVGpuDescriptor);
		commandList->SetComputeRootShaderResourceView(1, m_topLevelAccelerationStructure->GetGPUVirtualAddress());
		DispatchRays(commandList, m_dxrStateObject.Get(), &dispatchDesc);
		gfxContext.Finish(true);
	}
}
