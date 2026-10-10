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
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dDatumPlane.h>
#include <wy3dParamNames.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(DatumPlane)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(DatumPlane, _plane)
    REGISTER_FIELD(DatumPlane, _name)
END_FIELD_REGISTRATION()

DatumPlane::DatumPlane() : wy3d::Datum(), _plane()
{
}

DatumPlane::~DatumPlane()
{
}

wy::ErrorStatus DatumPlane::create(wydb::Transaction* pTrans, const wy3d::Plane& plane, DatumPlane*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }

    DatumPlane* pDatumPlane = new DatumPlane();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pDatumPlane);
    if (error != wy::ErrorStatus::Ok)
    {
        wydb::deleteElement(pDatumPlane);
        pDatumPlane = nullptr;
        return error;
    }

    error = pDatumPlane->setPlane(plane);
    CHECK_ERROR_FOR_CREATE(error, pDatumPlane)

    pOut = pDatumPlane;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus DatumPlane::setPlane(const wy3d::Plane& plane)
{
    if (!plane.isValid())
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (_plane == plane)
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kDatumPlane_plane);
    if (wy::ErrorStatus::Ok == error)
    {
        _plane = plane;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void DatumPlane::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::DATUM_PLANE_PARAM_PLANE;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr DatumPlane::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == DatumPlane::classInfo()->className())
    {
        if (ParamNames::DATUM_PLANE_PARAM_PLANE == paramName)
            return wydb::ParameterValue::createAny(_plane);
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus DatumPlane::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == DatumPlane::classInfo()->className())
    {
        if (ParamNames::DATUM_PLANE_PARAM_PLANE == paramName)
        {
            if (!paramValue.isAny()) return wy::ErrorStatus::InvalidInput;
            const auto* pAnyVal = dynamic_cast<const wydb::AnyParameterValue*>(&paramValue);
            if (!pAnyVal) return wy::ErrorStatus::InvalidInput;
            const auto* pPlane = pAnyVal->tryGet<wy3d::Plane>();
            if (!pPlane) return wy::ErrorStatus::InvalidInput;
            return this->setPlane(*pPlane);
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

wy::ErrorStatus DatumPlane::setName(const std::string& name)
{
    if (name == _name)
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kDatumPlane_name, wydb::ElementDataPieceType::Appearance);
    if (wy::ErrorStatus::Ok == error)
    {
        _name = name;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

bool DatumPlane::getFieldValue(wydb::FieldId fieldId, std::any& v)
{
    switch (fieldId.value())
    {
    case kDatumPlane_plane.value():
        v = _plane;
        return true;
    case kDatumPlane_name.value():
        v = _name;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, v);
        assert(baseRet);
        return baseRet;
    }
}

bool DatumPlane::setFieldValue(wydb::FieldId fieldId, const std::any& v)
{
    switch (fieldId.value())
    {
    case kDatumPlane_plane.value():
        _plane = std::any_cast<const wy3d::Plane&>(v);
        return true;
    case kDatumPlane_name.value():
        _name = std::any_cast<const std::string&>(v);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, v);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus DatumPlane::writeToFiler(wydb::OutFiler& f) const
{
    __baseClass::writeToFiler(f);

    wy::Vector3 planeOrigin = _plane.getOrigin();
    f << planeOrigin.x() << planeOrigin.y() << planeOrigin.z();
    wy::Vector3 planeNormal = _plane.getNormal();
    f << planeNormal.x() << planeNormal.y() << planeNormal.z();
    wy::Vector3 planeXDir = _plane.getXDir();
    f << planeXDir.x() << planeXDir.y() << planeXDir.z();

    f << _name;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus DatumPlane::readFromFiler(wydb::InFiler& f)
{
    __baseClass::readFromFiler(f);

    double x(0.0), y(0.0), z(0.0);
    f >> x >> y >> z;
    wy::Vector3 planeOrigin(x, y, z);
    f >> x >> y >> z;
    wy::Vector3 planeNormal(x, y, z);
    f >> x >> y >> z;
    wy::Vector3 planeXDir(x, y, z);
    _plane = wy3d::Plane(planeOrigin, planeNormal, planeXDir);

    f >> _name;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
