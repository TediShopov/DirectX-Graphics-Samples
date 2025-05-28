#include "Transform.h"
Transform* Transform::transform_default_ = nullptr;
float Transform::getYaw() const { return this->Yaw; }
float Transform::getPitch() const { return this->Pitch; }
float Transform::getRoll() const { return this->Roll; }
XMVECTOR Transform::getPosition() const
{
	return position;
}
XMVECTOR Transform::getScale() const
{
	return scale;
	XMVECTOR vec4;
	
}

XMVECTOR Transform::getOrigin() const { return this->origin; }

XMVECTOR Transform::getGlobalPosition() const
{
	this->update();
	//TODO check if this works with XMVector3Transform
	return XMVector4Transform(XMVectorSet(0, 0, 0, 1),this->getTransformMatrix());
	//return this->getTransformMatrix() * XMVectorSet(0, 0, 0, 1);
}

XMMATRIX Transform::getInverseMatrix()
{
	this->inverseMatrix = XMMatrixInverse(&XMVECTOR(),this->getTransformMatrix());
	//this->inverseMatrix = glm::affineInverse(this->getTransformMatrix());
	//this->inverseMatrix.Inverse(this->getTransformMatrix());
	return this->inverseMatrix;
}

bool Transform::forceUpdate() const
{
	if (toBeUpdated == false)
	{
		return false;
	}
	else
	{
		update();
		return true;
	}
}

void Transform::onNotify(const Transform* entity, TransformChanged event)
{
	this->update();
}





/// <summary>
/// Represent an objects transform in 3d space. Support tree structure of children and parents that expands the
/// openGL stack and allows better organization.
/// Translation, rotation and scale matrices have initial value of identity matrix.
/// Rotations are all 0
/// No parent
/// And no updating is necessary for now
/// </summary>
Transform::Transform() :
	position(XMVectorSet(0, 0, 0, 1)), scale(XMVectorSet(1, 1, 1, 0)), origin(XMVectorSet(0, 0, 0, 0)), Pitch(0), Yaw(0), Roll(0),
	originMatrix(XMMatrixIdentity()),
	translationMatrix(XMMatrixIdentity()),
	rotationMatrix(XMMatrixIdentity()),
	scaleMatrix(XMMatrixIdentity()),
	inverseMatrix(XMMatrixIdentity()),
	parent(nullptr), toBeUpdated(false), silentState(false)
{

}

Transform* Transform::get_default_transform()
{
	if (transform_default_ == nullptr)
	{
		transform_default_ = new Transform();
	}
	return transform_default_;
}


/// <summary>
/// This method makes sure that all the matrices that represent and combine into the transform are updated.
/// Every set method affect a private variable in the class and sets the "toBeUpdated" flag. If this flag is on
/// we update the matrcies accordingly and set the flag to false.
/// This is to prevent mulitple matrix multiplication before even the matrix is needed.
/// For example, objects pitch, yaw and roll could be updated one after another, which will change the rotation matrix
/// 3 times, but with this method it roatation matrix will be computed just once when it is needed.
/// </summary>
void Transform::update() const
{
	if (toBeUpdated)
	{
		//TODO check if origin will be even nendde
		originMatrix = XMMatrixTranslationFromVector(origin);
		//translationMatrix = glm::translate(glm::identity<XMMATRIX>(), glm::vec3(this->position));
		translationMatrix = XMMatrixTranslationFromVector(position);

	//	//rotationMatrix = glm::identity<XMMATRIX>();
	//	//rotationMatrix = XMMatrixIdentity();
	//	XMMATRIX yRotation, xRotation, zRotation;
	//	//Rotate Y
	//	//yRotation = glm::rotate(glm::identity<XMMATRIX>(), glm::radians(this->Yaw), glm::vec3(0, -1, 0));
	//	yRotation = XMMatrixRotationY(XMConvertToRadians(this->Yaw));
	//	//Rotate X
	////	xRotation = glm::rotate(glm::identity<XMMATRIX>(), glm::radians(this->Pitch), glm::vec3(-1, 0, 0));
	//	xRotation = XMMatrixRotationX(XMConvertToRadians(this->Pitch));

	//	//Roate Z
	////	zRotation = glm::rotate(glm::identity<XMMATRIX>(), glm::radians(this->Roll), glm::vec3(0, 0, -1));
	//	zRotation = XMMatrixRotationZ(XMConvertToRadians(this->Roll));

		if(composeRotationFromQuaternions)
		{
			rotationMatrix = XMMatrixRotationQuaternion(quaternion);
			
		}
		else
		{
			rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(this->Roll), XMConvertToRadians(this->Pitch),
			XMConvertToRadians(this->Yaw));
			
		}

		 //rotationMatrix = yRotation * xRotation * zRotation;
		

		//Scale
		//scaleMatrix = glm::scale(glm::identity<XMMATRIX>(), glm::vec3(scale));
		 scaleMatrix = XMMatrixScalingFromVector(scale);
		
		toBeUpdated = false;

		if (!silentState)
		{
			//notify(this, TransformOnChangedEvent());
			for (size_t i = 0; i < this->children.size(); i++)
			{
				this->children[i]->notify(this->children[i], TransformChanged());
			}

		}

	}


}

void Transform::setQuaternion(float x, float y, float z, float w)
{
	quaternion = XMVectorSet(x, y, z, w);
	toBeUpdated = true;
}

void Transform::setComposeRotationFromQuaternions(bool b)
{
	composeRotationFromQuaternions = b;
}

