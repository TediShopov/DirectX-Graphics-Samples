//#pragma once
//#include "DXF.h"
//#include "ProceduralMeshA.h"
//#include "TangentMesh.h"
//#include "Terrain.h"
//#include "TessellationQuad.h"
//enum SerializableMeshType 
//{
//	Plane, Cube, Sphere, Custom, TesselationQuad, ProceduralTerrain,NonSerializable
//};
//
//class SerializableMesh
//{
//private:
//	BaseMesh* _mesh;
//	
//public:
//	int _resolutionParam;
//	int _size;
//	SerializableMeshType _type;
//	std::string name;
//	std::string filepath;
//	bool generateTangentMesh=false;
//
//	static SerializableMesh ProceduralMesh(std::string name,ProceduralMeshData data, bool isTangent,
//ID3D12Device* device, ID3D12DeviceContext* deviceContext
//	)
//	{
//		 SerializableMesh serializableMesh;
//		 serializableMesh._type = SerializableMeshType::NonSerializable;
//		 serializableMesh._resolutionParam = 0;
//		 serializableMesh.filepath = "None";
//		 serializableMesh.name = name;
//
//		 serializableMesh.generateTangentMesh = isTangent;
//		 
//		 ProceduralMeshA* proc = new ProceduralMeshA(device, deviceContext, data.vertices, data.indices);
//		 serializableMesh._mesh = proc;
//		 if(serializableMesh.generateTangentMesh)
//		 {
//			 serializableMesh._mesh = new TangentMesh(device, deviceContext, proc);
//		 }
//		 return serializableMesh;
//
//
//
//	}
//	static SerializableMesh ProceduralMesh(std::string name,BaseMesh* mesh,bool isTangent,
//ID3D12Device* device, ID3D12DeviceContext* deviceContext
//	)
//	{
//		 SerializableMesh serializableMesh;
//		 serializableMesh._type = SerializableMeshType::NonSerializable;
//		 serializableMesh._resolutionParam = 0;
//		 serializableMesh.filepath = "None";
//		 serializableMesh.name = name;
//		 serializableMesh.generateTangentMesh = isTangent;
//		 
//		 ProceduralMeshA* proc = new ProceduralMeshA(device, deviceContext, mesh);
//		 serializableMesh._mesh = proc;
//		 if(serializableMesh.generateTangentMesh)
//		 {
//			 serializableMesh._mesh = new TangentMesh(device, deviceContext, proc);
//		 }
//		 return serializableMesh;
//
//
//
//	}
//
//	static SerializableMesh CustomMesh(std::string name,std::string filepath)
//	{
//		 SerializableMesh serializableMesh;
//		 serializableMesh._type = SerializableMeshType::Custom;
//		 serializableMesh._resolutionParam = 0;
//		 serializableMesh.filepath = filepath;
//		 serializableMesh.name = name;
//
//		 return serializableMesh;
//	}
//	void setManualMesh(BaseMesh* manual) {
//		this->_mesh = manual;
//	}
//
//	static SerializableMesh ShapeMesh(std::string name,SerializableMeshType type,int resParam=10,int size=1)
//	{
//		SerializableMesh serializableMesh;
//		serializableMesh._type = type;
//		serializableMesh._resolutionParam = resParam;
//		serializableMesh._size = size;
//		serializableMesh.name = name;
//		return serializableMesh;
//	}
//
//	BaseMesh* CreateMesh(ID3D12Device* device, ID3D12DeviceContext* deviceContext)
//	{
//		switch (_type)
//		{
//		case Plane:
//			_mesh=new PlaneMesh(device,deviceContext,_resolutionParam);
//			break;
//		case Cube:
//			_mesh = new CubeMesh(device, deviceContext, _resolutionParam);
//			break;
//		case Sphere:
//			_mesh = new SphereMesh(device, deviceContext, _resolutionParam);
//			break;
//		case ProceduralTerrain:
//			_mesh = new Terrain(device, deviceContext);
//			break;
//		case Custom:
//			_mesh = new AModel(device, filepath);
//			break;
//		case TesselationQuad:
//			_mesh = new TessellationQuad(device, deviceContext,_size,_resolutionParam);
//			break;
//		default:
//			break;
//		}
//
//		//TODO create tangent mesh
//
//		if (generateTangentMesh)
//		{
//			_mesh=new TangentMesh(device,deviceContext
//				, _mesh);
//		}
//		return _mesh;
//	}
//
//	BaseMesh* GetMesh() const
//	{
//		return _mesh;
//	}
//	bool HasInitializedMesh() { _mesh != nullptr; }
//
//
//	static bool ByMeshName(SerializableMesh a, SerializableMesh b) 
//	{
//		return a.name < b.name;
//	}
//};
//
