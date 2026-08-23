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
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dParamNames.h>
#include <wy3dParamEnumDef.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketch.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchProfileForSheet.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "utils/Util.h"
#include "topo/SketchTopoBuilder.h"
#include "topo/TopoNamingUtil.h"

#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(ExtrudedSheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(ExtrudedSheet, _sketchId)
    REGISTER_FIELD(ExtrudedSheet, _direction)
    REGISTER_FIELD(ExtrudedSheet, _depth)
    REGISTER_FIELD(ExtrudedSheet, _startOffset)
END_FIELD_REGISTRATION()

ExtrudedSheet::ExtrudedSheet() : wy3d::Sheet(), _sketchId(wydb::ElementId::kNull), _direction(ExtrusionDirection::OneSide), _depth(0.0), _startOffset(0.0)
{
}

ExtrudedSheet::~ExtrudedSheet()
{
}

wy::ErrorStatus ExtrudedSheet::create(
    wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch, double depth,
    ExtrudedSheet*& pOutSheet)
{
    return ExtrudedSheet::create(pTrans, pSketch, ExtrusionDirection::OneSide, depth, pOutSheet);
}

wy::ErrorStatus ExtrudedSheet::create(
    wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch,
    ExtrusionDirection direction,
    double depth,
    ExtrudedSheet*& pOutSheet)
{
    if (!pTrans)
    {
        pOutSheet = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }
    if (!pSketch)
    {
        pOutSheet = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }

    ExtrudedSheet* pSheet = new ExtrudedSheet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSheet);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSheet);
        pSheet = nullptr;
        return error;
    }

    error = pSheet->_setSketch(pSketch);
    CHECK_ERROR_FOR_CREATE(error, pSheet);
    error = pSheet->setDirection(direction);
    CHECK_ERROR_FOR_CREATE(error, pSheet);
    error = pSheet->setDepth(depth);
    CHECK_ERROR_FOR_CREATE(error, pSheet);

    pOutSheet = pSheet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus ExtrudedSheet::_setSketch(const wydb::ElementId& sketchId)
{
    if (sketchId == _sketchId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kExtrudedSheet_sketchId);
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

wy::ErrorStatus ExtrudedSheet::_setSketch(wy3d::Sketch* pSketch)
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
    error = this->_setSketch(pSketch->getId());
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

wy::ErrorStatus ExtrudedSheet::setDepth(double depth)
{
    if (std::fabs(depth) < wy3d::kMinValue || std::fabs(depth) > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (depth == _depth)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kExtrudedSheet_depth, wydb::ElementDataPieceType::Shape);
    if (wy::ErrorStatus::Ok == error)
    {
        _depth = depth;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus ExtrudedSheet::setStartOffset(double startOffset)
{
    if (startOffset > wy3d::kMaxValue || startOffset < -wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (startOffset == _startOffset)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kExtrudedSheet_startOffset, wydb::ElementDataPieceType::Shape);
    if (wy::ErrorStatus::Ok == error)
    {
        _startOffset = startOffset;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus ExtrudedSheet::setDirection(ExtrusionDirection direction)
{
    if (direction < ExtrusionDirection::OneSide || direction > ExtrusionDirection::Symmetric)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (direction == _direction)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kExtrudedSheet_direction, wydb::ElementDataPieceType::Shape);
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

void ExtrudedSheet::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::EXTRUSION_PARAM_DIRECTION;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::EXTRUSION_PARAM_DEPTH;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::EXTRUSION_PARAM_START_OFFSET;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr ExtrudedSheet::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == ExtrudedSheet::classInfo()->className())
    {
        if (ParamNames::EXTRUSION_PARAM_DEPTH == paramName)
        {
            return wydb::ParameterValue::createDouble(_depth);
        }
        else if (ParamNames::EXTRUSION_PARAM_START_OFFSET == paramName)
        {
            return wydb::ParameterValue::createDouble(_startOffset);
        }
        else if (ParamNames::EXTRUSION_PARAM_DIRECTION == paramName)
        {
            return wydb::ParameterValue::createAny(
                wy3d::ParamEnumDef(
                    {{static_cast<int>(ExtrusionDirection::OneSide), "One Side"},
                     {static_cast<int>(ExtrusionDirection::Symmetric), "Symmetric"}},
                    static_cast<int>(_direction)));
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, paramName);
}

static int _extractEnumValue(const wydb::ParameterValue& paramValue)
{
    if (paramValue.isInteger())
        return paramValue.asInteger();
    if (paramValue.isAny())
    {
        const auto* pAnyVal = dynamic_cast<const wydb::AnyParameterValue*>(&paramValue);
        if (pAnyVal)
        {
            const wy3d::ParamEnumDef* pDef = pAnyVal->tryGet<wy3d::ParamEnumDef>();
            if (pDef) return pDef->currentValue;
        }
    }
    return -1;
}

wy::ErrorStatus ExtrudedSheet::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == ExtrudedSheet::classInfo()->className())
    {
        if (ParamNames::EXTRUSION_PARAM_DEPTH == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setDepth(paramValue.asDouble());
        }
        else if (ParamNames::EXTRUSION_PARAM_START_OFFSET == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setStartOffset(paramValue.asDouble());
        }
        else if (ParamNames::EXTRUSION_PARAM_DIRECTION == paramName)
        {
            int enumValue = _extractEnumValue(paramValue);
            if (enumValue < static_cast<int>(ExtrusionDirection::OneSide) ||
                enumValue > static_cast<int>(ExtrusionDirection::Symmetric))
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setDirection(static_cast<ExtrusionDirection>(enumValue));
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool ExtrudedSheet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kExtrudedSheet_sketchId.value():
        value = _sketchId;
        return true;
    case kExtrudedSheet_depth.value():
        value = _depth;
        return true;
    case kExtrudedSheet_startOffset.value():
        value = _startOffset;
        return true;
    case kExtrudedSheet_direction.value():
        value = _direction;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool ExtrudedSheet::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kExtrudedSheet_sketchId.value():
        _sketchId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kExtrudedSheet_depth.value():
        _depth = std::any_cast<double>(value);
        return true;
    case kExtrudedSheet_startOffset.value():
        _startOffset = std::any_cast<double>(value);
        return true;
    case kExtrudedSheet_direction.value():
        _direction = std::any_cast<ExtrusionDirection>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus ExtrudedSheet::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);

    filer << _sketchId;
    if (filer.getFileVersion() > wydb::FileVersion(0, 18))
    {
        filer << static_cast<std::int32_t>(_direction);
    }
    filer << _depth << _startOffset;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus ExtrudedSheet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);

    filer >> _sketchId;
    if (filer.getFileVersion() > wydb::FileVersion(0, 18))
    {
        std::int32_t directionInt(0);
        filer >> directionInt;
        _direction = static_cast<ExtrusionDirection>(directionInt);
    }
    filer >> _depth >> _startOffset;
    return wy::ErrorStatus::Ok;
}

void ExtrudedSheet::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_sketchId.isNull())
    {
        dependencies.insert(_sketchId);
    }
}

