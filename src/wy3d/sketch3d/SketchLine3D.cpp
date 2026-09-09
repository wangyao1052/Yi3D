///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketch3DParamNames.h>
#include "utils/Util.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(SketchLine3D)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchLine3D, _startPnt)
    REGISTER_FIELD(SketchLine3D, _endPnt)
END_FIELD_REGISTRATION()

SketchLine3D::SketchLine3D() : wy3d::SketchCurve3D(), _startPnt(), _endPnt()
{
}

SketchLine3D::~SketchLine3D()
{
}

wy::ErrorStatus SketchLine3D::create(
    wydb::Transaction* pTrans,
    const wy::Vector3& startPnt,
    const wy::Vector3& endPnt,
    SketchLine3D*& pOut)
{
    pOut = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;

    SketchLine3D* pSketchLine3D = new SketchLine3D();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchLine3D);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSketchLine3D);
        return error;
    }

    error = pSketchLine3D->setStartPoint(startPnt);
    CHECK_ERROR_FOR_CREATE(error, pSketchLine3D)
    error = pSketchLine3D->setEndPoint(endPnt);
    CHECK_ERROR_FOR_CREATE(error, pSketchLine3D)

    pOut = pSketchLine3D;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchLine3D::setStartPoint(const wy::Vector3& startPnt)
{
    if (startPnt == _startPnt) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchLine3D_startPnt);
    if (wy::ErrorStatus::Ok == error)
    {
        _startPnt = startPnt;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchLine3D::setEndPoint(const wy::Vector3& endPnt)
{
    if (endPnt == _endPnt) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchLine3D_endPnt);
    if (wy::ErrorStatus::Ok == error)
    {
        _endPnt = endPnt;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::Vector3 SketchLine3D::getPointAt(double t, bool clamp) const
{
    if (clamp)
    {
        if (t < 0.0) t = 0.0;
        else if (t > 1.0) t = 1.0;
    }
    return _startPnt + (_endPnt - _startPnt) * t;
}

wy::Vector3 SketchLine3D::getDirectionAt(double t, bool clamp) const
{
    wy::Vector3 dir = _endPnt - _startPnt;
    dir.normalize();
    return dir;
}

bool SketchLine3D::isDegenerate(double tol) const
{
    return this->getLength() < tol;
}

void SketchLine3D::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_Z;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_Z;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_LINE3D_PARAM_LENGTH;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr SketchLine3D::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == SketchLine3D::classInfo()->className()) {
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_X == paramName)
            return wydb::ParameterValue::createDouble(_startPnt.x());
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_Y == paramName)
            return wydb::ParameterValue::createDouble(_startPnt.y());
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_Z == paramName)
            return wydb::ParameterValue::createDouble(_startPnt.z());
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_X == paramName)
            return wydb::ParameterValue::createDouble(_endPnt.x());
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_Y == paramName)
            return wydb::ParameterValue::createDouble(_endPnt.y());
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_Z == paramName)
            return wydb::ParameterValue::createDouble(_endPnt.z());
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_LENGTH == paramName)
            return wydb::ParameterValue::createDouble((_endPnt - _startPnt).length());
        return nullptr;
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus SketchLine3D::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == SketchLine3D::classInfo()->className()) {
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setStartPoint(wy::Vector3(paramValue.asDouble(), _startPnt.y(), _startPnt.z())); }
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setStartPoint(wy::Vector3(_startPnt.x(), paramValue.asDouble(), _startPnt.z())); }
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_START_Z == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setStartPoint(wy::Vector3(_startPnt.x(), _startPnt.y(), paramValue.asDouble())); }
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setEndPoint(wy::Vector3(paramValue.asDouble(), _endPnt.y(), _endPnt.z())); }
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setEndPoint(wy::Vector3(_endPnt.x(), paramValue.asDouble(), _endPnt.z())); }
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_END_Z == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setEndPoint(wy::Vector3(_endPnt.x(), _endPnt.y(), paramValue.asDouble())); }
        if (Sketch3DParamNames::SKETCH_LINE3D_PARAM_LENGTH == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            double length = paramValue.asDouble();
            if (length <= 0.0) return wy::ErrorStatus::InvalidInput;
            wy::Vector3 dir = _endPnt - _startPnt; dir.normalize();
            if (dir.length() < 0.5) dir.set(1.0, 0.0, 0.0);
            return this->setEndPoint(_startPnt + length * dir);
        }
        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchLine3D::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchLine3D_startPnt.value():
        value = _startPnt;
        return true;
    case kSketchLine3D_endPnt.value():
        value = _endPnt;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchLine3D::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchLine3D_startPnt.value():
        _startPnt = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kSketchLine3D_endPnt.value():
        _endPnt = std::any_cast<const wy::Vector3&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchLine3D::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _startPnt << _endPnt;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchLine3D::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _startPnt >> _endPnt;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
