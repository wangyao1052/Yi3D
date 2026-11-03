///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <wyVector3.h>
#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchPoint3D.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SketchPoint3D)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchPoint3D, _position)
END_FIELD_REGISTRATION()

SketchPoint3D::SketchPoint3D() : wy3d::SketchEntity3D(), _position()
{
}

SketchPoint3D::~SketchPoint3D()
{
}

wy::ErrorStatus SketchPoint3D::create(wydb::Transaction* pTrans, const wy::Vector3& position, SketchPoint3D*& pOut)
{
    pOut = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;

    SketchPoint3D* pSketchPoint3D = new SketchPoint3D();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchPoint3D);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSketchPoint3D);
        return error;
    }

    error = pSketchPoint3D->setPosition(position);
    CHECK_ERROR_FOR_CREATE(error, pSketchPoint3D)

    pOut = pSketchPoint3D;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchPoint3D::setPosition(const wy::Vector3& position)
{
    if (position == _position) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchPoint3D_position);
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

void SketchPoint3D::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchPoint3D::PARAM_ID;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchPoint3D::PARAM_POSITION_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchPoint3D::PARAM_POSITION_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchPoint3D::PARAM_POSITION_Z;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr SketchPoint3D::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (SketchPoint3D::PARAM_ID == paramName)
        return wydb::ParameterValue::createElementId(getId());
    if (className == SketchPoint3D::classInfo()->className()) {
        if (SketchPoint3D::PARAM_POSITION_X == paramName) return wydb::ParameterValue::createDouble(_position.x());
        if (SketchPoint3D::PARAM_POSITION_Y == paramName) return wydb::ParameterValue::createDouble(_position.y());
        if (SketchPoint3D::PARAM_POSITION_Z == paramName) return wydb::ParameterValue::createDouble(_position.z());
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus SketchPoint3D::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (SketchPoint3D::PARAM_ID == paramName)
        return wy::ErrorStatus::ParameterReadonly;
    if (className == SketchPoint3D::classInfo()->className()) {
        if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
        double d = paramValue.asDouble();
        if (SketchPoint3D::PARAM_POSITION_X == paramName) return this->setPosition(wy::Vector3(d, _position.y(), _position.z()));
        if (SketchPoint3D::PARAM_POSITION_Y == paramName) return this->setPosition(wy::Vector3(_position.x(), d, _position.z()));
        if (SketchPoint3D::PARAM_POSITION_Z == paramName) return this->setPosition(wy::Vector3(_position.x(), _position.y(), d));
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchPoint3D::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchPoint3D_position.value():
        value = _position;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchPoint3D::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchPoint3D_position.value():
        _position = std::any_cast<const wy::Vector3&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchPoint3D::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _position;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchPoint3D::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _position;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
