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
#include <cmath>

#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffset_Mode.hxx>
#include <GeomAbs_JoinType.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <BRep_Builder.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dOffsetSheet.h>
#include <wy3dSheet.h>
#include <wy3dImpl.h>
#include <wy3dParamNames.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "topo/TopoNamingUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(OffsetSheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(OffsetSheet, _sourceId)
    REGISTER_FIELD(OffsetSheet, _offset)
END_FIELD_REGISTRATION()

OffsetSheet::OffsetSheet() : wy3d::Sheet(),
    _sourceId(wydb::ElementId::kNull),
    _offset(0.0)
{
}

OffsetSheet::~OffsetSheet()
{
}

wy::ErrorStatus OffsetSheet::create(
    wydb::Transaction* pTrans,
    wy3d::Sheet* pSource,
    double offset,
    OffsetSheet*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }
    if (!pSource)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }
    if (std::fabs(offset) < wy3d::kMinValue ||
        std::fabs(offset) > wy3d::kMaxValue)
    {
        pOut = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }

    OffsetSheet* pObj = new OffsetSheet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pObj);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pObj);
        pObj = nullptr;
        return error;
    }

    error = pObj->setSource(pSource);
    CHECK_ERROR_FOR_CREATE(error, pObj);
    error = pObj->setOffset(offset);
    CHECK_ERROR_FOR_CREATE(error, pObj);

    pOut = pObj;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus OffsetSheet::setSource(const wydb::ElementId& sourceId)
{
    if (sourceId == _sourceId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kOffsetSheet_sourceId);
    if (wy::ErrorStatus::Ok == error)
    {
        _sourceId = sourceId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus OffsetSheet::setSource(wy3d::Sheet* pSource)
{
    assert(_sourceId.isNull());

    if (!pSource)
    {
        return wy::ErrorStatus::NullElementPointer;
    }

    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    assert(!pSource->getId().isNull());
    error = this->setSource(pSource->getId());
    if (wy::ErrorStatus::Ok != error)
    {
        return error;
    }

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus OffsetSheet::setOffset(double offset)
{
    if (std::fabs(offset) < wy3d::kMinValue ||
        std::fabs(offset) > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (offset == _offset)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(
        kOffsetSheet_offset, wydb::ElementDataPieceType::Shape);
    if (wy::ErrorStatus::Ok == error)
    {
        _offset = offset;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

void OffsetSheet::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::OFFSETSHEET_PARAM_SOURCE;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::OFFSETSHEET_PARAM_OFFSET;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr OffsetSheet::getParameterValue(
    const std::string& className,
    const std::string& paramName) const
{
    if (className == OffsetSheet::classInfo()->className())
    {
        if (ParamNames::OFFSETSHEET_PARAM_SOURCE == paramName)
        {
            return wydb::ParameterValue::createInteger(_sourceId.value());
        }
        else if (ParamNames::OFFSETSHEET_PARAM_OFFSET == paramName)
        {
            return wydb::ParameterValue::createDouble(_offset);
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus OffsetSheet::setParameterValue(
    const std::string& className,
    const std::string& paramName,
    const wydb::ParameterValue& paramValue)
{
    if (className == OffsetSheet::classInfo()->className())
    {
        if (ParamNames::OFFSETSHEET_PARAM_SOURCE == paramName)
        {
            assert(false);
            return wy::ErrorStatus::ParameterReadonly;
        }
        else if (ParamNames::OFFSETSHEET_PARAM_OFFSET == paramName)
        {
            if (!paramValue.isDouble())
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setOffset(paramValue.asDouble());
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool OffsetSheet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kOffsetSheet_sourceId.value():
        value = _sourceId;
        return true;
    case kOffsetSheet_offset.value():
        value = _offset;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool OffsetSheet::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kOffsetSheet_sourceId.value():
        _sourceId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kOffsetSheet_offset.value():
        _offset = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus OffsetSheet::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _sourceId << _offset;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus OffsetSheet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _sourceId >> _offset;
    return wy::ErrorStatus::Ok;
}

void OffsetSheet::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_sourceId.isNull())
    {
        dependencies.insert(_sourceId);
    }
}

bool OffsetSheet::onDependenciesErased(
    const std::set<wydb::ElementId>& erasedDependencies)
{
    if (!_sourceId.isNull() &&
        erasedDependencies.find(_sourceId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->setSource(wydb::ElementId::kNull);
        return true;
    }
    else
    {
        return __baseClass::onDependenciesErased(erasedDependencies);
    }
}

TopoDS_Shape OffsetSheet::generateShape(
    TopoNaming* pTopoNaming,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);

    const wy3d::Sheet* pSource = wy3d::Sheet::cast(pDb->getElement(_sourceId));
    if (!pSource)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::OFFSETSHEET_InvalidData));
        return TopoDS_Shape();
    }

    TopoDS_Shape sourceShape = pSource->getShape();
    if (sourceShape.IsNull())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::OFFSETSHEET_InvalidData));
        return TopoDS_Shape();
    }

    // collect all shells (recursively)
    std::vector<TopoDS_Shell> sourceShells;
    TopAbs_ShapeEnum sourceShapeType = sourceShape.ShapeType();
    if (sourceShapeType == TopAbs_SHELL)
    {
        sourceShells.push_back(TopoDS::Shell(sourceShape));
    }
    else if (sourceShapeType == TopAbs_COMPOUND)
    {
        for (TopExp_Explorer ex(sourceShape, TopAbs_SHELL); ex.More(); ex.Next())
            sourceShells.push_back(TopoDS::Shell(ex.Current()));
    }
    else
    {
        assert(false);
    }
    if (sourceShells.empty())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::OFFSETSHEET_InvalidData));
        return TopoDS_Shape();
    }

    unsigned int idValue = this->getId().value();
    try
    {
        std::vector<TopoDS_Shell> resultShells;
        for (const TopoDS_Shell& sourceShell : sourceShells)
        {
            BRepOffsetAPI_MakeOffsetShape mkOffset;
            mkOffset.PerformByJoin(sourceShell, _offset, wy3d::TOL * 10,
                BRepOffset_Skin, Standard_False, Standard_False,
                GeomAbs_Intersection);
            if (!mkOffset.IsDone())
            {
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<std::uint32_t>(ErrorCode::OFFSETSHEET_GenerateError));
                return TopoDS_Shape();
            }

            TopoDS_Shape resultShape = mkOffset.Shape();
            if (resultShape.IsNull())
            {
                assert(false);
                continue;
            }
            if (resultShape.ShapeType() == TopAbs_SHELL)
            {
                resultShells.push_back(TopoDS::Shell(resultShape));
            }
            else
            {
                assert(false);
                for (TopExp_Explorer ex(resultShape, TopAbs_SHELL); ex.More(); ex.Next())
                {
                    resultShells.push_back(TopoDS::Shell(ex.Current()));
                }
            }
        }

        if (resultShells.empty())
        {
            assert(false);
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::OFFSETSHEET_GenerateError));
            return TopoDS_Shape();
        }

        TopoDS_Compound compound;
        BRep_Builder brepBuilder;
        brepBuilder.MakeCompound(compound);
        for (const TopoDS_Shell& shell : resultShells)
        {
            brepBuilder.Add(compound, shell);
        }
        TopoNamingUtil::primitiveNaming(compound, idValue, *pTopoNaming);
        return compound;
    }
    catch (const Standard_Failure&)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::OFFSETSHEET_GenerateError));
        return TopoDS_Shape();
    }
}

NS_WY3D_END
