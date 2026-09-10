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
#include <TopoDS_Shell.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepFill_Filling.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dFilledSheet.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketch.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchProfile.h>
#include <wy3dSketch3DProfile.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "utils/Util.h"
#include "topo/SketchTopoBuilder.h"
#include "topo/TopoNamingUtil.h"
#include "topo/Sketch3DTopoBuilder.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(FilledSheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(FilledSheet, _sketchId)
END_FIELD_REGISTRATION()

FilledSheet::FilledSheet() : wy3d::Sheet(), _sketchId(wydb::ElementId::kNull)
{
}

FilledSheet::~FilledSheet()
{
}

void FilledSheet::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

wy::ErrorStatus FilledSheet::create(
    wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch,
    FilledSheet*& pOutSheet)
{
    pOutSheet = nullptr;

    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!pSketch) return wy::ErrorStatus::NullElementPointer;

    FilledSheet* pSheet = new FilledSheet();
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

wy::ErrorStatus FilledSheet::create(
    wydb::Transaction* pTrans,
    wy3d::Sketch3D* pSketch3D,
    FilledSheet*& pOutSheet)
{
    pOutSheet = nullptr;

    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!pSketch3D) return wy::ErrorStatus::NullElementPointer;

    FilledSheet* pSheet = new FilledSheet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSheet);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSheet);
        pSheet = nullptr;
        return error;
    }

    error = pSheet->setSketchImpl(pSketch3D);
    CHECK_ERROR_FOR_CREATE(error, pSheet);

    pOutSheet = pSheet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus FilledSheet::setSketchImpl(const wydb::ElementId& sketchId)
{
    if (sketchId == _sketchId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kFilledSheet_sketchId);
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

wy::ErrorStatus FilledSheet::setSketchImpl(wy3d::Sketch* pSketch)
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

wy::ErrorStatus FilledSheet::setSketchImpl(wy3d::Sketch3D* pSketch3D)
{
    assert(_sketchId.isNull());

    if (!pSketch3D)
    {
        return wy::ErrorStatus::NullElementPointer;
    }
    if (!pSketch3D->getParent().isNull())
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    assert(!pSketch3D->getId().isNull());
    error = this->setSketchImpl(pSketch3D->getId());
    if (wy::ErrorStatus::Ok != error)
    {
        return error;
    }

    error = pSketch3D->setParent(this->getId());
    if (wy::ErrorStatus::Ok != error)
    {
        return error;
    }

    return wy::ErrorStatus::Ok;
}

bool FilledSheet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kFilledSheet_sketchId.value():
        value = _sketchId;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool FilledSheet::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kFilledSheet_sketchId.value():
        _sketchId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus FilledSheet::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _sketchId;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus FilledSheet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _sketchId;
    return wy::ErrorStatus::Ok;
}

void FilledSheet::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_sketchId.isNull())
    {
        dependencies.insert(_sketchId);
    }
}

bool FilledSheet::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
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

TopoDS_Shape FilledSheet::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);

    GenerateShapeResult result;
    const wydb::Element* pSketchElem = pDb->getElement(_sketchId);
    if (const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pSketchElem))
    {
        result = this->generateShape2D(pSketch);
    }
    else if (const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pSketchElem))
    {
        result = this->generateShape3D(pSketch3D);
    }
    else
    {
        assert(false);
    }

    if (ErrorCode::NoError != result.errorCode)
    {
        assert(result.shape.IsNull());
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(result.errorCode));
        return TopoDS_Shape();
    }

    // Merge the names only after a successful generation
    pTopoNaming->merge(result.topoNaming, result.shape, result.shape);
    return result.shape;
}

