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
#include <algorithm>
#include <cmath>
#include <map>
#include <Geom_Curve.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <Bnd_Box.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepFill_Filling.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dFilledSheet.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketch.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchPoint.h>
#include <wy3dSketchProfile.h>
#include <wy3dSketch3DProfile.h>
#include <wy3dErrorCode.h>
#include <wy3dImpl.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "utils/OccUtil.h"
#include "utils/Util.h"
#include "topo/SketchTopoBuilder.h"
#include "topo/TopoNamingUtil.h"
#include "topo/Sketch3DTopoBuilder.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(FilledSheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(FilledSheet, _boundarySketchId)
    REGISTER_FIELD(FilledSheet, _constraintSketchIds)
END_FIELD_REGISTRATION()

FilledSheet::FilledSheet()
    : wy3d::Sheet(), _boundarySketchId(wydb::ElementId::kNull)
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

    error = pSheet->setBoundarySketchImpl(pSketch);
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

    error = pSheet->setBoundarySketchImpl(pSketch3D);
    CHECK_ERROR_FOR_CREATE(error, pSheet);

    pOutSheet = pSheet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus FilledSheet::setBoundarySketchImpl(const wydb::ElementId& sketchId)
{
    if (sketchId == _boundarySketchId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kFilledSheet_boundarySketchId);
    if (wy::ErrorStatus::Ok == error)
    {
        _boundarySketchId = sketchId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus FilledSheet::setBoundarySketchImpl(wy3d::Sketch* pSketch)
{
    assert(_boundarySketchId.isNull());

    if (!pSketch)
    {
        return wy::ErrorStatus::NullElementPointer;
    }
    if (!pSketch->getParent().isNull())
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    error = this->setBoundarySketchImpl(pSketch->getId());
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

wy::ErrorStatus FilledSheet::setBoundarySketchImpl(wy3d::Sketch3D* pSketch3D)
{
    assert(_boundarySketchId.isNull());

    if (!pSketch3D)
    {
        return wy::ErrorStatus::NullElementPointer;
    }
    if (!pSketch3D->getParent().isNull())
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    error = this->setBoundarySketchImpl(pSketch3D->getId());
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

wy::ErrorStatus FilledSheet::addConstraintSketch(wy3d::Sketch* pConstraintSketch)
{
    if (!pConstraintSketch) return wy::ErrorStatus::NullElementPointer;

    const wydb::ElementId constraintSketchId = pConstraintSketch->getId();
    const auto iter = std::find(_constraintSketchIds.cbegin(), _constraintSketchIds.cend(), constraintSketchId);
    if (_constraintSketchIds.cend() != iter) return wy::ErrorStatus::Ok;

    if (!pConstraintSketch->getParent().isNull()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->addConstraintSketchImpl(constraintSketchId);
    if (wy::ErrorStatus::Ok != error) return error;
    return pConstraintSketch->setOwner(this->getId());
}

wy::ErrorStatus FilledSheet::addConstraintSketch(wy3d::Sketch3D* pConstraintSketch)
{
    if (!pConstraintSketch) return wy::ErrorStatus::NullElementPointer;

    const wydb::ElementId constraintSketchId = pConstraintSketch->getId();
    const auto iter = std::find(_constraintSketchIds.cbegin(), _constraintSketchIds.cend(), constraintSketchId);
    if (_constraintSketchIds.cend() != iter) return wy::ErrorStatus::Ok;

    if (!pConstraintSketch->getParent().isNull()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->addConstraintSketchImpl(constraintSketchId);
    if (wy::ErrorStatus::Ok != error) return error;
    return pConstraintSketch->setParent(this->getId());
}

wy::ErrorStatus FilledSheet::removeConstraintSketch(wy3d::Sketch* pConstraintSketch)
{
    if (!pConstraintSketch) return wy::ErrorStatus::NullElementPointer;

    const wydb::ElementId constraintSketchId = pConstraintSketch->getId();
    const auto iter = std::find(_constraintSketchIds.cbegin(), _constraintSketchIds.cend(), constraintSketchId);
    if (_constraintSketchIds.cend() == iter) return wy::ErrorStatus::KeyNotFound;

    wy::ErrorStatus error = this->removeConstraintSketchImpl(constraintSketchId);
    if (wy::ErrorStatus::Ok != error) return error;
    return pConstraintSketch->setOwner(wydb::ElementId::kNull);
}

wy::ErrorStatus FilledSheet::removeConstraintSketch(wy3d::Sketch3D* pConstraintSketch)
{
    if (!pConstraintSketch) return wy::ErrorStatus::NullElementPointer;

    const wydb::ElementId constraintSketchId = pConstraintSketch->getId();
    const auto iter = std::find(_constraintSketchIds.cbegin(), _constraintSketchIds.cend(), constraintSketchId);
    if (_constraintSketchIds.cend() == iter) return wy::ErrorStatus::KeyNotFound;

    wy::ErrorStatus error = this->removeConstraintSketchImpl(constraintSketchId);
    if (wy::ErrorStatus::Ok != error) return error;
    return pConstraintSketch->setParent(wydb::ElementId::kNull);
}

wy::ErrorStatus FilledSheet::setConstraintSketchImpl(const std::vector<wydb::ElementId>& constraintSketchIds)
{
    if (_constraintSketchIds == constraintSketchIds)
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kFilledSheet_constraintSketchIds);
    if (wy::ErrorStatus::Ok == error)
    {
        _constraintSketchIds = constraintSketchIds;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus FilledSheet::addConstraintSketchImpl(const wydb::ElementId& constraintSketchId)
{
    if (constraintSketchId.isNull()) return wy::ErrorStatus::InvalidInput;
    // The boundary is the surface's own wire, it cannot also be a constraint
    if (constraintSketchId == _boundarySketchId) return wy::ErrorStatus::InvalidInput;

    const auto iter = std::find(_constraintSketchIds.cbegin(), _constraintSketchIds.cend(), constraintSketchId);
    if (_constraintSketchIds.cend() != iter) return wy::ErrorStatus::Ok; // already a constraint sketch

    wy::ErrorStatus error = this->prepareForFieldChange(kFilledSheet_constraintSketchIds);
    if (wy::ErrorStatus::Ok != error) return error;

    _constraintSketchIds.emplace_back(constraintSketchId);
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus FilledSheet::removeConstraintSketchImpl(const wydb::ElementId& constraintSketchId)
{
    const auto iter = std::find(_constraintSketchIds.cbegin(), _constraintSketchIds.cend(), constraintSketchId);
    if (_constraintSketchIds.cend() == iter) return wy::ErrorStatus::Ok; // not a constraint sketch

    wy::ErrorStatus error = this->prepareForFieldChange(kFilledSheet_constraintSketchIds);
    if (wy::ErrorStatus::Ok != error) return error;

    _constraintSketchIds.erase(iter);
    return wy::ErrorStatus::Ok;
}

bool FilledSheet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kFilledSheet_boundarySketchId.value():
        value = _boundarySketchId;
        return true;
    case kFilledSheet_constraintSketchIds.value():
        value = _constraintSketchIds;
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
    case kFilledSheet_boundarySketchId.value():
        _boundarySketchId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kFilledSheet_constraintSketchIds.value():
        _constraintSketchIds = std::any_cast<const std::vector<wydb::ElementId>&>(value);
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
    filer << _boundarySketchId;
    filer << static_cast<std::uint32_t>(_constraintSketchIds.size());
    for (const wydb::ElementId& constraintSketchId : _constraintSketchIds)
    {
        filer << constraintSketchId;
    }
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus FilledSheet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _boundarySketchId;
    std::uint32_t numConstraintSketches(0);
    filer >> numConstraintSketches;
    _constraintSketchIds.resize(numConstraintSketches);
    for (std::uint32_t i = 0; i < numConstraintSketches; ++i)
    {
        filer >> _constraintSketchIds[i];
    }
    return wy::ErrorStatus::Ok;
}

void FilledSheet::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_boundarySketchId.isNull())
    {
        dependencies.insert(_boundarySketchId);
    }
    for (const wydb::ElementId& constraintSketchId : _constraintSketchIds)
    {
        if (!constraintSketchId.isNull()) dependencies.insert(constraintSketchId);
    }
}

bool FilledSheet::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    if (!_boundarySketchId.isNull() &&
        erasedDependencies.find(_boundarySketchId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->setBoundarySketchImpl(wydb::ElementId::kNull);
        return true;
    }

    std::vector<wydb::ElementId> constraintSketchIds;
    constraintSketchIds.reserve(_constraintSketchIds.size());
    for (const wydb::ElementId& constraintSketchId : _constraintSketchIds)
    {
        if (erasedDependencies.find(constraintSketchId) == erasedDependencies.cend())
        {
            constraintSketchIds.emplace_back(constraintSketchId);
        }
    }
    if (constraintSketchIds.size() != _constraintSketchIds.size())
    {
        wy::ErrorStatus error = this->setConstraintSketchImpl(constraintSketchIds);
        assert(wy::ErrorStatus::Ok == error);
        responsed = true;
    }
    return responsed;
}

// Constraint sketch -> edges and points. A point constraint is punctual: the surface is pulled onto
// it, and unlike a curve it is no edge of the result
static ErrorCode makeConstraints(
    const wydb::Database* pDb,
    const wy3d::Sketch* pConstraintSketch,
    std::vector<TopoDS_Edge>& edges,
    std::vector<TopoDS_Vertex>& vertices)
{
    assert(pDb);
    SketchTopoBuilder sketchTopoBuilder(pConstraintSketch, true);
    for (auto iter = pConstraintSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::Element* pConstraintEntity = pDb->getElement(iter.current());
        if (const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pConstraintEntity))
        {
            if (pCurve->isConstruction()) continue;
            TopoDS_Edge edge = sketchTopoBuilder.makeEdge(pCurve);
            if (edge.IsNull()) return ErrorCode::FILLEDSHEET_ConstraintCurveInvalid;
            edges.emplace_back(edge);
        }
        else if (const wy3d::SketchPoint* pPoint = wy3d::SketchPoint::cast(pConstraintEntity))
        {
            const wy::Vector3 position = pConstraintSketch->getPlane().value(pPoint->getPosition());
            vertices.emplace_back(BRepBuilderAPI_MakeVertex(OccUtil::toPnt(position)));
        }
    }
    return (edges.empty() && vertices.empty())
        ? ErrorCode::FILLEDSHEET_ConstraintCurveInvalid : ErrorCode::NoError;
}

static ErrorCode makeConstraints(
    const wydb::Database* pDb,
    const wy3d::Sketch3D* pConstraintSketch,
    std::vector<TopoDS_Edge>& edges,
    std::vector<TopoDS_Vertex>& vertices) // a 3D sketch holds curves only
{
    assert(pDb);
    Sketch3DTopoBuilder sketch3DTopoBuilder(true);
    for (auto iter = pConstraintSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wy3d::SketchCurve3D* pCurve = wy3d::SketchCurve3D::cast(pDb->getElement(iter.current()));
        if (!pCurve) continue;
        TopoDS_Edge edge = sketch3DTopoBuilder.makeEdge(pCurve);
        if (edge.IsNull()) return ErrorCode::FILLEDSHEET_ConstraintCurveInvalid;
        edges.emplace_back(edge);
    }
    return (edges.empty() && vertices.empty())
        ? ErrorCode::FILLEDSHEET_ConstraintCurveInvalid : ErrorCode::NoError;
}

ErrorCode FilledSheet::collectConstraints(
    std::vector<TopoDS_Edge>& constraintEdges,
    std::vector<TopoDS_Vertex>& constraintVertices) const
{
    const wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    for (const wydb::ElementId& constraintSketchId : _constraintSketchIds)
    {
        const wydb::Element* pConstraintElem = pDb->getElement(constraintSketchId);
        ErrorCode constraintError(ErrorCode::FILLEDSHEET_ConstraintCurveInvalid);
        if (const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pConstraintElem))
        {
            constraintError = makeConstraints(pDb, pSketch, constraintEdges, constraintVertices);
        }
        else if (const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pConstraintElem))
        {
            constraintError = makeConstraints(pDb, pSketch3D, constraintEdges, constraintVertices);
        }
        if (ErrorCode::NoError != constraintError) return constraintError;
    }
    return ErrorCode::NoError;
}

// A constraint is soft: the plate solver may ignore it entirely. Measured (boundary
// diagonal ~150): honored constraints stay within 1.1e-2, ignored ones sit at 38..415, so this
// only asks whether the surface reaches the constraint at all, it does not nitpick deviations.
// Do not use G0Error(index): that path corrupts the heap in this OCCT version
// (G0Error() without an index is safe but measures boundaries only, never constraints)
static bool isAnyConstraintMissed(
    const TopoDS_Wire& boundaryWire,
    const TopoDS_Face& face,
    const std::vector<TopoDS_Edge>& constraintEdges,
    const std::vector<TopoDS_Vertex>& constraintVertices)
{
    constexpr double kMissRatio = 1e-3; // of the boundary bounding box diagonal
    double tolerance = wy3d::TOL;
    Bnd_Box box;
    BRepBndLib::Add(boundaryWire, box);
    if (!box.IsVoid())
    {
        double x0(0.0), y0(0.0), z0(0.0), x1(0.0), y1(0.0), z1(0.0);
        box.Get(x0, y0, z0, x1, y1, z1);
        const double dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
        tolerance = std::max(tolerance, kMissRatio * std::sqrt(dx * dx + dy * dy + dz * dz));
    }

    // No measurement, no verdict: missing a bad constraint beats reporting a good one
    const auto isMissed = [&face, tolerance](const TopoDS_Shape& constraint)
    {
        BRepExtrema_DistShapeShape dist(constraint, face);
        dist.Perform();
        return dist.IsDone() && dist.Value() > tolerance;
    };
    for (const TopoDS_Edge& constraintEdge : constraintEdges)
    {
        if (isMissed(constraintEdge)) return true;
    }
    for (const TopoDS_Vertex& constraintVertex : constraintVertices)
    {
        if (isMissed(constraintVertex)) return true;
    }
    return false;
}

// One closed boundary wire plus its constraints -> the face, and the names of that face's own
// edges. A constraint rules out the planar shortcut: it solves unconstrained and would ignore it.
// BRepFill rebuilds the boundary edges, so the names are re-derived from the built face's wires,
// and curve2Id has to come from the builder that produced the boundary edges
static ErrorCode generateFaceFromBoundary(
    const std::vector<TopoDS_Edge>& boundaryEdges,
    const TopoDS_Wire& boundaryWire,
    const std::vector<TopoDS_Edge>& constraintEdges,
    const std::vector<TopoDS_Vertex>& constraintVertices,
    const std::map<Handle(Geom_Curve), unsigned int>& curve2Id,
    TopoDS_Face& face,
    std::vector<TopoUtil::EdgeNamingInfo>& edgeNameInfos)
{
    face = TopoDS_Face();
    edgeNameInfos.clear();
    const bool hasConstraint = !constraintEdges.empty() || !constraintVertices.empty();

    if (!hasConstraint)
    {
        try
        {
            BRepBuilderAPI_MakeFace planarMakeFace(boundaryWire, Standard_True);
            if (planarMakeFace.IsDone())
            {
                face = planarMakeFace.Face();
            }
        }
        catch (const Standard_Failure&)
        {
            face = TopoDS_Face();
        }
    }

    if (face.IsNull())
    {
        try
        {
            BRepFill_Filling filling;
            for (const TopoDS_Edge& edge : boundaryEdges)
            {
                filling.Add(edge, GeomAbs_C0, Standard_True);
            }
            // IsBound=false puts the edge into myConstraints, the plate solver pulls the surface onto it
            for (const TopoDS_Edge& edge : constraintEdges)
            {
                filling.Add(edge, GeomAbs_C0, Standard_False);
            }
            // A point constraint pins the surface at that point, position only
            for (const TopoDS_Vertex& vertex : constraintVertices)
            {
                filling.Add(BRep_Tool::Pnt(vertex));
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

        if (!face.IsNull() && hasConstraint)
        {
            bool missed = false;
            try
            {
                missed = isAnyConstraintMissed(boundaryWire, face, constraintEdges, constraintVertices);
            }
            catch (const Standard_Failure&)
            {
                missed = false;
            }
            if (missed) return ErrorCode::FILLEDSHEET_ConstraintCurveNotSatisfied;
        }
    }
    if (face.IsNull()) return ErrorCode::FILLEDSHEET_GenerateError;

    // Record edge names from the built face's own wires: BRepFill rebuilds the
    // boundary edges, so the pre-fill wire edges are not the ones being picked
    TopTools_IndexedMapOfShape idxMapOfWire;
    TopExp::MapShapes(face, TopAbs_WIRE, idxMapOfWire);
    for (int k = 1; k <= idxMapOfWire.Extent(); ++k)
    {
        TopoDS_Wire faceWire = TopoDS::Wire(idxMapOfWire(k));
        assert(!faceWire.IsNull());
        TopoUtil::recordEdgeNamesOfWire_AppendedMode(faceWire, curve2Id, edgeNameInfos);
    }

    return ErrorCode::NoError;
}

// The shape a sheet arrives in: one shell per face inside a compound
static TopoDS_Shape compoundOfFaces(const std::vector<TopoDS_Face>& faces)
{
    TopoDS_Compound compound;
    BRep_Builder brepBuilder;
    brepBuilder.MakeCompound(compound);
    for (const TopoDS_Face& face : faces)
    {
        TopoDS_Shell shell;
        brepBuilder.MakeShell(shell);
        brepBuilder.Add(shell, face);
        brepBuilder.Add(compound, shell);
    }
    return compound;
}

TopoDS_Shape FilledSheet::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);

    GenerateShapeResult result;
    const wydb::Element* pSketchElem = pDb->getElement(_boundarySketchId);
    if (const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pSketchElem))
    {
        result = this->generateShape(pSketch);
    }
    else if (const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pSketchElem))
    {
        result = this->generateShape(pSketch3D);
    }
    else
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::FILLEDSHEET_InvalidData));
        return TopoDS_Shape();
    }

    if (ErrorCode::NoError != result.errorCode)
    {
        assert(result.shape.IsNull());
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(result.errorCode));
        return TopoDS_Shape();
    }

    pTopoNaming->merge(result.topoNaming, result.shape, result.shape);
    return result.shape;
}

