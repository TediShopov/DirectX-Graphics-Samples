#include "GraphicsCore.h"
#include "BufferManager.h"
#include "Camera.h"
#include "CommandContext.h"
#include "TemporalEffects.h"
#include "SSAO.h"
#include "SystemTime.h"
#include "ShadowCamera.h"
#include "ParticleEffects.h"
#include "TestRenderer.h"
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
//#include "CompiledShaders/Raytracing.h"


#include <dxcapi.h>
#include <vector>
#include "TestRaytracing.h"
#include "SurfelIrradianceAccumulation.h"





using namespace Math;
using namespace Graphics;
using namespace std;
//using namespace DX;

	//--- DATA ---
#pragma region DATA

#pragma region			ImGuiDecriptor

struct ExampleDescriptorHeapAllocator
	{
		ID3D12DescriptorHeap* Heap = nullptr;
		D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
		D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
		UINT                        HeapHandleIncrement;
		ImVector<int>               FreeIndices;

		void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
		{
			IM_ASSERT(Heap == nullptr && FreeIndices.empty());
			Heap = heap;
			D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
			HeapType = desc.Type;
			HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
			HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
			HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
			FreeIndices.reserve((int)desc.NumDescriptors);
			for (int n = desc.NumDescriptors; n > 0; n--)
				FreeIndices.push_back(n - 1);
		}
		void Destroy()
		{
			Heap = nullptr;
			FreeIndices.clear();
		}
		void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
		{
			IM_ASSERT(FreeIndices.Size > 0);
			int idx = FreeIndices.back();
			FreeIndices.pop_back();
			out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
			out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
		}
		void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
		{
			int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
			int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
			IM_ASSERT(cpu_idx == gpu_idx);
			FreeIndices.push_back(cpu_idx);
		}
	};

static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
ExampleDescriptorHeapAllocator	g_pd3dSrvDescHeapAlloc;

#pragma endregion


