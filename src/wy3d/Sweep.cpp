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
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>
#include <BRep_Tool.hxx>
#include <GeomLib.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <BRepLib.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepAlgoAPI_Cut.hxx>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSweep.h>
#include <wy3dParamNames.h>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dCurve.h>
#include <wy3dHelix.h>
#include <wy3dSketchProfile.h>
#include <wy3dSketchPath.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "utils/Util.h"
#include "topo/SketchTopoBuilder.h"
#include "topo/TopoNamingUtil.h"
#include "common/occ/OccUtil.h"
#include "topo/BooleanTopoShapeComparer.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Sweep)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Sweep, _pathId)
    REGISTER_FIELD(Sweep, _profileId)
END_FIELD_REGISTRATION()

Sweep::Sweep() : wy3d::Solid(), _pathId(wydb::ElementId::kNull), _profileId(wydb::ElementId::kNull)
{
}

Sweep::~Sweep()
{
}

wy::ErrorStatus Sweep::create(wydb::Transaction* pTrans, wy3d::Sketch* pPath, wy3d::Sketch* pProfile, Sweep*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }
    if (!pPath || !pProfile) { pOut = nullptr; return wy::ErrorStatus::NullElementPointer; }
    if (pPath->getId() == pProfile->getId()) { pOut = nullptr; return wy::ErrorStatus::InvalidInput; }

    Sweep* pSweep = new Sweep();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSweep);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSweep); pSweep = nullptr; return error; }

    error = pSweep->_setPath(pPath); CHECK_ERROR_FOR_CREATE(error, pSweep);
    error = pSweep->_setProfile(pProfile); CHECK_ERROR_FOR_CREATE(error, pSweep);

    pOut = pSweep;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sweep::create(wydb::Transaction* pTrans, wy3d::Curve* pPath, wy3d::Sketch* pProfile, Sweep*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }
    if (!pPath || !pProfile) { pOut = nullptr; return wy::ErrorStatus::NullElementPointer; }
    if (pPath->getId() == pProfile->getId()) { pOut = nullptr; return wy::ErrorStatus::InvalidInput; }

    Sweep* pSweep = new Sweep();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSweep);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSweep); pSweep = nullptr; return error; }

    error = pSweep->_setPath(pPath); CHECK_ERROR_FOR_CREATE(error, pSweep);
    error = pSweep->_setProfile(pProfile); CHECK_ERROR_FOR_CREATE(error, pSweep);

    pOut = pSweep;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sweep::createCut(wydb::Transaction* pTrans, wy3d::Sketch* pPath, wy3d::Sketch* pProfile, wy3d::Solid* pSolidToCut, Sweep*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }
    if (!pPath || !pProfile) { pOut = nullptr; return wy::ErrorStatus::NullElementPointer; }
    if (pPath->getId() == pProfile->getId()) { pOut = nullptr; return wy::ErrorStatus::InvalidInput; }

    Sweep* pSweep = new Sweep();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSweep);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSweep); pSweep = nullptr; return error; }

    error = pSweep->_setPath(pPath); CHECK_ERROR_FOR_CREATE(error, pSweep);
    error = pSweep->_setProfile(pProfile); CHECK_ERROR_FOR_CREATE(error, pSweep);
    error = pSweep->setCut(true); CHECK_ERROR_FOR_CREATE(error, pSweep);

    if (pSolidToCut) { error = pSolidToCut->addModification(pSweep); CHECK_ERROR_FOR_CREATE(error, pSweep); }

    pOut = pSweep;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sweep::createCut(wydb::Transaction* pTrans, wy3d::Curve* pPath, wy3d::Sketch* pProfile, wy3d::Solid* pSolidToCut, Sweep*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }
    if (!pPath || !pProfile) { pOut = nullptr; return wy::ErrorStatus::NullElementPointer; }
    if (pPath->getId() == pProfile->getId()) { pOut = nullptr; return wy::ErrorStatus::InvalidInput; }

    Sweep* pSweep = new Sweep();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSweep);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSweep); pSweep = nullptr; return error; }

    error = pSweep->_setPath(pPath); CHECK_ERROR_FOR_CREATE(error, pSweep);
    error = pSweep->_setProfile(pProfile); CHECK_ERROR_FOR_CREATE(error, pSweep);
    error = pSweep->setCut(true); CHECK_ERROR_FOR_CREATE(error, pSweep);

    if (pSolidToCut) { error = pSolidToCut->addModification(pSweep); CHECK_ERROR_FOR_CREATE(error, pSweep); }

    pOut = pSweep;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sweep::_setPath(const wydb::ElementId& pathId)
{
    if (pathId == _pathId) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSweep_pathId);
    if (wy::ErrorStatus::Ok == error)
    {
        _pathId = pathId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Sweep::_setPath(wy3d::Sketch* pSketch)
{
    assert(_pathId.isNull());
    if (!pSketch) return wy::ErrorStatus::NullElementPointer;
    if (!pSketch->getParent().isNull()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->_setPath(pSketch->getId());
    if (error != wy::ErrorStatus::Ok) return error;

    error = pSketch->setOwner(this->getId());
    return error;
}

wy::ErrorStatus Sweep::_setPath(wy3d::Curve* pCurve)
{
    assert(_pathId.isNull());
    if (!pCurve) return wy::ErrorStatus::NullElementPointer;
    if (!pCurve->getParent().isNull()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->_setPath(pCurve->getId());
    if (error != wy::ErrorStatus::Ok) return error;

    error = pCurve->setOwner(this->getId());
    return error;
}

wy::ErrorStatus Sweep::_setProfile(const wydb::ElementId& profileId)
{
    if (profileId == _profileId) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSweep_profileId);
    if (wy::ErrorStatus::Ok == error)
    {
        _profileId = profileId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Sweep::_setProfile(wy3d::Sketch* pSketch)
{
    assert(_profileId.isNull());
    if (!pSketch) return wy::ErrorStatus::NullElementPointer;
    if (!pSketch->getParent().isNull()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->_setProfile(pSketch->getId());
    if (error != wy::ErrorStatus::Ok) return error;

    error = pSketch->setOwner(this->getId());
    return error;
}


void Sweep::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

bool Sweep::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSweep_pathId.value():
        value = _pathId;
        return true;
    case kSweep_profileId.value():
        value = _profileId;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Sweep::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSweep_pathId.value():
        _pathId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kSweep_profileId.value():
        _profileId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Sweep::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _pathId << _profileId;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sweep::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _pathId >> _profileId;
    return wy::ErrorStatus::Ok;
}

void Sweep::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_pathId.isNull()) dependencies.insert(_pathId);
    if (!_profileId.isNull()) dependencies.insert(_profileId);
}

bool Sweep::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed(false);

    if (!_pathId.isNull() && erasedDependencies.find(_pathId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->_setPath(wydb::ElementId::kNull);
        responsed = true;
    }

    if (!_profileId.isNull() && erasedDependencies.find(_profileId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->_setProfile(wydb::ElementId::kNull);
        responsed = true;
    }

    if (responsed) return true;
    else return __baseClass::onDependenciesErased(erasedDependencies);
}

static ErrorCode makeSweep(
    unsigned int id,
    const TopoUtil::WireInfo& pathWireInfo,
    const std::vector<TopoUtil::WireInfo>& profileWireInfos,
    TopoDS_Shape& resultShape,
    TopoNaming& topoNaming)
{
    for (size_t i = 0; i < profileWireInfos.size(); ++i)
    {
        const TopoUtil::WireInfo& profileWireInfo = profileWireInfos[i];

        BRepOffsetAPI_MakePipeShell pipeShellMaker(pathWireInfo.wire);
        pipeShellMaker.SetMode(false);
        pipeShellMaker.SetTransitionMode(BRepBuilderAPI_RightCorner);
        pipeShellMaker.Add(profileWireInfo.wire);
        pipeShellMaker.Build();
        if (Standard_False == pipeShellMaker.IsDone()) return ErrorCode::TOPOSHAPE_GenerateShapeError;
        if (Standard_False == pipeShellMaker.MakeSolid()) return ErrorCode::TOPOSHAPE_GenerateShapeError;

        TopoNamingUtil::naming(pathWireInfo.wire, profileWireInfo.wire, pipeShellMaker,
            pathWireInfo.edgeNameInfos, profileWireInfo.edgeNameInfos, id, topoNaming);

        if (0 == i) { resultShape = pipeShellMaker.Shape(); }
        else
        {
            BRepAlgoAPI_Cut cut(resultShape, pipeShellMaker.Shape());
            if (Standard_False == cut.IsDone()) return ErrorCode::TOPOSHAPE_GenerateShapeError;

            BooleanTopoShapeComparer topoComparer(cut);
            topoComparer.perform();
            topoNaming.update(&topoComparer, id);

            resultShape = cut.Shape();
        }
    }

    return ErrorCode::NoError;
}

static ErrorCode createPathWire(
    const wy3d::Sketch& pathSketch,
    TopoUtil::WireInfo& pathWireInfo,
    wy::Vector3& pathStartPos,
    wy::Vector3& pathStartDir)
{
    SketchTopoBuilder sketchTopoBuilder(&pathSketch, true);

    SketchPath sketchPath(&pathSketch);
    if (!sketchPath.check())
    {
        std::shared_ptr<SketchError> pError = sketchPath.getError();
        if (pError) return pError->type;
        else return ErrorCode::PATH_InvalidPath;
    }

    const std::vector<BiCurve>& pathCurves = sketchPath.getPath();
    if (pathCurves.empty()) { assert(false); return ErrorCode::PATH_NoCurves; }

    BRepBuilderAPI_MakeWire makeWire;
    for (const BiCurve& curve : pathCurves)
    {
        const SketchCurve* pCurve = curve.curve;
        assert(pCurve);
        TopoDS_Edge edge = sketchTopoBuilder.makeEdge(pCurve);
        if (edge.IsNull()) { assert(false); continue; }
        if (curve.orient)
        {
            edge = TopoDS::Edge(edge.Reversed());
        }
        makeWire.Add(edge);
    }
    if (!makeWire.IsDone() || makeWire.Wire().IsNull()) { assert(false); return ErrorCode::TOPOSHAPE_GenerateShapeError; }
    pathWireInfo.wire = makeWire.Wire();

    assert(!pathCurves.empty());
    const BiCurve& startPathCurve = pathCurves[0];
    const wy3d::SketchCurve* pSweeptartCurve = startPathCurve.curve;
    assert(pSweeptartCurve);
    const wy3d::SketchPlane& pathPlane = pathSketch.getPlane();
    if (startPathCurve.orient)
    {
        pathStartPos = pathPlane.value(pSweeptartCurve->getStartPoint());
        wy::Vector2 dir2d = pSweeptartCurve->getDirectionAt(0.0);
        pathStartDir = pathPlane.value(dir2d) - pathPlane.value(wy::Vector2::kZero);
        pathStartDir.normalize();
    }
    else
    {
        pathStartPos = pathPlane.value(pSweeptartCurve->getEndPoint());
        wy::Vector2 dir2d = pSweeptartCurve->getDirectionAt(1.0);
        pathStartDir = pathPlane.value(dir2d) - pathPlane.value(wy::Vector2::kZero);
        pathStartDir = -pathStartDir;
        pathStartDir.normalize();
    }

    const std::map<Handle(Geom_Curve), unsigned int>& curve2Id = sketchTopoBuilder.getCurve2IdMap();
    TopoUtil::recordEdgeNamesOfWire_AppendedMode(pathWireInfo.wire, curve2Id, pathWireInfo.edgeNameInfos);

    return ErrorCode::NoError;
}

static ErrorCode createPathWire(
    const wy3d::Curve& pathCurve,
    TopoUtil::WireInfo& pathWireInfo,
    wy::Vector3& pathStartPos,
    wy::Vector3& pathStartDir)
{
    BRepBuilderAPI_MakeWire makeWire;
    TopoDS_Edge edge = pathCurve.getEdge();
    if (!edge.IsNull()) makeWire.Add(edge);
    if (!makeWire.IsDone() || makeWire.Wire().IsNull()) { assert(false); return ErrorCode::TOPOSHAPE_GenerateShapeError; }
    pathWireInfo.wire = makeWire.Wire();

    TopoUtil::EdgeNamingInfo edgeNameInfo;
    edgeNameInfo.edge = edge;
    edgeNameInfo.id = pathCurve.getId().value();
    edgeNameInfo.sibling = size_t(-1);
    pathWireInfo.edgeNameInfos.emplace_back(edgeNameInfo);

    BRepLib::BuildCurve3d(edge, wy3d::TOL);
    Standard_Real first(0.0), last(0.0);
    Handle(Geom_Curve) geomCurve = BRep_Tool::Curve(edge, first, last);
    if (geomCurve.IsNull()) { assert(false); return ErrorCode::PATH_InvalidPath; }
    gp_Pnt startPnt;
    gp_Vec tangent;
    geomCurve->D1(first, startPnt, tangent);
    if (tangent.Magnitude() <= wy3d::TOL) { assert(false); return ErrorCode::PATH_InvalidPath; }
    gp_Dir dir(tangent);
    pathStartPos.set(startPnt.X(), startPnt.Y(), startPnt.Z());
    pathStartDir.set(dir.X(), dir.Y(), dir.Z());

    return ErrorCode::NoError;
}

static ErrorCode createProfileWires(
    const wy3d::Sketch& profileSketch,
    const gp_Trsf& trsf,
    std::vector<std::vector<TopoUtil::WireInfo>>& profileWires)
{
    SketchProfile sketchProfile(&profileSketch);
    if (!sketchProfile.check())
    {
        std::shared_ptr<SketchError> pError = sketchProfile.getError();
        if (pError) return pError->type;
        else return ErrorCode::PROFILE_InvalidProfile;
    }

    const std::vector<SketchProfile::FaceSPtr>& sketchFaces = sketchProfile.getFaces();
    std::vector<std::vector<TopoUtil::WireInfo>> retWires;
    retWires.reserve(sketchFaces.size());

    for (const SketchProfile::FaceSPtr& pSketchFace : sketchFaces)
    {
        assert(pSketchFace);
        std::vector<TopoUtil::WireInfo> wireInfos;
        ErrorCode errorCode = TopoUtil::makeWires(&profileSketch, pSketchFace, trsf, wireInfos);
        if (ErrorCode::NoError != errorCode) return errorCode;
        retWires.emplace_back(std::move(wireInfos));
    }

    profileWires.swap(retWires);
    return ErrorCode::NoError;
}

TopoDS_Shape Sweep::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    wy3d::SketchPlane pathPlane;
    const wydb::Element* pPathElem = pDb->getElement(_pathId);
    const wy3d::Sketch* pPathSketch = wy3d::Sketch::cast(pPathElem);
    const wy3d::Curve* pPathCurve = wy3d::Curve::cast(pPathElem);
    if (pPathSketch && pPathSketch->getPlane().isValid())
    {
        pathPlane = pPathSketch->getPlane();
    }
    else if (!pPathCurve)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PATH_InvalidPath));
        return TopoDS_Shape();
    }

    const wy3d::Sketch* pProfileSketch = wy3d::Sketch::cast(pDb->getElement(_profileId));
    if (!pProfileSketch || !pProfileSketch->getPlane().isValid())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }
    const wy3d::SketchPlane& profilePlane = pProfileSketch->getPlane();
    assert(profilePlane.isValid());

    if (pPathSketch)
    {
        if (profilePlane.getNormal().dot(pathPlane.getNormal()) > 1e-3)
        {
            assert(false);
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::SWEEP_PathPlaneAndProfilePlaneAreNotOrthogonal));
            return TopoDS_Shape();
        }
    }

    TopoUtil::WireInfo pathWireInfo;
    wy::Vector3 pathStartPos;
    wy::Vector3 pathStartDir;
    ErrorCode errorCreatePathWire(ErrorCode::PATH_InvalidPath);
    if (pPathSketch)
    {
        errorCreatePathWire = createPathWire(*pPathSketch, pathWireInfo, pathStartPos, pathStartDir);
    }
    else if (pPathCurve)
    {
        errorCreatePathWire = createPathWire(*pPathCurve, pathWireInfo, pathStartPos, pathStartDir);
    }
    if (ErrorCode::NoError != errorCreatePathWire)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(errorCreatePathWire));
        return TopoDS_Shape();
    }
    assert(pathStartDir.length() > 0.5);

    std::vector<std::vector<TopoUtil::WireInfo>> profileWireInfos;
    {
        gp_Trsf trsf;
        ErrorCode error = createProfileWires(*pProfileSketch, trsf, profileWireInfos);
        if (ErrorCode::NoError != error)
        {
            assert(false);
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<unsigned int>(error));
            return TopoDS_Shape();
        }
    }

    std::vector<TopoDS_Shape> shapes;
    for (const std::vector<TopoUtil::WireInfo>& profileWireInfo : profileWireInfos)
    {
        TopoDS_Shape resultShape;
        ErrorCode error = makeSweep(this->getId().value(), pathWireInfo, profileWireInfo, resultShape, *pTopoNaming);
        if (ErrorCode::NoError != error)
        {
            assert(false);
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<unsigned int>(error));
            return TopoDS_Shape();
        }
        shapes.emplace_back(resultShape);
    }

    if (shapes.empty())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_NullShapeError));
        return TopoDS_Shape();
    }
    else if (shapes.size() == 1)
    {
        return shapes.front();
    }
    else
    {
        TopoDS_Compound compound;
        BRep_Builder brepBuilder;
        brepBuilder.MakeCompound(compound);
        for (const TopoDS_Shape& shape : shapes) { brepBuilder.Add(compound, shape); }
        return compound;
    }
}

NS_WY3D_END
