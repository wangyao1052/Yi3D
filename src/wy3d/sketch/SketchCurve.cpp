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
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchParamNames.h>

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SketchCurve)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchCurve, _isConstruction)
END_FIELD_REGISTRATION()

SketchCurve::SketchCurve() : wy3d::SketchEntity(), _isConstruction(false)
{
}

SketchCurve::~SketchCurve()
{
}

wy::ErrorStatus SketchCurve::setConstruction(bool isConstruction)
{
    if (isConstruction == _isConstruction)
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kSketchCurve_isConstruction);
    if (wy::ErrorStatus::Ok == error)
    {
        _isConstruction = isConstruction;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void SketchCurve::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ENTITY_ID;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_CURVE_IS_CONSTRUCTION;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr SketchCurve::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (SketchParamNames::SKETCH_ENTITY_ID == paramName)
        return wydb::ParameterValue::createElementId(getId());
    if (className == SketchCurve::classInfo()->className()) {
        if (SketchParamNames::SKETCH_CURVE_IS_CONSTRUCTION == paramName)
            return wydb::ParameterValue::createBoolean(_isConstruction);
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus SketchCurve::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (SketchParamNames::SKETCH_ENTITY_ID == paramName)
        return wy::ErrorStatus::ParameterReadonly;
    if (className == SketchCurve::classInfo()->className()) {
        if (SketchParamNames::SKETCH_CURVE_IS_CONSTRUCTION == paramName)
        {
            if (!paramValue.isBoolean()) return wy::ErrorStatus::InvalidInput;
            return this->setConstruction(paramValue.asBoolean());
        }
        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchCurve::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchCurve_isConstruction.value():
        value = _isConstruction;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchCurve::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchCurve_isConstruction.value():
        _isConstruction = std::any_cast<bool>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchCurve::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _isConstruction;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchCurve::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _isConstruction;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
