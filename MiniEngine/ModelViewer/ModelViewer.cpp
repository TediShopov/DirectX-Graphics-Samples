//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//

#include "GameCore.h"
#include "CameraController.h"
#include "BufferManager.h"
#include "Camera.h"
#include "CommandContext.h"
#include "TemporalEffects.h"
#include "MotionBlur.h"
#include "DepthOfField.h"
#include "PostEffects.h"
#include "SSAO.h"
#include "FXAA.h"
#include "SystemTime.h"
#include "TextRenderer.h"
#include "ParticleEffectManager.h"
#include "GameInput.h"
#include "SponzaRenderer.h"
#include "glTF.h"
#include "Renderer.h"
#include "Model.h"
#include "ModelLoader.h"
#include "ShadowCamera.h"
#include "Display.h"
#include "sstream"
#include <iomanip>
#include "TestRenderer.h"
#include <wrl/client.h>
#include <iostream>
#include "Imgui/imgui-master/imgui.h"
#include "CameraSequencer.h"
#include "CameraSequenceRunner.h"
#include "Imgui/imgui-master/imgui.h"
#include "Imgui/imgui-master/backends/imgui_impl_win32.h"
#include "Imgui/imgui-master/backends/imgui_impl_dx12.h"
#include "ExtendedUtility.h"

#define LEGACY_RENDERER

using namespace GameCore;
using namespace Math;
using namespace Graphics;
using namespace std;

using Renderer::MeshSorter;

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


 ExampleDescriptorHeapAllocator	g_pd3dSrvDescHeapAlloc;
class ModelViewer : public GameCore::IGameApp
{

 ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
public:
    bool m_cameraUpdatesEnabled = true;

    ModelViewer( void ) {}

    virtual void Startup( void ) override;
	virtual void InitImGui(void)
		{

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.MouseDrawCursor = true;
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



		ImGui_ImplWin32_Init(g_hWnd);
		ImGui_ImplDX12_InitInfo info;
		info.Device = g_Device;
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
    virtual void Cleanup( void ) override;

    virtual void Update( float deltaT ) override;
    void UpdateInput(float deltaT);
    virtual void RenderScene( void ) override;
    virtual void RenderUI(GraphicsContext& gfx) override;

    //Require hardware ray-tracing support
    bool RequiresRaytracingSupport() const override 
    { 
        return true; 
    }

private:

    Camera m_Camera;
    unique_ptr<CameraController> m_CameraController;

    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

    ModelInstance m_ModelInst;
    ShadowCamera m_SunShadowCamera;
	CameraSequencer m_CameraSequence;
	CameraSequenceRunner* m_SequenceRunner;
};

CREATE_APPLICATION( ModelViewer )

ExpVar g_SunLightIntensity("Viewer/Lighting/Sun Light Intensity", 4.0f, 0.0f, 16.0f, 0.1f);
NumVar g_SunOrientation("Viewer/Lighting/Sun Orientation", -0.5f, -100.0f, 100.0f, 0.1f );
NumVar g_SunInclination("Viewer/Lighting/Sun Inclination", 0.75f, 0.0f, 1.0f, 0.01f );

void ChangeIBLSet(EngineVar::ActionType);
void ChangeIBLBias(EngineVar::ActionType);

DynamicEnumVar g_IBLSet("Viewer/Lighting/Environment", ChangeIBLSet);
std::vector<std::pair<TextureRef, TextureRef>> g_IBLTextures;
NumVar g_IBLBias("Viewer/Lighting/Gloss Reduction", 2.0f, 0.0f, 10.0f, 1.0f, ChangeIBLBias);

void ChangeIBLSet(EngineVar::ActionType)
{
    int setIdx = g_IBLSet - 1;
    if (setIdx < 0)
    {
        Renderer::SetIBLTextures(nullptr, nullptr);
    }
    else
    {
        auto texturePair = g_IBLTextures[setIdx];
        Renderer::SetIBLTextures(texturePair.first, texturePair.second);
    }
}

void ChangeIBLBias(EngineVar::ActionType)
{
    Renderer::SetIBLBias(g_IBLBias);
    
}

#include <direct.h> // for _getcwd() to check data root path

void LoadIBLTextures()
{
    char CWD[256];
    _getcwd(CWD, 256);

    Utility::Printf("Loading IBL environment maps\n");

    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(L"Textures/*_diffuseIBL.dds", &ffd);

    g_IBLSet.AddEnum(L"None");

    if (hFind != INVALID_HANDLE_VALUE) do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

       std::wstring diffuseFile = ffd.cFileName;
       std::wstring baseFile = diffuseFile; 
       baseFile.resize(baseFile.rfind(L"_diffuseIBL.dds"));
       std::wstring specularFile = baseFile + L"_specularIBL.dds";

       TextureRef diffuseTex = TextureManager::LoadDDSFromFile(L"Textures/" + diffuseFile);
       if (diffuseTex.IsValid())
       {
           TextureRef specularTex = TextureManager::LoadDDSFromFile(L"Textures/" + specularFile);
           if (specularTex.IsValid())
           {
               g_IBLSet.AddEnum(baseFile);
               g_IBLTextures.push_back(std::make_pair(diffuseTex, specularTex));
           }
       }
    }
    while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);

    Utility::Printf("Found %u IBL environment map sets\n", g_IBLTextures.size());

    if (g_IBLTextures.size() > 0)
        g_IBLSet.Increment();
}

