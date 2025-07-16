#pragma once
#include "HashGridVisualization.h"

//#include "CompiledShaders/SimpleColorPS.h"
#include "CompiledShaders/MSMEDebugOverlayPS.h"
class MSMEVisualization :
    public HashGridVisualization
{

	virtual void InitializePSO( DXGI_FORMAT formats[2], DXGI_FORMAT depthFormat) override
	{
		HashGridVisualization::InitializePSO(formats, depthFormat);

		m_TestPSO.SetPixelShader(g_pSimpleColorPS, sizeof(g_pSimpleColorPS));


    //m_GBuffer.g_DepthPSO.SetVertexShader(g_pDepthViewerVS, sizeof(g_pDepthViewerVS));

	D3D12_INPUT_ELEMENT_DESC colorElem[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//m_TestPSO = Depth;
	//--- REPLACE THE ROOT SIGNATURE
	m_TestPSO.SetRootSignature(m_UHGRootSignature);

    m_TestPSO.SetRasterizerState(Graphics::RasterizerDefault);
	m_TestPSO.SetBlendState(Graphics::BlendDisable);
	m_TestPSO.SetDepthStencilState(Graphics::DepthStateTestEqual);
    m_TestPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	//m_TestPSO.SetRenderTargetFormats(2, formats, DepthFormat);
	//m_TestPSO.SetRenderTargetFormats(2, formats, depthFormat);
	m_TestPSO.SetRenderTargetFormats(1, &formats[0], DXGI_FORMAT_UNKNOWN);
	m_TestPSO.SetInputLayout(_countof(colorElem), colorElem);
	//--- CHANGE THE DEPTH STATE ALWAYS TO DRAW ON TOP OF GEOMETRY
	m_TestPSO.SetDepthStencilState(Graphics::DepthStateDisabled);
	//--- THIS HAS TO BE SET TO UNKNOWN FORMAT TO CONFORM TO FRAMEWORK
	//m_TestPSO.SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	//--- MAKE SURE THAT CULLING IS OFF AND BOTH SIDES ARE DRAWN
	m_TestPSO.SetRasterizerState(Graphics::RasterizerTwoSided);

	//-- CHANGE TO THE NEW SHADER FOR THE TRIANGLE
	m_TestPSO.SetVertexShader(g_pSimpleColorVS, sizeof(g_pSimpleColorVS));
	m_TestPSO.SetPixelShader(g_pMSMEDebugOverlayPS, sizeof(g_pMSMEDebugOverlayPS));

	m_TestPSO.Finalize();


	}

};

