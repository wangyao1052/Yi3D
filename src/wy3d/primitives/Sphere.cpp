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
#include <BRepPrimAPI_MakeSphere.hxx>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSphere.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Sphere)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Sphere, _radius)
END_FIELD_REGISTRATION()

Sphere::Sphere() : wy3d::Primitive(), _radius(0.0)
{
}

Sphere::~Sphere()
{
}

wy::ErrorStatus Sphere::create(wydb::Transaction* pTrans, double radius, Sphere*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }

    Sphere* pSphere = new Sphere();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSphere);
    if (error != wy::ErrorStatus::Ok)
    {
        wydb::deleteElement(pSphere);
        pSphere = nullptr;
        return error;
    }

    error = pSphere->setRadius(radius);
    CHECK_ERROR_FOR_CREATE(error, pSphere)

    pOut = pSphere;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sphere::setRadius(double r)
{
    if (r < wy3d::kMinValue || r > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kSphere_radius);
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


void Sphere::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = Sphere::PARAM_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Sphere::getParameterValue(const std::string& className, const std::string& n) const
{
    if (className == Sphere::classInfo()->className())
    {
        if (Sphere::PARAM_RADIUS == n)
        {
            return wydb::ParameterValue::createDouble(_radius);
        }
        return nullptr;
    }
    return __baseClass::getParameterValue(className, n);
}

wy::ErrorStatus Sphere::setParameterValue(const std::string& className, const std::string& n, const wydb::ParameterValue& v)
{
    if (className == Sphere::classInfo()->className())
    {
        if (Sphere::PARAM_RADIUS == n)
        {
            if (!v.isDouble())
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setRadius(v.asDouble());
        }
        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, n, v);
}

bool Sphere::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSphere_radius.value():
        value = _radius;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Sphere::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSphere_radius.value():
        _radius = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Sphere::writeToFiler(wydb::OutFiler& f) const
{
    __baseClass::writeToFiler(f);
    f << _radius;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sphere::readFromFiler(wydb::InFiler& f)
{
    __baseClass::readFromFiler(f);
    f >> _radius;
    return wy::ErrorStatus::Ok;
}

TopoDS_Shape Sphere::generateOriginalShape() const
{
    BRepPrimAPI_MakeSphere makeSphere(_radius);
    TopoDS_Shape shape = makeSphere.Shape();
    return shape;
}

NS_WY3D_END
