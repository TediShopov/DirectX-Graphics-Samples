#include "GraphicsCore.h"
#include "BufferManager.h"
#include "Camera.h"
#include "CommandContext.h"
#include "TemporalEffects.h"
#include "SSAO.h"
#include "SystemTime.h"
#include "ShadowCamera.h"
#include "ParticleEffects.h"
#include "SurfelGIRenderer.h"
#include "Renderer.h"
#include "LightManager.h"
#include "ReadbackBuffer.h"
#include "Math/Vector.h"
#include "ExtendedUtility.h"

#include <initializer_list>
#include "combaseapi.h"
#include "GameInput.h"

#include "SurfelGI.h"
#include "HashGridVisualization.h"
#include "MSMEVisualization.h"
#include "SurfelSpawnChanceVisualization.h"
#include "SurfelGIOnlyVisualization.h"
#include "SurfelSSRMIrradianceAccumulation.h"
#include <sstream>
#include "glTF.h"


//Imgui 
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"



// From Model
#include "ModelH3D.h"
#include "ModelOBJ.h"

// From ModelViewer

#include "CompiledShaders/DepthViewerVS.h"
#include "CompiledShaders/DepthViewerPS.h"
#include "CompiledShaders/ModelViewerVS.h"
#include "CompiledShaders/ModelViewerPS.h"
#include "CompiledShaders/TestRendererPS.h"
#include "CompiledShaders/SimpleMeshPS.h"
#include "CompiledShaders/SimpleMeshVS.h"
#include "CompiledShaders/SurfelAccelerationStructuresCS.h"
#include "CompiledShaders/SurfelGenerationCS.h"
#include "CompiledShaders/ScreenSpaceReflectionsPS.h"

#include <dxcapi.h>
#include <vector>
#include "TestRaytracing.h"
#include "SurfelIrradianceAccumulation.h"
#include "HBIL.h"
#include "GBufferDownsample.h"
#include "HBILInterleaved.h"
#include "CameraSequencer.h"
#include "CameraSequenceRunner.h"
#include "AdditiveBlendPass.h"

using namespace Math;
using namespace Graphics;
using namespace std;

namespace {
    std::unique_ptr<SurfelGIRendererContext> context;
}
	//--- DATA ---
#pragma region DATA

#pragma region			ImGuiDecriptor


