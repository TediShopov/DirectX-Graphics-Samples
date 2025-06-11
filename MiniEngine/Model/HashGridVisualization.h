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
	UHGProjectionResources m_projectionData;
	ByteAddressBuffer m_projectionBuffer;


	// --- DESCRIPTOR HEAR CONTAINING DEPTH BUFFER SRV ---
	RootSignature m_UHGRootSignature;

	// --- DESCRIPTOR HEAR CONTAINING DEPTH BUFFER SRV ---
	DescriptorHeap m_UHGTextures;


	void CreatePSO(GraphicsPSO Depth, DXGI_FORMAT formats[2], DXGI_FORMAT depthFormat)
	{
		D3D12_INPUT_ELEMENT_DESC colorElem[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		m_TestPSO = Depth;
		//--- REPLACE THE ROOT SIGNATURE
		m_TestPSO.SetRootSignature(m_UHGRootSignature);

		m_TestPSO.SetBlendState(Graphics::BlendDisable);
		m_TestPSO.SetDepthStencilState(Graphics::DepthStateTestEqual);
		//m_TestPSO.SetRenderTargetFormats(2, formats, DepthFormat);
		m_TestPSO.SetRenderTargetFormats(2, formats, depthFormat);
		m_TestPSO.SetInputLayout(_countof(colorElem), colorElem);
		//--- CHANGE THE DEPTH STATE ALWAYS TO DRAW ON TOP OF GEOMETRY
		m_TestPSO.SetDepthStencilState(Graphics::DepthStateDisabled);
		//--- THIS HAS TO BE SET TO UNKNOWN FORMAT TO CONFORM TO FRAMEWORK
		m_TestPSO.SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
		//--- MAKE SURE THAT CULLING IS OFF AND BOTH SIDES ARE DRAWN
		m_TestPSO.SetRasterizerState(Graphics::RasterizerTwoSided);

		//-- CHANGE TO THE NEW SHADER FOR THE TRIANGLE
		m_TestPSO.SetVertexShader(g_pSimpleColorVS, sizeof(g_pSimpleColorVS));
		m_TestPSO.SetPixelShader(g_pSimpleColorPS, sizeof(g_pSimpleColorPS));

		m_TestPSO.Finalize();

	}
	void CreateUHGRootSignature();

	void CreateUHGDescriptorHeap(
		const DepthBuffer& g_SceneDepthBuffer,
		const ColorBuffer& rayTracingOutColor
	);

	void SetUHGRootParameters(GraphicsContext& gfxContext,
		DepthBuffer& g_SceneDepthBuffer,
		ColorBuffer& rayTracingOutColor,
		const Camera& camera);


	void UHGTriangleRender(GraphicsContext& gfxContext,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissor,
		ColorBuffer& sceneColor,
		ColorBuffer& sceneNormal,
		DepthBuffer& g_SceneDepthBuffer,
		ColorBuffer& rayTracingOutColor,
		const Math::Camera& camera);


};

