#pragma once

#include "GraphicsCore.h"
#include "BufferManager.h"
//#include "Camera.h"
#include "CommandContext.h"
//#include "TemporalEffects.h"
//#include "SSAO.h"
//#include "SystemTime.h"
//#include "ShadowCamera.h"
//#include "ParticleEffects.h"
//#include "TestRenderer.h"
//#include "Renderer.h"

// From Model
//#include "ModelH3D.h"

// From ModelViewer
//#include "LightManager.h"

//#include "CompiledShaders/DepthViewerVS.h"
//#include "CompiledShaders/DepthViewerPS.h"
//#include "CompiledShaders/ModelViewerVS.h"
//#include "CompiledShaders/ModelViewerPS.h"
//#include "CompiledShaders/TestRendererPS.h"
//#include "CompiledShaders/SimpleColorPS.h"
//#include "CompiledShaders/SimpleColorVS.h"
#include "CompiledShaders/Raytracing.h"


#include <dxcapi.h>
#include <vector>
#include "DXSample.h"
#include "StepTimer.h"
#include "RaytracingHlslCompat.h"
#include "DirectXRaytracingHelper.h"

namespace TestRaytracing
{
	//---   RAY-TRACING RELATED
#pragma region RAY-TRACING DEMO


	// Create resources that depend on the device.
	void CreateDeviceDependentResources(
		D3D12_VERTEX_BUFFER_VIEW vertexBV,
		D3D12_INDEX_BUFFER_VIEW indexBV,
		ColorBuffer* outputBuffer
	);

// Update the application state with the new resolution.
	void UpdateCBForSizeChange(UINT width, UINT height);

	void DoRaytracing();

	void CreateRaytracingInterfaces();
	inline void ThrowIfFailed(HRESULT hr, const wchar_t* msg);

	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig);
	void CreateRootSignatures();

	// Local root signature and shader association
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);

	void CreateRaytracingPipelineStateObject();

	void CreateDescriptorHeap();

	void BuildAccelerationStructures(D3D12_VERTEX_BUFFER_VIEW vertexBV, D3D12_INDEX_BUFFER_VIEW indexBV);

	void BuildShaderTables();

	UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse);

	void CreateRaytracingOutputResource(ColorBuffer* outputBuffer);


#pragma endregion

};

