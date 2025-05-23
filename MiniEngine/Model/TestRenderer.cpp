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

// From Model
#include "ModelH3D.h"

// From ModelViewer
#include "LightManager.h"

#include "CompiledShaders/DepthViewerVS.h"
#include "CompiledShaders/DepthViewerPS.h"
#include "CompiledShaders/ModelViewerVS.h"
#include "CompiledShaders/ModelViewerPS.h"
#include "CompiledShaders/TestRendererPS.h"
#include "CompiledShaders/SimpleColorPS.h"
#include "CompiledShaders/SimpleColorVS.h"
//#include "CompiledShaders/Raytracing.h"


#include <dxcapi.h>
#include <vector>
//#include "TopLevelASGenerator.h"
//#include "DXRHelper.h"
//#include "BottomLevelASGenerator.h"
//#include "DXSample.h"
//#include "StepTimer.h"
//#include "RaytracingHlslCompat.h"
//#include "DirectXRaytracingHelper.h"
#include "TestRaytracing.h"



using namespace Math;
using namespace Graphics;
using namespace std;
//using namespace DX;

namespace TestRenderer
{
	void RenderLightShadows(GraphicsContext& gfxContext, const Camera& camera);

	enum eObjectFilter { kOpaque = 0x1, kCutout = 0x2, kTransparent = 0x4, kAll = 0xF, kNone = 0x0 };
	void RenderTriangleObject(GraphicsContext& Context, const Matrix4& ViewProjMat, const Vector3& viewerPos, eObjectFilter Filter = kAll);
	void RenderObjects(GraphicsContext& Context, const Matrix4& ViewProjMat, const Vector3& viewerPos, eObjectFilter Filter = kAll);

	GraphicsPSO m_DepthPSO = { (L"Sponza: Depth PSO") };
	GraphicsPSO m_ModelPSO = { (L"Sponza: Color PSO") };
	GraphicsPSO m_TestPSO = { (L"Sponza: Triangel Test PSO") };
	GraphicsPSO m_CutoutDepthPSO = { (L"Sponza: Cutout Depth PSO") };
	GraphicsPSO m_CutoutModelPSO = { (L"Sponza: Cutout Color PSO") };
	GraphicsPSO m_ShadowPSO(L"Sponza: Shadow PSO");
	GraphicsPSO m_CutoutShadowPSO(L"Sponza: Cutout Shadow PSO");

	ModelH3D m_Model;
	std::vector<bool> m_pMaterialIsCutout;

	Vector3 m_SunDirection;
	ShadowCamera m_SunShadow;

	ExpVar m_AmbientIntensity("Sponza/Lighting/Ambient Intensity", 0.1f, -16.0f, 16.0f, 0.1f);
	ExpVar m_SunLightIntensity("Sponza/Lighting/Sun Light Intensity", 4.0f, 0.0f, 16.0f, 0.1f);
	NumVar m_SunOrientation("Sponza/Lighting/Sun Orientation", -0.5f, -100.0f, 100.0f, 0.1f);
	NumVar m_SunInclination("Sponza/Lighting/Sun Inclination", 0.75f, 0.0f, 1.0f, 0.01f);
	NumVar ShadowDimX("Sponza/Lighting/Shadow Dim X", 5000, 1000, 10000, 100);
	NumVar ShadowDimY("Sponza/Lighting/Shadow Dim Y", 3000, 1000, 10000, 100);
	NumVar ShadowDimZ("Sponza/Lighting/Shadow Dim Z", 3000, 1000, 10000, 100);

	struct ColorVertex { Vector4 position;  Vector4 color; };

	float m_aspectRatio = 16.0f / 9.0f;

//    Vertex vertices[] =
//    {
//        // The sample raytraces in screen space coordinates.
//        // Since DirectX screen space coordinates are right handed (i.e. Y axis points down).
//        // Define the vertices in counter clockwise order ~ clockwise in left handed.
//        { 0, -offset, depthValue },
//        { -offset, offset, depthValue },
//        { offset, offset, depthValue }
//    };
	// Define the geometry for a triangle.
    //const float m_triDepthValue = 1.0f;
    const float m_triDepthValue = 0.1f;
	ColorVertex triangleVertices[3] =
	{
		{ { 0.0f, 0.25f * m_aspectRatio, m_triDepthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.25f, -0.25f * m_aspectRatio, m_triDepthValue,1}, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.25f, -0.25f * m_aspectRatio, m_triDepthValue,1}, { 0.0f, 0.0f, 1.0f, 1.0f } }
	};

	//The triangle geomtry buffer 
	ByteAddressBuffer m_GeometryBuffer;
	//The vertex and index resource views
	D3D12_VERTEX_BUFFER_VIEW    m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW    m_IndexBufferView;



	//---   RAY-TRACING RELATED

}

