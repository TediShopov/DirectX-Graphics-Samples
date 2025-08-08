#pragma once
#include "HBIL.h"

#include "CompiledShaders/GBufferSlice.h"
#include "CompiledShaders/ComputeHBIL_Interleaved.h"

__declspec(align(16)) struct HBILInterleavedBuffer {
	UINT	_targetResolutionX;
	UINT	_targetResolutionY;
	XMFLOAT2 _csDirection;					// Sampling direction in camera space

	// X=Index of the X pass in [0,3], Y=Index of the Y pass in [0,3], Z=Index of the render pass in [0,15]
	UINT	_renderPassIndexX;
	UINT	_renderPassIndexY;
	UINT	_renderPassIndexZ;
	float	_gatherSphereMaxRadius_m;		// Radius of the sphere that will gather our irradiance samples (in meters)

	XMFLOAT4	_bilateralValues;

	float	_gatherSphereMaxRadius_p;		// Radius of the sphere that will gather our irradiance samples (in pixels)
	float	_temporalAttenuationFactor;		// Attenuation factor of radiance from previous frame
	UINT	_jitterOffset;					// A jitter value in [0,67] that changes per frame

};

class GBufferSlice
{


	__declspec(align(16)) struct SliceParamsData
	{
		UINT quarterWidth;
		UINT quarterHeight;
		UINT tileWidth;
		UINT tileHeight;
	} SliceParams;
enum DownsampledGBuffer
	{
		DIFFUSE,DEPTH,NORMAL
	};
		//Hold the pointers to actual GBuffer
		//GBufferPtrs m_GBuffer;
		ColorBuffer* m_downsampledGBuffer;
		ColorBuffer m_slicedBuffer[3];

		ComputePSO m_PSO = {(L"GBuffer Slicing CS PSO")};
		RootSignature m_RootSignature;
		DescriptorHeap m_DescriptorHeap;

	public:
	//The output buffer in this case are the sliced GBuffers
	ColorBuffer* GetOuptutBuffer()
	{
		return m_slicedBuffer;

	}

		GBufferSlice(){}

