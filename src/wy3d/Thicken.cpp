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
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepLib.hxx>
#include <ShapeAnalysis_FreeBoundsProperties.hxx>
#include <BRepAlgo_Image.hxx>
#include <BRepOffset_Mode.hxx>
#include <GeomAbs_JoinType.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Iterator.hxx>
#include <BRep_Builder.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <BRepAlgoAPI_Fuse.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dThicken.h>
#include <wy3dSheet.h>
#include <wy3dImpl.h>
#include <wy3dParamNames.h>
#include <wy3dErrorCode.h>
#include <wy3dParamEnumDef.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include <BRepTools.hxx>

#include "topo/TopoNamingUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Thicken)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Thicken, _source)
    REGISTER_FIELD(Thicken, _thickness)
    REGISTER_FIELD(Thicken, _direction)
END_FIELD_REGISTRATION()

Thicken::Thicken() : wy3d::Solid(),
    _source(wydb::ElementId::kNull),
    _thickness(0.0),
    _direction(ThickenDirection::OneSide)
{
}

Thicken::~Thicken()
{
}

wy::ErrorStatus Thicken::create(
    wydb::Transaction* pTrans,
    wy3d::Sheet* pSheet,
    double thickness,
    ThickenDirection direction,
    Thicken*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }
    if (!pSheet)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }
    if (std::fabs(thickness) < wy3d::kMinValue ||
        std::fabs(thickness) > wy3d::kMaxValue)
    {
        pOut = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }

    Thicken* pObj = new Thicken();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pObj);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pObj);
        pObj = nullptr;
        return error;
    }

    error = pObj->setSourceSheet(pSheet);
    CHECK_ERROR_FOR_CREATE(error, pObj);
    error = pObj->setThickness(thickness);
    CHECK_ERROR_FOR_CREATE(error, pObj);
    error = pObj->setDirection(direction);
    CHECK_ERROR_FOR_CREATE(error, pObj);

    pOut = pObj;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Thicken::setSourceSheet(const wydb::ElementId& sheetId)
{
    if (sheetId == _source)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kThicken_source);
    if (wy::ErrorStatus::Ok == error)
    {
        _source = sheetId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Thicken::setSourceSheet(wy3d::Sheet* pSheet)
{
    assert(_source.isNull());

    if (!pSheet)
    {
        return wy::ErrorStatus::NullElementPointer;
    }

    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    assert(!pSheet->getId().isNull());
    error = this->setSourceSheet(pSheet->getId());
    if (wy::ErrorStatus::Ok != error)
    {
        return error;
    }

    error = pSheet->setParent(this->getId());
    if (wy::ErrorStatus::Ok != error)
    {
        return error;
    }

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Thicken::setThickness(double thickness)
{
    if (std::fabs(thickness) < wy3d::kMinValue ||
        std::fabs(thickness) > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (thickness == _thickness)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(
        kThicken_thickness, wydb::ElementDataPieceType::Shape);
    if (wy::ErrorStatus::Ok == error)
    {
        _thickness = thickness;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Thicken::setDirection(ThickenDirection direction)
{
    if (direction == _direction)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(
        kThicken_direction, wydb::ElementDataPieceType::Shape);
    if (wy::ErrorStatus::Ok == error)
    {
        _direction = direction;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

void Thicken::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::THICKEN_PARAM_SOURCE;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::THICKEN_PARAM_THICKNESS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::THICKEN_PARAM_DIRECTION;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr Thicken::getParameterValue(
    const std::string& className,
    const std::string& paramName) const
{
    if (className == Thicken::classInfo()->className())
    {
        if (ParamNames::THICKEN_PARAM_SOURCE == paramName)
        {
            return wydb::ParameterValue::createInteger(_source.value());
        }
        else if (ParamNames::THICKEN_PARAM_THICKNESS == paramName)
        {
            return wydb::ParameterValue::createDouble(_thickness);
        }
        else if (ParamNames::THICKEN_PARAM_DIRECTION == paramName)
        {
            return wydb::ParameterValue::createAny(
                wy3d::ParamEnumDef(
                    {{static_cast<int>(ThickenDirection::OneSide), "One Side"},
                     {static_cast<int>(ThickenDirection::Symmetric), "Symmetric"}},
                    static_cast<int>(_direction)));
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus Thicken::setParameterValue(
    const std::string& className,
    const std::string& paramName,
    const wydb::ParameterValue& paramValue)
{
    if (className == Thicken::classInfo()->className())
    {
        if (ParamNames::THICKEN_PARAM_SOURCE == paramName)
        {
            assert(false);
            return wy::ErrorStatus::ParameterReadonly;
        }
        else if (ParamNames::THICKEN_PARAM_THICKNESS == paramName)
        {
            if (!paramValue.isDouble())
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setThickness(paramValue.asDouble());
        }
        else if (ParamNames::THICKEN_PARAM_DIRECTION == paramName)
        {
            int enumVal = -1;
            if (paramValue.isInteger())
                enumVal = paramValue.asInteger();
            else if (paramValue.isAny())
            {
                const auto* pAnyVal = dynamic_cast<const wydb::AnyParameterValue*>(&paramValue);
                if (pAnyVal)
                {
                    const wy3d::ParamEnumDef* pDef = pAnyVal->tryGet<wy3d::ParamEnumDef>();
                    if (pDef) enumVal = pDef->currentValue;
                }
            }
            if (enumVal < 0 || enumVal > 1)
                return wy::ErrorStatus::InvalidInput;
            return this->setDirection(static_cast<ThickenDirection>(enumVal));
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Thicken::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kThicken_source.value():
        value = _source;
        return true;
    case kThicken_thickness.value():
        value = _thickness;
        return true;
    case kThicken_direction.value():
        value = _direction;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Thicken::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kThicken_source.value():
        _source = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kThicken_thickness.value():
        _thickness = std::any_cast<double>(value);
        return true;
    case kThicken_direction.value():
        _direction = std::any_cast<ThickenDirection>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Thicken::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _source << _thickness << static_cast<std::int32_t>(_direction);
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Thicken::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _source >> _thickness;
    std::int32_t directionInt(0);
    filer >> directionInt;
    _direction = static_cast<ThickenDirection>(directionInt);
    return wy::ErrorStatus::Ok;
}

void Thicken::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_source.isNull())
    {
        dependencies.insert(_source);
    }
}

bool Thicken::onDependenciesErased(
    const std::set<wydb::ElementId>& erasedDependencies)
{
    if (!_source.isNull() &&
        erasedDependencies.find(_source) != erasedDependencies.cend())
    {
        this->erase(true);
        this->setSourceSheet(wydb::ElementId::kNull);
        return true;
    }
    else
    {
        return __baseClass::onDependenciesErased(erasedDependencies);
    }
}

// Thicken a shell into a solid.
// 1. Offset the shell with BRepOffsetAPI_MakeOffsetShape
// 2. Find boundary wires, map to offset edges, create side walls via ThruSections
// 3. Sew original + side walls + offset >>> closed shell
// 4. Convert closed shell >>> solid
static std::pair<TopoDS_Shape, TopoNamingSPtr> thickenShell(
    const TopoDS_Shell& shell,
    double thickness,
    double tol,
    const TopoNaming& sourceNaming,
    unsigned int idValue)
{
    TopoDS_Shape nullShape;

    // offset the shell
    BRepOffsetAPI_MakeOffsetShape mkOffset;
    mkOffset.PerformByJoin(shell, thickness, tol,
        BRepOffset_Skin, Standard_False, Standard_False, GeomAbs_Intersection);
    if (!mkOffset.IsDone())
    {
        return { nullShape, nullptr };
    }
    TopoDS_Shape offsetShape = mkOffset.Shape();
    if (offsetShape.IsNull())
    {
        return { nullShape, nullptr };
    }
    TopAbs_ShapeEnum offsetShellType = offsetShape.ShapeType();
    if (TopAbs_SHELL != offsetShellType)
    {
        return { nullShape, nullptr };
    }
    TopoDS_Shell offsetShell = TopoDS::Shell(offsetShape);

    // find open boundary wires of the shell.
    // An open shell has free edges (belonging to only 1 face).
    // Those edges are connected into closed boundary loops (wires).
    // If the shell is already closed (no free edges), tricken is meaningless.
    ShapeAnalysis_FreeBoundsProperties freeCheck(shell);
    freeCheck.Perform();
    if (freeCheck.NbClosedFreeBounds() < 1)
    {
        return { nullShape, nullptr };
    }

    // Name offset faces and edges: v1:<srcFace/Edge>+@thickenId
    // Also copy source face names so they survive sewing
    TopoNamingSPtr pNaming = std::make_shared<TopoNaming>();
    TopoNamingUtil::naming(shell, sourceNaming, mkOffset, idValue, *pNaming, 1);

    // build side walls for each boundary
    BRep_Builder builder;
    std::vector<TopoDS_Shell> sideShells;
    std::vector<TopoDS_Wire> origWires;
    const BRepAlgo_Image& edgeImage = mkOffset.MakeOffset().OffsetEdgesFromShapes();
    for (Standard_Integer i = 1; i <= freeCheck.NbClosedFreeBounds(); ++i)
    {
        TopoDS_Wire origWire = freeCheck.ClosedFreeBound(i)->FreeBound();
        if (origWire.IsNull())
        {
            assert(false);
            continue;
        }
        origWires.push_back(origWire);

        // build offset wire by mapping each edge
        TopoDS_Wire offsetWire;
        builder.MakeWire(offsetWire);
        bool allMapped = true;
        for (TopExp_Explorer ex(origWire, TopAbs_EDGE); ex.More(); ex.Next())
        {
            const TopoDS_Edge& origEdge = TopoDS::Edge(ex.Current());
            if (!edgeImage.HasImage(origEdge))
            {
                assert(false);
                allMapped = false;
                break;
            }

            const TopTools_ListOfShape& images = edgeImage.Image(origEdge);
            TopTools_ListIteratorOfListOfShape it;
            TopoDS_Edge mappedEdge;
            Standard_Integer edgeCount = 0;
            for (it.Initialize(images); it.More(); it.Next())
            {
                if (it.Value().ShapeType() == TopAbs_EDGE)
                {
                    ++edgeCount;
                    mappedEdge = TopoDS::Edge(it.Value());
                }
            }
            if (edgeCount != 1)
            {
                assert(false);
                allMapped = false;
                break;
            }
            builder.Add(offsetWire, mappedEdge);
        }
        if (!allMapped || offsetWire.IsNull())
        {
            assert(false);
            continue;
        }

        // create side wall via loft between orig wire and offset wire
        BRepOffsetAPI_ThruSections sideBuilder(Standard_False, Standard_False);
        sideBuilder.AddWire(origWire);
        sideBuilder.AddWire(offsetWire);
        sideBuilder.Build();
        if (!sideBuilder.IsDone())
        {
            assert(false);
            return { nullShape, nullptr };
        }

        // name side wall faces: v1:<srcEdge>+@thickenId
        for (TopExp_Explorer ex(origWire, TopAbs_EDGE); ex.More(); ex.Next())
        {
            const TopoDS_Edge& sourceEdge = TopoDS::Edge(ex.Current());
            TopoName srcName;
            if (!sourceNaming.getName(sourceEdge, srcName) || srcName.empty())
            {
                assert(false);
                continue;
            }
            const TopTools_ListOfShape& generated = sideBuilder.Generated(sourceEdge);
            assert(generated.Size() == 1);
            for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
            {
                const TopoDS_Shape& genShape = iter.Value();
                assert(!genShape.IsNull());
                assert(genShape.ShapeType() == TopAbs_ShapeEnum::TopAbs_FACE);
                pNaming->setName(genShape, TopoNameBuilder(srcName).generated(idValue).build());
            }
        }

        TopoDS_Shape sideShape = sideBuilder.Shape();
        if (sideShape.ShapeType() != TopAbs_SHELL)
        {
            assert(false);
            return { nullShape, nullptr };
        }
        sideShells.push_back(TopoDS::Shell(sideShape));
    }

    // sew original + side walls + offset
    BRepBuilderAPI_Sewing sewer;
    sewer.Add(shell);
    for (const TopoDS_Shell& sideShell : sideShells)
        sewer.Add(sideShell);
    sewer.Add(offsetShell);
    sewer.Perform();
    TopoDS_Shape sewed = sewer.SewedShape();
    if (sewed.IsNull())
    {
        assert(false);
        return { nullShape, nullptr };
    }
    TopAbs_ShapeEnum sewedShapeType = sewed.ShapeType();
    if (TopAbs_SHELL != sewedShapeType)
    {
        assert(false);
        return { nullShape, nullptr };
    }

    // convert closed shell to solid
    TopoDS_Shell closedShell = TopoDS::Shell(sewed);
    BRepBuilderAPI_MakeSolid solidMaker(closedShell);
    TopoDS_Solid resultSolid;
    if (!solidMaker.IsDone())
    {
        assert(false);
        return { nullShape, nullptr };
    }
    resultSolid = solidMaker.Solid();
    BRepLib::OrientClosedSolid(resultSolid);

    // Migrate names through sewing: sewer.Modified(preSew) → postSew
    {
        TopoNamingSPtr pNewNaming = std::make_shared<TopoNaming>();
        const TopoNaming::NameMap& nameMap = pNaming->getNameMap();
        for (const auto& kvp : nameMap)
        {
            const TopoDS_Shape& postSew = sewer.Modified(kvp.first);
            if (!postSew.IsNull())
            {
                pNewNaming->setName(postSew, kvp.second);
            }
        }
        const TopoNaming::NameMap& nameMapSrc = sourceNaming.getNameMap();
        for (const auto& kvp : nameMapSrc)
        {
            const TopoDS_Shape& postSew = sewer.Modified(kvp.first);
            if (!postSew.IsNull())
            {
                pNewNaming->setName(postSew, kvp.second);
            }
        }
        *pNaming = *pNewNaming;
    }

    {
        // name remaining unnamed edges via adjacent face names
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
        TopExp::MapShapesAndAncestors(resultSolid, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);
        for (int i = 1; i <= edgeFaceMap.Extent(); ++i)
        {
            const TopoDS_Edge& edge = TopoDS::Edge(edgeFaceMap.FindKey(i));
            TopoName existing;
            if (pNaming->getName(edge, existing) && !existing.empty())
                continue;
            const TopTools_ListOfShape& faces = edgeFaceMap.FindFromIndex(i);
            if (faces.Extent() != 2)
            {
                assert(false);
                continue;
            }
            TopTools_ListIteratorOfListOfShape it(faces);
            TopoName name1, name2;
            if (pNaming->getName(it.Value(), name1) && !name1.empty())
            {
                it.Next();
                if (pNaming->getName(it.Value(), name2) && !name2.empty())
                {
                    pNaming->setName(edge,
                        TopoNameBuilder(name1).source(name2).generated(idValue).build());
                }
            }
        }
    }

    return { resultSolid, pNaming };
}

// Thicken to both sides without boolean fuse — sew two offset shells + side walls
static std::pair<TopoDS_Shape, TopoNamingSPtr> thickenShellBothSide(
    const TopoDS_Shell& shell,
    double thickness,
    double tol,
    const TopoNaming& sourceNaming,
    unsigned int idValue)
{
    TopoDS_Shape nullShape;

    // offset to both sides
    BRepOffsetAPI_MakeOffsetShape mkOffsetOut, mkOffsetIn;
    mkOffsetOut.PerformByJoin(shell,  thickness, tol,
        BRepOffset_Skin, Standard_False, Standard_False, GeomAbs_Intersection);
    mkOffsetIn.PerformByJoin(shell, -thickness, tol,
        BRepOffset_Skin, Standard_False, Standard_False, GeomAbs_Intersection);
    if (!mkOffsetOut.IsDone() || !mkOffsetIn.IsDone())
        return { nullShape, nullptr };

    TopoDS_Shape offsetOut = mkOffsetOut.Shape();
    TopoDS_Shape offsetIn  = mkOffsetIn.Shape();
    if (offsetOut.IsNull() || offsetIn.IsNull())
        return { nullShape, nullptr };

    TopAbs_ShapeEnum offsetOutShapeType = offsetOut.ShapeType();
    if (TopAbs_SHELL != offsetOutShapeType)
    {
        assert(false);
        return { nullShape, nullptr };
    }
    TopAbs_ShapeEnum offsetInShapeType = offsetIn.ShapeType();
    if (TopAbs_SHELL != offsetInShapeType)
    {
        assert(false);
        return { nullShape, nullptr };
    }

    // find boundary wires
    ShapeAnalysis_FreeBoundsProperties freeCheck(shell);
    freeCheck.Perform();
    if (freeCheck.NbClosedFreeBounds() < 1)
        return { nullShape, nullptr };

    // name offset faces and edges
    TopoNamingSPtr pNaming = std::make_shared<TopoNaming>();
    TopoNamingUtil::naming(shell, sourceNaming, mkOffsetOut, idValue, *pNaming, 1);
    TopoNamingUtil::naming(shell, sourceNaming, mkOffsetIn,  idValue, *pNaming, 2);

    // build side walls connecting offsetOut ↔ offsetIn at each boundary
    BRep_Builder builder;
    std::vector<TopoDS_Shell> sideShells;
    const BRepAlgo_Image& imgOut = mkOffsetOut.MakeOffset().OffsetEdgesFromShapes();
    const BRepAlgo_Image& imgIn  = mkOffsetIn.MakeOffset().OffsetEdgesFromShapes();
    for (Standard_Integer i = 1; i <= freeCheck.NbClosedFreeBounds(); ++i)
    {
        TopoDS_Wire origWire = freeCheck.ClosedFreeBound(i)->FreeBound();
        if (origWire.IsNull()) continue;

        TopoDS_Wire wireOut, wireIn;
        builder.MakeWire(wireOut);
        builder.MakeWire(wireIn);
        bool ok = true;

        for (TopExp_Explorer ex(origWire, TopAbs_EDGE); ex.More(); ex.Next())
        {
            const TopoDS_Edge& e = TopoDS::Edge(ex.Current());
            if (!imgOut.HasImage(e) || !imgIn.HasImage(e)) { ok = false; break; }

            auto mapEdge = [&ok](const TopTools_ListOfShape& images) -> TopoDS_Edge {
                TopTools_ListIteratorOfListOfShape it;
                TopoDS_Edge mappedEdge;
                Standard_Integer edgeCount = 0;
                for (it.Initialize(images); it.More(); it.Next())
                {
                    if (it.Value().ShapeType() == TopAbs_EDGE)
                    {
                        ++edgeCount;
                        mappedEdge = TopoDS::Edge(it.Value());
                    }
                }
                if (edgeCount != 1) { ok = false; return TopoDS_Edge(); }
                return mappedEdge;
            };

            TopoDS_Edge eOut = mapEdge(imgOut.Image(e));
            TopoDS_Edge eIn  = mapEdge(imgIn.Image(e));
            if (eOut.IsNull() || eIn.IsNull()) { ok = false; break; }
            builder.Add(wireOut, eOut);
            builder.Add(wireIn,  eIn);
        }

        if (!ok || wireOut.IsNull() || wireIn.IsNull()) continue;

        BRepOffsetAPI_ThruSections sideBuilder(Standard_False, Standard_False);
        sideBuilder.AddWire(wireOut);
        sideBuilder.AddWire(wireIn);
        sideBuilder.Build();
        if (!sideBuilder.IsDone())
            return { nullShape, nullptr };

        // name side wall faces: source edge name → Generated(wireOut edge) → face
        {
            TopExp_Explorer exOrig(origWire, TopAbs_EDGE);
            TopExp_Explorer exOut(wireOut, TopAbs_EDGE);
            for (; exOrig.More() && exOut.More(); exOrig.Next(), exOut.Next())
            {
                const TopoDS_Edge& sourceEdge = TopoDS::Edge(exOrig.Current());
                TopoName srcName;
                if (!sourceNaming.getName(sourceEdge, srcName) || srcName.empty())
                    continue;
                const TopoDS_Edge& wireEdge = TopoDS::Edge(exOut.Current());
                const TopTools_ListOfShape& generated = sideBuilder.Generated(wireEdge);
                for (TopTools_ListIteratorOfListOfShape iter(generated); iter.More(); iter.Next())
                {
                    if (iter.Value().ShapeType() == TopAbs_FACE)
                        pNaming->setName(iter.Value(),
                            TopoNameBuilder(srcName).generated(idValue).build());
                }
            }
        }

        TopoDS_Shape sideShape = sideBuilder.Shape();
        if (sideShape.ShapeType() != TopAbs_SHELL)
            return { nullShape, nullptr };
        sideShells.push_back(TopoDS::Shell(sideShape));
    }

    // sew offsetOut + offsetIn + side walls
    BRepBuilderAPI_Sewing sewer;
    sewer.Add(offsetOut);
    sewer.Add(offsetIn);
    for (const auto& s : sideShells)
        sewer.Add(s);
    sewer.Perform();

    TopoDS_Shape sewed = sewer.SewedShape();
    if (sewed.IsNull())
        return { nullShape, nullptr };

    if (sewed.ShapeType() != TopAbs_SHELL)
        return { nullShape, nullptr };

    // convert to solid
    TopoDS_Shell closedShell = TopoDS::Shell(sewed);
    BRepBuilderAPI_MakeSolid solidMaker(closedShell);
    if (!solidMaker.IsDone())
        return { nullShape, nullptr };
    TopoDS_Solid resultSolid = solidMaker.Solid();
    BRepLib::OrientClosedSolid(resultSolid);

    // migrate names through sewing
    {
        TopoNamingSPtr pNewNaming = std::make_shared<TopoNaming>();
        const auto& nameMap = pNaming->getNameMap();
        for (const auto& kvp : nameMap)
        {
            const TopoDS_Shape& postSew = sewer.Modified(kvp.first);
            if (!postSew.IsNull())
                pNewNaming->setName(postSew, kvp.second);
        }
        *pNaming = *pNewNaming;
    }

    // name remaining unnamed edges via adjacent face names
    {
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
        TopExp::MapShapesAndAncestors(resultSolid, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);
        for (int i = 1; i <= edgeFaceMap.Extent(); ++i)
        {
            const TopoDS_Edge& edge = TopoDS::Edge(edgeFaceMap.FindKey(i));
            TopoName existing;
            if (pNaming->getName(edge, existing) && !existing.empty())
                continue;
            const TopTools_ListOfShape& faces = edgeFaceMap.FindFromIndex(i);
            if (faces.Extent() != 2) continue;
            TopTools_ListIteratorOfListOfShape it(faces);
            TopoName name1, name2;
            if (pNaming->getName(it.Value(), name1) && !name1.empty())
            {
                it.Next();
                if (pNaming->getName(it.Value(), name2) && !name2.empty())
                {
                    pNaming->setName(edge,
                        TopoNameBuilder(name1).source(name2).generated(idValue).build());
                }
            }
        }
    }

    return { resultSolid, pNaming };
}

TopoDS_Shape Thicken::generateShape(
    TopoNaming* pTopoNaming,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);

    const wy3d::Sheet* pSheet = wy3d::Sheet::cast(
        pDb->getElement(_source));
    if (!pSheet)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::THICKEN_InvalidData));
        return TopoDS_Shape();
    }

    TopoDS_Shape sheetShape = pSheet->getShape();
    if (sheetShape.IsNull())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::THICKEN_InvalidData));
        return TopoDS_Shape();
    }

    // collect all shells (recursively)
    std::vector<TopoDS_Shell> shells;
    if (sheetShape.ShapeType() == TopAbs_SHELL)
    {
        shells.push_back(TopoDS::Shell(sheetShape));
    }
    else
    {
        for (TopExp_Explorer ex(sheetShape, TopAbs_SHELL); ex.More(); ex.Next())
            shells.push_back(TopoDS::Shell(ex.Current()));
    }

    if (shells.empty())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::THICKEN_InvalidData));
        return TopoDS_Shape();
    }

    unsigned int idValue = this->getId().value();
    const TopoNaming* pSourceNaming = pSheet->getTopoNaming();

    try
    {
        std::vector<TopoDS_Shape> thickenResults;
        std::vector<TopoNamingSPtr> allNamings;

        for (const TopoDS_Shell& shell : shells)
        {
            if (_direction == ThickenDirection::Symmetric)
            {
                auto [result, pShellNaming] = thickenShellBothSide(shell,
                    _thickness / 2.0, wy3d::TOL * 10, *pSourceNaming, idValue);
                if (result.IsNull())
                {
                    wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                        static_cast<std::uint32_t>(ErrorCode::THICKEN_GenerateError));
                    return TopoDS_Shape();
                }
                thickenResults.push_back(result);
                allNamings.push_back(pShellNaming);
            }
            else
            {
                auto [result, pShellNaming] = thickenShell(shell, _thickness, wy3d::TOL * 10,
                    *pSourceNaming, idValue);
                if (result.IsNull())
                {
                    wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                        static_cast<std::uint32_t>(ErrorCode::THICKEN_GenerateError));
                    return TopoDS_Shape();
                }
                thickenResults.push_back(result);
                allNamings.push_back(pShellNaming);
            }
        }

        // flush all namings only after all shells succeed
        for (const auto& pNaming : allNamings)
        {
            for (const auto& kvp : pNaming->getNameMap())
                pTopoNaming->setName(kvp.first, kvp.second);
        }

        // combine results
        TopoDS_Shape finalResult;
        if (thickenResults.size() == 1)
        {
            finalResult = thickenResults.front();
        }
        else
        {
            TopoDS_Compound compound;
            BRep_Builder brepBuilder;
            brepBuilder.MakeCompound(compound);
            for (const TopoDS_Shape& s : thickenResults)
            {
                brepBuilder.Add(compound, s);
            }
            finalResult = compound;
        }

#ifdef _DEBUG
        char szFileName[100] = { 0 };
        sprintf_s(szFileName, 100, "D:/logs/%d.txt", idValue);
        pTopoNaming->print(szFileName, finalResult);
#endif

        return finalResult;
    }
    catch (const Standard_Failure&)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::THICKEN_GenerateError));
        return TopoDS_Shape();
    }
}

NS_WY3D_END
