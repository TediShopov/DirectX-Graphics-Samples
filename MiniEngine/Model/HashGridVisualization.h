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


#include "SurfelGI.h"
#include "CommandContext.h"
#include "Renderer.h"
#include "Math/Vector.h"
#include <limits>


#include "CompiledShaders/SimpleColorPS.h"
#include "CompiledShaders/SimpleColorVS.h"


using namespace DirectX;
using namespace Math;

class HashGridVisualization
{
public:


	GraphicsPSO m_TestPSO = { (L"Sponza: Triangel Test PSO") };

	enum kUHGRoot {
		kProjectionResources = 0,
		kDepth = 1
	};

	__declspec(align(16)) struct UHGProjectionResources {

		XMMATRIX invViewProjeciton;
		float depthNear;
		float depthFar;
	};


	RootSignature m_UHGRootSignature;

	UHGProjectionResources m_projectionData;
	ByteAddressBuffer m_projectionBuffer;

	// --- DESCRIPTOR HEAR CONTAINING DEPTH BUFFER SRV ---
	DescriptorHeap m_UHGTextures;

	GBufferPtrs m_GBuffer;

void Setup(GBufferPtrs gbuffer,ColorBuffer* rayTracingOutColor);

	void SetupRenderStage(GraphicsContext& gfxContext,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissor,
		ColorBuffer& rayTracingOutColor,
		const Math::Camera& camera);


protected:
	void InitializePSO( DXGI_FORMAT formats[2], DXGI_FORMAT depthFormat);
	void InitializeRootSignature();

	void InitializeDescriptorHeap(ColorBuffer* rayTracingOutColor);

	void SetRootParameters(GraphicsContext& gfxContext,
	ColorBuffer& rayTracingOutColor,
		const Camera& camera);



};

