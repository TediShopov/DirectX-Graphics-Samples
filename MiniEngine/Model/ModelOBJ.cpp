#include "ModelOBJ.h"
#include "fstream"
#include "sstream"
#include "iostream"
#include "algorithm"
#include <cstddef>

ModelOBJ::ModelOBJ() {
}

ModelOBJ::~ModelOBJ()
{
}

void ModelOBJ::Clear()
{
}

bool ModelOBJ::Load(const std::wstring& filename)
{

	std::ifstream file(filename);
    if (!file) {
        return false;
    }

    //m_pPositions.clear();
    m_pVertices.clear();
    m_pIndices.clear();


    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") {
            XMFLOAT3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            //this->m_pPositions.push_back(pos);
            XMFLOAT2 tex = { 0,0 }; XMFLOAT3 normal = { 0,0, 0};
            this->m_pVertices.push_back({ pos,tex,normal });
        }
//        else if (type == "vt") {
//            XMFLOAT2 uv;
//            ss >> uv.x >> uv.y;
//            uv.y = 1.0f - uv.y; // Flip V
//            this->m_pTexCoords.push_back(uv);
//        }
//        else if (type == "vn") {
//            XMFLOAT3 norm;
//            ss >> norm.x >> norm.y >> norm.z;
//            this->m_pNormals.push_back(norm);
//        }
        else if (type == "f") {

            //If VT or VN are 0 just pad them with mock values



            std::string vStr;
			ss >> vStr;
            int pIdx = 0;
            int pIdxOne = 0;
            int pIdxTwo = 0;
			//sscanf(vStr.c_str(), "%d %d %d", &pIdx, &pIdxOne, &pIdxTwo);
			sscanf(vStr.c_str(), "%d", &pIdx);
			ss >> vStr;
			sscanf(vStr.c_str(), "%d", &pIdxOne);
			ss >> vStr;
			sscanf(vStr.c_str(), "%d", &pIdxTwo);
            m_pIndices.push_back(pIdx-1);
            m_pIndices.push_back(pIdxOne-1);
            m_pIndices.push_back(pIdxTwo - 1 );
        }
    }
    size_t vbSize = m_pVertices.size() * sizeof(VertexType);
    size_t ibSize = m_pIndices.size() * sizeof(UINT);


    std::vector<byte> unifiedBuffer(vbSize + ibSize);
    void* vertexOffsetInGB= unifiedBuffer.data() +0;
    memcpy(vertexOffsetInGB, m_pVertices.data(), vbSize);

    void* indexOffset = unifiedBuffer.data() + vbSize;
    memcpy(indexOffset, m_pIndices.data(), ibSize);

    std::vector<VertexType> reconstrueVb(m_pVertices.size());
    std::vector<UINT> reconstructedIb(m_pIndices.size());
    memcpy(reconstrueVb.data(), vertexOffsetInGB, vbSize);

    memcpy(reconstructedIb.data(), indexOffset, ibSize);

    m_GeometryBuffer.Create(L"Geometry Buffer", vbSize+ibSize, 1,unifiedBuffer.data());
    m_VertexBufferView = m_GeometryBuffer.VertexBufferView(0, (UINT)vbSize, sizeof(VertexType));
    m_IndexBufferView = m_GeometryBuffer.IndexBufferView(vbSize,(UINT)ibSize, true);

    return true;


}

bool ModelOBJ::CreateModelData()
{
    Renderer::ModelData d;
    d.m_AnimationCurves = std::vector<AnimationCurve>();
    d.m_AnimationKeyFrameData = std::vector<byte>();
    d.m_Animations = std::vector<AnimationSet>();

    Renderer::AxisAlignedBox aabb;
    Renderer::BoundingSphere bs;
    d.m_BoundingBox = aabb;
    d.m_BoundingSphere = bs;


    std::vector<byte> verticesBytes(m_pVertices.size() * sizeof(VertexType));
    memcpy(verticesBytes.data(), m_pVertices.data(), m_pVertices.size() * sizeof(VertexType));
    std::vector<VertexType> recreatoin(m_pVertices.size());
    memcpy(recreatoin.data(), verticesBytes.data(), m_pVertices.size() * sizeof(VertexType));

    //Check for any discrepancies
    for (size_t i = 0; i < m_pVertices.size(); i++)
    {
        if(m_pVertices[i].position.x != recreatoin[i].position.x)
        {
            int a = 3;
        }

    }

    d.m_GeometryData = verticesBytes;
    d.m_JointIBMs = std::vector<Math::Matrix4>();
    d.m_JointIndices = std::vector<UINT16>();

    d.m_MaterialConstants = std::vector<Renderer::MaterialConstantData>();
    d.m_MaterialTextures = std::vector<Renderer::MaterialTextureData>();

    d.m_Meshes = std::vector<Mesh*>();

    d.m_SceneGraph = std::vector<GraphNode>();

    d.m_TextureNames = std::vector<std::string>();
    d.m_TextureOptions = std::vector<byte>();
    return true;
}
