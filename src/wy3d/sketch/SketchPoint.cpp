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
#include <wyVector2.h>
#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchPoint.h>
#include <wy3dImpl.h>
#include <wy3dSketchParamNames.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SketchPoint)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchPoint, _position)
END_FIELD_REGISTRATION()

SketchPoint::SketchPoint() : wy3d::SketchEntity(), _position()
{
}

SketchPoint::~SketchPoint()
{
}

wy::ErrorStatus SketchPoint::create(wydb::Transaction* pTrans, const wy::Vector2& position, SketchPoint*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }

    SketchPoint* pSketchPoint = new SketchPoint();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchPoint);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSketchPoint); pSketchPoint = nullptr; return error; }

    error = pSketchPoint->setPosition(position); CHECK_ERROR_FOR_CREATE(error, pSketchPoint)

    pOut = pSketchPoint;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchPoint::setPosition(const wy::Vector2& position)
{
    if (position == _position) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchPoint_position);
    if (wy::ErrorStatus::Ok == error)
    {
        _position = position;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchPoint::rotateAround(const wy::Vector2& center, double angle)
{
    if (angle == 0.0) return wy::ErrorStatus::Ok;
    double cosTheta = std::cos(angle);
    double sinTheta = std::sin(angle);
    wy::Vector2 newPosition = SketchEntity::rotateAround(_position, center, cosTheta, sinTheta);
    return this->setPosition(newPosition);
}


void SketchPoint::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ENTITY_ID;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_POINT_PARAM_POSITION_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_POINT_PARAM_POSITION_Y;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr SketchPoint::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (SketchParamNames::SKETCH_ENTITY_ID == paramName)
        return wydb::ParameterValue::createElementId(getId());
    if (className == SketchPoint::classInfo()->className()) {
        if (SketchParamNames::SKETCH_POINT_PARAM_POSITION_X == paramName) return wydb::ParameterValue::createDouble(_position.x());
        if (SketchParamNames::SKETCH_POINT_PARAM_POSITION_Y == paramName) return wydb::ParameterValue::createDouble(_position.y());
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus SketchPoint::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (SketchParamNames::SKETCH_ENTITY_ID == paramName)
        return wy::ErrorStatus::ParameterReadonly;
    if (className == SketchPoint::classInfo()->className()) {
        if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
        double d = paramValue.asDouble();
        if (SketchParamNames::SKETCH_POINT_PARAM_POSITION_X == paramName) return this->setPosition(wy::Vector2(d, _position.y()));
        if (SketchParamNames::SKETCH_POINT_PARAM_POSITION_Y == paramName) return this->setPosition(wy::Vector2(_position.x(), d));
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchPoint::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchPoint_position.value():
        value = _position;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchPoint::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchPoint_position.value():
        _position = std::any_cast<const wy::Vector2&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchPoint::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _position;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchPoint::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _position;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