void ModelViewer::Startup( void )
{
    MotionBlur::Enable = false;
    TemporalEffects::EnableTAA = true;
    FXAA::Enable = false;
    PostEffects::EnableHDR = false;
    PostEffects::EnableAdaptation = false;
    SSAO::Enable = true;
    
    m_SequenceRunner = new CameraSequenceRunner(m_Camera);

    Renderer::Initialize();

    LoadIBLTextures();
    InitImGui();

    std::wstring gltfFileName;

    bool forceRebuild = false;
    uint32_t rebuildValue;
    if (CommandLineArgs::GetInteger(L"rebuild", rebuildValue))
        forceRebuild = rebuildValue != 0;

    if (CommandLineArgs::GetString(L"model", gltfFileName) == false)
    {
#ifdef LEGACY_RENDERER
        TestRenderer::Startup(m_Camera,g_hWnd);
        //Sponza::Startup(m_Camera);
#else
        m_ModelInst = Renderer::LoadModel(L"Sponza/PBR/sponza2.gltf", forceRebuild);
        m_ModelInst.Resize(100.0f * m_ModelInst.GetRadius());
        OrientedBox obb = m_ModelInst.GetBoundingBox();
        float modelRadius = Length(obb.GetDimensions()) * 0.5f;
        const Vector3 eye = obb.GetCenter() + Vector3(modelRadius * 0.5f, 0.0f, 0.0f);
        m_Camera.SetEyeAtUp( eye, Vector3(kZero), Vector3(kYUnitVector) );
#endif
    }
    else
    {
        m_ModelInst = Renderer::LoadModel(gltfFileName, forceRebuild);
        m_ModelInst.LoopAllAnimations();
        m_ModelInst.Resize(10.0f);

        MotionBlur::Enable = false;
    }

    m_Camera.SetZRange(1.0f, 10000.0f);
    if (gltfFileName.size() == 0)
        m_CameraController.reset(new FlyingFPSCamera(m_Camera, Vector3(kYUnitVector)));
    else
        m_CameraController.reset(new OrbitCamera(m_Camera, m_ModelInst.GetBoundingSphere(), Vector3(kYUnitVector)));

    EngineProfiling::Initialize();
}

void ModelViewer::Cleanup( void )
{
    m_ModelInst = nullptr;

    g_IBLTextures.clear();

#ifdef LEGACY_RENDERER
    TestRenderer::Cleanup();
    //Sponza::Cleanup();
#endif

    Renderer::Shutdown();
}

namespace Graphics
{
    extern EnumVar DebugZoom;
}

void ModelViewer::Update( float deltaT )
{
    ScopedTimer _prof(L"Update State");

    UpdateInput(deltaT);

    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Update");

    m_ModelInst.Update(gfxContext, deltaT);

    gfxContext.Finish();

    // We use viewport offsets to jitter sample positions from frame to frame (for TAA.)
    // D3D has a design quirk with fractional offsets such that the implicit scissor
    // region of a viewport is floor(TopLeftXY) and floor(TopLeftXY + WidthHeight), so
    // having a negative fractional top left, e.g. (-0.25, -0.25) would also shift the
    // BottomRight corner up by a whole integer.  One solution is to pad your viewport
    // dimensions with an extra pixel.  My solution is to only use positive fractional offsets,
    // but that means that the average sample position is +0.5, which I use when I disable
    // temporal AA.
    TemporalEffects::GetJitterOffset(m_MainViewport.TopLeftX, m_MainViewport.TopLeftY);

    m_MainViewport.Width = (float)g_SceneColorBuffer.GetWidth();
    m_MainViewport.Height = (float)g_SceneColorBuffer.GetHeight();
    m_MainViewport.MinDepth = 0.0f;
    m_MainViewport.MaxDepth = 1.0f;

    m_MainScissor.left = 0;
    m_MainScissor.top = 0;
    m_MainScissor.right = (LONG)g_SceneColorBuffer.GetWidth();
    m_MainScissor.bottom = (LONG)g_SceneColorBuffer.GetHeight();
}

