///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <BRepAlgoAPI_Splitter.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSplitFace.h>
#include <wy3dSheet.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "topo/Sketch3DTopoBuilder.h"
#include "topo/TopoNamingUtil.h"
#include "topo/SplitFaceTopoShapeComparer.h"
#include "SolidModificationUtil.h"
#include "utils/FilerUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SplitFace)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SplitFace, _faceNames)
    REGISTER_FIELD(SplitFace, _sketchId)
END_FIELD_REGISTRATION()

SplitFace::SplitFace() : wy3d::SolidModification(), _sketchId(wydb::ElementId::kNull)
{
}

SplitFace::~SplitFace()
{
}

void SplitFace::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

wy::ErrorStatus SplitFace::create(
    wydb::Transaction* pTrans,
    wy3d::Solid* pSolid,
    const std::vector<unsigned int>& faceIndices,
    wy3d::Sketch3D* pSketch3D,
    SplitFace*& pOutSplitFace)
{
    pOutSplitFace = nullptr;

    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!pSolid) return wy::ErrorStatus::NullElementPointer;
    if (faceIndices.empty()) return wy::ErrorStatus::InvalidInput;
    if (!pSketch3D) return wy::ErrorStatus::NullElementPointer;

    wy::ErrorStatus error = createImpl(pTrans,
        pSolid->getShape(), pSolid->getTopoNaming(),
        faceIndices, pSketch3D, pOutSplitFace);
    if (wy::ErrorStatus::Ok != error) return error;

    error = pSolid->addModification(pOutSplitFace);
    CHECK_ERROR_FOR_CREATE(error, pOutSplitFace);
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SplitFace::create(
    wydb::Transaction* pTrans,
    wy3d::Sheet* pSheet,
    const std::vector<unsigned int>& faceIndices,
    wy3d::Sketch3D* pSketch3D,
    SplitFace*& pOutSplitFace)
{
    pOutSplitFace = nullptr;

    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!pSheet) return wy::ErrorStatus::NullElementPointer;
    if (faceIndices.empty()) return wy::ErrorStatus::InvalidInput;
    if (!pSketch3D) return wy::ErrorStatus::NullElementPointer;
    
    wy::ErrorStatus error = createImpl(pTrans,
        pSheet->getShape(), pSheet->getTopoNaming(),
        faceIndices, pSketch3D, pOutSplitFace);
    if (wy::ErrorStatus::Ok != error) return error;

    error = pSheet->addModification(pOutSplitFace);
    CHECK_ERROR_FOR_CREATE(error, pOutSplitFace);
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SplitFace::createImpl(
    wydb::Transaction* pTrans,
    const TopoDS_Shape& shape,
    TopoNaming* pTopoNaming,
    const std::vector<unsigned int>& faceIndices,
    wy3d::Sketch3D* pSketch3D,
    SplitFace*& pOutSplitFace)
{
    assert(pTrans);
    assert(!faceIndices.empty());
    assert(pSketch3D);
    if (!pTopoNaming) { assert(false); return wy::ErrorStatus::InvalidInput; }

    TopoNameList faceNames;
    if (!TopoNamingUtil::assemblyTopoNames(*pTopoNaming, shape,
        TopAbs_ShapeEnum::TopAbs_FACE, faceIndices, faceNames))
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (faceNames.empty())
    {
        assert(false);
        return wy::ErrorStatus::InvalidInput;
    }

    SplitFace* pSplitFace = new SplitFace();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSplitFace);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSplitFace);
        return error;
    }

    error = pSplitFace->setSketchImpl(pSketch3D);
    CHECK_ERROR_FOR_CREATE(error, pSplitFace);
    error = pSplitFace->setFacesImpl(faceNames);
    CHECK_ERROR_FOR_CREATE(error, pSplitFace);

    pOutSplitFace = pSplitFace;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SplitFace::setFacesImpl(const TopoNameList& faceNames)
{
    if (faceNames.empty()) return wy::ErrorStatus::InvalidInput;
    if (faceNames == _faceNames) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSplitFace_faceNames);
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

wy::ErrorStatus SplitFace::setSketchImpl(wy3d::Sketch3D* pSketch3D)
{
    assert(_sketchId.isNull());

    if (!pSketch3D) return wy::ErrorStatus::NullElementPointer;
    if (!pSketch3D->getParent().isNull()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->setSketchIdImpl(pSketch3D->getId());
    if (wy::ErrorStatus::Ok == error)
    {
        error = pSketch3D->setParent(this->getId());
        return error;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SplitFace::setSketchIdImpl(const wydb::ElementId& sketchId)
{
    if (_sketchId == sketchId) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSplitFace_sketchId);
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

bool SplitFace::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSplitFace_faceNames.value():
        value = _faceNames;
        return true;
    case kSplitFace_sketchId.value():
        value = _sketchId;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SplitFace::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSplitFace_faceNames.value():
        _faceNames = std::any_cast<const TopoNameList&>(value);
        return true;
    case kSplitFace_sketchId.value():
        _sketchId = std::any_cast<wydb::ElementId>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SplitFace::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    FilerUtil::writeVector(filer, _faceNames);
    filer << _sketchId;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SplitFace::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    FilerUtil::readTopoNameList(filer, _faceNames);
    filer >> _sketchId;
    return wy::ErrorStatus::Ok;
}

void SplitFace::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_sketchId.isNull()) dependencies.insert(_sketchId);
}

