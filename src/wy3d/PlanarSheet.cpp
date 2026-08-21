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
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dPlanarSheet.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketch.h>
#include <wy3dSketchProfile.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "utils/Util.h"
#include "topo/SketchTopoBuilder.h"
#include "topo/TopoNamingUtil.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(PlanarSheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(PlanarSheet, _sketchId)
END_FIELD_REGISTRATION()

PlanarSheet::PlanarSheet() : wy3d::Sheet(), _sketchId(wydb::ElementId::kNull)
{
}

PlanarSheet::~PlanarSheet()
{
}

void PlanarSheet::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

wy::ErrorStatus PlanarSheet::create(
    wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch,
    PlanarSheet*& pOutSheet)
{
    if (!pTrans)
    {
        pOutSheet = nullptr;
        return wy::ErrorStatus::NullTransactionPointer;
    }
    if (!pSketch)
    {
        pOutSheet = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }

    PlanarSheet* pSheet = new PlanarSheet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSheet);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSheet);
        pSheet = nullptr;
        return error;
    }

    error = pSheet->setSketchImpl(pSketch);
    CHECK_ERROR_FOR_CREATE(error, pSheet);

    pOutSheet = pSheet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus PlanarSheet::setSketchImpl(const wydb::ElementId& sketchId)
{
    if (sketchId == _sketchId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kPlanarSheet_sketchId);
    if (wy::ErrorStatus::Ok == error)
    {
        _sketchId = sketchId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus PlanarSheet::setSketchImpl(wy3d::Sketch* pSketch)
{
    assert(_sketchId.isNull());

    if (!pSketch)
    {
        return wy::ErrorStatus::NullElementPointer;
    }
    if (!pSketch->getParent().isNull())
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    assert(!pSketch->getId().isNull());
    error = this->setSketchImpl(pSketch->getId());
    if (wy::ErrorStatus::Ok != error)
    {
        return error;
    }

    error = pSketch->setOwner(this->getId());
    if (wy::ErrorStatus::Ok != error)
    {
        return error;
    }

    return wy::ErrorStatus::Ok;
}

bool PlanarSheet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kPlanarSheet_sketchId.value():
        value = _sketchId;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool PlanarSheet::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kPlanarSheet_sketchId.value():
        _sketchId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus PlanarSheet::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _sketchId;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus PlanarSheet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _sketchId;
    return wy::ErrorStatus::Ok;
}

void PlanarSheet::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_sketchId.isNull())
    {
        dependencies.insert(_sketchId);
    }
}

bool PlanarSheet::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);
    if (!_sketchId.isNull() && erasedDependencies.find(_sketchId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->setSketchImpl(wydb::ElementId::kNull);
        return true;
    }
    return responsed;
}

TopoDS_Shape PlanarSheet::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(_sketchId));
    if (!pSketch)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }

    if (!pSketch->getPlane().isValid())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }

    SketchProfile sketchProfile(pSketch);
    if (!sketchProfile.check())
    {
        std::shared_ptr<SketchError> pError = sketchProfile.getError();
        unsigned int errorCode = pError ? static_cast<unsigned int>(pError->type)
            : static_cast<unsigned int>(ErrorCode::PROFILE_InvalidProfile);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), errorCode);
        return TopoDS_Shape();
    }
    const std::vector<SketchProfile::FaceSPtr>& sketchFaces = sketchProfile.getFaces();
    if (sketchFaces.empty())
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }

    std::vector<TopoDS_Face> resultFaces;
    resultFaces.reserve(sketchFaces.size());
    unsigned int profileIndex = 0;
    const bool isMultiProfile = (sketchFaces.size() > 1);
    for (const SketchProfile::FaceSPtr& pSketchFace : sketchFaces)
    {
        if (isMultiProfile) { ++profileIndex; }
        std::vector<TopoUtil::EdgeNamingInfo> edgeNameInfos;
        auto makeFaceRet = TopoUtil::makeFace(pSketch, pSketchFace, edgeNameInfos);
        if (makeFaceRet.first != ErrorCode::NoError)
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<unsigned int>(makeFaceRet.first));
            return TopoDS_Shape();
        }

        TopoDS_Face face = makeFaceRet.second;
        assert(!face.IsNull());
        resultFaces.emplace_back(face);

        unsigned int idValue = this->getId().value();
        TopoNamingUtil::naming(face, edgeNameInfos, idValue, *pTopoNaming, profileIndex);
    }

    TopoDS_Compound compound;
    BRep_Builder brepBuilder;
    brepBuilder.MakeCompound(compound);
    for (const TopoDS_Face& face : resultFaces)
    {
        TopoDS_Shell shell;
        brepBuilder.MakeShell(shell);
        brepBuilder.Add(shell, face);
        brepBuilder.Add(compound, shell);
    }

#ifdef _DEBUG
    char szFileName[100] = { 0 };
    sprintf_s(szFileName, 100, "D:/logs/%d.txt", this->getId().value());
    pTopoNaming->print(szFileName, compound);
#endif

    return compound;
}

NS_WY3D_END