bool ExtrudedSheet::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    if (!_sketchId.isNull() && erasedDependencies.find(_sketchId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->_setSketch(wydb::ElementId::kNull);
        return true;
    }
    else
    {
        return __baseClass::onDependenciesErased(erasedDependencies);
    }
}

TopoDS_Shape ExtrudedSheet::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
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

    const wy3d::SketchPlane& sketchPlane = pSketch->getPlane();
    if (!sketchPlane.isValid())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }
    wy::Vector3 sketchNormal = sketchPlane.getNormal();
    assert(sketchNormal.length() > 0.5);
    gp_Vec sketchNormalVec(sketchNormal.x(), sketchNormal.y(), sketchNormal.z());
    gp_Dir sketchNormalDir(sketchNormalVec);
    // Symmetric: the wire is moved by -|depth|/2 along the normal and extruded
    // by the full |depth|, so the shell spans [-|depth|/2, +|depth|/2]
    const bool isSymmetric = (ExtrusionDirection::Symmetric == _direction);
    const double halfDepth = isSymmetric ? std::fabs(_depth) / 2.0 : 0.0;
    const double faceOffsetAlongNormal = _startOffset - halfDepth;
    const double extrudeLen = isSymmetric ? std::fabs(_depth) : _depth;
    gp_Trsf sketchTranslation;
    if (faceOffsetAlongNormal != 0.0)
    {
        sketchTranslation.SetTranslation(sketchNormalVec * faceOffsetAlongNormal);
    }

    SketchProfileForSheet profileForSheet(pSketch);
    if (!profileForSheet.check())
    {
        std::shared_ptr<SketchError> pError = profileForSheet.getError();
        unsigned int errorCode = pError ? static_cast<unsigned int>(pError->type)
            : static_cast<unsigned int>(ErrorCode::PROFILE_InvalidProfile);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), errorCode);
        return TopoDS_Shape();
    }
    const std::vector<SketchProfile::LoopSPtr>& loops = profileForSheet.getLoops();
    if (loops.empty())
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }

    std::vector<TopoDS_Shape> shapes;
    for (const SketchProfile::LoopSPtr& pLoop : loops)
    {
        std::vector<TopoUtil::WireInfo> wireInfos;
        ErrorCode errorCode = TopoUtil::makeWires(pSketch, {pLoop}, sketchTranslation, wireInfos);
        if (errorCode != ErrorCode::NoError || wireInfos.size() != 1)
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<unsigned int>(errorCode != ErrorCode::NoError ? errorCode : ErrorCode::TOPOSHAPE_GenerateShapeError));
            return TopoDS_Shape();
        }

        assert(1 == wireInfos.size());
        const TopoUtil::WireInfo& wireInfo = wireInfos.front();
        BRepPrimAPI_MakePrism makePrism(wireInfo.wire, sketchNormalVec * extrudeLen, Standard_True);
        makePrism.Build();
        if (makePrism.IsDone())
        {
            TopoDS_Shape prism = makePrism.Shape();
            assert(TopAbs_ShapeEnum::TopAbs_SHELL == prism.ShapeType());
            shapes.emplace_back(prism);

            unsigned int idValue = this->getId().value();
            TopoNamingUtil::naming(wireInfo.wire, makePrism, wireInfo.edgeNameInfos,
                idValue, *pTopoNaming);

#ifdef _DEBUG
            char szFileName[100] = { 0 };
            sprintf_s(szFileName, 100, "D:/logs/%d.txt", idValue);
            pTopoNaming->print(szFileName, prism);
#endif
        }
        else
        {
            assert(false);
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_GenerateShapeError));
            return TopoDS_Shape();
        }
    }

    if (shapes.empty())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_NullShapeError));
        return TopoDS_Shape();
    }
    else
    {
        TopoDS_Compound compound;
        BRep_Builder brepBuilder;
        brepBuilder.MakeCompound(compound);
        for (const TopoDS_Shape& shape : shapes)
        {
            brepBuilder.Add(compound, shape);
        }
        return compound;
    }
}

NS_WY3D_END
