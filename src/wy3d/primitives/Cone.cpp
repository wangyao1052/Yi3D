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
#include <BRepPrimAPI_MakeCone.hxx>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dCone.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Cone)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Cone, _radius)
    REGISTER_FIELD(Cone, _height)
END_FIELD_REGISTRATION()

Cone::Cone() : wy3d::Primitive(), _radius(0.0), _height(0.0)
{
}

Cone::~Cone()
{
}

wy::ErrorStatus Cone::create(wydb::Transaction* pTrans, double radius, double height, Cone*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }

    Cone* pCone = new Cone();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pCone);
    if (error != wy::ErrorStatus::Ok)
    {
        wydb::deleteElement(pCone);
        pCone = nullptr;
        return error;
    }

    error = pCone->setRadius(radius);
    CHECK_ERROR_FOR_CREATE(error, pCone)
    error = pCone->setHeight(height);
    CHECK_ERROR_FOR_CREATE(error, pCone)

    pOut = pCone;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Cone::setRadius(double r)
{
    if (r < wy3d::kMinValue || r > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kCone_radius);
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

wy::ErrorStatus Cone::setHeight(double h)
{
    if (h < wy3d::kMinValue || h > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kCone_height);
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


void Cone::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = Cone::PARAM_HEIGHT;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Cone::PARAM_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Cone::getParameterValue(const std::string& className, const std::string& n) const
{
    if (className == Cone::classInfo()->className())
    {
        if (Cone::PARAM_RADIUS == n)
        {
            return wydb::ParameterValue::createDouble(_radius);
        }
        else if (Cone::PARAM_HEIGHT == n)
        {
            return wydb::ParameterValue::createDouble(_height);
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, n);
}

wy::ErrorStatus Cone::setParameterValue(const std::string& className, const std::string& n, const wydb::ParameterValue& v)
{
    if (className == Cone::classInfo()->className())
    {
        if (!v.isDouble())
        {
            return wy::ErrorStatus::InvalidInput;
        }

        double d = v.asDouble();
        if (Cone::PARAM_RADIUS == n)
        {
            return this->setRadius(d);
        }
        else if (Cone::PARAM_HEIGHT == n)
        {
            return this->setHeight(d);
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, n, v);
}

bool Cone::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kCone_radius.value():
        value = _radius;
        return true;
    case kCone_height.value():
        value = _height;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Cone::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kCone_radius.value():
        _radius = std::any_cast<double>(value);
        return true;
    case kCone_height.value():
        _height = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Cone::writeToFiler(wydb::OutFiler& f) const
{
    __baseClass::writeToFiler(f);
    f << _radius << _height;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Cone::readFromFiler(wydb::InFiler& f)
{
    __baseClass::readFromFiler(f);
    f >> _radius >> _height;
    return wy::ErrorStatus::Ok;
}

TopoDS_Shape Cone::generateOriginalShape() const
{
    BRepPrimAPI_MakeCone makeCone(_radius, 0.0, _height);
    TopoDS_Shape shape = makeCone.Shape();
    return shape;
}

NS_WY3D_END