void ModelViewer::UpdateInput(float deltaT)
{
	if (GameInput::IsFirstPressed(GameInput::kLShoulder))
		DebugZoom.Decrement();
	else if (GameInput::IsFirstPressed(GameInput::kRShoulder))
		DebugZoom.Increment();

	bool saveTestState = GameInput::IsFirstPressed(GameInput::kKey_z);

	if (saveTestState)
	{
		m_CameraSequence.AddStop(m_Camera);
	}


	EngineProfiling::SetPaused(m_SequenceRunner->IsRunning() == false);

	if (GameInput::IsFirstPressed(GameInput::kKey_space))
	{
		if (m_SequenceRunner->IsRunning() == false)
		{
			m_SequenceRunner->Start();
		}
		else {
			m_SequenceRunner->Pause();

		}

			m_cameraUpdatesEnabled = !m_cameraUpdatesEnabled;
	}


	if (m_cameraUpdatesEnabled)
	{
		if (ImGui::GetIO().WantCaptureKeyboard == false)
		{
			m_CameraController->Update(deltaT);

		}

	}
    else
    {
		if (ImGui::GetIO().WantCaptureKeyboard == false)
		{
			m_SequenceRunner->Update(deltaT);
		}
            
    }




}

void ModelViewer::RenderScene( void )
{

	EngineProfiling::OnFrameStart();
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");
	//EngineProfiling::BeginSession();


    uint32_t FrameIndex = TemporalEffects::GetFrameIndexMod2();
    const D3D12_VIEWPORT& viewport = m_MainViewport;
    const D3D12_RECT& scissor = m_MainScissor;
//	EngineProfiling::BeginSession();

    ParticleEffectManager::Update(gfxContext.GetComputeContext(), Graphics::GetFrameTime());

    if (m_ModelInst.IsNull())
    {
#ifdef LEGACY_RENDERER
        //Sponza::RenderScene(gfxContext, m_Camera, viewport, scissor);
		//EngineProfiling::BeginBlock(L"TestingBear", &gfxContext);
        TestRenderer::RenderScene(gfxContext, m_Camera, viewport, scissor,false,false);
		//EngineProfiling::EndBlock( &gfxContext);
		//EngineProfiling::EndBlockPerfSDK(&gfxContext);
#endif
    }
    else
    {
		// Update global constants
		float costheta = cosf(g_SunOrientation);
		float sintheta = sinf(g_SunOrientation);
		float cosphi = cosf(g_SunInclination * 3.14159f * 0.5f);
		float sinphi = sinf(g_SunInclination * 3.14159f * 0.5f);

		Vector3 SunDirection = Normalize(Vector3(costheta * cosphi, sinphi, sintheta * cosphi));
		Vector3 ShadowBounds = Vector3(m_ModelInst.GetRadius());
		//m_SunShadowCamera.UpdateMatrix(-SunDirection, m_ModelInst.GetCenter(), ShadowBounds,
		m_SunShadowCamera.UpdateMatrix(-SunDirection, Vector3(0, -500.0f, 0), Vector3(5000, 3000, 3000),
			(uint32_t)g_ShadowBuffer.GetWidth(), (uint32_t)g_ShadowBuffer.GetHeight(), 16);

		GlobalConstants globals;
		globals.ViewProjMatrix = m_Camera.GetViewProjMatrix();
		globals.SunShadowMatrix = m_SunShadowCamera.GetShadowMatrix();
		globals.CameraPos = m_Camera.GetPosition();
		globals.SunDirection = SunDirection;
		globals.SunIntensity = Vector3(Scalar(g_SunLightIntensity));

		// Begin rendering depth
		gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
		gfxContext.ClearDepth(g_SceneDepthBuffer);

		MeshSorter sorter(MeshSorter::kDefault);
		sorter.SetCamera(m_Camera);
		sorter.SetViewport(viewport);
		sorter.SetScissor(scissor);
		sorter.SetDepthStencilTarget(g_SceneDepthBuffer);
		sorter.AddRenderTarget(g_SceneColorBuffer);

		m_ModelInst.Render(sorter);

		sorter.Sort();

		{
			ScopedTimer _prof(L"Depth Pre-Pass", gfxContext);
			sorter.RenderMeshes(MeshSorter::kZPass, gfxContext, globals);
		}

		SSAO::Render(gfxContext, m_Camera);

		if (!SSAO::DebugDraw)
		{
			ScopedTimer _outerprof(L"Main Render", gfxContext);

			{
				ScopedTimer _prof(L"Sun Shadow Map", gfxContext);

				MeshSorter shadowSorter(MeshSorter::kShadows);
				shadowSorter.SetCamera(m_SunShadowCamera);
				shadowSorter.SetDepthStencilTarget(g_ShadowBuffer);

				m_ModelInst.Render(shadowSorter);

				shadowSorter.Sort();
				shadowSorter.RenderMeshes(MeshSorter::kZPass, gfxContext, globals);
			}

			gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
			gfxContext.ClearColor(g_SceneColorBuffer);

			{
				ScopedTimer _prof(L"Render Color", gfxContext);

				gfxContext.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
				gfxContext.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV_DepthReadOnly());
				gfxContext.SetViewportAndScissor(viewport, scissor);

				sorter.RenderMeshes(MeshSorter::kOpaque, gfxContext, globals);
			}

			Renderer::DrawSkybox(gfxContext, m_Camera, viewport, scissor);

			sorter.RenderMeshes(MeshSorter::kTransparent, gfxContext, globals);
		}
    }



    // Some systems generate a per-pixel velocity buffer to better track dynamic and skinned meshes.  Everything
    // is static in our scene, so we generate velocity from camera motion and the depth buffer.  A velocity buffer
    // is necessary for all temporal effects (and motion blur).
    MotionBlur::GenerateCameraVelocityBuffer(gfxContext, m_Camera, true);

    //TemporalEffects::ResolveImage(gfxContext);

    //ParticleEffectManager::Render(gfxContext, m_Camera, g_SceneColorBuffer, g_SceneDepthBuffer,  g_LinearDepth[FrameIndex]);
    

    // Until I work out how to couple these two, it's "either-or".
