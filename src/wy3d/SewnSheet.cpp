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

#include <map>
#include <unordered_map>
#include <set>
#include <cassert>
#include <algorithm>
#include <BRepBuilderAPI_Sewing.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSewnSheet.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dParamNames.h>

#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SewnSheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SewnSheet, _sourceIds)
    REGISTER_FIELD(SewnSheet, _tolerance)
END_FIELD_REGISTRATION()

SewnSheet::SewnSheet() : wy3d::Sheet(), _tolerance(1e-6)
{
}

SewnSheet::~SewnSheet()
{
}

wy::ErrorStatus SewnSheet::create(
    wydb::Transaction* pTrans,
    const std::vector<wy3d::Sheet*>& sources,
    double tolerance,
    SewnSheet*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullTransactionPointer;
    }
    if (sources.empty())
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }

    SewnSheet* pSheet = new SewnSheet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSheet);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSheet);
        pSheet = nullptr;
        return error;
    }

    std::set<wy3d::Sheet*> sourceSet;
    for (wy3d::Sheet* pSource : sources)
    {
        if (sourceSet.find(pSource) != sourceSet.cend())
        {
            continue;
        }
        sourceSet.insert(pSource);
        error = pSheet->addSource(pSource);
        CHECK_ERROR_FOR_CREATE(error, pSheet);
    }
    error = pSheet->setTolerance(tolerance);
    CHECK_ERROR_FOR_CREATE(error, pSheet);

    pOut = pSheet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SewnSheet::setSourcesImpl(const std::vector<wydb::ElementId>& sourceIds)
{
    if (_sourceIds == sourceIds)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kSewnSheet_sourceIds);
    if (wy::ErrorStatus::Ok == error)
    {
        _sourceIds = sourceIds;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SewnSheet::addSource(wy3d::Sheet* pSource)
{
    if (!pSource)
    {
        return wy::ErrorStatus::NullElementPointer;
    }
    if (!pSource->getParent().isNull())
    {
        return wy::ErrorStatus::InvalidInput;
    }

    const wydb::ElementId& sourceId = pSource->getId();
    if (std::find(_sourceIds.cbegin(), _sourceIds.cend(), sourceId) != _sourceIds.cend())
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus fieldError = this->prepareForFieldChange(kSewnSheet_sourceIds);
    if (wy::ErrorStatus::Ok == fieldError)
    {
        _sourceIds.emplace_back(sourceId);
        wy::ErrorStatus error = pSource->setParent(this->getId());
        assert(wy::ErrorStatus::Ok == error);
        return error;
    }
    else
    {
        return fieldError;
    }
}

wy::ErrorStatus SewnSheet::setTolerance(double tolerance)
{
    if (tolerance <= 0.0)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (tolerance == _tolerance)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kSewnSheet_tolerance, wydb::ElementDataPieceType::Shape);
    if (wy::ErrorStatus::Ok == error)
    {
        _tolerance = tolerance;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

void SewnSheet::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::SEWNSHEET_PARAM_TOLERANCE;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr SewnSheet::getParameterValue(
    const std::string& className,
    const std::string& paramName) const
{
    if (className == SewnSheet::classInfo()->className())
    {
        if (ParamNames::SEWNSHEET_PARAM_TOLERANCE == paramName)
        {
            return wydb::ParameterValue::createDouble(_tolerance);
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus SewnSheet::setParameterValue(
    const std::string& className,
    const std::string& paramName,
    const wydb::ParameterValue& paramValue)
{
    if (className == SewnSheet::classInfo()->className())
    {
        if (ParamNames::SEWNSHEET_PARAM_TOLERANCE == paramName)
        {
            if (!paramValue.isDouble())
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setTolerance(paramValue.asDouble());
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SewnSheet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSewnSheet_sourceIds.value():
        value = _sourceIds;
        return true;
    case kSewnSheet_tolerance.value():
        value = _tolerance;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SewnSheet::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSewnSheet_sourceIds.value():
        _sourceIds = std::any_cast<const std::vector<wydb::ElementId>&>(value);
        return true;
    case kSewnSheet_tolerance.value():
        _tolerance = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SewnSheet::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _tolerance;
    std::uint32_t numSources = _sourceIds.size();
    filer << numSources;
    for (const wydb::ElementId& sourceId : _sourceIds)
    {
        filer << sourceId;
    }
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SewnSheet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _tolerance;
    std::uint32_t numSources(0);
    filer >> numSources;
    _sourceIds.resize(numSources);
    for (std::uint32_t i = 0; i < numSources; ++i)
    {
        filer >> _sourceIds[i];
    }
    return wy::ErrorStatus::Ok;
}

void SewnSheet::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    dependencies.insert(_sourceIds.cbegin(), _sourceIds.cend());
}

bool SewnSheet::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    std::vector<wydb::ElementId> newSources;
    newSources.reserve(_sourceIds.size());
    for (const wydb::ElementId& sourceId : _sourceIds)
    {
        if (erasedDependencies.find(sourceId) == erasedDependencies.cend())
        {
            newSources.emplace_back(sourceId);
        }
    }
    if (newSources.size() == _sourceIds.size())
    {
        return responsed;
    }

    if (newSources.empty())
    {
        this->erase(true);
    }
    this->setSourcesImpl(newSources);
    return true;
}

TopoDS_Shape SewnSheet::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);

    std::vector<TopoDS_Shape> sourceShapes;
    sourceShapes.reserve(_sourceIds.size());
    TopoNamingSPtr pTotalSourceNaming = std::make_shared<TopoNaming>();
    for (const wydb::ElementId& sourceId : _sourceIds)
    {
        const wy3d::Sheet* pSource = wy3d::Sheet::cast(pDb->getElement(sourceId));
        if (!pSource)
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::SEWNSHEET_InvalidData));
            return TopoDS_Shape();
        }
        const TopoDS_Shape& sourceShape = pSource->getShape();
        if (sourceShape.IsNull())
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::SEWNSHEET_InvalidData));
            return TopoDS_Shape();
        }
        const TopoNaming* pSourceNaming = pSource->getTopoNaming();
        if (pSourceNaming)
        {
            pTotalSourceNaming->merge(*pSourceNaming, sourceShape, sourceShape);
        }
        sourceShapes.emplace_back(sourceShape);
    }

    BRepBuilderAPI_Sewing sewing(_tolerance);
    for (const TopoDS_Shape& sourceShape : sourceShapes)
    {
        sewing.Add(sourceShape);
    }
    sewing.Perform();
    TopoDS_Shape sewed = sewing.SewedShape();
    if (sewed.IsNull() || TopAbs_SHELL != sewed.ShapeType())
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::SEWNSHEET_GenerateError));
        return TopoDS_Shape();
    }
    TopoDS_Shell resultShell = TopoDS::Shell(sewed);

    {
        std::unordered_map<TopoDS_Shape, TopoNameBuilder, ShapeHasher, ShapeEqual> shp2NameBuilder;
        const TopoNaming::NameMap& nameMap = pTotalSourceNaming->getNameMap();
        for (const auto& kvp : nameMap)
        {
            TopoDS_Shape modified = sewing.ModifiedSubShape(kvp.first);
            if (modified.IsNull())
            {
                assert(false);
                continue;
            }
            shp2NameBuilder[modified].source(kvp.second);
        }
        for (const auto& kvp : shp2NameBuilder)
        {
            pTopoNaming->setName(kvp.first, kvp.second.build());
        }
    }

#ifdef _DEBUG
    char szFileName[100] = { 0 };
    sprintf_s(szFileName, 100, "D:/logs/%d.txt", this->getId().value());
    pTopoNaming->print(szFileName, resultShell);
#endif

    return resultShell;
}

NS_WY3D_END