void Transform::setSilent(bool b) { this->update(); this->silentState = b; }

void Transform::translate(float x, float y, float z)
{
	//this->position = XMVectorAdd((XMVECTOR)this->position, (XMVECTOR)XMVECTOR(x, y, z, 0.0f));
	XMVECTOR toAdd = XMVectorSet(x, y, z,0);
	this->position = this->position + toAdd;
	toBeUpdated = true;
}

void Transform::translate(XMVECTOR tranlsation) { this->position = this->position + tranlsation; }

void Transform::setOrigin(XMVECTOR origin)
{
	toBeUpdated = true;
	this->origin = origin;
}

void Transform::setPitch(float degrees) { this->Pitch = degrees; toBeUpdated = true; }
void Transform::setRoll(float degrees) { this->Roll = degrees; 	toBeUpdated = true; }
void Transform::setYaw(float degrees) { this->Yaw = degrees; 	toBeUpdated = true; }



void Transform::setRotation(float pitch, float yaw, float roll)
{
	this->Pitch = pitch;
	this->Yaw = yaw;
	this->Roll = roll;
	toBeUpdated = true;
}



void Transform::setScale(float x, float y, float z)
{
	this->scale = XMVectorSet(x, y, z, 0.0f);
	toBeUpdated = true;
}



/// <summary>
/// Get the transform matrix on the object.
/// </summary>
/// <returns></returns>
XMMATRIX Transform::getTransformMatrix() const
{
	//First we update our matrix if there are any new values in put into Position and Scale vectors, or if we have any new values
	//for Yaw, Pitch or Roll
	this->update();
	
	if (parent == nullptr)
	{
		//If the transform doesnt have parent return TransformM= TranslationM*RotationM*ScaleM
		return scaleMatrix*rotationMatrix*translationMatrix;

	}
	//If the transform does have a parent, pre-multiply it with our resulting  TransformMatrix
	//This is recursive method! So, if this transform's parent has a parent of its own, its matrix will also be used by
	// ... GreatGrandParentMatrix * GrandParentMatrix* ParentMatrix * ThisMatrix;
	//return    scaleMatrix * rotationMatrix * translationMatrix * parent->getTransformMatrix();
	return    rotationMatrix * scaleMatrix * translationMatrix * parent->getTransformMatrix();

}

std::vector<Transform*> Transform::getChildrenTransforms() const
{
	return this->children;
}

void Transform::setPosition(XMVECTOR pos)
{
	this->position = pos;
	toBeUpdated = true;
}

void Transform::setPosition(float x, float y, float z)
{
	this->position = XMVectorSet(x, y, z, 1.0f);
	toBeUpdated = true;
}



void Transform::removeChild(Transform* t)
{
	auto foundChild = std::find(children.begin(), children.end(), t);
	if (foundChild != children.end())
	{
		(*foundChild)->parent = nullptr;
		children.erase(foundChild);
	}
}

void Transform::addChild(Transform* t)
{
	this->children.push_back(t);
	t->parent = this;
}

void Transform::setParent(Transform* t)
{
	t->addChild(this);
}

//void Transform::setDirection(XMFLOAT4X4 dir)
//{
//	//YAW
//	float ya = 0, sq = 0, pa = 0;
//
//	sq = sqrt(dir.z * dir.z + dir.x * dir.x);
//	if (sq > 0)
//	{
//		XMConvertToDegrees(XMScalarACos())
//		ya = glm::degrees(acos(dir.z / sqrt(dir.z * dir.z + dir.x * dir.x)));
//	}
//
//
//
//	//PITCH
//	if (glm::length(dir) != 0)
//	{
//		pa = glm::degrees(asin(dir.y / glm::length(dir)));
//	}
//
//
//
//	if (dir.x > 0)
//	{
//		this->setRotation(pa, -ya, 0);
//	}
//	else
//	{
//		this->setRotation(pa, ya, 0);
//	}
//}


//https://stackoverflow.com/questions/60350349/directx-get-pitch-yaw-roll-from-xmmatrix
void Transform::extractYawPitchRoll(XMMATRIX matrix)
{
	//TODO test if this works properly
	XMFLOAT4X4 m_transform;
	XMStoreFloat4x4(&m_transform,matrix);
	this->Pitch = DirectX::XMScalarASin(-m_transform._32);

	DirectX::XMVECTOR from(DirectX::XMVectorSet(m_transform._12, m_transform._31, 0.0f, 0.0f));
	DirectX::XMVECTOR to(DirectX::XMVectorSet(m_transform._22, m_transform._33, 0.0f, 0.0f));
	DirectX::XMVECTOR res(DirectX::XMVectorATan2(from, to));

	this->Roll = DirectX::XMVectorGetX(res);
	this->Yaw = DirectX::XMVectorGetY(res);

}
void Transform::lookAt(XMVECTOR pos)
{

	//Make position local
	if (parent != nullptr)
	{
		//TODO check if this should be reversed
		pos = XMVector3Transform(pos, this->parent->getInverseMatrix());
		//pos = pos.Transform(this->parent->getInverseMatrix());
	}
	//Get the vector pointing to the desired position
	XMVECTOR dir = pos - this->getPosition();
	//Set the transform direction to this direction
	//TODO check if RH shoudl be used

	this->extractYawPitchRoll(XMMatrixLookAtLH(this->position, pos, XMVectorSet(0, 0, 1, 0)));
	//this->setDirection(dir);
}

