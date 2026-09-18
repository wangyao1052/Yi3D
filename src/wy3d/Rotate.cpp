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
#include <wy3dRotate.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dParamNames.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "topo/MoveRotateTopoShapeComparer.h"
#include "BodyModificationUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Rotate)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Rotate, _center)
    REGISTER_FIELD(Rotate, _axisDir)
    REGISTER_FIELD(Rotate, _angle)
END_FIELD_REGISTRATION()

Rotate::Rotate() : wy3d::BodyModification(), _center(), _axisDir(wy::Vector3::kZAxis), _angle(0.0)
{
}

Rotate::~Rotate()
{
}

wy::ErrorStatus Rotate::create(
    wydb::Transaction* pTrans,
    wy3d::Solid* pSolid,
    const wy::Vector3& centerPoint,
    const wy::Vector3& axisDirection,
    double angle,
    Rotate*& pOutRotate)
{
    pOutRotate = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!pSolid) return wy::ErrorStatus::NullElementPointer;

    Rotate* pRotate(nullptr);
    wy::ErrorStatus error = createImpl(pTrans, centerPoint, axisDirection, angle, pRotate);
    if (wy::ErrorStatus::Ok != error) return error;

    error = pSolid->addModification(pRotate);
    CHECK_ERROR_FOR_CREATE(error, pRotate);

    pOutRotate = pRotate;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Rotate::create(
    wydb::Transaction* pTrans,
    wy3d::Sheet* pSheet,
    const wy::Vector3& centerPoint,
    const wy::Vector3& axisDirection,
    double angle,
    Rotate*& pOutRotate)
{
    pOutRotate = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!pSheet) return wy::ErrorStatus::NullElementPointer;

    Rotate* pRotate(nullptr);
    wy::ErrorStatus error = createImpl(pTrans, centerPoint, axisDirection, angle, pRotate);
    if (wy::ErrorStatus::Ok != error) return error;

    error = pSheet->addModification(pRotate);
    CHECK_ERROR_FOR_CREATE(error, pRotate);

    pOutRotate = pRotate;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Rotate::createImpl(
    wydb::Transaction* pTrans,
    const wy::Vector3& centerPoint,
    const wy::Vector3& axisDirection,
    double angle,
    Rotate*& pOutRotate)
{
    Rotate* pRotate = new Rotate();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pRotate);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pRotate);
        pRotate = nullptr;
        return error;
    }

    error = pRotate->setCenterPoint(centerPoint);
    CHECK_ERROR_FOR_CREATE(error, pRotate);
    error = pRotate->setAxisDirection(axisDirection);
    CHECK_ERROR_FOR_CREATE(error, pRotate);
    error = pRotate->setAngle(angle);
    CHECK_ERROR_FOR_CREATE(error, pRotate);

    pOutRotate = pRotate;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Rotate::setCenterPoint(const wy::Vector3& center)
{
    if (center == _center)
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kRotate_center);
    if (wy::ErrorStatus::Ok == error)
    {
        _center = center;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Rotate::setAxisDirection(const wy::Vector3& dir)
{
    wy::Vector3 direction = dir;
    direction.normalize();
    if (direction.length() < 0.5)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (direction == _axisDir)
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kRotate_axisDir);
    if (wy::ErrorStatus::Ok == error)
    {
        _axisDir = direction;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Rotate::setAngle(double angle)
{
    if (angle == _angle)
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kRotate_angle);
    if (wy::ErrorStatus::Ok == error)
    {
        _angle = angle;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void Rotate::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::ROTATE_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::ROTATE_AXIS_DIRECTION_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::ROTATE_AXIS_DIRECTION_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::ROTATE_AXIS_DIRECTION_Z;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::ROTATE_CENTER_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::ROTATE_CENTER_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::ROTATE_CENTER_Z;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Rotate::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Rotate::classInfo()->className())
    {
        if (ParamNames::ROTATE_CENTER_X == paramName)
        {
            return wydb::ParameterValue::createDouble(_center.x());
        }
        else if (ParamNames::ROTATE_CENTER_Y == paramName)
        {
            return wydb::ParameterValue::createDouble(_center.y());
        }
        else if (ParamNames::ROTATE_CENTER_Z == paramName)
        {
            return wydb::ParameterValue::createDouble(_center.z());
        }
        else if (ParamNames::ROTATE_AXIS_DIRECTION_X == paramName)
        {
            return wydb::ParameterValue::createDouble(_axisDir.x());
        }
        else if (ParamNames::ROTATE_AXIS_DIRECTION_Y == paramName)
        {
            return wydb::ParameterValue::createDouble(_axisDir.y());
        }
        else if (ParamNames::ROTATE_AXIS_DIRECTION_Z == paramName)
        {
            return wydb::ParameterValue::createDouble(_axisDir.z());
        }
        else if (ParamNames::ROTATE_ANGLE == paramName)
        {
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_angle));
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus Rotate::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Rotate::classInfo()->className())
    {
        if (!paramValue.isDouble())
        {
            return wy::ErrorStatus::InvalidInput;
        }

        double v = paramValue.asDouble();
        if (ParamNames::ROTATE_CENTER_X == paramName)
        {
            return this->setCenterPoint(wy::Vector3(v, _center.y(), _center.z()));
        }
        else if (ParamNames::ROTATE_CENTER_Y == paramName)
        {
            return this->setCenterPoint(wy::Vector3(_center.x(), v, _center.z()));
        }
        else if (ParamNames::ROTATE_CENTER_Z == paramName)
        {
            return this->setCenterPoint(wy::Vector3(_center.x(), _center.y(), v));
        }
        else if (ParamNames::ROTATE_AXIS_DIRECTION_X == paramName)
        {
            return this->setAxisDirection(wy::Vector3(v, _axisDir.y(), _axisDir.z()));
        }
        else if (ParamNames::ROTATE_AXIS_DIRECTION_Y == paramName)
        {
            return this->setAxisDirection(wy::Vector3(_axisDir.x(), v, _axisDir.z()));
        }
        else if (ParamNames::ROTATE_AXIS_DIRECTION_Z == paramName)
        {
            return this->setAxisDirection(wy::Vector3(_axisDir.x(), _axisDir.y(), v));
        }
        else if (ParamNames::ROTATE_ANGLE == paramName)
        {
            return this->setAngle(wy3d::degreesToRadians(v));
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Rotate::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kRotate_center.value():
        value = _center;
        return true;
    case kRotate_axisDir.value():
        value = _axisDir;
        return true;
    case kRotate_angle.value():
        value = _angle;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Rotate::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kRotate_center.value():
        _center = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kRotate_axisDir.value():
        _axisDir = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kRotate_angle.value():
        _angle = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Rotate::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _center << _axisDir << _angle;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Rotate::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _center >> _axisDir >> _angle;
    return wy::ErrorStatus::Ok;
}

std::pair<bool, TopoDS_Shape> Rotate::modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    try
    {
        gp_Trsf rotTrsf;
        gp_Ax1 ax1(gp_Pnt(_center.x(), _center.y(), _center.z()),
            gp_Dir(_axisDir.x(), _axisDir.y(), _axisDir.z()));
        rotTrsf.SetRotation(ax1, _angle);
        BRepBuilderAPI_Transform transformer(shape, rotTrsf);
        TopoDS_Shape retShape = transformer.Shape();

        MoveRotateTopoShapeComparer topoComparer(transformer, shape);
        topoComparer.perform();
        pTopoNaming->update(&topoComparer, this->getId().value());

        return std::pair<bool, TopoDS_Shape>(true, retShape);
    }
    catch (const Standard_Failure&)
    {
    }

    wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
        static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_GenerateShapeError));
    return std::pair<bool, TopoDS_Shape>(false, shape);
}

NS_WY3D_END