FilledSheet::GenerateShapeResult FilledSheet::generateShape2D(const wy3d::Sketch* pSketch)
{
    assert(pSketch);
    if (!pSketch->getPlane().isValid())
    {
        assert(false);
        return GenerateShapeResult{ ErrorCode::PROFILE_InvalidProfile };
    }

    SketchProfile sketchProfile(pSketch);
    if (!sketchProfile.check())
    {
        std::shared_ptr<SketchError> pError = sketchProfile.getError();
        return GenerateShapeResult{ pError ? pError->type : ErrorCode::PROFILE_InvalidProfile };
    }
    const std::vector<SketchProfile::FaceSPtr>& sketchFaces = sketchProfile.getFaces();
    if (sketchFaces.empty())
    {
        return GenerateShapeResult{ ErrorCode::PROFILE_InvalidProfile };
    }

    GenerateShapeResult result;
    std::vector<TopoDS_Face> resultFaces;
    resultFaces.reserve(sketchFaces.size());
    unsigned int profileIndex = 0;
    const bool isMultiProfile = (sketchFaces.size() > 1);
    for (const SketchProfile::FaceSPtr& pSketchFace : sketchFaces)
    {
        if (isMultiProfile) { ++profileIndex; }
        std::vector<TopoUtil::EdgeNamingInfo> edgeNameInfos;
        std::pair<ErrorCode, TopoDS_Face> makeFaceRet = TopoUtil::makeFace(pSketch, pSketchFace, edgeNameInfos);
        if (ErrorCode::NoError != makeFaceRet.first)
        {
            return GenerateShapeResult{ makeFaceRet.first };
        }

        TopoDS_Face face = makeFaceRet.second;
        assert(!face.IsNull());
        resultFaces.emplace_back(face);

        unsigned int idValue = this->getId().value();
        TopoNamingUtil::naming(face, edgeNameInfos, idValue, result.topoNaming, profileIndex);
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
    result.errorCode = ErrorCode::NoError;
    result.shape = compound;
    return result;
}

FilledSheet::GenerateShapeResult FilledSheet::generateShape3D(const wy3d::Sketch3D* pSketch3D)
{
    Sketch3DProfile sketch3DProfile(pSketch3D);
    if (!sketch3DProfile.check())
    {
        std::shared_ptr<SketchError> pError = sketch3DProfile.getError();
        return GenerateShapeResult{ pError ? pError->type : ErrorCode::FILLEDSHEET_InvalidData };
    }

    // Build the wire from the ordered oriented curves (the same downstream
    // split as the 2D path: SketchProfile -> TopoUtil::makeFace)
    Sketch3DTopoBuilder sketch3DTopoBuilder(true);
    std::vector<TopoDS_Edge> orderedEdges;
    TopoDS_Wire wire;
    try
    {
        BRepBuilderAPI_MakeWire makeWire;
        for (const BiCurve3D& biCurve : sketch3DProfile.getLoop())
        {
            TopoDS_Edge edge = sketch3DTopoBuilder.makeEdge(biCurve.curve);
            if (edge.IsNull()) // defensive: degeneracy is pre-checked by the profile
            {
                return GenerateShapeResult{ ErrorCode::FILLEDSHEET_InvalidData };
            }
            orderedEdges.emplace_back(TopoDS::Edge(edge.Oriented(biCurve.orient ? TopAbs_FORWARD : TopAbs_REVERSED)));
            makeWire.Add(orderedEdges.back());
        }
        wire = makeWire.Wire();
    }
    catch (const Standard_Failure&)
    {
        return GenerateShapeResult{ ErrorCode::FILLEDSHEET_EdgesNotClosed };
    }
    if (wire.IsNull() || !wire.Closed()) // defensive: closure is pre-checked by the profile
    {
        return GenerateShapeResult{ ErrorCode::FILLEDSHEET_EdgesNotClosed };
    }

    TopoDS_Face face;
    try
    {
        BRepBuilderAPI_MakeFace planarMakeFace(wire, Standard_True);
        if (planarMakeFace.IsDone())
        {
            face = planarMakeFace.Face();
        }
    }
    catch (const Standard_Failure&)
    {
        face = TopoDS_Face();
    }

    if (face.IsNull())
    {
        try
        {
            BRepFill_Filling filling;
            for (const TopoDS_Edge& edge : orderedEdges)
            {
                filling.Add(edge, GeomAbs_C0, Standard_True);
            }
            filling.Build();
            if (filling.IsDone())
            {
                face = filling.Face();
            }
        }
        catch (const Standard_Failure&)
        {
            face = TopoDS_Face();
        }
    }
    if (face.IsNull())
    {
        return GenerateShapeResult{ ErrorCode::FILLEDSHEET_GenerateError };
    }

    // Record edge names from the built face's own wires: BRepFill rebuilds the
    // boundary edges, so the pre-fill wire edges are not the ones being picked
    std::vector<TopoUtil::EdgeNamingInfo> edgeNameInfos;
    TopTools_IndexedMapOfShape idxMapOfWire;
    TopExp::MapShapes(face, TopAbs_WIRE, idxMapOfWire);
    for (int k = 1; k <= idxMapOfWire.Extent(); ++k)
    {
        TopoDS_Wire faceWire = TopoDS::Wire(idxMapOfWire(k));
        assert(!faceWire.IsNull());
        TopoUtil::recordEdgeNamesOfWire_AppendedMode(faceWire, sketch3DTopoBuilder.getCurve2IdMap(), edgeNameInfos);
    }

    GenerateShapeResult result;
    TopoNamingUtil::naming(face, edgeNameInfos, this->getId().value(), result.topoNaming, 0);

    TopoDS_Compound compound;
    BRep_Builder brepBuilder;
    brepBuilder.MakeCompound(compound);
    TopoDS_Shell shell;
    brepBuilder.MakeShell(shell);
    brepBuilder.Add(shell, face);
    brepBuilder.Add(compound, shell);
    result.errorCode = ErrorCode::NoError;
    result.shape = compound;
    return result;
}

NS_WY3D_END
