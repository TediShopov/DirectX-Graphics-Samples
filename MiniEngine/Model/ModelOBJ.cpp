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
            XMFLOAT3 tangent = { 0,0,0 }; XMFLOAT3 bitangent = { 0,0,0 };
            this->m_pVertices.push_back({ pos,tex,normal, tangent,bitangent});
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
    size_t vbSize = m_pVertices.size() * sizeof(H3DVertex);
    size_t ibSize = m_pIndices.size() * sizeof(uint16_t);


    std::vector<byte> unifiedBuffer(vbSize + ibSize);
    void* vertexOffsetInGB= unifiedBuffer.data() +0;
    memcpy(vertexOffsetInGB, m_pVertices.data(), vbSize);

    void* indexOffset = unifiedBuffer.data() + vbSize;
    memcpy(indexOffset, m_pIndices.data(), ibSize);

    std::vector<H3DVertex> reconstrueVb(m_pVertices.size());
    std::vector<uint16_t> reconstructedIb(m_pIndices.size());
    memcpy(reconstrueVb.data(), vertexOffsetInGB, vbSize);

    memcpy(reconstructedIb.data(), indexOffset, ibSize);

    m_GeometryBuffer.Create(L"Geometry Buffer", vbSize+ibSize, 1,unifiedBuffer.data());
    m_VertexBufferView = m_GeometryBuffer.VertexBufferView(0, (uint16_t)vbSize, sizeof(H3DVertex));
    m_IndexBufferView = m_GeometryBuffer.IndexBufferView(vbSize,(uint16_t)ibSize, true);

    meshPtr->bounds;
    meshPtr->ibFormat = DXGI_FORMAT_R16_UINT;
    meshPtr->ibOffset = vbSize;
    meshPtr->ibSize = ibSize;

    meshPtr->vbOffset = 0;
    meshPtr->vbSize = vbSize;
    meshPtr->vbStride = sizeof(H3DVertex);



    meshPtr->materialCBV = 0;
    meshPtr->meshCBV = 0;
    meshPtr->numDraws = 1;
    meshPtr->numJoints = 0;
    meshPtr->pso = 0;
    meshPtr->psoFlags = 0;
    meshPtr->samplerTable = 0;
    meshPtr->srvTable = 0;
    meshPtr->startJoint = 0;
    meshPtr->vbDepthOffset = 0;
    meshPtr->vbDepthSize = 0;

    return true;


}

Renderer::ModelData ModelOBJ::CreateModelData()
{
    Renderer::ModelData d;
    d.m_AnimationCurves = std::vector<AnimationCurve>();
    d.m_AnimationKeyFrameData = std::vector<byte>();
    d.m_Animations = std::vector<AnimationSet>();

    Renderer::AxisAlignedBox aabb;
    Renderer::BoundingSphere bs;
    d.m_BoundingBox = aabb;
    d.m_BoundingSphere = bs;


    std::vector<byte> verticesBytes(m_pVertices.size() * sizeof(H3DVertex));
    memcpy(verticesBytes.data(), m_pVertices.data(), m_pVertices.size() * sizeof(H3DVertex));
    std::vector<H3DVertex> recreatoin(m_pVertices.size());
    memcpy(recreatoin.data(), verticesBytes.data(), m_pVertices.size() * sizeof(H3DVertex));

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


    std::vector<Mesh*>singleMesh;
    singleMesh.push_back(meshPtr);
    d.m_Meshes =singleMesh;


    GraphNode node;
    node.hasChildren = false;
    node.hasSibling = false;
    node.rotation = Math::Quaternion();
    node.scale = XMFLOAT3{100,100,100};
    std::vector<GraphNode> singleGraph;
    singleGraph.push_back(node);

    d.m_SceneGraph = singleGraph;
    d.m_TextureNames = std::vector<std::string>();
    d.m_TextureOptions = std::vector<byte>();
    return d;
}

void ModelOBJ::AttemptCreateH3DModel(ModelH3D* modelToFill)
{
    size_t vbSize = m_pVertices.size() * sizeof(H3DVertex);
    size_t ibSize = m_pIndices.size() * sizeof(uint16_t);


    //--- HEADER ---
    //Remember to initialize at a later stage
    modelToFill->m_Header.boundingBox;
    modelToFill->m_Header.indexDataByteSize = ibSize;
    modelToFill->m_Header.vertexDataByteSize= vbSize;
    modelToFill->m_Header.vertexDataByteSizeDepth = 0;
    modelToFill->m_Header.materialCount = 1;
    modelToFill->m_Header.meshCount = 1;


    //---MESHES---
    ModelH3D::Mesh h3dMesh;
    h3dMesh.attrib[ModelH3D::attrib_position].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_position].components = 3;
    h3dMesh.attrib[ModelH3D::attrib_position].offset = 0;

    h3dMesh.attrib[ModelH3D::attrib_texcoord0].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_texcoord0].offset = 12;
    h3dMesh.attrib[ModelH3D::attrib_texcoord0].components = 2;
    
    h3dMesh.attrib[ModelH3D::attrib_normal].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_normal].offset = 20;
    h3dMesh.attrib[ModelH3D::attrib_normal].components = 3;

    h3dMesh.attrib[ModelH3D::attrib_normal].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_normal].offset = 32;
    h3dMesh.attrib[ModelH3D::attrib_normal].components = 3;

    h3dMesh.attrib[ModelH3D::attrib_normal].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_normal].offset = 44;
    h3dMesh.attrib[ModelH3D::attrib_normal].components = 3;


    h3dMesh.attribsEnabled = 
        ModelH3D::attrib_position | ModelH3D::attrib_texcoord0 | ModelH3D::attrib_normal | ModelH3D::attrib_mask_tangent| ModelH3D::attrib_bitangent;

    h3dMesh.indexCount = m_pIndices.size();
    //h3dMesh.indexDataByteOffset = vbSize;
    h3dMesh.indexDataByteOffset = 0;

    h3dMesh.vertexCount = m_pVertices.size();
    h3dMesh.vertexDataByteOffset = 0;

    h3dMesh.vertexDataByteOffsetDepth = 0;

    h3dMesh.materialIndex = 0;
    h3dMesh.vertexStride = sizeof(H3DVertex);

    h3dMesh.attribsEnabledDepth = 0;
    h3dMesh.vertexStrideDepth = 0;
    h3dMesh.vertexCountDepth = 0;

    ModelH3D::Mesh* meshArray= new ModelH3D::Mesh[1]{
        h3dMesh
    }
    ;
    //meshArray[0] = *meshPtr;
    modelToFill->m_pMesh = meshArray;


    //---Materials ---
    ModelH3D::Material* material = new ModelH3D::Material;
    material->ambient = Color(0.1f,0.1f,0.1f,1.0f);
    material->diffuse = Color(1.0f,0.1f,0.1f,1.0f);
    material->specular = Color(1.0f,1.0f,1.0,1.0f);

    modelToFill->m_pMaterial = material;


    //--- Unified Geometry Data ---
    //modelToFill->m_pIndexData = (unsigned char*)m_pIndices;
    modelToFill->m_pIndexData = reinterpret_cast<unsigned char*>(m_pIndices.data());
    modelToFill->m_pVertexData = reinterpret_cast<unsigned char*>(m_pVertices.data());
    //memcpy(   modelToFill->m_pIndexData, m_pIndices.data(), ibSize);
    //memcpy(modelToFill->m_pVertexData, m_pVertices.data(), vbSize);
}
