//// Mesh.h
//#ifndef _MESH_H_
//#define _MESH_H_
//
//#include <d3d12.h>
//#include <directxmath.h>
//#include "texture.h"
//
//using namespace DirectX;
//
//class Mesh
//{
//protected:
//
//	struct VertexType
//	{
//		XMFLOAT3 position;
//		XMFLOAT2 texture;
//		XMFLOAT3 normal;
//	};
//
//public:
//	Mesh(ID3D12Device* device, ID3D12DeviceContext* deviceContext, WCHAR* textureFilename);
//	~Mesh();
//
//	void SendData(ID3D12DeviceContext*);
//	int GetIndexCount();
//	ID3D12ShaderResourceView* GetTexture();
//
//protected:
//	bool InitBuffers(ID3D12Device*);
//	void LoadTexture(ID3D12Device*, ID3D12DeviceContext* deviceContext, WCHAR*);
//	
//protected:
//	ID3D12Buffer *m_vertexBuffer, *m_indexBuffer;
//	int m_vertexCount, m_indexCount;
//	Texture* m_Texture;
//};
//
//#endif