bool SplitFace::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    if (!_sketchId.isNull() && erasedDependencies.find(_sketchId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->setSketchIdImpl(wydb::ElementId::kNull);
        return true;
    }

    return responsed;
}

std::pair<bool, TopoDS_Shape> SplitFace::modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);

    if (_faceNames.empty() || _sketchId.isNull())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::SPLITFACE_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    const wy3d::Sketch3D* pToolSketch = wy3d::Sketch3D::cast(pDb->getElement(_sketchId));
    if (!pToolSketch)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::SPLITFACE_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    std::vector<TopoDS_Face> targetFaces;
    ErrorCode errorCode = SolidModificationUtil::getTopoFacesByTopoNamings<
        ErrorCode::SPLITFACE_InvalidData,
        ErrorCode::SPLITFACE_FaceNotExists>(*pTopoNaming, _faceNames, targetFaces);
    if (ErrorCode::NoError != errorCode)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<unsigned int>(errorCode));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    try
    {
        Sketch3DTopoBuilder sketch3DTopoBuilder(false);
        TopTools_ListOfShape toolEdges;
        std::vector<std::uint32_t> toolCurveIds;
        toolCurveIds.reserve(10);
        for (wy::Iterator<wydb::ElementId> iter = pToolSketch->createIterator(); !iter.isDone(); iter.moveNext())
        {
            const wy3d::SketchCurve3D* pCurve = wy3d::SketchCurve3D::cast(pDb->getElement(iter.current()));
            if (!pCurve) continue;
            TopoDS_Edge edge = sketch3DTopoBuilder.makeEdge(pCurve);
            if (edge.IsNull()) continue;
            toolEdges.Append(edge);
            toolCurveIds.push_back(pCurve->getId().value());
        }
        if (toolEdges.IsEmpty())
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::SPLITFACE_CurveNotExists));
            return std::pair<bool, TopoDS_Shape>(false, shape);
        }

        BRepAlgoAPI_Splitter splitter;
        splitter.SetToFillHistory(Standard_True);
        splitter.SetFuzzyValue(1e-5);
        TopTools_ListOfShape arguments;
        arguments.Append(shape);
        splitter.SetArguments(arguments);
        splitter.SetTools(toolEdges);
        splitter.Build();
        if (!splitter.IsDone())
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::SPLITFACE_GenerateError));
            return std::pair<bool, TopoDS_Shape>(false, shape);
        }

        bool anyTargetFaceSplit = false;
        for (const TopoDS_Face& targetFace : targetFaces)
        {
            if (splitter.Modified(targetFace).Extent() > 1 || splitter.IsDeleted(targetFace))
            {
                anyTargetFaceSplit = true;
                break;
            }
        }
        if (!anyTargetFaceSplit)
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::SPLITFACE_NoFaceSplit));
            return std::pair<bool, TopoDS_Shape>(false, shape);
        }

        TopoDS_Shape retShape = splitter.Shape();
        SplitFaceTopoShapeComparer topoComparer(splitter, shape);
        topoComparer.perform();
#ifdef _DEBUG
        // Cross-check: the history and the delta the naming is driven by must tell the same story.
        const auto deltaSaysSplit = [&topoComparer, &targetFaces]()
        {
            for (const auto& kvp : topoComparer.getFaceDelta().addedSingle)
            {
                if (ShapeEvolution::Split != kvp.second.evolution) continue;
                for (const TopoDS_Face& targetFace : targetFaces)
                {
                    if (kvp.second.source.IsSame(targetFace)) return true;
                }
            }
            return false;
        };
        assert(deltaSaysSplit());
#endif

        if (toolCurveIds.size() == static_cast<size_t>(toolEdges.Extent()))
        {
            TopTools_ListIteratorOfListOfShape edgeIter(toolEdges);
            for (size_t i = 0; edgeIter.More(); edgeIter.Next(), ++i)
            {
                const std::uint32_t curveId = toolCurveIds[i];
                if (0 == curveId)
                {
                    assert(false);
                    continue;
                }
                pTopoNaming->setName(edgeIter.Value(), TopoNameBuilder().id(curveId).build());
            }
        }
        else
        {
            assert(false);
        }
        pTopoNaming->update(&topoComparer, this->getId().value());
        this->recordNewFaces(topoComparer.getFaceDelta(), pTopoNaming);
        return std::pair<bool, TopoDS_Shape>(true, retShape);
    }
    catch (const Standard_Failure&)
    {
    }

    wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
        static_cast<std::uint32_t>(ErrorCode::SPLITFACE_GenerateError));
    return std::pair<bool, TopoDS_Shape>(false, shape);
}

NS_WY3D_END