#pragma endregion
	 DescriptorHeap SurfelGIRenderer::SSRHeap = DescriptorHeap();
	 ColorBuffer SurfelGIRenderer::colorCopyBuffer = ColorBuffer();
	 DepthBuffer SurfelGIRenderer::depthCopyBuffer = DepthBuffer();
	 ModelH3D* SurfelGIRenderer::m_Model = nullptr;
	 ModelH3D* SurfelGIRenderer::m_ModelExtra = nullptr;

	 struct SunData {
		 XMFLOAT3 sunDirection;
		 float sunOrientation;
		 float sunInclination;
		 float ambientLightIntensity;
		 float sunLightIntensity;

	 };
	 SunData m_sunData;

	NumVar m_SunOrientation = NumVar("Sponza/Lighting/Sun Orientation", -0.5f, -100.0f, 100.0f, 0.1f);
	NumVar m_SunInclination = NumVar("Sponza/Lighting/Sun Inclination", 0.75f, 0.0f, 1.0f, 0.01f);
	NumVar ShadowDimX = NumVar("Sponza/Lighting/Shadow Dim X", 5000, 1000, 10000, 100);
	NumVar ShadowDimY= NumVar("Sponza/Lighting/Shadow Dim Y", 3000, 1000, 10000, 100);
	NumVar ShadowDimZ= NumVar("Sponza/Lighting/Shadow Dim Z", 3000, 1000, 10000, 100);

	 bool m_enableDebugOverlay = true;
	 bool m_stopSurfelUpdate = false;
	 bool m_prevStopSurfelUpdate = true;
	 bool m_renderOnlyCurrentCellSurfels = false;
	 bool m_useSimpleAlgorithm = true;

	 //Control if we should be updating the debug rays sample from the HBIL pass
	 bool m_hbil_updateDebug = true;
	 //Control if we should be drawing those on the rays in the scene in global space
	 bool m_hbil_drawDebug = true;
	 //Control if we should drawing render results of hbil on texture quad in front of camera
	 bool m_hbil_render = true;
	 Camera last_camera_data;
	 //Vector3 m_hbil_cameraLastPos;


	 
	 //bool m_useSSRMonly = false;
	 bool m_enableBlending = true;
	 bool m_useSSRMonly = false;
	 bool m_drawPhysicalSurfelInstances = false;
	 bool m_drawSSRTest = false;


	 int m_debugOverlayMode = 0;

	 SurfelSSRMIrradianceAccumulation SSRMIrradianceAccumulation;

	DescriptorHeap renderTargetHeap;
	GraphicsPSO m_DepthPSO = { (L"Sponza: Depth PSO") };

	GraphicsPSO m_ModelPSO = { (L"Sponza: Color PSO") };
	GraphicsPSO m_ModelSSRPSO = { (L"Sponza: SSR PSO") };
	GraphicsPSO m_ModelSimplifiedPSO = { (L"Sponza: Simplified Color PSO") };

	GraphicsPSO m_TestSpherePSO = { (L"Sponza: Sphere Test PSO") };

	GraphicsPSO m_CutoutDepthPSO = { (L"Sponza: Cutout Depth PSO") };
	GraphicsPSO m_CutoutModelPSO = { (L"Sponza: Cutout Color PSO") };
	GraphicsPSO m_ShadowPSO = (L"Sponza: Shadow PSO");
	GraphicsPSO m_CutoutShadowPSO = (L"Sponza: Cutout Shadow PSO");


	ModelOBJ OBJModel;

	PSConstants SurfelGIRenderer::psConstants = PSConstants{};

	std::vector<bool> m_pMaterialIsCutout;


	 struct ColorVertex { Vector4 position;  Vector4 color; };
	 float m_aspectRatio = 16.0f / 9.0f;
	  float m_depthValue = 0.1f;
	  float _TRI_SCALE = 0.3f;
	  //float QUAD_SCALE = 0.8f;
	  float QUAD_SCALE = 1;
	 ColorVertex triangleVertices[3] =
	{
		{ { 0.0f, _TRI_SCALE * m_aspectRatio, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { _TRI_SCALE ,-_TRI_SCALE * m_aspectRatio, m_depthValue,1}, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -_TRI_SCALE, -_TRI_SCALE * m_aspectRatio, m_depthValue,1}, { 0.0f, 0.0f, 1.0f, 1.0f } }
	};

	 ColorVertex fullScreenQuad[6] =
	{
		{ { -QUAD_SCALE, -QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { QUAD_SCALE, QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { -QUAD_SCALE, QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },

		{ { -QUAD_SCALE, -QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { QUAD_SCALE, QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { QUAD_SCALE, -QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
	};

	//The triangle geomtry buffer 
	ByteAddressBuffer m_GeometryBuffer;
	ByteAddressBuffer m_QuadGB;
	//The vertex and index resource views
	D3D12_VERTEX_BUFFER_VIEW    m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW    m_IndexBufferView;

	D3D12_VERTEX_BUFFER_VIEW    m_QuadVB;
	D3D12_INDEX_BUFFER_VIEW    m_QuadIB;

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;



#pragma endregion


	void SurfelGIRenderer::SetupScene() 
	{

		m_Model = new ModelH3D();
		m_ModelExtra = new ModelH3D();

		ModelOBJ planeSSR;
		bool loadedPlane =planeSSR.Load(L"D:/MScSurfelBasedGI/DirectX-Graphics-Samples/MiniEngine/Model/OBJ/plane.obj");
		//OBJModel.Load(L"D:/MScSurfelBasedGI/DirectX-Graphics-Samples/MiniEngine/Model/OBJ/Bunny.obj");
		OBJModel.Load(L"D:/MScSurfelBasedGI/DirectX-Graphics-Samples/MiniEngine/Model/OBJ/bunnyextra.obj");

		bool laodedExtra =m_Model->Load(L"Sponza/sponza.h3d");
		OBJModel.InsertIntoH3DModel(m_Model);
		planeSSR.InsertIntoH3DModel(m_Model);
		bool save =m_Model->SaveH3D(L"Sponza/SponzaA.h3d");
		bool load = m_Model->Load(L"Sponza/SponzaA.h3d");




	}
	///--- INTIIALIZATION ---
	void SurfelGIRenderer::Startup(Math::Camera& camera,std::vector<IParameterBlock*>& parameters, HWND hwnd)
	{
		context = std::make_unique<SurfelGIRendererContext>();
		context->SurfelIllumination = std::make_unique<SurfelGI>();  
		context->GridVisualization = std::make_unique<HashGridVisualization>();  
		context->GridMSMEVisualization = std::make_unique<MSMEVisualization>();  
		context->SurfelSpawnVisualization = std::make_unique<SurfelSpawnChanceVisualization>();  
		context->SurfelGIVisualization = std::make_unique<SurfelGIOnlyVisualization>();  
		context->MaterialBindingDebug = std::make_unique<SurfelGIOnlyVisualization>();  
		context->frameIndex = 0;


		context->sphere = nullptr;
		context->disc = nullptr;
		context->transform = std::make_unique<Transform>();
		context->transform->setScale(10, 10, 10);

		DXGI_FORMAT ColorFormat = g_SceneColorBuffer.GetFormat();
		DXGI_FORMAT NormalFormat = g_SceneNormalBuffer.GetFormat();
		DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();
		DXGI_FORMAT ShadowFormat = g_ShadowBuffer.GetFormat();
		//DXGI_FORMAT AlbedoFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
		


		//m_Transform.setScale(50,50,50);

		m_sunData.ambientLightIntensity = 0;
		m_sunData.sunInclination = 1.0f;
		m_sunData.sunOrientation = -20;
		m_sunData.sunLightIntensity = 1.0f;

		D3D12_INPUT_ELEMENT_DESC vertElem[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_INPUT_ELEMENT_DESC colorElem[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		D3D12_INPUT_ELEMENT_DESC simpleVertElemnt[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Depth-only (2x rate)
		m_DepthPSO.SetRootSignature(Renderer::m_RootSig);
		//m_DepthPSO.SetRasterizerState(RasterizerDefault);
		m_DepthPSO.SetRasterizerState(RasterizerTwoSided);
		m_DepthPSO.SetBlendState(BlendNoColorWrite);
		m_DepthPSO.SetDepthStencilState(DepthStateReadWrite);
		m_DepthPSO.SetInputLayout(_countof(vertElem), vertElem);
		m_DepthPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_DepthPSO.SetRenderTargetFormats(0, nullptr, DepthFormat);
		m_DepthPSO.SetVertexShader(g_pDepthViewerVS, sizeof(g_pDepthViewerVS));
		m_DepthPSO.Finalize();
		//
		//    // Depth-only shading but with alpha testing
		m_CutoutDepthPSO = m_DepthPSO;;
		m_CutoutDepthPSO.SetPixelShader(g_pDepthViewerPS, sizeof(g_pDepthViewerPS));
		m_CutoutDepthPSO.SetRasterizerState(RasterizerTwoSided);
		m_CutoutDepthPSO.Finalize();

		// Depth-only but with a depth bias and/or render only backfaces
		m_ShadowPSO = m_DepthPSO;
		m_ShadowPSO.SetRasterizerState(RasterizerShadow);
		m_ShadowPSO.SetRenderTargetFormats(0, nullptr, g_ShadowBuffer.GetFormat());
		m_ShadowPSO.Finalize();

		// Shadows with alpha testing
		m_CutoutShadowPSO = m_ShadowPSO;
		m_CutoutShadowPSO.SetPixelShader(g_pDepthViewerPS, sizeof(g_pDepthViewerPS));
		m_CutoutShadowPSO.SetRasterizerState(RasterizerShadowTwoSided);
		m_CutoutShadowPSO.Finalize();

		DXGI_FORMAT formats[2] = { ColorFormat, NormalFormat };
		DXGI_FORMAT formatsWithAlbedo[2] = { ColorFormat, NormalFormat };

		//--- CONTAINS ONLY A SINGLE COLOR PASS FOR NOW ---
		// Full color pass
		m_ModelPSO = m_DepthPSO;
		m_ModelPSO.SetBlendState(BlendDisable);
		//m_ModelPSO.SetDepthStencilState(DepthStateReadWrite);
		m_ModelPSO.SetDepthStencilState(DepthStateTestEqual);
		//m_ModelPSO.SetDepthStencilState(DepthStateTestEqual);
		m_ModelPSO.SetRenderTargetFormats(2, formats, DepthFormat);

		m_ModelPSO.SetVertexShader(g_pModelViewerVS, sizeof(g_pModelViewerVS));
		//--- REPLACE WITH CUSTOM PIXEL SHADER ---
		//    m_ModelPSO.SetPixelShader( g_pModelViewerPS, sizeof(g_pModelViewerPS) );
		m_ModelPSO.SetPixelShader(g_pTestRendererPS, sizeof(g_pTestRendererPS));
		m_ModelPSO.Finalize();

		m_CutoutModelPSO = m_ModelPSO;
		m_CutoutModelPSO.SetRasterizerState(RasterizerTwoSided);
		m_CutoutModelPSO.Finalize();
		//--- DEMO PASS FOR RENDERING SPHERE ---
		// Full color pass
		m_TestSpherePSO = m_DepthPSO;
		m_TestSpherePSO.SetBlendState(BlendDisable);
		//m_TestSpherePSO.SetRenderTargetFormats(2, formats, DepthFormat);
		m_TestSpherePSO.SetRenderTargetFormats(1, formats, DepthFormat);
		m_TestSpherePSO.SetInputLayout(_countof(simpleVertElemnt), simpleVertElemnt);
		//--- CHANGE THE DEPTH STATE ALWAYS TO DRAW ON TOP OF GEOMETRY
		//m_TestSpherePSO.SetDepthStencilState(DepthStateDisabled);
		m_TestSpherePSO.SetDepthStencilState(DepthStateReadWrite);
		//--- THIS HAS TO BE SET TO UNKNOWN FORMAT TO CONFORM TO FRAMEWORK
		//m_TestSpherePSO.SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
		//--- MAKE SURE THAT CULLING IS OFF AND BOTH SIDES ARE DRAWN
		m_TestSpherePSO.SetRasterizerState(RasterizerTwoSided);
		//-- CHANGE TO THE NEW SHADER FOR THE TRIANGLE
		m_TestSpherePSO.SetVertexShader(g_pSimpleMeshVS, sizeof(g_pSimpleMeshVS));
		m_TestSpherePSO.SetPixelShader(g_pSimpleMeshPS, sizeof(g_pSimpleMeshPS));
		m_TestSpherePSO.Finalize();


		m_ModelSSRPSO = m_DepthPSO;
		m_ModelSSRPSO.SetBlendState(BlendDisable);
		m_ModelSSRPSO.SetDepthStencilState(DepthStateReadWrite);
		m_ModelSSRPSO.SetRenderTargetFormats(2, formats, DepthFormat);
		m_ModelSSRPSO.SetVertexShader(g_pModelViewerVS, sizeof(g_pModelViewerVS));
		m_ModelSSRPSO.SetPixelShader(g_pScreenSpaceReflectionsPS, sizeof(g_pScreenSpaceReflectionsPS));
		m_ModelSSRPSO.Finalize();

		SetupScene();

		int meshCountPrevious = m_Model->GetMeshCount();

		InitTriangleModel();
		InitQuadModel();
		InitSphereModel();

		context->disc = std::make_unique<DiscMesh>(10);

		// The caller of this function can override which materials are considered cutouts
		m_pMaterialIsCutout.resize(m_Model->GetMaterialCount());
		for (uint32_t i = 0; i < m_Model->GetMaterialCount(); ++i)
		{

			const ModelH3D::Material& mat = m_Model->GetMaterial(i);
			XMVECTOR(mat.ambient).m128_f32[0] = 0;
			XMVECTOR(mat.ambient).m128_f32[1] = 0;
			if (std::string(mat.texDiffusePath).find("thorn") != std::string::npos ||
				std::string(mat.texDiffusePath).find("plant") != std::string::npos ||
				std::string(mat.texDiffusePath).find("chain") != std::string::npos)
			{
				m_pMaterialIsCutout[i] = true;
			}
			else
			{
				m_pMaterialIsCutout[i] = false;
			}
		}

		ParticleEffects::InitFromJSON(L"Sponza/particles.json");

		//Camera Setup needed ? 
		float modelRadius = Length(m_Model->GetBoundingBox().GetDimensions()) * 0.5f;
		const Vector3 eye = m_Model->GetBoundingBox().GetCenter() + Vector3(modelRadius * 0.5f, 0.0f, 0.0f);
		camera.SetEyeAtUp(-eye, Vector3(kZero), Vector3(kYUnitVector));
		//camera.SetPosition(Vector3(m_Transform.getPosition()));
		camera.SetPosition(Vector3(-874.0f,234,-53));




		uint32_t VertexStride = m_Model->GetVertexStride();

		GBufferPtrs gbuffer{
			&g_SceneColorBuffer,
			&colorCopyBuffer,
			&g_SceneNormalBuffer,
			&g_SceneDepthBuffer,


		};

		TestRaytracing::CreateOutputTextureUAV(&g_SceneColorBuffer);
		SurfelIrradianceAccumulation::CreateOutputTextureUAV(&g_SceneColorBuffer);

		context->SurfelIllumination->Setup(
			gbuffer
		);

		TestRaytracing::CreateDeviceDependentResources(
			*context->transform,
			*m_Model,
			&Graphics::g_SceneColorBuffer,
			context->SurfelIllumination->nonShaderVisibleHeap
			//context->SurfelIllumination->srvHeap
		);
		SurfelIrradianceAccumulation::CreateDeviceDependentResources(
			*context->transform,
			*m_Model,
			&Graphics::g_SceneColorBuffer,
			context->SurfelIllumination->nonShaderVisibleHeap
			//context->SurfelIllumination->srvHeap

		);


		//ExtendedUtility::CopyDescriptorsToHeap(Renderer::m_CommonTextures,)


		//CopyColorAndDepthBuffers(args.gfx);



		SSRHeap.Create(L"SSR Heap", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
		colorCopyBuffer.Create(L"Color Copy Buffer", g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight(), 1, ColorFormat);
		depthCopyBuffer.Create(L"Depth Copy Buffer A", g_SceneDepthBuffer.GetWidth(), g_SceneDepthBuffer.GetHeight(), 1, DepthFormat);

		ExtendedUtility::CopyDescriptorsToHeap(
			SSRHeap,
			{
				colorCopyBuffer.GetSRV(),
				depthCopyBuffer.GetDepthSRV()
			},
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		SSRMIrradianceAccumulation.Setup(colorCopyBuffer, context->SurfelIllumination->nonShaderVisibleHeap);



		context->GridVisualization->Setup(
			gbuffer,
			&TestRaytracing::GetOutputBuffer()

		);

		context->m_HBIL = std::make_unique<HBIL>();
		context->m_GBufferDownsample = std::make_unique<GBufferDownsample>();
		//m_GBufferSlice = new GBufferSlice();
		context->m_GBufferDownsample->Setup(gbuffer, context->GridVisualization->m_TestPSO);
		//m_GBufferSlice->Setup(m_GBufferDownsample->GetDownsampledBufferPtr(), GridVisualization->m_TestPSO);

		context->m_HBIL->Setup(
			gbuffer,
			context->m_GBufferDownsample->GetDownsampledBufferPtr(),
			context->GridVisualization->m_TestPSO
		);
//		m_HBILInterleaved->Setup(
//			gbuffer,
//			m_GBufferSlice->GetOuptutBuffer(),
//			GridVisualization->m_TestPSO
//		);
		context->GridMSMEVisualization->Setup(
			gbuffer,
			&TestRaytracing::GetOutputBuffer()
		);
		context->SurfelSpawnVisualization->Setup(
			gbuffer,
			&TestRaytracing::GetOutputBuffer()
		);
		context->SurfelGIVisualization->Setup(
			gbuffer,
			&context->SurfelIllumination->m_OutputTexture
		);
		context->MaterialBindingDebug->Setup(
			gbuffer,
			&TestRaytracing::GetOutputBuffer()
		);


		//SurfelIllumination->SetupInformed(&context->m_HBIL->m_OutputBentCone,&context->m_HBIL->m_OutputIrradiance);
		context->SurfelIllumination->SetupInformed(&context->m_HBIL->m_OutputBentCone, &Graphics::g_SSAOFullScreen);


		//Allocate just and extra descriptor table entry
		uint32_t DestCount = 10;
		// Allocate a descriptor table for the common textures
		Renderer::m_CommonTextures = Renderer::s_TextureHeap.Alloc(2);

		uint32_t SourceCounts[] = { 1, 1, 1, 1, 1, 1, 1, 1,1 };
		D3D12_CPU_DESCRIPTOR_HANDLE SourceTextures[] =
		{
			GetDefaultTexture(kBlackCubeMap),
			GetDefaultTexture(kBlackCubeMap),
			g_SSAOFullScreen.GetSRV(),
			g_ShadowBuffer.GetSRV(),
			Lighting::m_LightBuffer.GetSRV(),
			Lighting::m_LightShadowArray.GetSRV(),
			Lighting::m_LightGrid.GetSRV(),
			Lighting::m_LightGridBitMask.GetSRV(),
			context->SurfelIllumination->m_OutputTexture.GetSRV(),
			context->m_HBIL->m_OutputIrradiance.GetSRV(),
		};
		//       TestRaytracing::GetOutputBuffer().GetSRV()
		g_Device->CopyDescriptors(1, &Renderer::m_CommonTextures, &DestCount, DestCount, SourceTextures, SourceCounts, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


		parameters.push_back(context->SurfelIllumination.get());
		parameters.push_back(context->m_HBIL.get());


	}

	bool m_useSurfelInformedSBGI = true;
	bool m_resetSurfelNextFrame = true;
	 void SurfelGIRenderer::SetSurfelIlluminationAlgorithm(bool isHbilInformedSBGI) 
	{
		m_useSurfelInformedSBGI = isHbilInformedSBGI;
		m_resetSurfelNextFrame = true;
	}
	void SurfelGIRenderer::InitQuadModel()
	{
		uint32_t indices[6] = { 0, 1, 2,3,4,5 };

		size_t vertexStride = sizeof(ColorVertex);
		size_t vertexDataSize = sizeof(fullScreenQuad);
		size_t indexDataSize = sizeof(indices);

		// 2. Allocate upload buffer (vertex + index)
		size_t totalSize = vertexDataSize + indexDataSize;
		void* uploadMem = _aligned_malloc(totalSize, 16);
		assert(uploadMem);

		void* vertexData = uploadMem;
		void* indexData = static_cast<uint8_t*>(uploadMem) + vertexDataSize;

		memcpy(vertexData, fullScreenQuad, vertexDataSize);
		memcpy(indexData, indices, indexDataSize);
		//--- Upload buffer to GPU
		m_QuadGB.Create(L"Full Screen Quad", totalSize, 1, uploadMem);
		//--- Create buffer views
		m_QuadVB
			= m_QuadGB.VertexBufferView(0, vertexDataSize, vertexStride);
		m_QuadIB
			= m_QuadGB.IndexBufferView(vertexDataSize, indexDataSize, true);

	}
	void SurfelGIRenderer::InitTriangleModel()
	{
		uint32_t indices[3] = { 0, 1, 2 };

		size_t vertexStride = sizeof(ColorVertex);
		size_t vertexDataSize = sizeof(triangleVertices);
		size_t indexDataSize = sizeof(indices);

		// 2. Allocate upload buffer (vertex + index)
		size_t totalSize = vertexDataSize + indexDataSize;
		void* uploadMem = _aligned_malloc(totalSize, 16);
		assert(uploadMem);

		void* vertexData = uploadMem;
		void* indexData = static_cast<uint8_t*>(uploadMem) + vertexDataSize;

		memcpy(vertexData, triangleVertices, vertexDataSize);
		memcpy(indexData, indices, indexDataSize);


		ColorVertex tempColorVerts[3];
		memcpy(tempColorVerts, vertexData, vertexDataSize);

		//--- Upload buffer to GPU
		m_GeometryBuffer.Create(L"Colored Triangle", totalSize, 1, uploadMem);
		//--- Create buffer views
		m_VertexBufferView
			= m_GeometryBuffer.VertexBufferView(0, vertexDataSize, vertexStride);
		m_IndexBufferView
			= m_GeometryBuffer.IndexBufferView(vertexDataSize, indexDataSize, true);

	}
	void SurfelGIRenderer::InitSphereModel()
	{
		context->sphere = std::make_unique<SphereMesh>(2);
	}

	UINT SurfelGIRenderer::FindClosesSurfelToPosition(Math::Vector3 position)
	{
		float closestDistance = 999999.0f;
		UINT closesSurfelId = -1;
		std::vector<SurfelData>& surfels = context->SurfelIllumination->m_SurfelData.m_Actual;
		for (size_t i = 0; i < surfels.size(); i++)
		{
			if (surfels[i].radius.GetX() <= 0.5f)
				continue;
			Vector3 surfelPosition = Vector3(surfels[i].position);
			float distance = XMVector3Length((XMVECTOR)(surfelPosition - position)).m128_f32[0];
			if (distance < closestDistance)
			{
				closestDistance = distance;
				closesSurfelId = i;
			}
		}
		return closesSurfelId;


	}

	///---	CLEANUP ---
	void SurfelGIRenderer::Cleanup(void)
	{

		m_Model->Clear();
		Lighting::Shutdown();
		TextureManager::Shutdown();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext(ImGui::GetCurrentContext());
	}

	VSConstants SurfelGIRenderer::SetupObjectVSConstants(RenderObjectInstancePassArgs args)
	{
		VSConstants vsConstants;
		//vsConstants.modelToWorld = Matrix4(SurfelGIRenderer::m_Transform.getTransformMatrix());
		vsConstants.modelToWorld = Matrix4(context->transform->getTransformMatrix());
		vsConstants.modelToProjection = args.viewProjMat;
		vsConstants.modelToShadow = context->sunShadow.GetShadowMatrix();
		XMStoreFloat3(&vsConstants.viewerPos, args.viewerPos);
		return vsConstants;

	}

	void SurfelGIRenderer::RenderFullScreenQuad(GraphicsContext& gfxContext)
	{
		//uint32_t VertexStride = m_Model->GetVertexStride();
		const UINT vertexBufferSize = sizeof(fullScreenQuad);

		//---TEMPORARILY switch index and vertex buffers
		gfxContext.SetIndexBuffer(m_QuadIB);
		gfxContext.SetVertexBuffer(0, m_QuadVB);

		//--- Draw three indices of the triangle
		gfxContext.DrawIndexed(6, 0, 0);

		//--- Switch Back To Sponza model
		gfxContext.SetIndexBuffer(m_Model->GetIndexBuffer());
		gfxContext.SetVertexBuffer(0, m_Model->GetVertexBuffer());
	}
	///--- RENDERING ---
	void SurfelGIRenderer::RenderScreenSpaceTriangle(GraphicsContext& gfxContext)
	{
		//uint32_t VertexStride = m_Model->GetVertexStride();
		const UINT vertexBufferSize = sizeof(triangleVertices);

		//---TEMPORARILY switch index and vertex buffers
		gfxContext.SetIndexBuffer(m_IndexBufferView);
		gfxContext.SetVertexBuffer(0, m_VertexBufferView);

		//--- Draw three indices of the triangle
		gfxContext.DrawIndexed(3, 0, 0);

		//--- Switch Back To Sponza model
		gfxContext.SetIndexBuffer(m_Model->GetIndexBuffer());
		gfxContext.SetVertexBuffer(0, m_Model->GetVertexBuffer());
	}
	void SurfelGIRenderer::RenderOBJObject(const RenderObjectInstancePassArgs args)
	{
		VSConstants vsConstants = SetupObjectVSConstants(args);

		args.gfx.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
		
		args.gfx.SetIndexBuffer(OBJModel.GetIndexBuffer());
		args.gfx.SetVertexBuffer(0,OBJModel.GetVertexBuffer());
		//--- Draw three indices of the triangle
		args.gfx.DrawIndexed(OBJModel.m_pIndices.size(), 0, 0);

		//--- Switch Back To Sponza model
		args.gfx.SetIndexBuffer(m_Model->GetIndexBuffer());
		args.gfx.SetVertexBuffer(0, m_Model->GetVertexBuffer());


	}
	void SurfelGIRenderer::RenderOBJObjectCorrectPipeline(const RenderObjectInstancePassArgs args)
	{
		VSConstants vsConstants = SetupObjectVSConstants(args);
		args.gfx.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);



		const ModelH3D::Mesh& mesh = m_Model->GetMesh(34);

		uint32_t indexCount = mesh.indexCount;
		
		auto format = m_Model->GetIndexBuffer().Format;
			//uint32_t startIndex = 0;
		uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint16_t);
		//uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint32_t);
		uint32_t baseVertex = mesh.vertexDataByteOffset / 56;

		
		args.gfx.DrawIndexed(indexCount, startIndex, baseVertex);

	}
	void SurfelGIRenderer::RenderSpheresAlongRay(Vector4 color,Vector3 rayOrigin, Vector3 rayDirection,int samplesAlongRay,float offset, RenderObjectInstancePassArgs args)
	{
		//E.g after 20 samples the offset from original position should be "offset"
		//  rayDir*raySamples = offset


		for (size_t i = 0; i < samplesAlongRay; i++)
		{
			Vector3 currentPoint = rayOrigin + (rayDirection * i * offset/samplesAlongRay);
			Transform t;
			t.setPosition(currentPoint);
			float scale = 5;
			t.setScale(scale,scale,scale);

			VSConstants vsConstants = SetupObjectVSConstants(args);
			vsConstants.modelToWorld =Matrix4(t.getTransformMatrix());
			args.gfx.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);

			args.gfx.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(Vector4), &color);

			RenderSphereObject(args);
		}

	}

	void SurfelGIRenderer::RenderSphereAt(Vector4 color,float scale, Vector4 position, RenderObjectInstancePassArgs args) {

			Transform t;
			t.setPosition(position);
			t.setScale(scale,scale,scale);

			VSConstants vsConstants = SetupObjectVSConstants(args);
			vsConstants.modelToWorld =Matrix4(t.getTransformMatrix());
			args.gfx.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);

			args.gfx.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(Vector4), &color);

			RenderSphereObject(args);
	}
	void SurfelGIRenderer::RenderSurfelAt(Vector4 color,Vector4 normal,float scale, Vector4 position, RenderObjectInstancePassArgs args) {

		args.gfx.SetIndexBuffer(context->disc->m_IndexBufferView);
		args.gfx.SetVertexBuffer(0, context->disc->m_VertexBufferView);


			Transform t;
			//UINT surfelIndex = SurfelIllumination->m_SurfelListActual[i];
			//SurfelData s = SurfelIllumination->m_SurfelDataArray[surfelIndex];
			SurfelData s;
			s.color = color;
			s.normal = normal;
			s.radius = Vector4(scale,scale,scale,1);
			s.position = position;
			Vector4 extrudedPossition = s.position + (s.normal * 1.0f);


			VSConstants vsConstants = SetupObjectVSConstants(args);

			t.setPosition(extrudedPossition);

			if (XMVector4Length(s.normal).m128_f32[0] > 0.001f)
			{
				Vector4 quat = Vector4(GetRotationQuaternionFromUpToDirection(s.normal));
				t.setQuaternion(quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW());
				t.setComposeRotationFromQuaternions(true);

				t.setScale(s.radius.GetX(), s.radius.GetX(), s.radius.GetX());


				vsConstants.modelToWorld = Matrix4(t.getTransformMatrix());
				XMStoreFloat3(&vsConstants.viewerPos, args.viewerPos);

				args.gfx.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
				args.gfx.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(Vector4), &s.color);
				//--- Draw three indices of the triangle
				args.gfx.DrawIndexed(context->disc->m_Indices.size(), 0, 0);

			}
	}

	void SurfelGIRenderer::RenderSphereObject(const RenderObjectInstancePassArgs args)
	{
		const UINT vertexBufferSize = sizeof(triangleVertices);
		args.gfx.SetIndexBuffer(context->sphere->m_IndexBufferView);
		args.gfx.SetVertexBuffer(0, context->sphere->m_VertexBufferView);

		//gfxContext.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(Vector4), &s.color);
		//--- Draw three indices of the triangle
		args.gfx.DrawIndexed(context->sphere->m_Indices.size(), 0, 0);

		//--- Switch Back To Sponza model
		args.gfx.SetIndexBuffer(m_Model->GetIndexBuffer());
		args.gfx.SetVertexBuffer(0, m_Model->GetVertexBuffer());

	}

	XMVECTOR SurfelGIRenderer::GetRotationQuaternionFromUpToDirection(FXMVECTOR targetDirection)
	{
		const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // original surfel normal
		XMVECTOR dir = XMVector3Normalize(targetDirection);

		float dot = XMVectorGetX(XMVector3Dot(up, dir));

		// Case 1: Vectors are already aligned
		if (dot > 0.99f)
		{
			return XMQuaternionIdentity();
		}
		int a = 3;

		// Case 2: Vectors are opposite
		if (dot < -0.9999f)
		{
			// Pick an arbitrary perpendicular axis to rotate 180 degrees
			XMVECTOR axis = XMVector3Cross(up, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
			if (XMVector3LengthSq(axis).m128_f32[0] < 1e-6f)
			{
				axis = XMVector3Cross(up, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
			}
			axis = XMVector3Normalize(axis);
			return XMQuaternionRotationAxis(axis, XM_PI);
		}

		// General case: shortest arc quaternion between up and direction
		XMVECTOR axis = XMVector3Cross(up, dir);
		float angle = acosf(dot); // angle between vectors
		return XMQuaternionRotationAxis(axis, angle);
	}


	void SurfelGIRenderer::GetRelevantSurfels( UINT& from, UINT& to )
	{
		UniformGrid grid = context->SurfelIllumination->m_SurfelGen.UniformGrid;
		SurfelDebugData data = context->SurfelIllumination->m_SurfelDebugActual;

		UINT gridDimX =
			grid.dimensions.GetX() / grid.cellSize.GetX();
		UINT gridDimY=
			grid.dimensions.GetY() / grid.cellSize.GetY();

		UINT gridDimZ =
			grid.dimensions.GetZ() / grid.cellSize.GetZ();

		UINT linearIndex = data.PointedCellX  +
           data.PointedCellY * gridDimX   +
           data.PointedCellZ * gridDimX * gridDimY;


		 from = context->SurfelIllumination->m_SurfelGrid.m_Actual[linearIndex];
		 to = context->SurfelIllumination->m_SurfelGrid.m_Actual[linearIndex+1];

	}

	void SurfelGIRenderer::RenderRelevantSurfels(const RenderObjectInstancePassArgs args)
	{
		VSConstants vsConstants =SetupObjectVSConstants(args);

		const UINT vertexBufferSize = sizeof(triangleVertices);
		args.gfx.SetIndexBuffer(context->disc->m_IndexBufferView);
		args.gfx.SetVertexBuffer(0, context->disc->m_VertexBufferView);

		UINT surfelListIndexFrom ;
		UINT surfelListIndexTo ;
		GetRelevantSurfels(surfelListIndexFrom, surfelListIndexTo);

		for (size_t i = surfelListIndexFrom; i < surfelListIndexTo; ++i)
		{

			Transform t;
			//UINT surfelIndex = SurfelIllumination->m_SurfelListActual[i];
			UINT surfelIndex = context->SurfelIllumination->m_SurfelList.m_Actual[i];
			//SurfelData s = SurfelIllumination->m_SurfelDataArray[surfelIndex];
			SurfelData s = context->SurfelIllumination->m_SurfelData.m_Actual[surfelIndex];
			Vector4 extrudedPossition = s.position + (s.normal * 1.0f);



			t.setPosition(extrudedPossition);

			if (XMVector4Length(s.normal).m128_f32[0] > 0.001f)
			{
				Vector4 quat = Vector4(GetRotationQuaternionFromUpToDirection(s.normal));
				t.setQuaternion(quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW());
				t.setComposeRotationFromQuaternions(true);

				t.setScale(s.radius.GetX(), s.radius.GetX(), s.radius.GetX());


				vsConstants.modelToWorld = Matrix4(t.getTransformMatrix());
				XMStoreFloat3(&vsConstants.viewerPos, args.viewerPos);

				args.gfx.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
				args.gfx.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(Vector4), &s.color);
				//--- Draw three indices of the triangle
				args.gfx.DrawIndexed(context->disc->m_Indices.size(), 0, 0);

			}

		}
		//--- Switch Back To Sponza model
		args.gfx.SetIndexBuffer(m_Model->GetIndexBuffer());
		args.gfx.SetVertexBuffer(0, m_Model->GetVertexBuffer());

	}
	void SurfelGIRenderer::RenderSurfels(const RenderObjectInstancePassArgs args)
	{

		float modelRadius = Length(m_Model->GetBoundingBox().GetDimensions()) * 0.5f;
		const Vector3 eye = m_Model->GetBoundingBox().GetCenter() + Vector3(modelRadius * 0.5f, 0.0f, 0.0f);
		const Vector3 relSphereOffset(0, 0, 0.1);
		const Vector3 offset = relSphereOffset * modelRadius;


		for each (SurfelData s in context->SurfelIllumination->m_SurfelData.m_Actual)
		{
			if ((float)s.radius.GetX() == 0)
			{
				continue;
			}
			if (s.isSurfelCap)
			{
				//Different visualization strategy for the surfel NF 
				//Render sphere with radius at position
				RenderSphereAt(Vector4(0,0,0,1), s.radius.GetX()/2.0f, s.position, args);

				VSConstants vsConstants = SetupObjectVSConstants(args);
				const UINT vertexBufferSize = sizeof(triangleVertices);
				//---TEMPORARILY switch index and vertex buffers
				args.gfx.SetIndexBuffer(context->disc->m_IndexBufferView);
				args.gfx.SetVertexBuffer(0, context->disc->m_VertexBufferView);

				//This postioing woudl be extruded along bent normal
				Transform t;
				Vector4 extrudedPosition = s.position + (s.normal * s.height);
				t.setPosition(extrudedPosition);

				Vector4 quat = Vector4(GetRotationQuaternionFromUpToDirection(s.normal));
				t.setQuaternion(quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW());
				t.setComposeRotationFromQuaternions(true);
				t.setScale(s.radius.GetX(), s.radius.GetX(), s.radius.GetX());

				vsConstants.modelToWorld = Matrix4(t.getTransformMatrix());
				XMStoreFloat3(&vsConstants.viewerPos, args.viewerPos);

				args.gfx.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
				Vector4 tempYellow = Vector4(1, 0, 1, 1);
				//gfxContext.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(Vector4), &s.color);
				args.gfx.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(Vector4), &tempYellow);


				//--- Draw three indices of the triangle
				args.gfx.DrawIndexed(context->disc->m_Indices.size(), 0, 0);


				//Render a surfel by offsetting height alogn bent normal


			}
			else if (XMVector4Length(s.normal).m128_f32[0] > 0.001f)
			{
				VSConstants vsConstants = SetupObjectVSConstants(args);
				const UINT vertexBufferSize = sizeof(triangleVertices);
				//---TEMPORARILY switch index and vertex buffers
				args.gfx.SetIndexBuffer(context->disc->m_IndexBufferView);
				args.gfx.SetVertexBuffer(0, context->disc->m_VertexBufferView);

			Transform t;
			t.setPosition(s.position);

				Vector4 quat = Vector4(GetRotationQuaternionFromUpToDirection(s.normal));
				t.setQuaternion(quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW());
				t.setComposeRotationFromQuaternions(true);
				t.setScale(s.radius.GetX(), s.radius.GetX(), s.radius.GetX());

				vsConstants.modelToWorld = Matrix4(t.getTransformMatrix());
				XMStoreFloat3(&vsConstants.viewerPos, args.viewerPos);

				args.gfx.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
				args.gfx.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(Vector4), &s.color);

				//--- Draw three indices of the triangle
				args.gfx.DrawIndexed(context->disc->m_Indices.size(), 0, 0);

			}
			else {
				int a = 3;
			}


		}
		//--- Switch Back To Sponza model
		args.gfx.SetIndexBuffer(m_Model->GetIndexBuffer());
		args.gfx.SetVertexBuffer(0, m_Model->GetVertexBuffer());

	}
	void SurfelGIRenderer::RenderObjects(const RenderObjectInstancePassArgs args)
	{
		struct VSConstanstsWithModel
		{
			Matrix4 modelToProjection;
			Matrix4 modelToShadow;
			Matrix4 modelToWorld;
			XMFLOAT3 viewerPos;
		} vsConstants;
		vsConstants.modelToProjection = args.viewProjMat;
		vsConstants.modelToShadow = context->sunShadow.GetShadowMatrix();
		vsConstants.modelToWorld = Matrix4(XMMatrixIdentity());
		XMStoreFloat3(&vsConstants.viewerPos, args.viewerPos);


		__declspec(align(16)) uint32_t materialIdx = 0xFFFFFFFFul;

		uint32_t VertexStride = m_Model->GetVertexStride();
		UINT a = sizeof(vsConstants);

		for (uint32_t meshIndex = 0; meshIndex < m_Model->GetMeshCount(); meshIndex++)
		{
			if (meshIndex > 34)
				continue;

			if (meshIndex > 33)
			{
				vsConstants.modelToWorld = Matrix4(context->transform->getTransformMatrix());


			}
			args.gfx.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
			const ModelH3D::Mesh& mesh = m_Model->GetMesh(meshIndex);

			uint32_t indexCount = mesh.indexCount;
		
			auto format = m_Model->GetIndexBuffer().Format;
			//uint32_t startIndex = 0;
			uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint16_t);
			//uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint32_t);
			uint32_t baseVertex = mesh.vertexDataByteOffset / VertexStride;

			if (mesh.materialIndex != materialIdx)
			{
				if (m_pMaterialIsCutout[mesh.materialIndex] && !(args.filter & kCutout) ||
					!m_pMaterialIsCutout[mesh.materialIndex] && !(args.filter & kOpaque))
					continue;

				materialIdx = mesh.materialIndex;
				args.gfx.SetDescriptorTable(Renderer::kMaterialSRVs, m_Model->GetSRVs(materialIdx));
				args.gfx.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(uint32_t), &materialIdx);
			}

			args.gfx.DrawIndexed(indexCount, startIndex, baseVertex);
		}

	}

	void SurfelGIRenderer::CopyColorAndDepthBuffers(GraphicsContext& gfxContext) 
	{
		//Copy The Color Buffer
		gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
		gfxContext.TransitionResource(colorCopyBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
		gfxContext.CopyBuffer(colorCopyBuffer, g_SceneColorBuffer);
		
		//Copy The Depth Buffer
		gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
		gfxContext.TransitionResource(depthCopyBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
		gfxContext.CopyBuffer(depthCopyBuffer, g_SceneDepthBuffer);

	 }

	void SurfelGIRenderer::RenderSSR(GraphicsContext& gfxContext, const Camera& camera,UINT objectIndex) 
	{

		struct VSConstanstsWithModel
		{
			Matrix4 modelToProjection;
			Matrix4 modelToShadow;
			Matrix4 modelToWorld;
			XMFLOAT3 viewerPos;
		} vsConstants;
		vsConstants.modelToProjection = camera.GetViewProjMatrix();
		vsConstants.modelToShadow = context->sunShadow.GetShadowMatrix();
		vsConstants.modelToWorld = Matrix4(XMMatrixIdentity());
		XMStoreFloat3(&vsConstants.viewerPos, camera.GetPosition());
		__declspec(align(16)) uint32_t materialIdx = 0xFFFFFFFFul;

		uint32_t VertexStride = m_Model->GetVertexStride();
		UINT a = sizeof(vsConstants);
		UINT meshIndex = objectIndex;

		vsConstants.modelToWorld = Matrix4(context->transform->getTransformMatrix());


		gfxContext.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
		const ModelH3D::Mesh& mesh = m_Model->GetMesh(meshIndex);

		uint32_t indexCount = mesh.indexCount;

		auto format = m_Model->GetIndexBuffer().Format;
		uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint16_t);
		uint32_t baseVertex = mesh.vertexDataByteOffset / VertexStride;


		__declspec(align(16)) struct SSRCameraData
		{
			Matrix4 cameraViewMatrix;
			Matrix4 cameraProjMatrix;
			Matrix4 cameraWorldMatrix;
			Matrix4 inverseViewMatrix;
			Matrix4 inverseProjMatrix;
			Vector3 CameraPosition;
		};
		__declspec(align(16)) struct SSRParameters
		{
			int useSSR;
			float maxLengthInWorldUnits;
			int maxSteps;
			float thicknessInUnits;
			float resolution;
			int width;
			int height;
		};

		__declspec(align(16)) struct CommonSSR {

			SSRCameraData cameraData;
			SSRParameters ssrParameters;

		}  commonSSR;
		Transform  camTempT;
		camTempT.setPosition(camera.GetPosition());

		commonSSR.cameraData.cameraProjMatrix = camera.GetProjMatrix();
		commonSSR.cameraData.cameraViewMatrix = camera.GetViewMatrix();
		commonSSR.cameraData.cameraWorldMatrix = Matrix4(camTempT.getTransformMatrix());

		commonSSR.cameraData.inverseProjMatrix = Matrix4(XMMatrixInverse(nullptr,camera.GetProjMatrix()));
		commonSSR.cameraData.inverseViewMatrix = Matrix4(XMMatrixInverse(nullptr,camera.GetViewMatrix()));
		commonSSR.cameraData.CameraPosition = camera.GetPosition();

		//Transpose all matrices if necessary
		commonSSR.cameraData.cameraProjMatrix = Matrix4(XMMatrixTranspose(commonSSR.cameraData.cameraProjMatrix));
		commonSSR.cameraData.cameraViewMatrix = Matrix4(XMMatrixTranspose(commonSSR.cameraData.cameraViewMatrix));
		commonSSR.cameraData.cameraWorldMatrix = Matrix4(XMMatrixTranspose(commonSSR.cameraData.cameraWorldMatrix));
		commonSSR.cameraData.inverseProjMatrix = Matrix4(XMMatrixTranspose(commonSSR.cameraData.inverseProjMatrix));
		commonSSR.cameraData.inverseViewMatrix = Matrix4(XMMatrixTranspose(commonSSR.cameraData.inverseViewMatrix));

		commonSSR.ssrParameters.useSSR = true;
		commonSSR.ssrParameters.width = g_SceneColorBuffer.GetWidth();
		commonSSR.ssrParameters.height = g_SceneColorBuffer.GetHeight();
		commonSSR.ssrParameters.maxSteps = 1000;
		commonSSR.ssrParameters.maxLengthInWorldUnits = 1000;
		commonSSR.ssrParameters.thicknessInUnits = 50;
		commonSSR.ssrParameters.resolution = 1;


		if (mesh.materialIndex != materialIdx)
		{
			materialIdx = mesh.materialIndex;
			gfxContext.SetDescriptorTable(Renderer::kMaterialSRVs, m_Model->GetSRVs(materialIdx));
			gfxContext.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(commonSSR), &commonSSR);
		}

		gfxContext.DrawIndexed(indexCount, startIndex, baseVertex);

	}
	void SurfelGIRenderer::RenderLightShadowsPass(GraphicsContext& gfxContext, const Camera& camera)
	{
		using namespace Lighting;

		ScopedTimer _prof(L"RenderLightShadows", gfxContext);

		static uint32_t LightIndex = 0;
		if (LightIndex >= MaxLights)
			return;

		m_LightShadowTempBuffer.BeginRendering(gfxContext);
		{
			gfxContext.SetPipelineState(m_ShadowPSO);
			RenderObjectInstancePassArgs opaqueLigthShadows{
				gfxContext,
				m_LightShadowMatrix[LightIndex],
				camera.GetPosition(),
				kOpaque
			};

			//Same camera position and view matrix but different filter
			RenderObjectInstancePassArgs cutoutLightShadows = opaqueLigthShadows;
			cutoutLightShadows.filter = kCutout;

			RenderObjects(opaqueLigthShadows);
			gfxContext.SetPipelineState(m_CutoutShadowPSO);
			RenderObjects(cutoutLightShadows);
		}
		m_LightShadowTempBuffer.EndRendering(gfxContext);

		gfxContext.TransitionResource(m_LightShadowTempBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
		gfxContext.TransitionResource(m_LightShadowArray, D3D12_RESOURCE_STATE_COPY_DEST);

		gfxContext.CopySubresource(m_LightShadowArray, LightIndex, m_LightShadowTempBuffer, 0);

		gfxContext.TransitionResource(m_LightShadowArray, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		++LightIndex;
	}



	float bunnyScale = 1000;

	void SurfelGIRenderer::RenderImGuiUI(GraphicsContext& gfx) {
		ImGuiIO& io = ImGui::GetIO();


		if (io.WantCaptureKeyboard == false)
		{

			bool pressedDebugButton = GameInput::IsFirstPressed(GameInput::kKey_f);
			bool pressedDebugMode = GameInput::IsFirstPressed(GameInput::kKey_g);
			bool pressedStopButton = GameInput::IsFirstPressed(GameInput::kKey_b);
			bool pressedOnlyRelevantButton = GameInput::IsFirstPressed(GameInput::kKey_t);
			bool pressedToggleFillAccelerationStructure = GameInput::IsFirstPressed(GameInput::kKey_z);
			bool pressedInclinationPlus = GameInput::IsFirstPressed(GameInput::kKey_j);
			bool pressedInclinatoinMinus = GameInput::IsFirstPressed(GameInput::kKey_k);
			bool pressedOrientationPlus = GameInput::IsFirstPressed(GameInput::kKey_u);
			bool pressedOriantationMinus = GameInput::IsFirstPressed(GameInput::kKey_i);
			bool pressedToggleApplyBlend = GameInput::IsFirstPressed(GameInput::kKey_n);
			bool pressedTogglePhysicalSurfels = GameInput::IsFirstPressed(GameInput::kKey_h);

			bool pressedResetSurfels = GameInput::IsFirstPressed(GameInput::kKey_x);
			bool pressedResetSurfelsIrradiance = GameInput::IsFirstPressed(GameInput::kKey_c);
			bool pressedToggleSSR = GameInput::IsFirstPressed(GameInput::kKey_m);
			bool pressedToggleDebugHBIL = GameInput::IsFirstPressed(GameInput::kKey_p);
			//bool pressedToggleRenderAOOnScreen = GameInput::IsFirstPressed(GameInput::kKey_z);

			//ImGui::Checkbox("Use Blending", &m_useBle);

			if (pressedToggleDebugHBIL)
			{
				m_hbil_render = !m_hbil_render;
				//m_hbil_updateDebug = !m_hbil_updateDebug;

			}

			if (pressedToggleSSR)
			{
				m_drawSSRTest = !m_drawSSRTest;

			}
			if (pressedResetSurfels)
			{
				context->SurfelIllumination->ResetSurfels(gfx);

			}
			if (pressedResetSurfelsIrradiance)
			{
				context->SurfelIllumination->ReadbackSurfelData(gfx);
				//Reset Surfels Code
				context->SurfelIllumination->ResetSurfelsIrradiance(gfx);

			}



			if (pressedTogglePhysicalSurfels)
				m_drawPhysicalSurfelInstances = !m_drawPhysicalSurfelInstances;
			if (pressedToggleApplyBlend)
				m_enableBlending = !m_enableBlending;


			if (pressedToggleFillAccelerationStructure)
			{
				m_useSimpleAlgorithm = !m_useSimpleAlgorithm;

			}

			if (pressedOnlyRelevantButton)
				m_renderOnlyCurrentCellSurfels = !m_renderOnlyCurrentCellSurfels;
			if (pressedDebugButton)
				m_enableDebugOverlay = !m_enableDebugOverlay;

			if (pressedDebugMode)
			{
				m_debugOverlayMode++;
				if (m_debugOverlayMode > 7)
					m_debugOverlayMode = 0;

				//m_debugOverlayMode = !((bool)m_debugOverlayMode);

			}

			if (pressedStopButton)
				m_stopSurfelUpdate = !m_stopSurfelUpdate;

			if (pressedInclinationPlus)
				m_sunData.sunInclination += 0.1f;
			if (pressedInclinatoinMinus)
				m_sunData.sunInclination -= 0.1f;

			if (pressedOrientationPlus)
				m_sunData.sunOrientation += 5.0f;
			if (pressedOriantationMinus)
				m_sunData.sunOrientation -= 5.0f;


		}

		// Your ImGui UI code here
		ImGui::Begin("Surfel Gen CB");

		ImGui::DragFloat("Sun Ambient", &m_sunData.ambientLightIntensity, 0.01f, 0, 1.0f);
		ImGui::Checkbox("Use Augmented", &m_useSurfelInformedSBGI);

		context->SurfelIllumination->RenderImGui();

		static bool debugCollapsingHeader = true;
		if(ImGui::CollapsingHeader("Debug", &debugCollapsingHeader))
		{
			ImGui::Checkbox("Enable Debug Overlay", &m_enableDebugOverlay);
			ImGui::Checkbox("Stop Surfle Spawn Recycle", &m_stopSurfelUpdate);
			//ImGui::Checkbox("Fill Acceleration Structures Simple Algorithm", &m_useSimpleAlgorithm);
			ImGui::Checkbox("Render Only Relevant Surfels", &m_renderOnlyCurrentCellSurfels);
			ImGui::DragInt("Debug Mode", &m_debugOverlayMode);

		}

		static bool sunControlCollapsingHeader = true;
		if(ImGui::CollapsingHeader("Sun Data", &sunControlCollapsingHeader))
		{
			//ImGui::DragFloat3("Sun Direction", &m_sunData.sunDirection.x,0.01,0,1.0f);
			ImGui::DragFloat("Sun Orientation", &m_sunData.sunOrientation,5,-100,100.0f);
			ImGui::DragFloat("Sun Inclination", &m_sunData.sunInclination,0.01,0,1.0f);
			ImGui::DragFloat("Sun Ambient Light ", &m_sunData.ambientLightIntensity,0.01,0,1.0f);
			ImGui::DragFloat("Sun Intensity", &m_sunData.sunLightIntensity,0.01,0,1.0f);

		}
		static bool hbilDebug = true;
		if(ImGui::CollapsingHeader("hbilDebug Data", &hbilDebug))
		{
			//ImGui::DragFloat3("Sun Direction", &m_sunData.sunDirection.x,0.01,0,1.0f);
			ImGui::Checkbox("Update HBIL", &m_hbil_updateDebug);
			ImGui::Checkbox("Render HBIL", &m_hbil_render);
			ImGui::Checkbox("Debug Visualize HBIL Rays ", &m_hbil_drawDebug);


		//m_HBILExtraCB._gatherSphereMaxRadius_m = 1000;
		//m_HBILExtraCB._gatherSphereMaxRadius_p = 50;
			ImGui::SliderFloat("Gathre Sphere Max Radius Meters", &context->m_HBIL->m_HBILExtraCB._gatherSphereMaxRadius_m,100,4000);
			ImGui::SliderFloat("Garher Spherre Max Radius Pixels", &context->m_HBIL->m_HBILExtraCB._gatherSphereMaxRadius_p,0,1500);

		}


		//ImGui::Checkbox("Use Screen-Space Ray Marching Only ", &m_useSSRMonly);
		ImGui::Checkbox("Draw Physical Surfel Objects", &m_drawPhysicalSurfelInstances);

		ImGui::DragFloat("Bunny Scale", &bunnyScale,1.0,1,3000);
		context->transform->setScale(bunnyScale, bunnyScale, bunnyScale);


		std::stringstream ss;
		UINT from, to;
		GetRelevantSurfels(from, to);
		SurfelDebugData dd = context->SurfelIllumination->m_SurfelDebugActual;
		ss << "Surfel In Pointed Cell: " << dd.PointedCellX << " " << dd.PointedCellY << " " << dd.PointedCellZ;
		ss << "\n";
		ImGui::Text(ss.str().c_str());

		for (size_t i = from; i < to; i++)
		{
			std::stringstream perSurfelStringStream;
			UINT surfelIndex = context->SurfelIllumination->m_SurfelList.m_Actual[i];
			SurfelData s = context->SurfelIllumination->m_SurfelData.m_Actual[surfelIndex];
			perSurfelStringStream << "	S ID:	" << surfelIndex << "	POS:" << s.position.GetX() << "," << s.position.GetY() << "," << s.position.GetZ() << ",";

			ImGui::Text(perSurfelStringStream.str().c_str());

			ImVec4 imguiColorVecTemp(s.color.GetX(), s.color.GetY(), s.color.GetZ(), 1);

			//ImVec4 imguiColorVecTemp(0,1,0,1);
			ImVec4 imguiBgColorVec(1,1,1,1);


			std::stringstream surfelColorName;
			//surfelColorName << "SURFEL COLOR" << i ;
			surfelColorName << "SURFEL COLOR: " << i << s.color.GetX() << ":	" << s.color.GetY()	  << ":		"	 << s.color.GetZ();
			std::string a = surfelColorName.str();

			ImVec4 imguiWhite(1,1,1,1);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertFloat4ToU32(imguiWhite));
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertFloat4ToU32(imguiColorVecTemp));

			ImGui::InputText(a.c_str(), &a[0], a.size());
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();


		}
		std::stringstream cameraPositionString;
		auto camPos = context->lastCamera.GetPosition();
		cameraPositionString << "Camera Position: " << camPos.GetX() << "	" << camPos.GetY() << "	" << camPos.GetZ();
		ImGui::Text(cameraPositionString.str().c_str());

		std::stringstream closestSurfelIDString;
		UINT sID = FindClosesSurfelToPosition(camPos);
		closestSurfelIDString << "Closest Surfel: " << sID;
		ImGui::Text(closestSurfelIDString.str().c_str());

		ImGui::End();

	}

	 void SurfelGIRenderer::SetCommandListDefaultState(GraphicsContext& gfx)
	{
		gfx.SetRootSignature(Renderer::m_RootSig);
		gfx.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Renderer::s_TextureHeap.GetHeapPointer());
		gfx.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		gfx.SetIndexBuffer(m_Model->GetIndexBuffer());
		gfx.SetVertexBuffer(0, m_Model->GetVertexBuffer());

	}

	const ModelH3D& SurfelGIRenderer::GetModel()
	{
		return *SurfelGIRenderer::m_Model;
	}


XMVECTOR VectorProjection(XMVECTOR u, XMVECTOR v, float* scalarOut)
{
    XMVECTOR dotProduct = XMVector3Dot(u, v);
    XMVECTOR vSquaredLength = XMVector3LengthSq(v);

    // Prevent divide by zero
    XMVECTOR zeroMask = XMVectorEqual(vSquaredLength, XMVectorZero());
    XMVECTOR safeLength = XMVectorSelect(vSquaredLength, XMVectorReplicate(1.0f), zeroMask);

    XMVECTOR scalar = XMVectorDivide(dotProduct, safeLength);

    if (scalarOut)
        *scalarOut = XMVectorGetX(scalar);

    return XMVectorMultiply(scalar, v);
}
	 void SurfelGIRenderer::RenderColorPass(RenderArgs& args)
	 {
		UpdateColorPassPixelShaderArgs(context->frameIndex);

		 context->lastCamera = args.camera;
		 {
			 ScopedTimer _prof2(L"Render Color", args.gfx);

			 args.gfx.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			 args.gfx.InsertUAVBarrier(context->SurfelIllumination->m_SurfelGrid.m_GPUBuffer);
			 args.gfx.InsertUAVBarrier(context->SurfelIllumination->m_SurfelList.m_GPUBuffer);
			 args.gfx.InsertUAVBarrier(context->SurfelIllumination->m_SurfelData.m_GPUBuffer);

			 //args.gfx.TransitionResource(TestRaytracing::GetOutputBuffer(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			 args.gfx.SetDescriptorTable(Renderer::kCommonSRVs, Renderer::m_CommonTextures);
			 args.gfx.SetDynamicConstantBufferView(Renderer::kMaterialConstants, sizeof(psConstants), &psConstants);

			 {
				 args.gfx.SetPipelineState(m_ModelPSO);
				 args.gfx.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
				 D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ g_SceneColorBuffer.GetRTV(), g_SceneNormalBuffer.GetRTV() };
				 args.gfx.SetRenderTargets(ARRAYSIZE(rtvs), rtvs, g_SceneDepthBuffer.GetDSV_DepthReadOnly());
				 args.gfx.SetViewportAndScissor(args.viewport, args.scissor);
			 }

			 RenderObjects(RenderObjectInstancePassArgs{args.gfx,args.camera.GetViewProjMatrix(),args.camera.GetPosition(),kOpaque});
			 //Diffuse only from the scene rended pass
			 //CopyColorAndDepthBuffers(args.gfx);

			 args.gfx.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			 args.gfx.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);


			 if (m_drawPhysicalSurfelInstances)
			 {
				 ScopedTimer _prof3(L"Render Sphere", args.gfx);
				 args.gfx.SetPipelineState(m_TestSpherePSO);
				 args.gfx.SetRootSignature(m_TestSpherePSO.GetRootSignature());
				 args.gfx.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
				 args.gfx.TransitionResource(g_SceneNormalBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
				 args.gfx.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
				 D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ g_SceneColorBuffer.GetRTV(), g_SceneNormalBuffer.GetRTV() };
				 args.gfx.SetRenderTargets(ARRAYSIZE(rtvs), rtvs, g_SceneDepthBuffer.GetDSV_DepthReadOnly());
				 args.gfx.SetViewportAndScissor(args.viewport, args.scissor);


				 
				 auto renderOpaqueInstancesArgs = 
					 RenderObjectInstancePassArgs{ args.gfx, args.camera.GetViewProjMatrix(), args.camera.GetPosition(), kOpaque };



				 if (m_renderOnlyCurrentCellSurfels)
					 RenderRelevantSurfels(renderOpaqueInstancesArgs);
				 else
					 RenderSurfels(renderOpaqueInstancesArgs);

				 if(m_hbil_drawDebug && context->m_HBIL->m_DebugHBILActual.size()>0)
				 {
					 //DEBUG RENDERING OF RAYS OF HBIL SAMPLED WITH SPHERES
					 auto d = context->m_HBIL->m_DebugHBILActual[0];

					 float samples = 10;
					 float offset = 50;
					 Vector4 red = Vector4(1, 0, 0, 1);
					 Vector4 blue = Vector4(0, 0, 1, 1);
					 Vector4 green = Vector4(0, 1, 0, 1);
					 Vector4 magenta = Vector4(1, 0, 1, 1);
					 Vector4 yellow = Vector4(1, 1, 0, 1);
					 Vector4 white = Vector4(1, 1, 1, 1);


					 float HALF_FOV = tan(args.camera.GetFOV()/2);

					 //Render local args.camera-space vectors
					 Vector3 LocalSpaceAt = Vector3(XMLoadFloat4(&d.reconstructedWorldSpacePosition));
					 Vector3 LocalCameraUp = Vector3(XMLoadFloat4(&d.localCameraDirectionUp));
					 Vector3 LocalCameraRight = Vector3(XMLoadFloat4(&d.localCameraDirectionRight));
					 Vector3 LocalCameraAt = Vector3(XMLoadFloat4(&d.localCameraDirectionAt));

					 RenderSpheresAlongRay(blue,LocalSpaceAt, LocalCameraUp, samples,offset,renderOpaqueInstancesArgs);
					 RenderSpheresAlongRay(blue, LocalSpaceAt, LocalCameraRight, samples, offset, renderOpaqueInstancesArgs);
				     RenderSpheresAlongRay(blue, LocalSpaceAt, LocalCameraAt, samples, offset, renderOpaqueInstancesArgs);
					 
					 Vector4 BentNormalAtW = Vector4(XMLoadFloat4(&d.bentNormalAtW));

					 for (size_t i = 0; i < 16; i++)
					 {
						 auto currentAngleDebugData = context->m_HBIL->m_DebugHBILActual[i];
						 Vector4 wsFront = Vector4(XMLoadFloat4(&currentAngleDebugData.wsSampleFront));
						Vector4 wsBack = Vector4(XMLoadFloat4(&currentAngleDebugData.wsSampleBack));
						RenderSphereAt(red, 1, wsFront, renderOpaqueInstancesArgs);
						RenderSphereAt(green, 1, wsBack, renderOpaqueInstancesArgs);
					 }
					 

					 XMVECTOR normalizedNormal = XMVector3Normalize(BentNormalAtW);
					 RenderSpheresAlongRay(magenta, LocalSpaceAt, Vector3(normalizedNormal), samples, offset, renderOpaqueInstancesArgs);

					 float height = BentNormalAtW.GetW();
					 XMVECTOR worldPosition = XMVectorAdd(LocalSpaceAt,XMVectorScale(normalizedNormal,height));
						 
					 //float cosAngle = BentNormalAtW.GetW() / 2.0f;
					 float cosAngle = cosf(25.0f);
					 float sinAngle = sqrt(1.0 - cos(cosAngle));
					 float tanAngle = sinAngle / cosAngle;
						 
					 //Calculate cone radius based on bent cone angle and height
					 float radiusOfCone = abs(height) * tanAngle * 2;


					 radiusOfCone = min(40.0f, radiusOfCone);

					 RenderSphereAt(green, 3, Vector4(d.wsSampleAverage), renderOpaqueInstancesArgs);
					 RenderSurfelAt(yellow,Vector4(normalizedNormal), radiusOfCone, Vector4(worldPosition),renderOpaqueInstancesArgs);
				 }
			 }


			 if (m_drawSSRTest)
			 {
				 ScopedTimer _prof3(L"Render SSR", args.gfx);
				 args.gfx.SetPipelineState(m_ModelSSRPSO);
				 args.gfx.SetRootSignature(m_ModelSSRPSO.GetRootSignature());
				 args.gfx.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SSRHeap.GetHeapPointer());

				 //CopyColorAndDepthBuffers(args.gfx);
				 //PIXEL SHADER RESOURCE 
				 args.gfx.TransitionResource(colorCopyBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				 args.gfx.TransitionResource(depthCopyBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

				 args.gfx.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
				 args.gfx.TransitionResource(g_SceneNormalBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);

				 args.gfx.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
				 D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ g_SceneColorBuffer.GetRTV(), g_SceneNormalBuffer.GetRTV() };
				 args.gfx.SetRenderTargets(ARRAYSIZE(rtvs), rtvs, g_SceneDepthBuffer.GetDSV_DepthReadOnly());
				 args.gfx.SetViewportAndScissor(args.viewport, args.scissor);
				 args.gfx.SetDescriptorTable(Renderer::kCommonSRVs, SSRHeap[0]);
				 //args.camera.GetViewMatrix();
				 //args.camera.GetProjMatrix();
				 RenderSSR(args.gfx, args.camera, 35);
			 }

				//                --- SKIP NORMAL CUTOUTS---
				//args.gfx.SetPipelineState(m_CutoutModelPSO);
				//RenderObjects( args.gfx, args.camera.GetViewProjMatrix(), args.camera.GetPosition(), TestRenderer::kCutout );
		 }

	 }

	 void SurfelGIRenderer::RenderDebugOverlayPass(RenderArgs& args)
	 {
		 if (m_debugOverlayMode == 0)
		 {
			 ScopedTimer _prof3(L"Surfel Density Debug Overlay", args.gfx);
			 context->GridVisualization->SetupRenderStage(args.gfx, args.viewport, args.scissor,
				 TestRaytracing::GetOutputBuffer(),
				 args.camera);

			 context->SurfelIllumination->UpdateProjection(args.camera);
			 context->SurfelIllumination->SendParametersGraphics(args.gfx);

			 args.gfx.InsertUAVBarrier(context->SurfelIllumination->m_SurfelGrid.m_GPUBuffer);
			 args.gfx.InsertUAVBarrier(context->SurfelIllumination->m_SurfelList.m_GPUBuffer);
			 args.gfx.InsertUAVBarrier(context->SurfelIllumination->m_SurfelStack.m_GPUBuffer);
			 args.gfx.InsertUAVBarrier(TestRaytracing::GetOutputBuffer());
			 RenderFullScreenQuad(args.gfx);

		 }
		 else if (m_debugOverlayMode == 1)
		 {
			 ScopedTimer _prof3(L"Surfel GI Only Debug Overlay", args.gfx);

			 context->SurfelGIVisualization->SetupRenderStage(args.gfx, args.viewport, args.scissor,
				 context->SurfelIllumination->m_OutputTexture,
				 args.camera);

			 args.gfx.TransitionResource(context->SurfelIllumination->m_OutputTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,true);

			 context->SurfelGIVisualization->SetRootParameters(args.gfx, context->SurfelIllumination->m_OutputTexture);
			 RenderFullScreenQuad(args.gfx);
		 }
		 else if (m_debugOverlayMode == 2)
		 {
			 ScopedTimer _prof3(L"Surfel MSME Debug Overlay", args.gfx);

			 context->GridMSMEVisualization->SetupRenderStage(args.gfx, args.viewport, args.scissor,
				 TestRaytracing::GetOutputBuffer(),
				 args.camera);

			 args.gfx.InsertUAVBarrier(TestRaytracing::GetOutputBuffer());

			 context->SurfelIllumination->UpdateProjection(args.camera);
			 context->SurfelIllumination->SendParametersGraphics(args.gfx);

			 RenderFullScreenQuad(args.gfx);

		 }
		 else if (m_debugOverlayMode == 3)
		 {
			 ScopedTimer _prof3(L"Material Binding Ouptut", args.gfx);

			 args.gfx.TransitionResource(TestRaytracing::GetOutputBuffer(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			 context->MaterialBindingDebug->SetupRenderStage(args.gfx, args.viewport, args.scissor,
				 TestRaytracing::GetOutputBuffer(),
				 args.camera);
			 context->MaterialBindingDebug->SetRootParameters(args.gfx, TestRaytracing::GetOutputBuffer());
			 RenderFullScreenQuad(args.gfx);
		 }
		 else if (m_debugOverlayMode >= 4)
		 {
			 ScopedTimer _prof3(L"Surfel Spawn Chance Debug Overlay", args.gfx);
			 context->SurfelSpawnVisualization->SetupRenderStage(args.gfx, args.viewport, args.scissor,
				 TestRaytracing::GetOutputBuffer(),
				 args.camera);

			 UINT localDebugMode = m_debugOverlayMode - 4;


			 args.gfx.InsertUAVBarrier(TestRaytracing::GetOutputBuffer());

			 context->SurfelIllumination->UpdateProjection(args.camera);
			 
			 //SurfelIllumination->SendParametersGraphics(gfxContext);
			 context->SurfelIllumination->SendParametersInformedGraphics(args.gfx);
			 context->SurfelSpawnVisualization->SetDebugMode(args.gfx,localDebugMode);

			 RenderFullScreenQuad(args.gfx);
		 }








	 }
	void SurfelGIRenderer::RenderScene(RenderArgs& args)
	{
		if (m_resetSurfelNextFrame == true)
		{
			context->SurfelIllumination->ResetSurfels(args.gfx);
			m_resetSurfelNextFrame = false;
		}

		ComputeContext& cfx = reinterpret_cast<ComputeContext&>(args.gfx);


		if (m_stopSurfelUpdate == false || m_stopSurfelUpdate == true && m_stopSurfelUpdate != m_prevStopSurfelUpdate)
		{
			context->SurfelIllumination->FillAccelerationStructuresReduceThenScan(cfx);
		}
		//Control GPU Readback
		//SurfelIllumination->ReadbakcSurfelDebugData(args.gfx);
		if (m_stopSurfelUpdate == true && m_stopSurfelUpdate != m_prevStopSurfelUpdate)
		{
			context->SurfelIllumination->ReadbackSurfelAccelerationStructure(args.gfx);
			context->SurfelIllumination->ReadbackSurfelData(args.gfx);
		}

		if (m_stopSurfelUpdate == false)
		{
			cfx.InsertUAVBarrier(context->SurfelIllumination->m_SurfelData.m_GPUBuffer);
			args.gfx.InsertUAVBarrier(context->SurfelIllumination->m_SurfelData.m_GPUBuffer);
			SurfelIrradianceAccumulation::DoRaytracing(
				cfx,
				args.camera,
				context->SurfelIllumination->descriptorHeap,
				context->SurfelIllumination->m_SurfelGen.UniformGrid,
				context->SurfelIllumination->m_SurfelData.m_Actual);
		}


		Renderer::UpdateGlobalDescriptors();




		UpdateContextDirectionalLight();



		TestRaytracing::directionalLightData = {
			Vector4(context->sunDirection, 1),
			Vector4(1.0f) * context->sunLightIntensity,
			Vector4(1.0f) * context->ambientIntensity,
		};
		SurfelIrradianceAccumulation::directionalLightData =
		{
			Vector4(context->sunDirection, 1),
			Vector4(1.0f) * context->sunLightIntensity,
			Vector4(1.0f) * context->ambientIntensity,
		};

		SetCommandListDefaultState(args.gfx);

		RenderLightShadowsPass(args.gfx, args.camera);

		RenderZPrePass(args);



		SSAO::Render(args.gfx, args.camera);

		if (!args.skipDiffuse)
		{
			Lighting::FillLightGrid(args.gfx, args.camera);

			if (!SSAO::DebugDraw)
				SetupRenderTargets(args);
		}

		if (!args.skipShadow && !SSAO::DebugDraw)
			RenderShadowMapPass(args);

		if (!args.skipDiffuse)
		{
			if (!SSAO::DebugDraw)
			{
				if (SSAO::AsyncCompute)
				{
					args.gfx.Flush();
					SetCommandListDefaultState(args.gfx);
					// Make the 3D queue wait for the Compute queue to finish SSAO
					g_CommandManager.GetGraphicsQueue().StallForProducer(g_CommandManager.GetComputeQueue());
				}

				RenderColorPass(args);

				if (m_enableDebugOverlay)
					RenderDebugOverlayPass(args);

			}
		}


		if (m_useSurfelInformedSBGI)
		{
			context->m_GBufferDownsample->Dispatch(cfx, args.camera);
			{
				if (m_hbil_render)
				{
					RenderHBIL(args);
				}
				if (m_hbil_updateDebug)
				{
					context->m_HBIL->ReadDebugHBIL(args.gfx, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

				}
			}
		}

		// --- SURFEL PASS
		context->frameIndex++;
		//Set the frame index
		uint32_t FrameIndex = TemporalEffects::GetFrameIndexMod2();
		context->SurfelIllumination->m_SurfelGen.FrameIndex = context->frameIndex;

		if (m_stopSurfelUpdate == false)
		{
			if(m_useSurfelInformedSBGI)
				context->SurfelIllumination->SpawnSurfelsInformed(cfx, args.camera);
			else
				context->SurfelIllumination->SpawnSurfels(cfx, args.camera);
		}

		if (m_drawPhysicalSurfelInstances)
		{
			context->SurfelIllumination->ReadbackSurfelData(args.gfx);
		}

		context->SurfelIllumination->ApplySurfels(cfx, args.camera);

		if (m_stopSurfelUpdate == false)
			context->SurfelIllumination->RecycleSurfels(cfx, args.camera);

		m_prevStopSurfelUpdate = m_stopSurfelUpdate;
		CopyColorAndDepthBuffers(args.gfx);
	}

	void SurfelGIRenderer::SetupRenderTargets(RenderArgs& args)
	{
		ScopedTimer _prof(L"Main Render", args.gfx);
		{
			args.gfx.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
			args.gfx.TransitionResource(g_SceneNormalBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
			args.gfx.ClearColor(g_SceneColorBuffer);
		}
	}

	void SurfelGIRenderer::RenderHBIL(RenderArgs& args)
	{
		ScopedTimer _prof(L"Render HBIL", args.gfx);
		ImVec2 mousePos = ImGui::GetMousePos();
		float verticalFovRad = args.camera.GetFOV();
		float TAN_HALF_FOV = tan(verticalFovRad * 0.5);
		context->m_HBIL->SetMousePos(mousePos.x, mousePos.y);
		context->m_HBIL->RenderHBIL(args.gfx, args.camera);
		RenderFullScreenQuad(args.gfx);
		last_camera_data = args.camera;
	}

	void SurfelGIRenderer::RenderShadowMapPass(RenderArgs& args)
	{
		SetCommandListDefaultState(args.gfx);
		{

			ScopedTimer _prof2(L"Render Shadow Map", args.gfx);
			//--- SHADOWS PASS ---

			context->sunShadow.UpdateMatrix(-context->sunDirection, Vector3(0, -500.0f, 0), Vector3(ShadowDimX, ShadowDimY, ShadowDimZ),
				(uint32_t)g_ShadowBuffer.GetWidth(), (uint32_t)g_ShadowBuffer.GetHeight(), 16);

			RenderObjectInstancePassArgs renderInstancesShadowMapArgs{
				args.gfx, context->sunShadow.GetViewProjMatrix(), args.camera.GetPosition(), kOpaque
			};
//			RenderObjectInstancePassArgs renderInstancesShadowMapArgs{
//				args.gfx, context->sunShadow.GetViewProjMatrix(), context->sunShadow.GetPosition(), kOpaque
//			};

			g_ShadowBuffer.BeginRendering(args.gfx);
			args.gfx.SetPipelineState(m_ShadowPSO);
			RenderObjects(renderInstancesShadowMapArgs);

			args.gfx.SetPipelineState(m_CutoutShadowPSO);
			renderInstancesShadowMapArgs.filter = kCutout;
			RenderObjects(renderInstancesShadowMapArgs);

			g_ShadowBuffer.EndRendering(args.gfx);
		}
	}

	void SurfelGIRenderer::UpdateContextDirectionalLight()
	{
		m_SunOrientation = m_sunData.sunOrientation;
		m_SunInclination = m_sunData.sunInclination;
		context->sunLightIntensity = m_sunData.sunInclination;
		context->ambientIntensity = m_sunData.ambientLightIntensity;

		float costheta = cosf(m_SunOrientation);
		float sintheta = sinf(m_SunOrientation);
		float cosphi = cosf(m_SunInclination * 3.14159f * 0.5f);
		float sinphi = sinf(m_SunInclination * 3.14159f * 0.5f);
		//m_SunDirection = Normalize(Vector3(costheta * cosphi, sinphi, sintheta * cosphi));
		context->sunDirection = Normalize(Vector3(costheta * cosphi, sinphi, sintheta * cosphi));
	}

	void SurfelGIRenderer::UpdateColorPassPixelShaderArgs(const uint32_t& FrameIndex)
	{
		psConstants.sunDirection = context->sunDirection;
		psConstants.sunLight = Vector3(1.0f, 1.0f, 1.0f) * context->sunLightIntensity;
		psConstants.ambientLight = Vector3(1.0f, 1.0f, 1.0f) * context->ambientIntensity;
		psConstants.ShadowTexelSize[0] = 1.0f / g_ShadowBuffer.GetWidth();
		psConstants.InvTileDim[0] = 1.0f / Lighting::LightGridDim;
		psConstants.InvTileDim[1] = 1.0f / Lighting::LightGridDim;
		psConstants.TileCount[0] = Math::DivideByMultiple(g_SceneColorBuffer.GetWidth(), Lighting::LightGridDim);
		psConstants.TileCount[1] = Math::DivideByMultiple(g_SceneColorBuffer.GetHeight(), Lighting::LightGridDim);
		psConstants.FirstLightIndex[0] = Lighting::m_FirstConeLight;
		psConstants.FirstLightIndex[1] = Lighting::m_FirstConeShadowedLight;
		psConstants.FrameIndexMod2 = FrameIndex;
		if (m_useSurfelInformedSBGI)
			psConstants.UseConfidenceWeight = 255;
		else
			psConstants.UseConfidenceWeight = 0;
		psConstants.AOThreshold = context->SurfelIllumination->m_SurfelGen.AOVariables.x;
	}

	void SurfelGIRenderer::RenderZPrePass(RenderArgs& args)
	{
		{
			ScopedTimer _prof(L"Z PrePass", args.gfx);
			RenderObjectInstancePassArgs renderInstancesCameraArgs
			{ args.gfx, args.camera.GetViewProjMatrix(), args.camera.GetPosition(), kOpaque };

			args.gfx.SetDynamicConstantBufferView(Renderer::kMaterialConstants, sizeof(psConstants), &psConstants);

			{
				ScopedTimer _prof2(L"Opaque", args.gfx);
				{
					args.gfx.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
					args.gfx.ClearDepth(g_SceneDepthBuffer);
					args.gfx.SetPipelineState(m_DepthPSO);
					args.gfx.SetDepthStencilTarget(g_SceneDepthBuffer.GetDSV());
					args.gfx.SetViewportAndScissor(args.viewport, args.scissor);
				}

				renderInstancesCameraArgs.filter = kOpaque;
				RenderObjects(renderInstancesCameraArgs);

			}

			//--- CUTOUT RENDERING ---
			{
				ScopedTimer _prof2(L"Cutout", args.gfx);
				{
					args.gfx.SetPipelineState(m_CutoutDepthPSO);
				}
				renderInstancesCameraArgs.filter = kCutout;
				RenderObjects(renderInstancesCameraArgs);
			}
		}
	}
