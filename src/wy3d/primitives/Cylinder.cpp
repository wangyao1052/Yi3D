///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dCylinder.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Cylinder)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Cylinder, _radius)
    REGISTER_FIELD(Cylinder, _height)
END_FIELD_REGISTRATION()

Cylinder::Cylinder() : wy3d::Primitive(), _radius(0.0), _height(0.0)
{
}

Cylinder::~Cylinder()
{
}

wy::ErrorStatus Cylinder::create(wydb::Transaction* pTrans, double radius, double height, Cylinder*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }

    Cylinder* pCylinder = new Cylinder();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pCylinder);
    if (error != wy::ErrorStatus::Ok)
    {
        wydb::deleteElement(pCylinder);
        pCylinder = nullptr;
        return error;
    }

    error = pCylinder->setRadius(radius);
    CHECK_ERROR_FOR_CREATE(error, pCylinder)
    error = pCylinder->setHeight(height);
    CHECK_ERROR_FOR_CREATE(error, pCylinder)

    pOut = pCylinder;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Cylinder::setRadius(double r)
{
    if (r < wy3d::kMinValue || r > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kCylinder_radius);
    if (wy::ErrorStatus::Ok == error)
    {
        _radius = r;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Cylinder::setHeight(double h)
{
    if (h < wy3d::kMinValue || h > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kCylinder_height);
    if (wy::ErrorStatus::Ok == error)
    {
        _height = h;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void Cylinder::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = Cylinder::PARAM_HEIGHT;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Cylinder::PARAM_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Cylinder::getParameterValue(const std::string& className, const std::string& n) const
{
    if (className == Cylinder::classInfo()->className())
    {
        if (Cylinder::PARAM_RADIUS == n)
        {
            return wydb::ParameterValue::createDouble(_radius);
        }
        else if (Cylinder::PARAM_HEIGHT == n)
        {
            return wydb::ParameterValue::createDouble(_height);
        }
    }
    return __baseClass::getParameterValue(className, n);
}

wy::ErrorStatus Cylinder::setParameterValue(const std::string& className, const std::string& n, const wydb::ParameterValue& v)
{
    if (className == Cylinder::classInfo()->className())
    {
        if (Cylinder::PARAM_RADIUS == n)
        {
            if (!v.isDouble())
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setRadius(v.asDouble());
        }
        else if (Cylinder::PARAM_HEIGHT == n)
        {
            if (!v.isDouble())
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setHeight(v.asDouble());
        }
    }
    return __baseClass::setParameterValue(className, n, v);
}

bool Cylinder::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kCylinder_radius.value():
        value = _radius;
        return true;
    case kCylinder_height.value():
        value = _height;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Cylinder::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kCylinder_radius.value():
        _radius = std::any_cast<double>(value);
        return true;
    case kCylinder_height.value():
        _height = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Cylinder::writeToFiler(wydb::OutFiler& f) const
{
    __baseClass::writeToFiler(f);
    f << _radius << _height;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Cylinder::readFromFiler(wydb::InFiler& f)
{
    __baseClass::readFromFiler(f);
    f >> _radius >> _height;
    return wy::ErrorStatus::Ok;
}

TopoDS_Shape Cylinder::generateOriginalShape() const
{
    BRepPrimAPI_MakeCylinder makeCylinder(_radius, _height);
    TopoDS_Shape shape = makeCylinder.Shape();
    return shape;
}

NS_WY3D_END
