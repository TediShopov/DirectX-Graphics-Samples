#pragma once

#include "RootSignature.h"
#include <d3d12.h>
#include "BufferManager.h"
#include "Math/Vector.h"
#include "Math/Matrix4.h"

#include "Camera.h"
#include "GameInput.h"
#include "imgui.h"

#include <initializer_list>
struct GBufferPtrs
{
	ColorBuffer* g_Color;
	ColorBuffer* g_Diffuse;
	ColorBuffer* g_Normal;
	DepthBuffer* g_Depth;

};

using namespace Math;
class ExtendedUtility
{
public:

	static char MapToChar(GameInput::DigitalInput key)
	{
		using GI = GameInput::DigitalInput;

		switch (key)
		{
		case GI::kKey_a: return 'a';
		case GI::kKey_b: return 'b';
		case GI::kKey_c: return 'c';
		case GI::kKey_d: return 'd';
		case GI::kKey_e: return 'e';
		case GI::kKey_f: return 'f';
		case GI::kKey_g: return 'g';
		case GI::kKey_h: return 'h';
		case GI::kKey_i: return 'i';
		case GI::kKey_j: return 'j';
		case GI::kKey_k: return 'k';
		case GI::kKey_l: return 'l';
		case GI::kKey_m: return 'm';
		case GI::kKey_n: return 'n';
		case GI::kKey_o: return 'o';
		case GI::kKey_p: return 'p';
		case GI::kKey_q: return 'q';
		case GI::kKey_r: return 'r';
		case GI::kKey_s: return 's';
		case GI::kKey_t: return 't';
		case GI::kKey_u: return 'u';
		case GI::kKey_v: return 'v';
		case GI::kKey_w: return 'w';
		case GI::kKey_x: return 'x';
		case GI::kKey_y: return 'y';
		case GI::kKey_z: return 'z';
		default: return ' ';
		}
	}

	static ImGuiKey MapGameInputKeyToImGuiKey(GameInput::DigitalInput key)
	{
		using GI = GameInput::DigitalInput;

		switch (key)
		{
		case GI::kKey_a: return ImGuiKey_A;
		case GI::kKey_b: return ImGuiKey_B;
		case GI::kKey_c: return ImGuiKey_C;
		case GI::kKey_d: return ImGuiKey_D;
		case GI::kKey_e: return ImGuiKey_E;
		case GI::kKey_f: return ImGuiKey_F;
		case GI::kKey_g: return ImGuiKey_G;
		case GI::kKey_h: return ImGuiKey_H;
		case GI::kKey_i: return ImGuiKey_I;
		case GI::kKey_j: return ImGuiKey_J;
		case GI::kKey_k: return ImGuiKey_K;
		case GI::kKey_l: return ImGuiKey_L;
		case GI::kKey_m: return ImGuiKey_M;
		case GI::kKey_n: return ImGuiKey_N;
		case GI::kKey_o: return ImGuiKey_O;
		case GI::kKey_p: return ImGuiKey_P;
		case GI::kKey_q: return ImGuiKey_Q;
		case GI::kKey_r: return ImGuiKey_R;
		case GI::kKey_s: return ImGuiKey_S;
		case GI::kKey_t: return ImGuiKey_T;
		case GI::kKey_u: return ImGuiKey_U;
		case GI::kKey_v: return ImGuiKey_V;
		case GI::kKey_w: return ImGuiKey_W;
		case GI::kKey_x: return ImGuiKey_X;
		case GI::kKey_y: return ImGuiKey_Y;
		case GI::kKey_z: return ImGuiKey_Z;
			// ... map all alphanumeric keys ...
		case GI::kKey_space: return ImGuiKey_Space;
		case GI::kKey_escape: return ImGuiKey_Escape;
		case GI::kKey_left: return ImGuiKey_LeftArrow;
		case GI::kKey_right: return ImGuiKey_RightArrow;
		case GI::kKey_up: return ImGuiKey_UpArrow;
		case GI::kKey_down: return ImGuiKey_DownArrow;
		case GI::kKey_lshift: return ImGuiKey_LeftShift; // Or RightShift
		case GI::kKey_lcontrol: return ImGuiKey_LeftCtrl;   // Or RightCtrl
		case GI::kKey_lalt: return ImGuiKey_LeftAlt;     // Or RightAlt
		default: return ImGuiKey_None;
		}
	}


	static Matrix4 CreatePerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ)
	{

		float yScale = 1.0f / tanf(fovY * 0.5f);
		float xScale = yScale / aspect;
		float zRange = farZ - nearZ;
		const float lhPerspectiveData[16] =
		{

			xScale, 0.0f,    0.0f,                     0.0f,
			0.0f,   yScale,  0.0f,                     0.0f,
			0.0f,   0.0f,    farZ / zRange,            1.0f,
			0.0f,   0.0f,    -nearZ * farZ / zRange,   0.0f
		};

		return Matrix4(lhPerspectiveData);
		//return Matrix4();
	}
	static Matrix4 ExtendedUtility::GetLHViewMatrix(const Camera& camera)
	{
		//Get the camera position up, right and forward vectors
		Vector3 position = camera.GetPosition();
		Vector3 forward = camera.GetForwardVec();
		Vector3 right = camera.GetRightVec();
		Vector3 up = camera.GetUpVec();

		const float lhViewMatrixData[16] = {
			(float)right.GetX(), (float)up.GetX(), (float)forward.GetX(), 0.0f,
			(float)right.GetY(), (float)up.GetY(), (float)forward.GetY(), 0.0f,
			(float)right.GetZ(), (float)up.GetZ(), (float)forward.GetZ(), 0.0f,
			-(float)Dot(right, position), -(float)Dot(up, position), -(float)Dot(forward, position), 1.0f
		};

		// Construct left-handed view matrix
		Matrix4 viewMatrix = Matrix4(lhViewMatrixData);
		return viewMatrix;

	}



	static void ExtendedUtility::CopyDescriptorsToHeap(
		ID3D12DescriptorHeap* targetHeap,
		std::initializer_list<D3D12_CPU_DESCRIPTOR_HANDLE> srcDescriptors,
		D3D12_DESCRIPTOR_HEAP_TYPE heapType,
		UINT& currentOffset,
		UINT descriptorSize);

	/// <summary>
	/// Utility mehtod that copies initializer list of source descriptor to a given heap. Must specify the heap type as well.
	/// </summary>
	static void CopyDescriptorsToHeap(
		DescriptorHeap& targetHeap,
		std::initializer_list<D3D12_CPU_DESCRIPTOR_HANDLE> srcDescriptors,
		D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	static void CopyDescriptorsToHeap(DescriptorHeap& targetHeap,
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srcDescriptors,
		D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	// Read one vertex's attribute of type T
	template<typename T>
	static T ReadAttribute(
		uint8_t* base,
		size_t stride,
		size_t index,
		size_t offset
	)
	{
		return *reinterpret_cast<T*>(base + index * stride + offset);
	}
	template<typename T>
	static T ReadAttribute(
		uint8_t* base,
		size_t index,
		size_t offset
	)
	{
		return *reinterpret_cast<T*>(base + index * sizeof(T) + offset);
	}

	static XMFLOAT4 PadToXMFLOAT4(XMFLOAT3 a, float w = 1.0f) {
		return XMFLOAT4(a.x, a.y, a.z, w);

	}


};