void TestRenderer::Startup( Camera& Camera )
{
    DXGI_FORMAT ColorFormat = g_SceneColorBuffer.GetFormat();
    DXGI_FORMAT NormalFormat = g_SceneNormalBuffer.GetFormat();
    DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();
    //DXGI_FORMAT ShadowFormat = g_ShadowBuffer.GetFormat();

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

    // Depth-only (2x rate)
    m_DepthPSO.SetRootSignature(Renderer::m_RootSig);
    m_DepthPSO.SetRasterizerState(RasterizerDefault);
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
    m_ModelPSO.SetDepthStencilState(DepthStateTestEqual);
    m_ModelPSO.SetRenderTargetFormats(2, formats, DepthFormat);
    
    m_ModelPSO.SetVertexShader( g_pModelViewerVS, sizeof(g_pModelViewerVS) );
//--- REPLACE WITH CUSTOM PIXEL SHADER ---
//    m_ModelPSO.SetPixelShader( g_pModelViewerPS, sizeof(g_pModelViewerPS) );
    m_ModelPSO.SetPixelShader( g_pTestRendererPS, sizeof(g_pTestRendererPS) );
    m_ModelPSO.Finalize();

//    m_CutoutModelPSO = m_ModelPSO;
//    m_CutoutModelPSO.SetRasterizerState(RasterizerTwoSided);
//    m_CutoutModelPSO.Finalize();



    //--- DEMO PASS FOR RENDERING TRIANGLE ---
    // Full color pass
    m_TestPSO = m_DepthPSO;
    m_TestPSO.SetBlendState(BlendDisable);
    m_TestPSO.SetDepthStencilState(DepthStateTestEqual);
    m_TestPSO.SetRenderTargetFormats(2, formats, DepthFormat);

    m_TestPSO.SetInputLayout(_countof(colorElem), colorElem);
    //--- CHANGE THE DEPTH STATE ALWAYS TO DRAW ON TOP OF GEOMETRY
    m_TestPSO.SetDepthStencilState(DepthStateDisabled);
    //--- THIS HAS TO BE SET TO UNKNOWN FORMAT TO CONFORM TO FRAMEWORK
    m_TestPSO.SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
    //--- MAKE SURE THAT CULLING IS OFF AND BOTH SIDES ARE DRAWN
    m_TestPSO.SetRasterizerState(RasterizerTwoSided);

    //-- CHANGE TO THE NEW SHADER FOR THE TRIANGLE
    m_TestPSO.SetVertexShader( g_pSimpleColorVS, sizeof(g_pSimpleColorVS) );
    m_TestPSO.SetPixelShader( g_pSimpleColorPS, sizeof(g_pSimpleColorPS) );

    m_TestPSO.Finalize();

    // LOADING SPONZA AS WELL
    ASSERT(m_Model.Load(L"Sponza/sponza.h3d"), "Failed to load model");
    ASSERT(m_Model.GetMeshCount() > 0, "Model contains no meshes");
    InitTriangleModel();

    // The caller of this function can override which materials are considered cutouts
    m_pMaterialIsCutout.resize(m_Model.GetMaterialCount());
    for (uint32_t i = 0; i < m_Model.GetMaterialCount(); ++i)
    {
        const ModelH3D::Material& mat = m_Model.GetMaterial(i);
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
    float modelRadius = Length(m_Model.GetBoundingBox().GetDimensions()) * 0.5f;
    const Vector3 eye = m_Model.GetBoundingBox().GetCenter() + Vector3(modelRadius * 0.5f, 0.0f, 0.0f);
    Camera.SetEyeAtUp( eye, Vector3(kZero), Vector3(kYUnitVector) );


    Lighting::CreateRandomLights(m_Model.GetBoundingBox().GetMin(), m_Model.GetBoundingBox().GetMax());

    //Create device dependent resource for ray tracing for the triangle 
    TestRaytracing::CreateDeviceDependentResources(
        m_VertexBufferView,
        m_IndexBufferView,
        &Graphics::g_SceneColorBuffer
    );
}

const ModelH3D& TestRenderer::GetModel()
{
    return TestRenderer::m_Model;
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
//   m_VertexBuffer 
//        = m_GeometryBuffer.VertexBufferView(0,  (uint32_t)vertexDataSize,vertexStride);
	m_VertexBufferView
		= m_GeometryBuffer.VertexBufferView(0, vertexDataSize, vertexStride);
	//    m_IndexBuffer  
	//        = m_GeometryBuffer.IndexBufferView((uint32_t)vertexDataSize, DXGI_FORMAT_R16_UINT, (uint32_t)indexDataSize);

	m_IndexBufferView
		= m_GeometryBuffer.IndexBufferView(vertexDataSize, indexDataSize, true);

	// Optional: store CPU copy (not required if you don't need access)
//    m_pVertexData = reinterpret_cast<uint8_t*>(_aligned_malloc(vertexDataSize, 16));
//    m_pIndexData  = reinterpret_cast<uint8_t*>(_aligned_malloc(indexDataSize, 16));
//    memcpy(m_pVertexData, triangleVerts, vertexDataSize);
//    memcpy(m_pIndexData, indices, indexDataSize);

	// Done!
}
//void TestRenderer::CheckRaytracingSupport(GraphicsContext gfx) {
////	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
////    ID3D12Device device;
//////    D3D12_DEVIce
//////	m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
//////		&options5, sizeof(options5));
//////	if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
//////		throw std::runtime_error("Raytracing not supported on device");
//	}

void TestRenderer::Cleanup( void )
{
    m_Model.Clear();
    Lighting::Shutdown();
    TextureManager::Shutdown();
}

void TestRenderer::RenderTriangleObject(GraphicsContext& gfxContext, const Matrix4& ViewProjMat, const Vector3& viewerPos, eObjectFilter Filter)
{
    //uint32_t VertexStride = m_Model.GetVertexStride();
	const UINT vertexBufferSize = sizeof(triangleVertices);
    //---TEMPORARILY switch index and vertex buffers
	gfxContext.SetIndexBuffer(m_IndexBufferView);
	gfxContext.SetVertexBuffer(0, m_VertexBufferView);

    //--- Draw three indices of the triangle
	gfxContext.DrawIndexed(3, 0, 0);
    
    //--- Switch Back To Sponza model
	gfxContext.SetIndexBuffer(m_Model.GetIndexBuffer());
	gfxContext.SetVertexBuffer(0, m_Model.GetVertexBuffer());

}

void TestRenderer::RenderObjects( GraphicsContext& gfxContext, const Matrix4& ViewProjMat, const Vector3& viewerPos, eObjectFilter Filter )
{
    struct VSConstants
    {
        Matrix4 modelToProjection;
        Matrix4 modelToShadow;
        XMFLOAT3 viewerPos;
    } vsConstants;
    vsConstants.modelToProjection = ViewProjMat;
    vsConstants.modelToShadow = m_SunShadow.GetShadowMatrix();
    XMStoreFloat3(&vsConstants.viewerPos, viewerPos);

    gfxContext.SetDynamicConstantBufferView(Renderer::kMeshConstants, sizeof(vsConstants), &vsConstants);

    __declspec(align(16)) uint32_t materialIdx = 0xFFFFFFFFul;

    uint32_t VertexStride = m_Model.GetVertexStride();

    for (uint32_t meshIndex = 0; meshIndex < m_Model.GetMeshCount(); meshIndex++)
    {
        const ModelH3D::Mesh& mesh = m_Model.GetMesh(meshIndex);

        uint32_t indexCount = mesh.indexCount;
        uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint16_t);
        uint32_t baseVertex = mesh.vertexDataByteOffset / VertexStride;

        if (mesh.materialIndex != materialIdx)
        {
            if ( m_pMaterialIsCutout[mesh.materialIndex] && !(Filter & kCutout) ||
                !m_pMaterialIsCutout[mesh.materialIndex] && !(Filter & kOpaque) )
                continue;

            materialIdx = mesh.materialIndex;
            gfxContext.SetDescriptorTable(Renderer::kMaterialSRVs, m_Model.GetSRVs(materialIdx));

            gfxContext.SetDynamicConstantBufferView(Renderer::kCommonCBV, sizeof(uint32_t), &materialIdx);
        }

        gfxContext.DrawIndexed(indexCount, startIndex, baseVertex);
    }
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
        //RenderObjects(gfxContext, m_LightShadowMatrix[LightIndex], camera.GetPosition(), kOpaque);
        gfxContext.SetPipelineState(m_CutoutShadowPSO);
        //RenderObjects(gfxContext, m_LightShadowMatrix[LightIndex], camera.GetPosition(), kCutout);
    }
    m_LightShadowTempBuffer.EndRendering(gfxContext);

    gfxContext.TransitionResource(m_LightShadowTempBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
    gfxContext.TransitionResource(m_LightShadowArray, D3D12_RESOURCE_STATE_COPY_DEST);

    gfxContext.CopySubresource(m_LightShadowArray, LightIndex, m_LightShadowTempBuffer, 0);

    gfxContext.TransitionResource(m_LightShadowArray, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    ++LightIndex;
}

void TestRenderer::RenderScene(
    GraphicsContext& gfxContext,
    const Camera& camera,
    const D3D12_VIEWPORT& viewport,
    const D3D12_RECT& scissor,
    bool skipDiffusePass,
    bool skipShadowMap)
{
    TestRaytracing::DoRaytracing();
    Renderer::UpdateGlobalDescriptors();

    uint32_t FrameIndex = TemporalEffects::GetFrameIndexMod2();

    float costheta = cosf(m_SunOrientation);
    float sintheta = sinf(m_SunOrientation);
    float cosphi = cosf(m_SunInclination * 3.14159f * 0.5f);
    float sinphi = sinf(m_SunInclination * 3.14159f * 0.5f);
    m_SunDirection = Normalize(Vector3( costheta * cosphi, sinphi, sintheta * cosphi ));

    __declspec(align(16)) struct
    {
        Vector3 sunDirection;
        Vector3 sunLight;
        Vector3 ambientLight;
        float ShadowTexelSize[4];

        float InvTileDim[4];
        uint32_t TileCount[4];
        uint32_t FirstLightIndex[4];

		uint32_t FrameIndexMod2;
    } psConstants;

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

    // Set the default state for command lists
    auto& pfnSetupGraphicsState = [&](void)
    {
        gfxContext.SetRootSignature(Renderer::m_RootSig);
        gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Renderer::s_TextureHeap.GetHeapPointer());
        gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        gfxContext.SetIndexBuffer(m_Model.GetIndexBuffer());
        gfxContext.SetVertexBuffer(0, m_Model.GetVertexBuffer());
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
            RenderObjects(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), kOpaque );
        }

       //--- CUTOUT RENDERING ---
        {
            ScopedTimer _prof2(L"Cutout", gfxContext);
            {
                gfxContext.SetPipelineState(m_CutoutDepthPSO);
            }
            RenderObjects(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), kCutout );
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

            {
                ScopedTimer _prof2(L"Render Color", gfxContext);

                gfxContext.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

                gfxContext.SetDescriptorTable(Renderer::kCommonSRVs, Renderer::m_CommonTextures);
                gfxContext.SetDynamicConstantBufferView(Renderer::kMaterialConstants, sizeof(psConstants), &psConstants);

                {
                    gfxContext.SetPipelineState(m_ModelPSO);
                    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
                    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ g_SceneColorBuffer.GetRTV(), g_SceneNormalBuffer.GetRTV() };
                    gfxContext.SetRenderTargets(ARRAYSIZE(rtvs), rtvs, g_SceneDepthBuffer.GetDSV_DepthReadOnly());
                    gfxContext.SetViewportAndScissor(viewport, scissor);
                }
                RenderObjects( gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque );
                {
                    ScopedTimer _prof3(L"Render Triangle", gfxContext);
                    gfxContext.SetPipelineState(m_TestPSO);
                    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
                    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[]{ g_SceneColorBuffer.GetRTV(), g_SceneNormalBuffer.GetRTV() };
                    gfxContext.SetRenderTargets(ARRAYSIZE(rtvs), rtvs, g_SceneDepthBuffer.GetDSV_DepthReadOnly());
                    gfxContext.SetViewportAndScissor(viewport, scissor);
					RenderTriangleObject(gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kOpaque);
                }

//                --- SKIP NORMAL CUTOUTS---
//                gfxContext.SetPipelineState(m_CutoutModelPSO);
//                RenderObjects( gfxContext, camera.GetViewProjMatrix(), camera.GetPosition(), TestRenderer::kCutout );
            }
        }
    }
}
