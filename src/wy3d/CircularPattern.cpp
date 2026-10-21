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

#include <gp_Ax1.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dCircularPattern.h>
#include <wy3dSolid.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "topo/TopoNamingUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(CircularPattern)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(CircularPattern, _centerPoint)
    REGISTER_FIELD(CircularPattern, _axisDirection)
    REGISTER_FIELD(CircularPattern, _totalAngle)
    REGISTER_FIELD(CircularPattern, _instanceCount)
    REGISTER_FIELD(CircularPattern, _isClockWise)
END_FIELD_REGISTRATION()

CircularPattern::CircularPattern() : wy3d::Pattern(),
    _centerPoint(wy::Vector3::kZero), _axisDirection(wy::Vector3::kZAxis),
    _totalAngle(wy3d::TWO_PI), _instanceCount(1), _isClockWise(false)
{
}

CircularPattern::~CircularPattern()
{
}

wy::ErrorStatus CircularPattern::create(
    wydb::Transaction* pTrans,
    wy3d::Solid* pOwner, const wy3d::Solid* pSource,
    const wy::Vector3& centerPoint, const wy::Vector3& axisDirection,
    double totalAngle, std::uint32_t instanceCount, bool isClockWise,
    CircularPattern*& pOutPattern)
{
    if (!pOwner || !pSource) { pOutPattern = nullptr; return wy::ErrorStatus::NullElementPointer; }
    if (pSource->getParent() != pOwner->getId() || pSource->getParent().isNull())
    { pOutPattern = nullptr; return wy::ErrorStatus::InvalidInput; }

    CircularPattern* pPattern = new CircularPattern();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pPattern);
    if (wy::ErrorStatus::Ok != error) { wydb::deleteElement(pPattern); pPattern = nullptr; return error; }

    error = pPattern->_setSource(pSource); CHECK_ERROR_FOR_CREATE(error, pPattern);
    error = pPattern->setCenterPoint(centerPoint); CHECK_ERROR_FOR_CREATE(error, pPattern);
    error = pPattern->setAxisDirection(axisDirection); CHECK_ERROR_FOR_CREATE(error, pPattern);
    error = pPattern->setTotalAngle(totalAngle); CHECK_ERROR_FOR_CREATE(error, pPattern);
    error = pPattern->setInstanceCount(instanceCount); CHECK_ERROR_FOR_CREATE(error, pPattern);
    error = pPattern->setClockWise(isClockWise); CHECK_ERROR_FOR_CREATE(error, pPattern);

    error = pOwner->addModification(pPattern); CHECK_ERROR_FOR_CREATE(error, pPattern);

    pOutPattern = pPattern;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus CircularPattern::setCenterPoint(const wy::Vector3& centerPoint)
{
    if (centerPoint == _centerPoint) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kCircularPattern_centerPoint);
    if (wy::ErrorStatus::Ok == error)
    {
        _centerPoint = centerPoint;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus CircularPattern::setAxisDirection(const wy::Vector3& axisDirection)
{
    if (axisDirection.length() <= wy3d::EPS) return wy::ErrorStatus::InvalidInput;
    if (axisDirection == _axisDirection) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kCircularPattern_axisDirection);
    if (wy::ErrorStatus::Ok == error)
    {
        _axisDirection = axisDirection;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus CircularPattern::setTotalAngle(double totalAngle)
{
    if (totalAngle <= wy3d::TOL) return wy::ErrorStatus::InvalidInput;
    if (totalAngle > wy3d::TWO_PI) totalAngle = wy3d::TWO_PI;
    if (totalAngle == _totalAngle) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kCircularPattern_totalAngle);
    if (wy::ErrorStatus::Ok == error)
    {
        _totalAngle = totalAngle;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus CircularPattern::setInstanceCount(std::uint32_t instanceCount)
{
    if (instanceCount == 0 || instanceCount > wy3d::kMaxCircularPatternCount) return wy::ErrorStatus::InvalidInput;
    if (instanceCount == _instanceCount) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kCircularPattern_instanceCount);
    if (wy::ErrorStatus::Ok == error)
    {
        _instanceCount = instanceCount;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus CircularPattern::setClockWise(bool value)
{
    if (_isClockWise == value) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kCircularPattern_isClockWise);
    if (wy::ErrorStatus::Ok == error)
    {
        _isClockWise = value;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void CircularPattern::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = CircularPattern::PARAM_AXIS_DIRECTION_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = CircularPattern::PARAM_AXIS_DIRECTION_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = CircularPattern::PARAM_AXIS_DIRECTION_Z;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = CircularPattern::PARAM_CENTER_POINT_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = CircularPattern::PARAM_CENTER_POINT_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = CircularPattern::PARAM_CENTER_POINT_Z;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = CircularPattern::PARAM_INSTANCE_COUNT;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = CircularPattern::PARAM_IS_CLOCKWISE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = CircularPattern::PARAM_TOTAL_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr CircularPattern::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == CircularPattern::classInfo()->className()) {
        if (CircularPattern::PARAM_CENTER_POINT_X == paramName)
            return wydb::ParameterValue::createDouble(_centerPoint.x());
        if (CircularPattern::PARAM_CENTER_POINT_Y == paramName)
            return wydb::ParameterValue::createDouble(_centerPoint.y());
        if (CircularPattern::PARAM_CENTER_POINT_Z == paramName)
            return wydb::ParameterValue::createDouble(_centerPoint.z());
        if (CircularPattern::PARAM_AXIS_DIRECTION_X == paramName)
            return wydb::ParameterValue::createDouble(_axisDirection.x());
        if (CircularPattern::PARAM_AXIS_DIRECTION_Y == paramName)
            return wydb::ParameterValue::createDouble(_axisDirection.y());
        if (CircularPattern::PARAM_AXIS_DIRECTION_Z == paramName)
            return wydb::ParameterValue::createDouble(_axisDirection.z());
        if (CircularPattern::PARAM_TOTAL_ANGLE == paramName)
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_totalAngle));
        if (CircularPattern::PARAM_INSTANCE_COUNT == paramName)
            return wydb::ParameterValue::createInteger(_instanceCount);
        if (CircularPattern::PARAM_IS_CLOCKWISE == paramName)
            return wydb::ParameterValue::createBoolean(_isClockWise);
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus CircularPattern::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == CircularPattern::classInfo()->className()) {
        if (CircularPattern::PARAM_CENTER_POINT_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setCenterPoint(wy::Vector3(paramValue.asDouble(), _centerPoint.y(), _centerPoint.z())); }
        if (CircularPattern::PARAM_CENTER_POINT_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setCenterPoint(wy::Vector3(_centerPoint.x(), paramValue.asDouble(), _centerPoint.z())); }
        if (CircularPattern::PARAM_CENTER_POINT_Z == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setCenterPoint(wy::Vector3(_centerPoint.x(), _centerPoint.y(), paramValue.asDouble())); }
        if (CircularPattern::PARAM_AXIS_DIRECTION_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setAxisDirection(wy::Vector3(paramValue.asDouble(), _axisDirection.y(), _axisDirection.z())); }
        if (CircularPattern::PARAM_AXIS_DIRECTION_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setAxisDirection(wy::Vector3(_axisDirection.x(), paramValue.asDouble(), _axisDirection.z())); }
        if (CircularPattern::PARAM_AXIS_DIRECTION_Z == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setAxisDirection(wy::Vector3(_axisDirection.x(), _axisDirection.y(), paramValue.asDouble())); }
        if (CircularPattern::PARAM_TOTAL_ANGLE == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setTotalAngle(wy3d::degreesToRadians(paramValue.asDouble())); }
        if (CircularPattern::PARAM_INSTANCE_COUNT == paramName)
        { if (!paramValue.isInteger()) return wy::ErrorStatus::InvalidInput; int c = paramValue.asInteger(); if (c <= 0) return wy::ErrorStatus::InvalidInput; return setInstanceCount(c); }
        if (CircularPattern::PARAM_IS_CLOCKWISE == paramName)
        { if (!paramValue.isBoolean()) return wy::ErrorStatus::InvalidInput; return setClockWise(paramValue.asBoolean()); }

        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool CircularPattern::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kCircularPattern_centerPoint.value():
        value = _centerPoint;
        return true;
    case kCircularPattern_axisDirection.value():
        value = _axisDirection;
        return true;
    case kCircularPattern_totalAngle.value():
        value = _totalAngle;
        return true;
    case kCircularPattern_instanceCount.value():
        value = _instanceCount;
        return true;
    case kCircularPattern_isClockWise.value():
        value = _isClockWise;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool CircularPattern::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kCircularPattern_centerPoint.value():
        _centerPoint = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kCircularPattern_axisDirection.value():
        _axisDirection = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kCircularPattern_totalAngle.value():
        _totalAngle = std::any_cast<double>(value);
        return true;
    case kCircularPattern_instanceCount.value():
        _instanceCount = std::any_cast<std::uint32_t>(value);
        return true;
    case kCircularPattern_isClockWise.value():
        _isClockWise = std::any_cast<bool>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus CircularPattern::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _centerPoint << _axisDirection << _totalAngle << _instanceCount << _isClockWise;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus CircularPattern::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _centerPoint >> _axisDirection >> _totalAngle >> _instanceCount >> _isClockWise;
    return wy::ErrorStatus::Ok;
}

std::pair<bool, TopoDS_Shape> CircularPattern::modifyOwnerShapeImpl(
    const TopoDS_Shape& shape, TopoNaming* pTopoNaming,
    const TopoDS_Shape& sourceShape, const wy3d::TopoNaming* pSourceNaming,
    bool isCut,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    assert(pSourceNaming);

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    TopoDS_Shape retShape = shape;
    unsigned int instanceCount = (0 == _instanceCount) ? 1 : _instanceCount;
    if (instanceCount <= 1)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(wy3d::ErrorCode::warnTOPOSHAPE_NullShape));
        return std::pair<bool, TopoDS_Shape>(false, retShape);
    }
    if (instanceCount > wy3d::kMaxCircularPatternCount)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(wy3d::ErrorCode::ELEMENT_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, retShape);
    }

    if (_totalAngle <= 0.0)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(wy3d::ErrorCode::ELEMENT_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, retShape);
    }
    if (_totalAngle <= wy3d::TOL)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(wy3d::ErrorCode::warnTOPOSHAPE_NullShape));
        return std::pair<bool, TopoDS_Shape>(false, retShape);
    }

    double angleDelta;
    if (_totalAngle >= wy3d::TWO_PI - wy3d::TOL)
    {
        angleDelta = wy3d::TWO_PI / instanceCount;
    }
    else
    {
        angleDelta = _totalAngle / (instanceCount - 1);
    }
    if (_isClockWise) angleDelta = -angleDelta;

    bool success(true);
    gp_Pnt centerPnt(_centerPoint.x(), _centerPoint.y(), _centerPoint.z());

    for (unsigned int i = 2; i <= instanceCount; ++i)
    {
        gp_Trsf trsf;
        try
        {
            gp_Ax1 ax1(centerPnt, gp_Dir(_axisDirection.x(), _axisDirection.y(), _axisDirection.z()));
            trsf.SetRotation(ax1, (i - 1) * angleDelta);
        }
        catch (const Standard_Failure&)
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::ELEMENT_InvalidData));
            success = false;
            break;
        }

        BRepBuilderAPI_Transform transformer(sourceShape, trsf);
        TopoDS_Shape instShape = transformer.Shape();

        std::vector<std::uint32_t> suffix;
        suffix.emplace_back(this->getId().value());
        suffix.emplace_back(i);
        TopoNamingSPtr pInstTopoNaming = std::make_shared<TopoNaming>();
        bool namingRet = TopoNamingUtil::patternNaming(sourceShape, *pSourceNaming,
            suffix, transformer, *pInstTopoNaming);
        assert(namingRet);

        auto ret = this->modifyOwnerShapeByInstance(retShape, pTopoNaming,
            instShape, pInstTopoNaming.get(), isCut, feedbackCollector);
        retShape = ret.second;
        if (!ret.first)
        {
            success = false;
            break;
        }
    }

    return std::pair<bool, TopoDS_Shape>(success, retShape);
}

NS_WY3D_END
