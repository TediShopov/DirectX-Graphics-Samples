#pragma once
#include <vector>
#include "DirectXMath.h"
//#include "DXF.h"



#include "Subject.h"
#include "Observer.h"

#define VEC3_COMPONENTS(vec) vec.x, vec.y, vec.z
#define VEC4_COMPONENTS(vec) vec.x, vec.y, vec.z, vec.w

using namespace DirectX;
struct TransformChanged
{
	int id = 0;
};
class Transform : Observer<Transform, TransformChanged>, Subject<Transform, TransformChanged>
{
private:


	static Transform* transform_default_;
	//Value to check if matrices should be updated appropriately with position and scale vectors and with Roll,Pitch,Yaw
	//Used to avoid unecessary matrix multiplication before transform being used
	mutable bool toBeUpdated;
	mutable bool silentState;
	float Roll;
	float Pitch;
	float Yaw;

	//TODO Ensure that XMVECTOR and XMMATRIX variables are  aligned to 16 bytes correctly. CHeck on windows x86
	XMVECTOR origin;
	XMVECTOR position;
	XMVECTOR scale;
	
	mutable XMMATRIX originMatrix;
	mutable XMMATRIX translationMatrix;
	mutable XMMATRIX rotationMatrix;
	mutable XMMATRIX scaleMatrix;
	mutable XMMATRIX inverseMatrix;

	std::vector<Transform*> children;

	void update() const;
	bool composeRotationFromQuaternions = false;
	XMVECTOR quaternion;

public:
	void setQuaternion(float x, float y, float z, float w);

	void setComposeRotationFromQuaternions(bool b);

	Transform* parent;


	float getYaw() const;
	float getPitch() const;
	float getRoll() const;

	XMVECTOR getPosition() const;
	XMVECTOR getScale() const;
	XMVECTOR getOrigin() const;


	XMVECTOR getGlobalPosition() const;
	XMMATRIX getInverseMatrix();
	XMMATRIX getTransformMatrix() const;
	std::vector< Transform*> getChildrenTransforms() const;

	void setPosition(XMVECTOR pos);
	void setPosition(float x, float y, float z);

	Transform();
	static Transform* get_default_transform();


	void removeChild(Transform* t);
	void addChild(Transform* t);
	void setParent(Transform* t);

	void setYaw(float v);
	void setPitch(float v);
	void setRoll(float v);


	void setSilent(bool b);

	void translate(float x, float y, float z);
	void translate(XMVECTOR tranlsation);
	void setOrigin(XMVECTOR origin);


	void setRotation(float pitch, float yaw, float roll);
	void setScale(float x, float y, float z);

	//void setDirection(XMFLOAT4X4 dir);
	void extractYawPitchRoll(XMMATRIX matrix);

	void lookAt(XMVECTOR pos);

	bool forceUpdate() const;
	void onNotify(const Transform* entity, TransformChanged event) override;
};