UINT TestRenderer::frameIndex = 0;

	 SurfelGI* TestRenderer::SurfelIllumination = nullptr;
 HashGridVisualization* TestRenderer::GridVisualization = nullptr;
	 MSMEVisualization* TestRenderer::GridMSMEVisualization = nullptr;
	 SurfelGIOnlyVisualization* TestRenderer::SurfelGIVisualization = nullptr;
	 SurfelGIOnlyVisualization* TestRenderer::MaterialBindingDebug = nullptr;

	 ModelH3D* TestRenderer::m_Model = nullptr;
	 ModelH3D* TestRenderer::m_ModelExtra = nullptr;

	 Math::Camera TestRenderer::lastUsedCamera = Math::Camera();

	 SphereMesh* TestRenderer::m_Sphere = nullptr;
	 DiscMesh* TestRenderer::m_Disc = nullptr;
	 Transform TestRenderer::m_Transform;

	 DescriptorHeap TestRenderer::SSRHeap = DescriptorHeap();
	 ColorBuffer TestRenderer::colorCopyBuffer = ColorBuffer();
	 DepthBuffer TestRenderer::depthCopyBuffer = DepthBuffer();

	 struct SunData {
		 XMFLOAT3 sunDirection;
		 float sunOrientation;
		 float sunInclination;
		 float ambientLightIntensity;
		 float sunLightIntensity;

	 };
	 SunData m_sunData;
	//-- DIRECTIONAL LIGHT PROPERTIES
	 Math::Vector3 TestRenderer::m_SunDirection;
	 ShadowCamera TestRenderer::m_SunShadow;
	 ExpVar TestRenderer::m_AmbientIntensity = ExpVar("Ambient Light Intensity",0.1f);
	 ExpVar TestRenderer::m_SunLightIntensity = ExpVar("Sun Light Intensity",1.0f);

	NumVar m_SunOrientation = NumVar("Sponza/Lighting/Sun Orientation", -0.5f, -100.0f, 100.0f, 0.1f);
	NumVar m_SunInclination = NumVar("Sponza/Lighting/Sun Inclination", 0.75f, 0.0f, 1.0f, 0.01f);
	NumVar ShadowDimX = NumVar("Sponza/Lighting/Shadow Dim X", 5000, 1000, 10000, 100);
	NumVar ShadowDimY= NumVar("Sponza/Lighting/Shadow Dim Y", 3000, 1000, 10000, 100);
	NumVar ShadowDimZ= NumVar("Sponza/Lighting/Shadow Dim Z", 3000, 1000, 10000, 100);

	 bool m_enableDebugOverlay = true;
	 bool m_stopSurfelUpdate = true;
	 bool m_prevStopSurfelUpdate = false;
	 bool m_renderOnlyCurrentCellSurfels = false;
	 bool m_useSimpleAlgorithm = true;
	 
	 //bool m_useSSRMonly = false;
	 bool m_useSSRMonly = true;
	 bool m_drawPhysicalSurfelInstances = true;
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

	TestRenderer::PSConstants TestRenderer::psConstants = TestRenderer::PSConstants{};

	std::vector<bool> m_pMaterialIsCutout;


	 struct ColorVertex { Vector4 position;  Vector4 color; };
	 float m_aspectRatio = 16.0f / 9.0f;
	  float m_depthValue = 0.1f;
	  float _TRI_SCALE = 0.3f;
	  float QUAD_SCALE = 0.8f;
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


	void TestRenderer::SetupScene() 
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
	void TestRenderer::Startup(Math::Camera& camera, HWND hwnd)
	{
//		DragonModel.Load(L"OBJ/Dragon.obj");
//		DragonModel.Load(L"D:/MScSurfelBasedGI/DirectX-Graphics-Samples/MiniEngine/Model/OBJ/Dragon.obj");

		SurfelIllumination = new SurfelGI();  // Definition (allocates storage)
		GridVisualization = new HashGridVisualization();
		GridMSMEVisualization = new MSMEVisualization();
		SurfelGIVisualization = new SurfelGIOnlyVisualization();
		MaterialBindingDebug = new SurfelGIOnlyVisualization();
		frameIndex = 0;

		m_Sphere = nullptr;
		m_Disc = nullptr;
		m_Transform = Transform();

		DXGI_FORMAT ColorFormat = g_SceneColorBuffer.GetFormat();
		DXGI_FORMAT NormalFormat = g_SceneNormalBuffer.GetFormat();
		DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();
		DXGI_FORMAT ShadowFormat = g_ShadowBuffer.GetFormat();


		//m_Transform.setScale(50,50,50);
		m_Transform.setScale(10,10,10);

		m_sunData.ambientLightIntensity = 0.1f;
		//m_sunData.ambientLightIntensity = 0;
		m_sunData.sunInclination = 1.0f;
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

		m_Disc = new DiscMesh(10);

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
		camera.SetEyeAtUp(eye, Vector3(kZero), Vector3(kYUnitVector));
		camera.SetPosition(Vector3(m_Transform.getPosition()));


		uint32_t VertexStride = m_Model->GetVertexStride();
		//Allocate just and extra descriptor table entry
		uint32_t DestCount = 9;
		// Allocate a descriptor table for the common textures
		Renderer::m_CommonTextures = Renderer::s_TextureHeap.Alloc(1);

		uint32_t SourceCounts[] = { 1, 1, 1, 1, 1, 1, 1, 1,1 };

		TestRaytracing::CreateOutputTextureUAV(&g_SceneColorBuffer);
		SurfelIrradianceAccumulation::CreateOutputTextureUAV(&g_SceneColorBuffer);

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
			TestRaytracing::GetOutputBuffer().GetSRV()

		};
		//       TestRaytracing::GetOutputBuffer().GetSRV()
		g_Device->CopyDescriptors(1, &Renderer::m_CommonTextures, &DestCount, DestCount, SourceTextures, SourceCounts, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


		GBufferPtrs gbuffer{
			&g_SceneColorBuffer,
			&g_SceneNormalBuffer,
			&g_SceneDepthBuffer

		};

		SurfelIllumination->Setup(
			gbuffer
		);

		TestRaytracing::CreateDeviceDependentResources(
			m_Transform,
			*m_Model,
			&Graphics::g_SceneColorBuffer,
			SurfelIllumination->nonShaderVisibleHeap
			//SurfelIllumination->srvHeap
		);
		SurfelIrradianceAccumulation::CreateDeviceDependentResources(
			m_Transform,
			*m_Model,
			&Graphics::g_SceneColorBuffer,
			SurfelIllumination->nonShaderVisibleHeap
			//SurfelIllumination->srvHeap

		);

		SSRHeap.Create(L"SSR Heap", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
		colorCopyBuffer.Create(L"Color Copy Buffer", g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight(), 1, ColorFormat);
		depthCopyBuffer.Create(L"Depth Copy Buffer A", g_SceneDepthBuffer.GetWidth(), g_SceneDepthBuffer.GetHeight(),1, DepthFormat);

		ExtendedUtility::CopyDescriptorsToHeap(
			SSRHeap,
			{
				colorCopyBuffer.GetSRV(),
				depthCopyBuffer.GetDepthSRV()
			},
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		SSRMIrradianceAccumulation.Setup(colorCopyBuffer, SurfelIllumination->nonShaderVisibleHeap);



		GridVisualization->Setup(
			gbuffer,
			&TestRaytracing::GetOutputBuffer()

		);
		GridMSMEVisualization->Setup(
			gbuffer,
			&TestRaytracing::GetOutputBuffer()
		);
		SurfelGIVisualization->Setup(
			gbuffer,
			&SurfelIllumination->m_OutputTexture
		);
		MaterialBindingDebug->Setup(
			gbuffer,
			&TestRaytracing::GetOutputBuffer()
		);


		//Imgui Startup

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.MouseDrawCursor = true;
		//io.WantCaptureMouse = true;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Optional


		//ImGui::StyleColorsDark(); // or ImGui::StyleColorsClassic();

		// Setup Platform/Renderer bindings
		//renderTargetHeap.Create(L"Render Target Heap", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 10, true);
		ID3D12Device* device = Graphics::g_Device;
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 10;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			if (device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
			{
				int a = 3;
				//return false;

			}
			g_pd3dSrvDescHeapAlloc.Create(device, g_pd3dSrvDescHeap);
		}



		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_InitInfo info;
		info.Device = device;
		info.NumFramesInFlight = 2;
		info.CommandQueue = Graphics::g_CommandManager.GetCommandQueue();
		// info.DSVFormat = DXGI;
		// info.RTVFormat = Graphics::g_OverlayBuffer.GetFormat();

		info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		info.DSVFormat = DXGI_FORMAT_UNKNOWN;

		info.SrvDescriptorHeap = g_pd3dSrvDescHeap;
		info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
			{
				g_pd3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle);
				return;
			};
		info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
			{
				g_pd3dSrvDescHeapAlloc.Free(cpu_handle, gpu_handle);
				return;
			};

		bool t = ImGui_ImplDX12_Init(&info);



	}
	void TestRenderer::InitQuadModel()
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
	void TestRenderer::InitTriangleModel()
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
	void TestRenderer::InitSphereModel()
	{
		m_Sphere = new SphereMesh(2);
	}

	UINT TestRenderer::FindClosesSurfelToPosition(Math::Vector3 position)
	{
		float closestDistance = 999999.0f;
		UINT closesSurfelId = -1;
		std::vector<SurfelData>& surfels = SurfelIllumination->m_SurfelDataArray;
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
	void TestRenderer::Cleanup(void)
	{
		m_Model->Clear();
		Lighting::Shutdown();
		TextureManager::Shutdown();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext(ImGui::GetCurrentContext());
	}

	TestRenderer::VSConstants TestRenderer::SetupObjectVSConstants(RENDER_OBJECT_INSTANCE_PARAMS)
	{
		VSConstants vsConstants;
		vsConstants.modelToWorld = Matrix4(TestRenderer::m_Transform.getTransformMatrix());
		vsConstants.modelToProjection = ViewProjMat;
		vsConstants.modelToShadow = m_SunShadow.GetShadowMatrix();
		XMStoreFloat3(&vsConstants.viewerPos, viewerPos);
		return vsConstants;

	}

	void TestRenderer::RenderFullScreenQuad(GraphicsContext& gfxContext)
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
	void TestRenderer::RenderScreenSpaceTriangle(GraphicsContext& gfxContext)
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
	void TestRenderer::RenderOBJObject(RENDER_OBJECT_INSTANCE_PARAMS)
	{
		VSConstants vsConstants = SetupObjectVSConstants(gfxContext, ViewProjMat, viewerPos, Filter);
		gfxContext.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
		
		gfxContext.SetIndexBuffer(OBJModel.GetIndexBuffer());
		gfxContext.SetVertexBuffer(0,OBJModel.GetVertexBuffer());
		//--- Draw three indices of the triangle
		gfxContext.DrawIndexed(OBJModel.m_pIndices.size(), 0, 0);

		//--- Switch Back To Sponza model
		gfxContext.SetIndexBuffer(m_Model->GetIndexBuffer());
		gfxContext.SetVertexBuffer(0, m_Model->GetVertexBuffer());


	}
	void TestRenderer::RenderOBJObjectCorrectPipeline(RENDER_OBJECT_INSTANCE_PARAMS)
	{
		VSConstants vsConstants = SetupObjectVSConstants(gfxContext, ViewProjMat, viewerPos, Filter);
		gfxContext.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);



		const ModelH3D::Mesh& mesh = m_Model->GetMesh(34);

		uint32_t indexCount = mesh.indexCount;
		
		auto format = m_Model->GetIndexBuffer().Format;
			//uint32_t startIndex = 0;
		uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint16_t);
		//uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint32_t);
		uint32_t baseVertex = mesh.vertexDataByteOffset / 56;

		
		//gfxContext.Set
		//gfxContext.SetIndexBuffer(OBJModel.GetIndexBuffer());
		//gfxContext.SetVertexBuffer(0,OBJModel.GetVertexBuffer());
		//--- Draw three indices of the triangle
		//gfxContext.DrawIndexed(OBJModel.m_pIndices.size(), 0, 0);

		gfxContext.DrawIndexed(indexCount, startIndex, baseVertex);
		//--- Switch Back To Sponza model
		//gfxContext.SetIndexBuffer(m_Model->GetIndexBuffer());
		//gfxContext.SetVertexBuffer(0, m_Model->GetVertexBuffer());



	}
	void TestRenderer::RenderSphereObject(RENDER_OBJECT_INSTANCE_PARAMS)
	{
		VSConstants vsConstants = SetupObjectVSConstants(gfxContext, ViewProjMat, viewerPos, Filter);
		gfxContext.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
		
		const UINT vertexBufferSize = sizeof(triangleVertices);
		gfxContext.SetIndexBuffer(m_Sphere->m_IndexBufferView);
		gfxContext.SetVertexBuffer(0, m_Sphere->m_VertexBufferView);
		//--- Draw three indices of the triangle
		gfxContext.DrawIndexed(m_Sphere->m_Indices.size(), 0, 0);

		//--- Switch Back To Sponza model
		gfxContext.SetIndexBuffer(m_Model->GetIndexBuffer());
		gfxContext.SetVertexBuffer(0, m_Model->GetVertexBuffer());

	}

	XMVECTOR TestRenderer::GetRotationQuaternionFromUpToDirection(FXMVECTOR targetDirection)
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


	void TestRenderer::GetRelevantSurfels( UINT& from, UINT& to )
	{
		UniformGrid grid = SurfelIllumination->m_SurfelGen.UniformGrid;
		SurfelDebugData data = SurfelIllumination->m_SurfelDebugActual;

		UINT gridDimX =
			grid.dimensions.GetX() / grid.cellSize.GetX();
		UINT gridDimY=
			grid.dimensions.GetY() / grid.cellSize.GetY();

		UINT gridDimZ =
			grid.dimensions.GetZ() / grid.cellSize.GetZ();

		UINT linearIndex = data.PointedCellX  +
           data.PointedCellY * gridDimX   +
           data.PointedCellZ * gridDimX * gridDimY;


		 from = SurfelIllumination->m_SurfelGrid.m_Actual[linearIndex];
		 to = SurfelIllumination->m_SurfelGrid.m_Actual[linearIndex+1];

	}

	void TestRenderer::RenderRelevantSurfels(RENDER_OBJECT_INSTANCE_PARAMS)
	{
		VSConstants vsConstants =SetupObjectVSConstants(gfxContext, ViewProjMat, viewerPos, Filter);

		const UINT vertexBufferSize = sizeof(triangleVertices);
		gfxContext.SetIndexBuffer(m_Disc->m_IndexBufferView);
		gfxContext.SetVertexBuffer(0, m_Disc->m_VertexBufferView);

		UINT surfelListIndexFrom ;
		UINT surfelListIndexTo ;
		GetRelevantSurfels(surfelListIndexFrom, surfelListIndexTo);

		for (size_t i = surfelListIndexFrom; i < surfelListIndexTo; ++i)
		{

			Transform t;
			//UINT surfelIndex = SurfelIllumination->m_SurfelListActual[i];
			UINT surfelIndex = SurfelIllumination->m_SurfelList.m_Actual[i];
			SurfelData s = SurfelIllumination->m_SurfelDataArray[surfelIndex];
			Vector4 extrudedPossition = s.position + (s.normal * 1.0f);



			t.setPosition(extrudedPossition);

			if (XMVector4Length(s.normal).m128_f32[0] > 0.001f)
			{
				Vector4 quat = Vector4(GetRotationQuaternionFromUpToDirection(s.normal));
				t.setQuaternion(quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW());
				t.setComposeRotationFromQuaternions(true);

				t.setScale(s.radius.GetX(), s.radius.GetX(), s.radius.GetX());


				vsConstants.modelToWorld = Matrix4(t.getTransformMatrix());
				XMStoreFloat3(&vsConstants.viewerPos, viewerPos);

				gfxContext.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
				gfxContext.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(Vector4), &s.color);
				//--- Draw three indices of the triangle
				gfxContext.DrawIndexed(m_Disc->m_Indices.size(), 0, 0);

			}

		}
		//--- Switch Back To Sponza model
		gfxContext.SetIndexBuffer(m_Model->GetIndexBuffer());
		gfxContext.SetVertexBuffer(0, m_Model->GetVertexBuffer());

	}
	void TestRenderer::RenderSurfels(RENDER_OBJECT_INSTANCE_PARAMS)
	{

		float modelRadius = Length(m_Model->GetBoundingBox().GetDimensions()) * 0.5f;
		const Vector3 eye = m_Model->GetBoundingBox().GetCenter() + Vector3(modelRadius * 0.5f, 0.0f, 0.0f);
		const Vector3 relSphereOffset(0, 0, 0.1);
		const Vector3 offset = relSphereOffset * modelRadius;

		VSConstants vsConstants = SetupObjectVSConstants(gfxContext, ViewProjMat, viewerPos, Filter);
		const UINT vertexBufferSize = sizeof(triangleVertices);
		//---TEMPORARILY switch index and vertex buffers
		gfxContext.SetIndexBuffer(m_Disc->m_IndexBufferView);
		gfxContext.SetVertexBuffer(0, m_Disc->m_VertexBufferView);

		for each (SurfelData s in SurfelIllumination->m_SurfelDataArray)
		{
			if ((float)s.radius.GetX() == 0)
			{
				continue;
			}


			Transform t;
			Vector4 extrudedPossition = s.position ;



			t.setPosition(extrudedPossition);

			if (XMVector4Length(s.normal).m128_f32[0] > 0.001f)
			{
				Vector4 quat = Vector4(GetRotationQuaternionFromUpToDirection(s.normal));
				t.setQuaternion(quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW());
				t.setComposeRotationFromQuaternions(true);
				t.setScale(s.radius.GetX(), s.radius.GetX(), s.radius.GetX());

				vsConstants.modelToWorld = Matrix4(t.getTransformMatrix());
				XMStoreFloat3(&vsConstants.viewerPos, viewerPos);

				gfxContext.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
				gfxContext.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(Vector4), &s.color);

				//--- Draw three indices of the triangle
				gfxContext.DrawIndexed(m_Disc->m_Indices.size(), 0, 0);

			}
			else {
				int a = 3;
			}


		}
		//--- Switch Back To Sponza model
		gfxContext.SetIndexBuffer(m_Model->GetIndexBuffer());
		gfxContext.SetVertexBuffer(0, m_Model->GetVertexBuffer());

	}
	void TestRenderer::RenderObjects(RENDER_OBJECT_INSTANCE_PARAMS)
	{
		struct VSConstanstsWithModel
		{
			Matrix4 modelToProjection;
			Matrix4 modelToShadow;
			Matrix4 modelToWorld;
			XMFLOAT3 viewerPos;
		} vsConstants;
		vsConstants.modelToProjection = ViewProjMat;
		vsConstants.modelToShadow = m_SunShadow.GetShadowMatrix();
		vsConstants.modelToWorld = Matrix4(XMMatrixIdentity());
		XMStoreFloat3(&vsConstants.viewerPos, viewerPos);


		__declspec(align(16)) uint32_t materialIdx = 0xFFFFFFFFul;

		uint32_t VertexStride = m_Model->GetVertexStride();
		UINT a = sizeof(vsConstants);

		for (uint32_t meshIndex = 0; meshIndex < m_Model->GetMeshCount(); meshIndex++)
		{
			if (meshIndex > 34)
				continue;

			if (meshIndex > 33)
			{
				vsConstants.modelToWorld = Matrix4(m_Transform.getTransformMatrix());


			}
			gfxContext.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);
			const ModelH3D::Mesh& mesh = m_Model->GetMesh(meshIndex);

			uint32_t indexCount = mesh.indexCount;
		
			auto format = m_Model->GetIndexBuffer().Format;
			//uint32_t startIndex = 0;
			uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint16_t);
			//uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint32_t);
			uint32_t baseVertex = mesh.vertexDataByteOffset / VertexStride;

			if (mesh.materialIndex != materialIdx)
			{
				if (m_pMaterialIsCutout[mesh.materialIndex] && !(Filter & kCutout) ||
					!m_pMaterialIsCutout[mesh.materialIndex] && !(Filter & kOpaque))
					continue;

				materialIdx = mesh.materialIndex;
				gfxContext.SetDescriptorTable(Renderer::kMaterialSRVs, m_Model->GetSRVs(materialIdx));
				gfxContext.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(uint32_t), &materialIdx);
			}

			gfxContext.DrawIndexed(indexCount, startIndex, baseVertex);
		}

	}

	void TestRenderer::CopyColorAndDepthBuffers(GraphicsContext& gfxContext) 
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

	void TestRenderer::RenderSSR(GraphicsContext& gfxContext, const Camera& camera,UINT objectIndex) 
	{

		struct VSConstanstsWithModel
		{
			Matrix4 modelToProjection;
			Matrix4 modelToShadow;
			Matrix4 modelToWorld;
			XMFLOAT3 viewerPos;
		} vsConstants;
		vsConstants.modelToProjection = camera.GetViewProjMatrix();
		vsConstants.modelToShadow = m_SunShadow.GetShadowMatrix();
		vsConstants.modelToWorld = Matrix4(XMMatrixIdentity());
		XMStoreFloat3(&vsConstants.viewerPos, camera.GetPosition());
		__declspec(align(16)) uint32_t materialIdx = 0xFFFFFFFFul;

		uint32_t VertexStride = m_Model->GetVertexStride();
		UINT a = sizeof(vsConstants);
		UINT meshIndex = objectIndex;

		vsConstants.modelToWorld = Matrix4(m_Transform.getTransformMatrix());


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
			Vector3 cameraPosition;
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
		commonSSR.cameraData.cameraPosition = camera.GetPosition();

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
		commonSSR.ssrParameters.thicknessInUnits = 200;
		commonSSR.ssrParameters.resolution = 1;


		if (mesh.materialIndex != materialIdx)
		{
			materialIdx = mesh.materialIndex;
			gfxContext.SetDescriptorTable(Renderer::kMaterialSRVs, m_Model->GetSRVs(materialIdx));
			gfxContext.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(commonSSR), &commonSSR);
		}

		gfxContext.DrawIndexed(indexCount, startIndex, baseVertex);

	}
	void TestRenderer::RenderLightShadows(GraphicsContext& gfxContext, const Camera& camera)
	{
		using namespace Lighting;

		ScopedTimer _prof(L"RenderLightShadows", gfxContext);

		static uint32_t LightIndex = 0;
		if (LightIndex >= MaxLights)
			return;

		m_LightShadowTempBuffer.BeginRendering(gfxContext);
		{
			gfxContext.SetPipelineState(m_ShadowPSO);
			RenderObjects(gfxContext, m_LightShadowMatrix[LightIndex], camera.GetPosition(), kOpaque);
			gfxContext.SetPipelineState(m_CutoutShadowPSO);
			RenderObjects(gfxContext, m_LightShadowMatrix[LightIndex], camera.GetPosition(), kCutout);
		}
		m_LightShadowTempBuffer.EndRendering(gfxContext);

		gfxContext.TransitionResource(m_LightShadowTempBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
		gfxContext.TransitionResource(m_LightShadowArray, D3D12_RESOURCE_STATE_COPY_DEST);

		gfxContext.CopySubresource(m_LightShadowArray, LightIndex, m_LightShadowTempBuffer, 0);

		gfxContext.TransitionResource(m_LightShadowArray, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		++LightIndex;
	}



	float bunnyScale = 1000;

	void TestRenderer::RenderImGuiUI(GraphicsContext& gfx) {
		ImGuiIO& io = ImGui::GetIO();


		bool pressedDebugButton = GameInput::IsFirstPressed(GameInput::kKey_f);
		bool pressedDebugMode = GameInput::IsFirstPressed(GameInput::kKey_g);
		bool pressedStopButton = GameInput::IsFirstPressed(GameInput::kKey_b);
		bool pressedOnlyRelevantButton = GameInput::IsFirstPressed(GameInput::kKey_t);
		bool pressedToggleFillAccelerationStructure = GameInput::IsFirstPressed(GameInput::kKey_z);
		bool pressedInclinationPlus = GameInput::IsFirstPressed(GameInput::kKey_j);
		bool pressedInclinatoinMinus = GameInput::IsFirstPressed(GameInput::kKey_k);
		bool pressedOrientationPlus = GameInput::IsFirstPressed(GameInput::kKey_u);
		bool pressedOriantationMinus = GameInput::IsFirstPressed(GameInput::kKey_i);
		bool pressedToggleUseSSRMOnly = GameInput::IsFirstPressed(GameInput::kKey_n);
		bool pressedTogglePhysicalSurfels = GameInput::IsFirstPressed(GameInput::kKey_h);

		bool pressedResetSurfels = GameInput::IsFirstPressed(GameInput::kKey_x);
		bool pressedResetSurfelsIrradiance = GameInput::IsFirstPressed(GameInput::kKey_c);
		bool pressedToggleSSR = GameInput::IsFirstPressed(GameInput::kKey_m);

		if (pressedToggleSSR)
		{
			m_drawSSRTest = !m_drawSSRTest;

		}
		if (pressedResetSurfels)
		{
			SurfelIllumination->ResetSurfels(gfx);

		}
		if (pressedResetSurfelsIrradiance)
		{
			SurfelIllumination->ReadbackSurfelData(gfx);
			//Reset Surfels Code
			SurfelIllumination->ResetSurfelsIrradiance(gfx);

		}


		if (pressedTogglePhysicalSurfels)
			m_drawPhysicalSurfelInstances = !m_drawPhysicalSurfelInstances;
		if (pressedToggleUseSSRMOnly)
			m_useSSRMonly = !m_useSSRMonly;


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
			if (m_debugOverlayMode > 3)
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


		bool pressedOne = GameInput::IsPressed(GameInput::kMouse0);
		io.AddMouseButtonEvent(0, pressedOne);

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Your ImGui UI code here
		ImGui::Begin("Surfel Gen CB");

		//ImGui::SliderInt("Surfel Num", &tempSurfelNumber,0,10000);
//		bool applySurfelNum = false;
//		if (ImGui::Checkbox("Apply New Surfel Num", &applySurfelNum))
//		{
//			SurfelIllumination->RecreateSurfelListBuffers(tempSurfelNumber);
//		}


		static bool spawnThresholdsCollapsingHeader = true;
		if (ImGui::CollapsingHeader("Spawning Thresholds", &spawnThresholdsCollapsingHeader))
		{

			ImGui::DragInt("Per Cell Surfel Limit", &SurfelIllumination->m_SurfelGen.kPerCellSurfelLimit);
			ImGui::DragInt("Placement Threshold", &SurfelIllumination->m_SurfelGen.gPlacementThreshold);
			ImGui::DragInt("Removal Threshold", &SurfelIllumination->m_SurfelGen.gRemovalThreshold);
		}

		static bool spawnChancesCollapsingHeader = true;
		if(ImGui::CollapsingHeader("Spawn Chances", &spawnChancesCollapsingHeader))
		{
			//Used for altering the 0-1 range chance
			ImGui::DragFloat("Chance Power", &SurfelIllumination->m_SurfelGen.gChancePower);
			ImGui::DragFloat("Chance Mulitply", &SurfelIllumination->m_SurfelGen.gChanceMultiply);



		}

		static bool debugCollapsingHeader = true;
		if(ImGui::CollapsingHeader("Debug", &debugCollapsingHeader))
		{
			ImGui::Checkbox("Enable Debug Overlay", &m_enableDebugOverlay);
			ImGui::Checkbox("Stop Surfle Spawn Recycle", &m_stopSurfelUpdate);
			ImGui::Checkbox("Fill Acceleration Structures Simple Algorithm", &m_useSimpleAlgorithm);
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


		ImGui::Checkbox("Use Screen-Space Ray Marching Only ", &m_useSSRMonly);
		ImGui::Checkbox("Draw Physical Surfel Objects", &m_drawPhysicalSurfelInstances);

		ImGui::DragFloat("Bunny Scale", &bunnyScale,1.0,1,3000);
		m_Transform.setScale(bunnyScale, bunnyScale, bunnyScale);


		std::stringstream ss;
		UINT from, to;
		GetRelevantSurfels(from, to);
		SurfelDebugData dd = SurfelIllumination->m_SurfelDebugActual;
		ss << "Surfel In Pointed Cell: " << dd.PointedCellX << " " << dd.PointedCellY << " " << dd.PointedCellZ;
		ss << "\n";

		for (size_t i = from; i < to; i++)
		{
			UINT surfelIndex = SurfelIllumination->m_SurfelList.m_Actual[i];
			SurfelData s = SurfelIllumination->m_SurfelDataArray[surfelIndex];
			ss << "	S ID:	" << surfelIndex << "	POS:" << s.position.GetX() << "," << s.position.GetY() << "," << s.position.GetZ() << ",";
			ss << "\n";
		}


		ImGui::Text(ss.str().c_str());


		std::stringstream cameraPositionString;
		auto camPos = lastUsedCamera.GetPosition();
		cameraPositionString << "Camera Position: " << camPos.GetX() << "	" << camPos.GetY() << "	" << camPos.GetZ();
		ImGui::Text(cameraPositionString.str().c_str());

		std::stringstream closestSurfelIDString;
		UINT sID = FindClosesSurfelToPosition(camPos);
		closestSurfelIDString << "Closest Surfel: " << sID;
		ImGui::Text(closestSurfelIDString.str().c_str());




		//ImGui::ShowDemoWindow();
		//ImGui::Text("Hello from ImGui!");
		ImGui::End();
		ImGui::Render();

		ID3D12GraphicsCommandList* cmdList = gfx.GetCommandList();
		ID3D12DescriptorHeap* ppHeaps[] = {
		g_pd3dSrvDescHeap,      // CBV/SRV/UAV heap (for textures)
		};
		cmdList->SetDescriptorHeaps(1, ppHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

	}

	const ModelH3D& TestRenderer::GetModel()
	{
		return *TestRenderer::m_Model;
	}


	 void TestRenderer::RenderColor(RENDER_SCENE_PARAMS)
	 {
		 lastUsedCamera = camera;
		 {
			 ScopedTimer _prof2(L"Render Color", gfxContext);

			 gfxContext.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			 //gfxContext.TransitionResource(TestRaytracing::GetOutputBuffer(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			 gfxContext.SetDescriptorTable(Renderer::kCommonSRVs, Renderer::m_CommonTextures);
			 gfxContext.SetDynamicConstantBufferView(Renderer::kMaterialConstants, sizeof(psConstants), &psConstants);

			 {
				 gfxContext.SetPipelineState(m_ModelPSO);
				 gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
				 D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ g_SceneColorBuffer.GetRTV(), g_SceneNormalBuffer.GetRTV() };
				 gfxContext.SetRenderTargets(ARRAYSIZE(rtvs), rtvs, g_SceneDepthBuffer.GetDSV_DepthReadOnly());
				 gfxContext.SetViewportAndScissor(viewport, scissor);
			 }
			 RenderObjects(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);
			 CopyColorAndDepthBuffers(gfxContext);

			 gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			 gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);


			 if (m_drawPhysicalSurfelInstances)
			 {
				 ScopedTimer _prof3(L"Render Sphere", gfxContext);
				 gfxContext.SetPipelineState(m_TestSpherePSO);
				 gfxContext.SetRootSignature(m_TestSpherePSO.GetRootSignature());
				 gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
				 gfxContext.TransitionResource(g_SceneNormalBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
				 gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
				 D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ g_SceneColorBuffer.GetRTV(), g_SceneNormalBuffer.GetRTV() };
				 gfxContext.SetRenderTargets(ARRAYSIZE(rtvs), rtvs, g_SceneDepthBuffer.GetDSV_DepthReadOnly());
				 gfxContext.SetViewportAndScissor(viewport, scissor);
				 //RenderSphereObject(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);
				 if (m_renderOnlyCurrentCellSurfels)
					 RenderRelevantSurfels(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);
				 else
					 RenderSurfels(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);

			 }


			 if (m_drawSSRTest)
			 {
				 ScopedTimer _prof3(L"Render SSR", gfxContext);
				 gfxContext.SetPipelineState(m_ModelSSRPSO);
				 gfxContext.SetRootSignature(m_ModelSSRPSO.GetRootSignature());
				 gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SSRHeap.GetHeapPointer());

				 CopyColorAndDepthBuffers(gfxContext);
				 //PIXEL SHADER RESOURCE 
				 gfxContext.TransitionResource(colorCopyBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				 gfxContext.TransitionResource(depthCopyBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

				 gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
				 gfxContext.TransitionResource(g_SceneNormalBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);

				 gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
				 D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ g_SceneColorBuffer.GetRTV(), g_SceneNormalBuffer.GetRTV() };
				 gfxContext.SetRenderTargets(ARRAYSIZE(rtvs), rtvs, g_SceneDepthBuffer.GetDSV_DepthReadOnly());
				 gfxContext.SetViewportAndScissor(viewport, scissor);
				 gfxContext.SetDescriptorTable(Renderer::kCommonSRVs, SSRHeap[0]);
				 //camera.GetViewMatrix();
				 //camera.GetProjMatrix();
				 RenderSSR(gfxContext, camera, 35);
			 }

			 //			 {
			 //				 ScopedTimer _prof3(L"Render OBJ", gfxContext);
			 //				 gfxContext.SetPipelineState(m_ModelSimplifiedPSO);
			 //				 gfxContext.SetRootSignature(m_ModelSimplifiedPSO.GetRootSignature());
			 //				 gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			 //				 gfxContext.TransitionResource(g_SceneNormalBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			 //				 gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
			 //				 D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ g_SceneColorBuffer.GetRTV(), g_SceneNormalBuffer.GetRTV() };
			 //				 gfxContext.SetRenderTargets(ARRAYSIZE(rtvs), rtvs, g_SceneDepthBuffer.GetDSV_DepthReadOnly());
			 //				 gfxContext.SetViewportAndScissor(viewport, scissor);
			 //				 //RenderOBJObject(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);
			 //				 RenderOBJObjectCorrectPipeline(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);
			 //			 }

		 }

	 }

	 void TestRenderer::RenderDebugOverlay(RENDER_SCENE_PARAMS)
	 {
		 if (m_debugOverlayMode == 0)
		 {
			 ScopedTimer _prof3(L"Surfel Density Debug Overlay", gfxContext);
			 GridVisualization->SetupRenderStage(gfxContext, viewport, scissor,
				 TestRaytracing::GetOutputBuffer(),
				 camera);

			 SurfelIllumination->UpdateProjection(camera);
			 SurfelIllumination->SendParametersGraphics(gfxContext);

			 gfxContext.InsertUAVBarrier(SurfelIllumination->m_SurfelGrid.m_GPUBuffer);
			 gfxContext.InsertUAVBarrier(SurfelIllumination->m_SurfelList.m_GPUBuffer);
			 gfxContext.InsertUAVBarrier(SurfelIllumination->m_SurfelStack);
			 gfxContext.InsertUAVBarrier(TestRaytracing::GetOutputBuffer());
			 RenderFullScreenQuad(gfxContext);

		 }
		 else if (m_debugOverlayMode == 1)
		 {
			 ScopedTimer _prof3(L"Surfel GI Only Debug Overlay", gfxContext);

			 SurfelGIVisualization->SetupRenderStage(gfxContext, viewport, scissor,
				 SurfelIllumination->m_OutputTexture,
				 camera);

			 gfxContext.TransitionResource(SurfelIllumination->m_OutputTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,true);

			 SurfelGIVisualization->SetRootParameters(gfxContext, SurfelIllumination->m_OutputTexture);
			 RenderFullScreenQuad(gfxContext);
		 }
		 else if (m_debugOverlayMode == 2)
		 {
			 ScopedTimer _prof3(L"Surfel MSME Debug Overlay", gfxContext);

			 GridMSMEVisualization->SetupRenderStage(gfxContext, viewport, scissor,
				 TestRaytracing::GetOutputBuffer(),
				 camera);

//			 gfxContext.InsertUAVBarrier(SurfelIllumination->m_SurfelGrid.m_GPUBuffer);
//			 gfxContext.InsertUAVBarrier(SurfelIllumination->m_SurfelList);
//			 gfxContext.InsertUAVBarrier(SurfelIllumination->m_SurfelData);
//			 gfxContext.InsertUAVBarrier(SurfelIllumination->m_SurfelStack);
			 gfxContext.InsertUAVBarrier(TestRaytracing::GetOutputBuffer());

			 SurfelIllumination->UpdateProjection(camera);
			 SurfelIllumination->SendParametersGraphics(gfxContext);

			 RenderFullScreenQuad(gfxContext);

		 }
		 else if (m_debugOverlayMode == 3)
		 {
			 ScopedTimer _prof3(L"Material Binding Ouptut", gfxContext);
			 gfxContext.TransitionResource(TestRaytracing::GetOutputBuffer(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			 MaterialBindingDebug->SetupRenderStage(gfxContext, viewport, scissor,
				 TestRaytracing::GetOutputBuffer(),
				 camera);
			 MaterialBindingDebug->SetRootParameters(gfxContext, TestRaytracing::GetOutputBuffer());
			 RenderFullScreenQuad(gfxContext);
		 }








	 }
	void TestRenderer::RenderScene(RENDER_SCENE_PARAMS)
	{


		ComputeContext& cfx = reinterpret_cast<ComputeContext&>(gfxContext);

		SurfelIllumination->FillAccelerationStructuresReduceThenScan(cfx);
		//SurfelIllumination->FillAccelerationStructures(cfx);

		if(m_stopSurfelUpdate == true && m_stopSurfelUpdate != m_prevStopSurfelUpdate)
		{
			SurfelIllumination->ReadbakcSurfelDebugData(gfxContext);
			SurfelIllumination->ReadbackSurfelAccelerationStructure(gfxContext);
		}


		//TestRaytracing::DoRaytracing(camera, SurfelIllumination->descriptorHeap,SurfelIllumination->m_SurfelGen.UniformGrid);

		if (m_stopSurfelUpdate == false)
		{
			if (m_useSSRMonly == false)
			{
				SurfelIrradianceAccumulation::DoRaytracing(
					camera,
					SurfelIllumination->descriptorHeap,
					SurfelIllumination->m_SurfelGen.UniformGrid,
					SurfelIllumination->m_SurfelDataArray);
			}
			else
			{
				cfx.InsertUAVBarrier(SurfelIllumination->m_SurfelData.m_GPUBuffer);
				SSRMIrradianceAccumulation.Dispatch(cfx, camera);
			}

		}


		Renderer::UpdateGlobalDescriptors();

		Vector3 pos = camera.GetPosition();

		uint32_t FrameIndex = TemporalEffects::GetFrameIndexMod2();


		m_SunOrientation = m_sunData.sunOrientation;
		m_SunInclination = m_sunData.sunInclination;
		m_SunLightIntensity = m_sunData.sunInclination;
		m_AmbientIntensity = m_sunData.ambientLightIntensity;

		float costheta = cosf(m_SunOrientation);
		float sintheta = sinf(m_SunOrientation);
		float cosphi = cosf(m_SunInclination * 3.14159f * 0.5f);
		float sinphi = sinf(m_SunInclination * 3.14159f * 0.5f);
		m_SunDirection = Normalize(Vector3(costheta * cosphi, sinphi, sintheta * cosphi));



		psConstants.sunDirection = m_SunDirection;
		psConstants.sunLight = Vector3(1.0f, 1.0f, 1.0f) * m_SunLightIntensity;
		psConstants.ambientLight = Vector3(1.0f, 1.0f, 1.0f) * m_AmbientIntensity;
		psConstants.ShadowTexelSize[0] = 1.0f / g_ShadowBuffer.GetWidth();
		psConstants.InvTileDim[0] = 1.0f / Lighting::LightGridDim;
		psConstants.InvTileDim[1] = 1.0f / Lighting::LightGridDim;
		psConstants.TileCount[0] = Math::DivideByMultiple(g_SceneColorBuffer.GetWidth(), Lighting::LightGridDim);
		psConstants.TileCount[1] = Math::DivideByMultiple(g_SceneColorBuffer.GetHeight(), Lighting::LightGridDim);
		psConstants.FirstLightIndex[0] = Lighting::m_FirstConeLight;
		psConstants.FirstLightIndex[1] = Lighting::m_FirstConeShadowedLight;
		psConstants.FrameIndexMod2 = FrameIndex;

		TestRaytracing::directionalLightData.sunDirection = Vector4(m_SunDirection, 1);
		TestRaytracing::directionalLightData.sunColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f) * m_SunLightIntensity;
		TestRaytracing::directionalLightData.ambientColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f) * m_AmbientIntensity;
		SurfelIrradianceAccumulation::directionalLightData.sunDirection = Vector4(m_SunDirection, 1);
		SurfelIrradianceAccumulation::directionalLightData.sunColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f) * m_SunLightIntensity;
		SurfelIrradianceAccumulation::directionalLightData.ambientColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f) * m_AmbientIntensity;

		// Set the default state for command lists
		auto& pfnSetupGraphicsState = [&](void)
			{
				gfxContext.SetRootSignature(Renderer::m_RootSig);
				gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Renderer::s_TextureHeap.GetHeapPointer());
				gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				gfxContext.SetIndexBuffer(m_Model->GetIndexBuffer());
				gfxContext.SetVertexBuffer(0, m_Model->GetVertexBuffer());
			};

		pfnSetupGraphicsState();

		RenderLightShadows(gfxContext, camera);

		{
			ScopedTimer _prof(L"Z PrePass", gfxContext);

			gfxContext.SetDynamicConstantBufferView(Renderer::kMaterialConstants, sizeof(psConstants), &psConstants);

			{
				ScopedTimer _prof2(L"Opaque", gfxContext);
				{
					gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
					gfxContext.ClearDepth(g_SceneDepthBuffer);
					gfxContext.SetPipelineState(m_DepthPSO);
					gfxContext.SetDepthStencilTarget(g_SceneDepthBuffer.GetDSV());
					gfxContext.SetViewportAndScissor(viewport, scissor);
				}
				RenderObjects(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), kOpaque);
			}

			//--- CUTOUT RENDERING ---
			{
				ScopedTimer _prof2(L"Cutout", gfxContext);
				{
					gfxContext.SetPipelineState(m_CutoutDepthPSO);
				}
				RenderObjects(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), kCutout);
			}
		}



		SSAO::Render(gfxContext, camera);

		if (!skipDiffusePass)
		{
			Lighting::FillLightGrid(gfxContext, camera);

			if (!SSAO::DebugDraw)
			{
				ScopedTimer _prof(L"Main Render", gfxContext);
				{
					gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
					gfxContext.TransitionResource(g_SceneNormalBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
					gfxContext.ClearColor(g_SceneColorBuffer);
				}
			}
		}

		if (!skipShadowMap)
		{
			if (!SSAO::DebugDraw)
			{
				//--- SHADOWS PASS ---
				pfnSetupGraphicsState();
				{
					ScopedTimer _prof2(L"Render Shadow Map", gfxContext);

					m_SunShadow.UpdateMatrix(-m_SunDirection, Vector3(0, -500.0f, 0), Vector3(ShadowDimX, ShadowDimY, ShadowDimZ),
						(uint32_t)g_ShadowBuffer.GetWidth(), (uint32_t)g_ShadowBuffer.GetHeight(), 16);

					g_ShadowBuffer.BeginRendering(gfxContext);
					gfxContext.SetPipelineState(m_ShadowPSO);
					RenderObjects(gfxContext, m_SunShadow.GetViewProjMatrix(), camera.GetPosition(), kOpaque);
					gfxContext.SetPipelineState(m_CutoutShadowPSO);
					RenderObjects(gfxContext, m_SunShadow.GetViewProjMatrix(), camera.GetPosition(), kCutout);
					g_ShadowBuffer.EndRendering(gfxContext);
				}
			}
		}

		if (!skipDiffusePass)
		{
			if (!SSAO::DebugDraw)
			{
				if (SSAO::AsyncCompute)
				{
					gfxContext.Flush();
					pfnSetupGraphicsState();

					// Make the 3D queue wait for the Compute queue to finish SSAO
					g_CommandManager.GetGraphicsQueue().StallForProducer(g_CommandManager.GetComputeQueue());
				}

				RenderColor(gfxContext, camera, viewport, scissor, skipDiffusePass, skipShadowMap);

				if (m_enableDebugOverlay)
				{
					RenderDebugOverlay(gfxContext, camera, viewport, scissor, skipDiffusePass, skipShadowMap);
				}
				//                --- SKIP NORMAL CUTOUTS---
				//gfxContext.SetPipelineState(m_CutoutModelPSO);
				//RenderObjects( gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kCutout );

			}
		}



		// --- SURFEL PASS
		frameIndex++;
		//Set the frame index
		SurfelIllumination->m_SurfelGen.FrameIndex = frameIndex;

		if (m_stopSurfelUpdate == false)
			SurfelIllumination->SpawnSurfels(cfx, camera);

		if (m_drawPhysicalSurfelInstances)
		{
			SurfelIllumination->ReadbackSurfelData(gfxContext);

		}
		SurfelIllumination->ApplySurfels(cfx, camera);
		
		if (m_stopSurfelUpdate == false)
			SurfelIllumination->RecycleSurfels(cfx, camera);

		m_prevStopSurfelUpdate = m_stopSurfelUpdate;

	}
