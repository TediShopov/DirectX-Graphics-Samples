#pragma once
#include "VectorMath.h"
#include "TextureManager.h"
#include "GpuBuffer.h"
#include "DescriptorHeap.h"
#include "../Core/Math/BoundingBox.h"
#include "Geometry/BaseMesh.h"
#include "ModelLoader.h"

struct OBJVertex {
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
    XMFLOAT3 normal;

};


class ModelOBJ
{
public:

    ModelOBJ();
    ~ModelOBJ();

    void Clear();


//    std::vector<XMFLOAT3> m_pPositions;
//    std::vector<XMFLOAT3> m_pNormals;
//    std::vector<XMFLOAT2> m_pTexCoords;
    std::vector<VertexType> m_pVertices;
    std::vector<UINT> m_pIndices;


    virtual bool Load(const std::wstring& filename);
     bool CreateModelData();
     uint32_t GetVertexStride() const { return m_VertexStride; }
    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBuffer() const { return m_VertexBufferView; }
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBuffer() const { return m_IndexBufferView; }


//    void CreateVertexBufferSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;
//    void CreateIndexBufferSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;

    // These are used at runtime during rendering
//    ByteAddressBuffer m_VertexBuffer;
//    ByteAddressBuffer m_IndexBuffer;
    ByteAddressBuffer m_GeometryBuffer;

    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
    uint32_t m_VertexStride;

};