		void CreateRootSignature()
		{
			//Todo initialize the samplers correctly
		  SamplerDesc LinearClamp;
		  LinearClamp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		  LinearClamp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		  LinearClamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		  LinearClamp.MinLOD = 0;

		  m_RootSignature.Reset(3, 1);
		  m_RootSignature.InitStaticSampler(0, LinearClamp);

		  m_RootSignature[0].InitAsConstantBuffer(0);
		  //The first SRV is the input texture in full resolution
		  m_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3);
		  //The first UAV is the output texture in quarter resolution
		  m_RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 3);

		  m_RootSignature.Finalize(L"CS GBuffer Slice Root Signature");
		}
		void CreatePSO()
		{
		  m_PSO.SetRootSignature(m_RootSignature);
		  m_PSO.SetComputeShader(g_pGBufferSlice, sizeof(g_pGBufferSlice));
		  m_PSO.Finalize();

		}

		void InitializeUAVs()
		{
			UINT width = m_downsampledGBuffer[0].GetWidth()/4;
			UINT height = m_downsampledGBuffer[0].GetHeight()/4;
			m_slicedBuffer[DEPTH].CreateArray(L"Sliced  Depth Buffer", width, height, 16, m_downsampledGBuffer[DEPTH].GetFormat());
			m_slicedBuffer[DIFFUSE].CreateArray(L"Sliced Diffuse Buffer", width, height, 16, m_downsampledGBuffer[DIFFUSE].GetFormat());
			m_slicedBuffer[NORMAL].CreateArray(L"Sliced Normal Buffer", width, height, 16, m_downsampledGBuffer[NORMAL].GetFormat());
		}


		void CreateDescriptorHeap() {

		  m_DescriptorHeap.Create(L"GBuffer Slice Heap", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 6);
		  
		  ExtendedUtility::CopyDescriptorsToHeap(m_DescriptorHeap, {
			  m_downsampledGBuffer[DEPTH].GetSRV(),
			  m_downsampledGBuffer[DIFFUSE].GetSRV(),
			  m_downsampledGBuffer[NORMAL].GetSRV(),
			  m_slicedBuffer[DEPTH].GetUAV(),
			  m_slicedBuffer[DIFFUSE].GetUAV(),
			  m_slicedBuffer[NORMAL].GetUAV(),
			  }
		  );

		}

		void Dispatch(ComputeContext& cfx,const Camera& camera) {

			ScopedTimer _prof(L"GBuffer Slice", cfx);

			cfx.SetPipelineState(m_PSO);
			cfx.SetRootSignature(m_RootSignature);
			cfx.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,m_DescriptorHeap.GetHeapPointer());

			cfx.TransitionResource(m_downsampledGBuffer[DIFFUSE], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cfx.TransitionResource(m_downsampledGBuffer[DEPTH], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cfx.TransitionResource(m_downsampledGBuffer[NORMAL], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			cfx.TransitionResource(m_slicedBuffer[DEPTH], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);
			cfx.TransitionResource(m_slicedBuffer[DIFFUSE], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);
			cfx.TransitionResource(m_slicedBuffer[NORMAL], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);


			SliceParams.quarterHeight = m_downsampledGBuffer[DIFFUSE].GetHeight();
			SliceParams.quarterWidth = m_downsampledGBuffer[DIFFUSE].GetWidth();
			SliceParams.tileHeight = SliceParams.quarterHeight / 4;
			SliceParams.tileWidth = SliceParams.quarterWidth / 4;

			cfx.SetDynamicConstantBufferView(0, sizeof(SliceParamsData), &SliceParams);
			cfx.SetDescriptorTable(1,m_DescriptorHeap[0]);
			cfx.SetDescriptorTable(2,m_DescriptorHeap[3]);
			cfx.Dispatch2D(SliceParams.quarterWidth, SliceParams.quarterHeight, 8, 8);
		}

		bool m_debugReadingEnabled = true;
		

		void Setup(ColorBuffer* quarterResBuffer,GraphicsPSO quadPSO)
		{
			m_downsampledGBuffer = quarterResBuffer;

			InitializeUAVs();
			CreateDescriptorHeap();
			CreateRootSignature();
			CreatePSO();
			

		}




};



class HBILInterleaved :
    public HBIL
{
	HBILInterleavedBuffer m_HBILInterleavedData;

	 struct ColorVertex { Vector4 position;  Vector4 color; };
	 float m_aspectRatio = 16.0f / 9.0f;
	 float m_depthValue = 0.1f;
	  float _TRI_SCALE = 0.3f;
	  float QUAD_SCALE = 1.0f;
	  //float QUAD_SCALE = 1;

	 ColorVertex fullScreenQuad[6] =
	{
		{ { -QUAD_SCALE, -QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { QUAD_SCALE, QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { -QUAD_SCALE, QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },

		{ { -QUAD_SCALE, -QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { QUAD_SCALE, QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { QUAD_SCALE, -QUAD_SCALE, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
	};

	 uint32_t indices[6] = { 0, 1, 2,3,4,5 };

	 ByteAddressBuffer m_QuadGB[16];
	//D3D12_VERTEX_BUFFER_VIEW    m_QuadVB;
	//D3D12_INDEX_BUFFER_VIEW    m_QuadIB;



	void InitQuadModel()
	{

		for (int y = 0; y < 4; ++y)
		{
			for (int x = 0; x < 4; ++x)
			{
				int z = y * 4 + x;


				size_t vertexStride = sizeof(ColorVertex);
				size_t vertexDataSize = sizeof(fullScreenQuad);
				size_t indexDataSize = sizeof(indices);

				float tileSizeX = 2.0f / 4.0f;
				float tileSizeY = 2.0f / 4.0f;

				XMFLOAT2 tileStartNDC;
				tileStartNDC.x = -1 + (float)x * tileSizeX;
				tileStartNDC.y = 1 - (float)y * tileSizeX;
				XMFLOAT2 tileEndNDC;
				tileEndNDC.x = tileStartNDC.x + tileSizeX;
				tileEndNDC.y = tileStartNDC.y - tileSizeY;

				ColorVertex tileQuad[6] =
				{
					{ { tileStartNDC.x, tileStartNDC.y, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
					{ { tileEndNDC.x, tileEndNDC.y, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
					{ { tileStartNDC.x, tileEndNDC.y, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },

					{ { tileStartNDC.x, tileStartNDC.y, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
					{ { tileEndNDC.x, tileEndNDC.y, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
					{ { tileEndNDC.x, tileStartNDC.y, m_depthValue,1}, { 1.0f, 0.0f, 0.0f, 1.0f } },
				};

				// 2. Allocate upload buffer (vertex + index)
				size_t totalSize = vertexDataSize + indexDataSize;
				void* uploadMem = _aligned_malloc(totalSize, 16);
				assert(uploadMem);

				void* vertexData = uploadMem;
				void* indexData = static_cast<uint8_t*>(uploadMem) + vertexDataSize;

				memcpy(vertexData, tileQuad, vertexDataSize);
				memcpy(indexData, indices, indexDataSize);
				//--- Upload buffer to GPU
				m_QuadGB[z].Create(L"Full Screen Quad", totalSize, 1, uploadMem);

			}

		}







	}



	void RenderFullScreenQuad(GraphicsContext& gfxContext,UINT z)
	{
		const UINT vertexBufferSize = sizeof(fullScreenQuad);

		uint32_t indices[6] = { 0, 1, 2,3,4,5 };
		//---TEMPORARILY switch index and vertex buffers
		gfxContext.SetIndexBuffer(m_QuadGB[z].IndexBufferView(sizeof(fullScreenQuad), sizeof(indices), true));
		gfxContext.SetVertexBuffer(0, m_QuadGB[z].VertexBufferView(0, sizeof(fullScreenQuad), sizeof(ColorVertex)));

		//--- Draw three indices of the triangle
		gfxContext.DrawIndexed(6, 0, 0);
	}

public:

	void CreateHBILPSO(GraphicsPSO quadRenderingPSO) override {
		m_HBILRenderPass = quadRenderingPSO;

		m_HBILRenderPass.SetRootSignature(m_HBILRenderRS);
		//DXGI_FORMAT formats[2] = { DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R11G11B10_FLOAT };
		//m_HBILRenderPass.SetRenderTargetFormats(2, formats,DXGI_FORMAT_UNKNOWN);
		m_HBILRenderPass.SetPixelShader(g_pComputeHBIL_Interleaved, sizeof(g_pComputeHBIL_Interleaved));

		m_HBILRenderPass.Finalize();

	}

    //TODO change the root signature to used the new cbuffer layout
	void Setup(GBufferPtrs gbuffer, ColorBuffer* slicedGBuffers, GraphicsPSO quadPSO) override {

		HBIL::Setup(gbuffer, slicedGBuffers, quadPSO);

		InitQuadModel();
		m_inputBuffers = slicedGBuffers;


//		m_HBILHeap.Create(L"HBIL INTERLEAVED HEAP", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5);
//
//		ExtendedUtility::CopyDescriptorsToHeap(m_HBILHeap, {
//		m_inputBuffers[0].GetSRV(),
//		m_inputBuffers[1].GetSRV(),
//		//quarterResGBuffer[DIFFUSE].GetSRV(),
//		m_inputBuffers[2].GetSRV(),
//		m_BlueNoiseTexture.GetSRV(),
//		m_DebugHBIL.GetUAV()
//			}
//		);


		CreateHBILPSO(quadPSO);
	 }


	void RenderHBIL(GraphicsContext& gfx, const Camera& camera)
	{
		ScopedTimer _prof(L"Render HBIL", gfx);

		m_MainHBILCB._deltaTime = 0.1;
		m_MainHBILCB._resolution.x = m_GBuffer.g_Color->GetWidth();
		m_MainHBILCB._resolution.y = m_GBuffer.g_Color->GetHeight();
		m_MainHBILCB._coneAngleBias = 0.1f;
		m_MainHBILCB._framesCount = framesCount;
		m_MainHBILCB._flags = 0;


		UpdateCameraCBufferLH(camera, m_HBILCameraCB);


		const int totalFrames = 16;   // 16 frames per full rotation
		float angle = (2.0f * 3.14159265f * (framesCount % totalFrames)) / totalFrames;

		// Calculate the normalized 2D direction vector on the unit circle
		m_HBILInterleavedData._csDirection.x = cos(angle);
		m_HBILInterleavedData._csDirection.y = sin(angle);


		m_HBILInterleavedData._gatherSphereMaxRadius_m = 200;
		m_HBILInterleavedData._gatherSphereMaxRadius_p = 400;

		m_HBILExtraCB._bilateralValues = XMFLOAT4(0, 0, 0, 0);
		m_HBILExtraCB._temporalAttenuationFactor = 0;

		m_HBILInterleavedData._temporalAttenuationFactor = 0.01f;
		m_HBILInterleavedData._bilateralValues = XMFLOAT4(0, 0, 0, 0);
		m_HBILInterleavedData._jitterOffset = framesCount % 67;


		gfx.SetPipelineState(m_HBILRenderPass);
		gfx.SetRootSignature(m_HBILRenderRS);
		gfx.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_HBILHeap.GetHeapPointer());


		//Transition Resources
		gfx.TransitionResource(m_inputBuffers[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
		gfx.TransitionResource(m_inputBuffers[1], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
		gfx.TransitionResource(m_inputBuffers[2], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);



		gfx.SetDynamicConstantBufferView(0, sizeof(HBIL_MAIN), &m_MainHBILCB);
		gfx.SetDynamicConstantBufferView(1, sizeof(CB_Camera), &m_HBILCameraCB);
		gfx.SetDynamicConstantBufferView(2, sizeof(CBSH), &m_CBSH);

		gfx.SetDescriptorTable(4, m_HBILHeap[0]);
		gfx.SetDescriptorTable(5, m_HBILHeap[4]);

		//Just render the first tile for now

		m_HBILInterleavedData._targetResolutionX = m_inputBuffers[0].GetWidth() * 4;
		m_HBILInterleavedData._targetResolutionY = m_inputBuffers[0].GetHeight() * 4;


		for (int y = 0; y < 4; ++y)
		{

			for (int x = 0; x < 4; ++x)

			{
				int z = y * 4 + x;

				m_HBILInterleavedData._renderPassIndexX = x;
				m_HBILInterleavedData._renderPassIndexY = y;
				m_HBILInterleavedData._renderPassIndexZ = z;

 

				//cb._jitterOffset= m_inputBuffers[0].GetHeight();
				//cb._bilateralValues= m_inputBuffers[0].GetHeight();
				//cb._temporalAttenuationFactor= m_inputBuffers[0].GetHeight();


				// ... set rest of constants

				// Update cbuffer
				gfx.SetDynamicConstantBufferView(3, sizeof(HBILInterleavedBuffer), &m_HBILInterleavedData);

				RenderFullScreenQuad(gfx,z);
				// Dispatch or draw just for this tile
				//gfx.Dispatch(ceil(481 / 8), ceil(270 / 8), 1); // if compute shader
				// or draw quad if using pixel shader
			}
		}


		framesCount++;

	}

};

