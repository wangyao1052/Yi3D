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
#include <memory>
#include <vector>

#include <BRep_Builder.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dOffsetSheet.h>
#include <wy3dSheet.h>
#include <wy3dImpl.h>
#include <wy3dParamNames.h>
#include <wy3dParamEnumDef.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <utils/wy3dSheetOffsetUtil.h>

#include "topo/TopoNamingUtil.h"
#include "topo/OffsetSheetTopoShapeComparer.h"
#include "BodyModificationUtil.h"
#include "utils/FilerUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(OffsetSheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(OffsetSheet, _target)
    REGISTER_FIELD(OffsetSheet, _faceNames)
    REGISTER_FIELD(OffsetSheet, _offset)
END_FIELD_REGISTRATION()

OffsetSheet::OffsetSheet() : wy3d::BodyModification(), _target(Target::WholeSheet), _offset(0.0)
{
}

OffsetSheet::~OffsetSheet()
{
}

wy::ErrorStatus OffsetSheet::create(
    wydb::Transaction* pTrans,
    wy3d::Sheet* pSheet,
    double offset,
    OffsetSheet*& pOut)
{
    return createImpl(pTrans, pSheet, Target::WholeSheet, std::vector<unsigned int>(), offset, pOut);
}

wy::ErrorStatus OffsetSheet::create(
    wydb::Transaction* pTrans,
    wy3d::Sheet* pSheet,
    const std::vector<unsigned int>& faceIndices,
    double offset,
    OffsetSheet*& pOut)
{
    return createImpl(pTrans, pSheet, Target::SelectedFaces, faceIndices, offset, pOut);
}

