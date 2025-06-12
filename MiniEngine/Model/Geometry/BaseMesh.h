
#pragma once
#include "GpuBuffer.h"
#include <DirectXMath.h>
#include <vector>

struct VertexType
{
	XMFLOAT3 position;
	XMFLOAT2 texture;
	XMFLOAT3 normal;
};


///**

//* \class Base Mesh
//*
//* \brief The parent for all mesh objects. Provides default functionality.
//*
//* Can be inherited to create custom meshes. Provide functions for sending data to GPU memory, getting index count and storing geometry data.
//*
//* \author Paul Robertson
//*/
//
//
//#ifndef _BASEMESH_H_
//#define _BASEMESH_H_
//
//#include <d3d11.h>
//#include <DirectXMath.h>
//#include <vector>
//
//using namespace DirectX;
//
//struct VertexType
//{
//	XMFLOAT3 position;
//	XMFLOAT2 texture;
//	XMFLOAT3 normal;
//};
//class BaseMesh
//{
//protected:
//
//	/// Default struct for general vertex data include position, texture coordinates and normals
//
//	/// Default vertex struct for geometry with only position and colour
//	struct VertexType_Colour
//	{
//		XMFLOAT3 position;
//		XMFLOAT4 colour;
//	};
//
//	/// Default vertex struct for geometry with only position and texture coordinates.
//	struct VertexType_Texture
//	{
//		XMFLOAT3 position;
//		XMFLOAT2 texture;
//	};
//
//
//	//Adapted from https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/barycentric-coordinates.html
//	bool rayTriangleIntersect(
//XMVECTOR orig, XMVECTOR dir, XMVECTOR v0, XMVECTOR v1, XMVECTOR v2, XMVECTOR& P,
//	float& t, float& u, float& v);
//
//public:
//	bool allRayMeshIntersections(XMVECTOR orig, XMVECTOR dir, std::vector<XMVECTOR>* intersections = nullptr);
//	//Returns the closest ray-mesh intersection between ray and mesh
//	bool rayMeshIntersect(XMVECTOR orig, XMVECTOR dir, XMVECTOR* closestIntersection);
//	/// Empty constructor
//	ID3D11Device* device;
//	ID3D11DeviceContext* deviceContext;
//	BaseMesh(ID3D11Device* device,ID3D11DeviceContext* deviceContext);
//
//	~BaseMesh(); 
//
//
//	std::vector<VertexType> vertices;
//	std::vector<unsigned long> indices ;
//void computeBoundingBox();
//	/// Transfers mesh data to the GPU.
//	virtual void sendData(ID3D11DeviceContext* deviceContext, D3D_PRIMITIVE_TOPOLOGY top = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//	int getIndexCount();			///< Returns total index value of the mesh
//	int getVertexCount() { return vertexCount; }
//	//D3D11_INPUT_ELEMENT_DESC getInputLayout();
//
//	ID3D11Buffer* vertexBuffer, * indexBuffer;
//	ID3D11Buffer* vertexStagingBuffer, * indexStagingBuffer;
//	//Bounding Box
//	XMFLOAT3 boundingBoxMax;
//	XMFLOAT3 boundingBoxMin;
//
//	void changeOrigin(ID3D11Device* device,XMVECTOR origin);
//
//protected:
//
//	virtual void initBuffers(ID3D11Device*) = 0;
//	ID3D11Buffer* CreateStagingBuffer(ID3D11Device* device, ID3D11Buffer* sourceBuffer, ID3D11Buffer* stagingBufferPtr);
//	//Variable for accessing the computed vertices and indices 
//	//D3D11_INPUT_ELEMENT_DESC *inputLayout;
//	int vertexCount, indexCount;
//	
//};
//
//#endif