FilledSheet::GenerateShapeResult FilledSheet::generateShape(const wy3d::Sketch* pSketch)
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
    if (sketchFaces.size() != 1)
    {
        return GenerateShapeResult{ ErrorCode::FILLEDSHEET_EdgesNotClosed };
    }
    const SketchProfile::FaceSPtr& pSketchFace = sketchFaces.front();
    if (1 != pSketchFace->loops.size())
    {
        return GenerateShapeResult{ ErrorCode::FILLEDSHEET_EdgesNotClosed };
    }

    SketchTopoBuilder sketchTopoBuilder(pSketch, true);
    std::vector<TopoDS_Edge> boundaryEdges;
    TopoDS_Wire wire;
    try
    {
        const SketchProfile::LoopSPtr& pOuterLoop = pSketchFace->loops.front();
        assert(pOuterLoop);
        for (const BiCurve& curve : pOuterLoop->curves)
        {
            const wy3d::SketchCurve* pCurve = curve.curve;
            assert(pCurve);
            TopoDS_Edge edge = sketchTopoBuilder.makeEdge(pCurve);
            if (edge.IsNull()) return GenerateShapeResult{ ErrorCode::PROFILE_InvalidProfile };
            boundaryEdges.emplace_back(
                TopoDS::Edge(edge.Oriented(curve.orient ? TopAbs_FORWARD : TopAbs_REVERSED)));
        }
        if (pOuterLoop->isClockWise)
        {
            std::reverse(boundaryEdges.begin(), boundaryEdges.end());
            for (TopoDS_Edge& edge : boundaryEdges) { edge = TopoDS::Edge(edge.Reversed()); }
        }

        BRepBuilderAPI_MakeWire makeWire;
        for (const TopoDS_Edge& edge : boundaryEdges) { makeWire.Add(edge); }
        wire = makeWire.Wire();
    }
    catch (const Standard_Failure&)
    {
        return GenerateShapeResult{ ErrorCode::FILLEDSHEET_EdgesNotClosed };
    }
    if (wire.IsNull() || !wire.Closed())
    {
        return GenerateShapeResult{ ErrorCode::FILLEDSHEET_EdgesNotClosed };
    }

    // With no constraint the sketch's own plane answers the question exactly, so the flat face is
    // the better one to keep; a constraint lifts the patch off that plane
    if (_constraintSketchIds.empty())
    {
        std::vector<TopoUtil::EdgeNamingInfo> edgeNameInfos;
        std::pair<ErrorCode, TopoDS_Face> makeFaceRet = TopoUtil::makeFace(pSketch, pSketchFace, edgeNameInfos);
        if (ErrorCode::NoError != makeFaceRet.first)
        {
            return GenerateShapeResult{ makeFaceRet.first };
        }

        TopoDS_Face face = makeFaceRet.second;
        assert(!face.IsNull());

        GenerateShapeResult result;
        TopoNamingUtil::naming(face, edgeNameInfos, this->getId().value(), result.topoNaming, 0);
        result.errorCode = ErrorCode::NoError;
        result.shape = compoundOfFaces({ face });
        return result;
    }

    // Constraint edges first: a constraint sketch that yields no edge is reported before anything else
    std::vector<TopoDS_Edge> constraintEdges;
    std::vector<TopoDS_Vertex> constraintVertices;
    ErrorCode constraintError = this->collectConstraints(constraintEdges, constraintVertices);
    if (ErrorCode::NoError != constraintError)
    {
        return GenerateShapeResult{ constraintError };
    }

    

    TopoDS_Face face;
    std::vector<TopoUtil::EdgeNamingInfo> edgeNameInfos;
    ErrorCode faceError = generateFaceFromBoundary(boundaryEdges, wire, constraintEdges,
        constraintVertices, sketchTopoBuilder.getCurve2IdMap(), face, edgeNameInfos);
    if (ErrorCode::NoError != faceError)
    {
        return GenerateShapeResult{ faceError };
    }

    GenerateShapeResult result;
    TopoNamingUtil::naming(face, edgeNameInfos, this->getId().value(), result.topoNaming, 0);
    result.errorCode = ErrorCode::NoError;
    result.shape = compoundOfFaces({ face });
    return result;
}

FilledSheet::GenerateShapeResult FilledSheet::generateShape(const wy3d::Sketch3D* pSketch3D)
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

    // Constraint edges first: a constraint sketch that yields no edge is reported before anything else
    std::vector<TopoDS_Edge> constraintEdges;
    std::vector<TopoDS_Vertex> constraintVertices;
    ErrorCode constraintError = this->collectConstraints(constraintEdges, constraintVertices);
    if (ErrorCode::NoError != constraintError)
    {
        return GenerateShapeResult{ constraintError };
    }

    TopoDS_Face face;
    std::vector<TopoUtil::EdgeNamingInfo> edgeNameInfos;
    ErrorCode faceError = generateFaceFromBoundary(orderedEdges, wire, constraintEdges,
        constraintVertices, sketch3DTopoBuilder.getCurve2IdMap(), face, edgeNameInfos);
    if (ErrorCode::NoError != faceError)
    {
        return GenerateShapeResult{ faceError };
    }

    GenerateShapeResult result;
    TopoNamingUtil::naming(face, edgeNameInfos, this->getId().value(), result.topoNaming, 0);
    result.errorCode = ErrorCode::NoError;
    result.shape = compoundOfFaces({ face });
    return result;
}

NS_WY3D_END
