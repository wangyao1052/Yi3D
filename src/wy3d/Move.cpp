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

#include <BRepBuilderAPI_Transform.hxx>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dMove.h>
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
WYDB_IMPLEMENT_MEMBERS(Move)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Move, _vector)
END_FIELD_REGISTRATION()

Move::Move() : wy3d::BodyModification(), _vector()
{
}

Move::~Move()
{
}

wy::ErrorStatus Move::create(
    wydb::Transaction* pTrans,
    wy3d::Solid* pSolid,
    const wy::Vector3& moveVector,
    Move*& pOutMove)
{
    pOutMove = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!pSolid) return wy::ErrorStatus::NullElementPointer;

    Move* pMove(nullptr);
    wy::ErrorStatus error = createImpl(pTrans, moveVector, pMove);
    if (wy::ErrorStatus::Ok != error) return error;

    error = pSolid->addModification(pMove);
    CHECK_ERROR_FOR_CREATE(error, pMove);

    pOutMove = pMove;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Move::create(
    wydb::Transaction* pTrans,
    wy3d::Sheet* pSheet,
    const wy::Vector3& moveVector,
    Move*& pOutMove)
{
    pOutMove = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!pSheet) return wy::ErrorStatus::NullElementPointer;

    Move* pMove(nullptr);
    wy::ErrorStatus error = createImpl(pTrans, moveVector, pMove);
    if (wy::ErrorStatus::Ok != error) return error;

    error = pSheet->addModification(pMove);
    CHECK_ERROR_FOR_CREATE(error, pMove);

    pOutMove = pMove;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Move::createImpl(
    wydb::Transaction* pTrans,
    const wy::Vector3& moveVector,
    Move*& pOutMove)
{
    Move* pMove = new Move();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pMove);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pMove);
        pMove = nullptr;
        return error;
    }
    error = pMove->setVector(moveVector);
    CHECK_ERROR_FOR_CREATE(error, pMove);

    pOutMove = pMove;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Move::setVector(const wy::Vector3& vector)
{
    if (vector == _vector)
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kMove_vector);
    if (wy::ErrorStatus::Ok == error)
    {
        _vector = vector;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void Move::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::MOVE_VECTOR_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::MOVE_VECTOR_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::MOVE_VECTOR_Z;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Move::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Move::classInfo()->className())
    {
        if (ParamNames::MOVE_VECTOR_X == paramName)
        {
            return wydb::ParameterValue::createDouble(_vector.x());
        }
        else if (ParamNames::MOVE_VECTOR_Y == paramName)
        {
            return wydb::ParameterValue::createDouble(_vector.y());
        }
        else if (ParamNames::MOVE_VECTOR_Z == paramName)
        {
            return wydb::ParameterValue::createDouble(_vector.z());
        }
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus Move::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Move::classInfo()->className())
    {
        if (ParamNames::MOVE_VECTOR_X == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setVector(wy::Vector3(paramValue.asDouble(), _vector.y(), _vector.z()));
        }
        else if (ParamNames::MOVE_VECTOR_Y == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setVector(wy::Vector3(_vector.x(), paramValue.asDouble(), _vector.z()));
        }
        else if (ParamNames::MOVE_VECTOR_Z == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setVector(wy::Vector3(_vector.x(), _vector.y(), paramValue.asDouble()));
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Move::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kMove_vector.value():
        value = _vector;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Move::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kMove_vector.value():
        _vector = std::any_cast<const wy::Vector3&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Move::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _vector;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Move::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _vector;
    return wy::ErrorStatus::Ok;
}

std::pair<bool, TopoDS_Shape> Move::modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    try
    {
        gp_Trsf translation;
        translation.SetTranslation(gp_Vec(_vector.x(), _vector.y(), _vector.z()));
        BRepBuilderAPI_Transform transformer(shape, translation);
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