//    if (DepthOfField::Enable)
//        DepthOfField::Render(gfxContext, m_Camera.GetNearClip(), m_Camera.GetFarClip());
//    else
//        MotionBlur::RenderObjectBlur(gfxContext, g_VelocityBuffer);
//


    if(GameInput::IsPressed(GameInput::kKey_space))
    {
		gfxContext.PIXSetMarker(L"Here am I");
    }

    

    gfxContext.Finish();
	EngineProfiling::OnFrameEnd();
//    if(GameInput::IsPressed(GameInput::kKey_space))
//    {
//        EngineProfiling::CollectReport();
//    }
	//EngineProfiling::EndSessoin();

}


const char* D3D12AutoBreadcrumbOpToString(D3D12_AUTO_BREADCRUMB_OP op)
{
    switch (op)
    {
    case D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT: return "Begin Even";
    case D3D12_AUTO_BREADCRUMB_OP_DISPATCH: return "Dispatch";
    // ... add more as needed
    default: return "Unknown";
    }
}

void DumpDREDInfoA(ID3D12Device* device)
{
    Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> dred;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dred))))
    {
        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs = {};
        HRESULT a = dred->GetAutoBreadcrumbsOutput(&breadcrumbs);

        if (breadcrumbs.pHeadAutoBreadcrumbNode)
        {
            OutputDebugString(L"[DRED] AutoBreadcrumbs captured.\n");
            // Optionally, walk the breadcrumb linked list and print command list call traces
        }

        D3D12_DRED_PAGE_FAULT_OUTPUT pageFault = {};
        HRESULT b = dred->GetPageFaultAllocationOutput(&pageFault);

        if (pageFault.PageFaultVA)
        {
            wchar_t buffer[256];
            swprintf_s(buffer, L"[DRED] Page fault at GPU VA: 0x%llx\n", pageFault.PageFaultVA);
            OutputDebugString(buffer);
        }
    }
}
void ModelViewer::RenderUI(GraphicsContext& gfx)
{
	auto io = ImGui::GetIO();

	bool pressedOne = GameInput::IsPressed(GameInput::kMouse0);
	io.AddMouseButtonEvent(0, pressedOne);
	if (io.WantCaptureKeyboard)
	{
		// --- Keyboard ---
		for (int key = 0; key < GameInput::kNumDigitalInputs; ++key)
		{

			GameInput::DigitalInput giKey = static_cast<GameInput::DigitalInput>(key);
			ImGuiKey imguiKey = ExtendedUtility::MapGameInputKeyToImGuiKey(giKey);
			if (imguiKey != ImGuiKey_None)
			{
				bool pressed = GameInput::IsFirstPressed(giKey);
				if (pressed)
				{
					io.AddKeyEvent(imguiKey, true);
					io.AddInputCharacter(ExtendedUtility::MapToChar(giKey));
				}
				else
				{
					io.AddKeyEvent(imguiKey, false);

				}
			}
		}

	}


	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	m_CameraSequence.RenderImGui();
	m_SequenceRunner->SetSequence(&m_CameraSequence.GetMutableConfig());
	m_SequenceRunner->RenderImGui();
	TestRenderer::RenderImGuiUI(gfx);


	ImGui::Render();

	ID3D12GraphicsCommandList* cmdList = gfx.GetCommandList();
	ID3D12DescriptorHeap* ppHeaps[] = {
	g_pd3dSrvDescHeap,      // CBV/SRV/UAV heap (for textures)
	};
	cmdList->SetDescriptorHeaps(1, ppHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
	gfx.SetScissor(0, 0, g_DisplayWidth, g_DisplayHeight);



}