wy::ErrorStatus OffsetSheet::createImpl(
    wydb::Transaction* pTrans,
    wy3d::Sheet* pSheet,
    Target target,
    const std::vector<unsigned int>& faceIndices,
    double offset,
    OffsetSheet*& pOut)
{
    pOut = nullptr;

    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!pSheet) return wy::ErrorStatus::NullElementPointer;

    TopoNameList faceNames;
    if (Target::SelectedFaces == target)
    {
        if (faceIndices.empty()) return wy::ErrorStatus::InvalidInput;
        const TopoNaming* pTopoNaming = pSheet->getTopoNaming();
        if (!pTopoNaming)
        {
            assert(false);
            return wy::ErrorStatus::InvalidInput;
        }
        if (!TopoNamingUtil::assemblyTopoNames(*pTopoNaming, pSheet->getShape(),
            TopAbs_ShapeEnum::TopAbs_FACE, faceIndices, faceNames))
        {
            return wy::ErrorStatus::InvalidInput;
        }
        if (faceNames.empty())
        {
            assert(false);
            return wy::ErrorStatus::InvalidInput;
        }
    }

    OffsetSheet* pOffsetSheet = new OffsetSheet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pOffsetSheet);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pOffsetSheet);
        pOffsetSheet = nullptr;
        return error;
    }

    error = pOffsetSheet->setTargetImpl(target);
    CHECK_ERROR_FOR_CREATE(error, pOffsetSheet);
    if (Target::SelectedFaces == target)
    {
        error = pOffsetSheet->setFaceNamesImpl(faceNames);
        CHECK_ERROR_FOR_CREATE(error, pOffsetSheet);
    }
    error = pOffsetSheet->setOffset(offset);
    CHECK_ERROR_FOR_CREATE(error, pOffsetSheet);

    error = pSheet->addModification(pOffsetSheet);
    CHECK_ERROR_FOR_CREATE(error, pOffsetSheet);

    pOut = pOffsetSheet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus OffsetSheet::setTargetImpl(Target target)
{
    if (target < Target::WholeSheet || target > Target::SelectedFaces)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (target == _target)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(
        kOffsetSheet_target, wydb::ElementDataPieceType::Shape);
    if (wy::ErrorStatus::Ok == error)
    {
        _target = target;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus OffsetSheet::setFaceNamesImpl(const TopoNameList& faceNames)
{
    if (faceNames == _faceNames)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(
        kOffsetSheet_faceNames, wydb::ElementDataPieceType::Shape);
    if (wy::ErrorStatus::Ok == error)
    {
        _faceNames = faceNames;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
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
        def.name = ParamNames::OFFSETSHEET_PARAM_TARGET;
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
        if (ParamNames::OFFSETSHEET_PARAM_TARGET == paramName)
        {
            return wydb::ParameterValue::createAny(
                wy3d::ParamEnumDef(
                    {{static_cast<int>(Target::WholeSheet), "Whole Sheet"},
                     {static_cast<int>(Target::SelectedFaces), "Selected Faces"}},
                    static_cast<int>(_target)));
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
        if (ParamNames::OFFSETSHEET_PARAM_TARGET == paramName)
        {
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
    case kOffsetSheet_target.value():
        value = _target;
        return true;
    case kOffsetSheet_faceNames.value():
        value = _faceNames;
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
    case kOffsetSheet_target.value():
        _target = std::any_cast<Target>(value);
        return true;
    case kOffsetSheet_faceNames.value():
        _faceNames = std::any_cast<const TopoNameList&>(value);
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

    filer << static_cast<std::int32_t>(_target) << _offset;
    FilerUtil::writeVector(filer, _faceNames);

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus OffsetSheet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);

    std::int32_t target(0);
    filer >> target >> _offset;
    _target = static_cast<Target>(target);
    FilerUtil::readTopoNameList(filer, _faceNames);

    return wy::ErrorStatus::Ok;
}

std::pair<bool, TopoDS_Shape> OffsetSheet::modifyOwnerShape(
    const TopoDS_Shape& shape,
    TopoNaming* pTopoNaming,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    this->clearNewFaces();

    const auto reportError = [&](ErrorCode errorCode)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(errorCode));
    };

    if (shape.IsNull())
    {
        assert(false);
        reportError(ErrorCode::OFFSETSHEET_InvalidData);
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    if (std::fabs(_offset) < wy3d::kMinValue || std::fabs(_offset) > wy3d::kMaxValue)
    {
        assert(false);
        reportError(ErrorCode::OFFSETSHEET_InvalidOffset);
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    std::vector<TopoDS_Face> targetFaces;
    if (Target::SelectedFaces == _target)
    {
        if (_faceNames.empty())
        {
            reportError(ErrorCode::OFFSETSHEET_NoFaceSelected);
            return std::pair<bool, TopoDS_Shape>(false, shape);
        }
        const ErrorCode errorCode = BodyModificationUtil::getTopoFacesByTopoNamings<
            ErrorCode::OFFSETSHEET_InvalidData,
            ErrorCode::OFFSETSHEET_FaceNotExists>(*pTopoNaming, _faceNames, targetFaces);
        if (ErrorCode::NoError != errorCode)
        {
            reportError(errorCode);
            return std::pair<bool, TopoDS_Shape>(false, shape);
        }
        if (!SheetOffsetUtil::findFaceOccurrences(shape, targetFaces))
        {
            reportError(ErrorCode::OFFSETSHEET_FaceNotExists);
            return std::pair<bool, TopoDS_Shape>(false, shape);
        }
    }

    try
    {
        std::vector<TopoDS_Shape> regions;
        const bool collected = (Target::WholeSheet == _target)
            ? SheetOffsetUtil::collectShellRegions(shape, regions)
            : SheetOffsetUtil::collectFaceRegions(shape, targetFaces, regions);
        if (!collected || regions.empty())
        {
            reportError(ErrorCode::OFFSETSHEET_GenerateError);
            return std::pair<bool, TopoDS_Shape>(false, shape);
        }

        // 逐区域偏置
        std::vector<std::shared_ptr<BRepOffsetAPI_MakeOffsetShape>> offsetAlgos;
        std::vector<TopoDS_Shape> offsetShapes;
        offsetAlgos.reserve(regions.size());
        offsetShapes.reserve(regions.size());
        for (const TopoDS_Shape& region : regions)
        {
            std::shared_ptr<BRepOffsetAPI_MakeOffsetShape> pOffsetAlgo =
                std::make_shared<BRepOffsetAPI_MakeOffsetShape>();
            TopoDS_Shape offsetShape;
            if (!SheetOffsetUtil::offsetRegion(region, _offset, *pOffsetAlgo, offsetShape))
            {
                reportError(ErrorCode::OFFSETSHEET_GenerateError);
                return std::pair<bool, TopoDS_Shape>(false, shape);
            }
            offsetShapes.emplace_back(offsetShape);
            offsetAlgos.emplace_back(pOffsetAlgo);
        }

        TopoDS_Compound compound;
        BRep_Builder builder;
        builder.MakeCompound(compound);
        builder.Add(compound, shape);
        for (const TopoDS_Shape& offsetShape : offsetShapes)
        {
            builder.Add(compound, offsetShape);
        }

        assert(!offsetAlgos.empty());
        OffsetSheetTopoShapeComparer topoComparer(offsetAlgos, shape, compound);
        topoComparer.perform();
        pTopoNaming->update(&topoComparer, this->getId().value());

        this->recordNewFaces(topoComparer.getFaceDelta(), pTopoNaming);

        return std::pair<bool, TopoDS_Shape>(true, compound);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
    }

    reportError(ErrorCode::OFFSETSHEET_GenerateError);
    return std::pair<bool, TopoDS_Shape>(false, shape);
}

NS_WY3D_END
