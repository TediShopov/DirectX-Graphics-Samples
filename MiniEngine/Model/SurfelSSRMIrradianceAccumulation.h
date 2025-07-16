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
#include "UniformGrid.h"
#include "ExtendedUtility.h"
#include "ModelH3D.h"
#include "CommandContext.h"

class SurfelSSRMIrradianceAccumulation
{
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
public:
	UINT width;
	UINT height;
	
	//Holds the color and depth buffer copies necessary for common SSR
	DescriptorHeap SRVHeap;
	RootSignature m_rootSignature;
	ComputePSO m_SSRMIrradiancePSO = { (L"Surfel SSRM Irradiance Accumulation") };

	void CreateRootSig();
	void CreateHeaps(ColorBuffer colorBuff,DescriptorHeap nonShaderVisibleSurfelHeap);
	void InitializePSOs();

	void Setup(ColorBuffer colorBuff,DescriptorHeap nonShaderVisibleSurfelHeap);
	void Dispatch(ComputeContext& cfxA,const Camera& camera);



};

