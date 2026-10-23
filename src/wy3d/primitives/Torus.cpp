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
#include <BRepPrimAPI_MakeTorus.hxx>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dTorus.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Torus)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Torus, _majorRadius)
    REGISTER_FIELD(Torus, _minorRadius)
END_FIELD_REGISTRATION()

Torus::Torus() : wy3d::Primitive(), _majorRadius(0.0), _minorRadius(0.0)
{
}

Torus::~Torus()
{
}

wy::ErrorStatus Torus::create(wydb::Transaction* pTrans, double majorRadius, double minorRadius, Torus*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }

    Torus* pTorus = new Torus();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pTorus);
    if (error != wy::ErrorStatus::Ok)
    {
        wydb::deleteElement(pTorus);
        pTorus = nullptr;
        return error;
    }

    error = pTorus->setMajorRadius(majorRadius);
    CHECK_ERROR_FOR_CREATE(error, pTorus)
    error = pTorus->setMinorRadius(minorRadius);
    CHECK_ERROR_FOR_CREATE(error, pTorus)

    pOut = pTorus;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Torus::setMajorRadius(double r)
{
    if (r < wy3d::kMinValue || r > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (r < _minorRadius)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kTorus_majorRadius);
    if (wy::ErrorStatus::Ok == error)
    {
        _majorRadius = r;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Torus::setMinorRadius(double r)
{
    if (r < wy3d::kMinValue || r > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (r > _majorRadius)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kTorus_minorRadius);
    if (wy::ErrorStatus::Ok == error)
    {
        _minorRadius = r;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void Torus::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = Torus::PARAM_MAJOR_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Torus::PARAM_MINOR_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Torus::getParameterValue(const std::string& className, const std::string& n) const
{
    if (className == Torus::classInfo()->className())
    {
        if (Torus::PARAM_MAJOR_RADIUS == n)
        {
            return wydb::ParameterValue::createDouble(_majorRadius);
        }
        else if (Torus::PARAM_MINOR_RADIUS == n)
        {
            return wydb::ParameterValue::createDouble(_minorRadius);
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, n);
}

wy::ErrorStatus Torus::setParameterValue(const std::string& className, const std::string& n, const wydb::ParameterValue& v)
{
    if (className == Torus::classInfo()->className())
    {
        if (!v.isDouble())
        {
            return wy::ErrorStatus::InvalidInput;
        }

        double d = v.asDouble();
        if (Torus::PARAM_MAJOR_RADIUS == n)
        {
            return this->setMajorRadius(d);
        }
        else if (Torus::PARAM_MINOR_RADIUS == n)
        {
            return this->setMinorRadius(d);
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, n, v);
}

bool Torus::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kTorus_majorRadius.value():
        value = _majorRadius;
        return true;
    case kTorus_minorRadius.value():
        value = _minorRadius;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Torus::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kTorus_majorRadius.value():
        _majorRadius = std::any_cast<double>(value);
        return true;
    case kTorus_minorRadius.value():
        _minorRadius = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Torus::writeToFiler(wydb::OutFiler& f) const
{
    __baseClass::writeToFiler(f);
    f << _majorRadius << _minorRadius;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Torus::readFromFiler(wydb::InFiler& f)
{
    __baseClass::readFromFiler(f);
    f >> _majorRadius >> _minorRadius;
    return wy::ErrorStatus::Ok;
}

TopoDS_Shape Torus::generateOriginalShape() const
{
    BRepPrimAPI_MakeTorus makeTorus(_majorRadius, _minorRadius);
    TopoDS_Shape shape = makeTorus.Shape();
    return shape;
}

NS_WY3D_END
