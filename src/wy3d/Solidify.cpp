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
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Shell.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepLib.hxx>
#include <BRep_Tool.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSolidify.h>
#include <wy3dSheet.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dParamNames.h>

#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Solidify)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Solidify, _sourceId)
END_FIELD_REGISTRATION()

Solidify::Solidify() : wy3d::Solid(), _sourceId(wydb::ElementId::kNull)
{
}

Solidify::~Solidify()
{
}

wy::ErrorStatus Solidify::create(wydb::Transaction* pTrans, wy3d::Sheet* pSource, Solidify*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullTransactionPointer;
    }
    if (!pSource)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }

    Solidify* pSolidify = new Solidify();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSolidify);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSolidify);
        pSolidify = nullptr;
        return error;
    }

    error = pSolidify->setSourceImpl(pSource);
    CHECK_ERROR_FOR_CREATE(error, pSolidify);

    pOut = pSolidify;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Solidify::setSourceImpl(const wydb::ElementId& sourceId)
{
    if (sourceId == _sourceId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kSolidify_sourceId);
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

wy::ErrorStatus Solidify::setSourceImpl(wy3d::Sheet* pSource)
{
    assert(_sourceId.isNull());

    if (!pSource)
    {
        return wy::ErrorStatus::NullElementPointer;
    }
    if (!pSource->getParent().isNull())
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    assert(!pSource->getId().isNull());
    error = this->setSourceImpl(pSource->getId());
    if (wy::ErrorStatus::Ok != error)
    {
        return error;
    }

    error = pSource->setParent(this->getId());
    if (wy::ErrorStatus::Ok != error)
    {
        return error;
    }

    return wy::ErrorStatus::Ok;
}

void Solidify::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::SOLIDIFY_PARAM_SOURCE;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr Solidify::getParameterValue(
    const std::string& className,
    const std::string& paramName) const
{
    if (className == Solidify::classInfo()->className())
    {
        if (ParamNames::SOLIDIFY_PARAM_SOURCE == paramName)
        {
            return wydb::ParameterValue::createInteger(_sourceId.value());
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus Solidify::setParameterValue(
    const std::string& className,
    const std::string& paramName,
    const wydb::ParameterValue& paramValue)
{
    if (className == Solidify::classInfo()->className())
    {
        if (ParamNames::SOLIDIFY_PARAM_SOURCE == paramName)
        {
            assert(false);
            return wy::ErrorStatus::ParameterReadonly;
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Solidify::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSolidify_sourceId.value():
        value = _sourceId;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Solidify::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSolidify_sourceId.value():
        _sourceId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Solidify::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _sourceId;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Solidify::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _sourceId;
    return wy::ErrorStatus::Ok;
}

void Solidify::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_sourceId.isNull())
    {
        dependencies.insert(_sourceId);
    }
}

bool Solidify::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    if (!_sourceId.isNull() && erasedDependencies.find(_sourceId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->setSourceImpl(wydb::ElementId::kNull);
        return true;
    }
    else
    {
        return __baseClass::onDependenciesErased(erasedDependencies);
    }
}

TopoDS_Shape Solidify::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);

    const wy3d::Sheet* pSource = wy3d::Sheet::cast(pDb->getElement(_sourceId));
    if (!pSource)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::SOLIDIFY_InvalidData));
        return TopoDS_Shape();
    }
    const TopoDS_Shape& sourceShape = pSource->getShape();
    if (sourceShape.IsNull())
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::SOLIDIFY_InvalidData));
        return TopoDS_Shape();
    }

    TopTools_IndexedMapOfShape shellMap;
    TopExp::MapShapes(sourceShape, TopAbs_SHELL, shellMap);
    if (shellMap.Extent() != 1)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::SOLIDIFY_InvalidData));
        return TopoDS_Shape();
    }

    TopoDS_Shell shell = TopoDS::Shell(shellMap(1));
    if (!BRep_Tool::IsClosed(shell))
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::SOLIDIFY_GenerateError));
        return TopoDS_Shape();
    }

    BRepBuilderAPI_MakeSolid makeSolid(shell);
    if (!makeSolid.IsDone())
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::SOLIDIFY_GenerateError));
        return TopoDS_Shape();
    }
    TopoDS_Solid resultSolid = makeSolid.Solid();
    Standard_Boolean orientRet = BRepLib::OrientClosedSolid(resultSolid);
    assert(Standard_True == orientRet);

    const TopoNaming* pSourceNaming = pSource->getTopoNaming();
    if (pSourceNaming)
    {
        pTopoNaming->merge(*pSourceNaming, sourceShape, sourceShape);
    }

    return resultSolid;
}

NS_WY3D_END
