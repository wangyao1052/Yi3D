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
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dRevolvedSheet.h>
#include <wy3dParamNames.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchProfileForSheet.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "utils/Util.h"
#include "utils/RevolutionUtil.h"
#include "topo/SketchTopoBuilder.h"
#include "topo/TopoNamingUtil.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(RevolvedSheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(RevolvedSheet, _sketchId)
    REGISTER_FIELD(RevolvedSheet, _axisId)
    REGISTER_FIELD(RevolvedSheet, _startAngle)
    REGISTER_FIELD(RevolvedSheet, _endAngle)
END_FIELD_REGISTRATION()

RevolvedSheet::RevolvedSheet() : wy3d::Sheet(),
    _sketchId(wydb::ElementId::kNull),
    _axisId(wydb::ElementId::kNull),
    _startAngle(0.0),
    _endAngle(wy3d::TWO_PI)
{
}

RevolvedSheet::~RevolvedSheet()
{
}

wy::ErrorStatus RevolvedSheet::create(
    wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch, const wy3d::SketchCurve* pAxis, double startAngle, double endAngle,
    RevolvedSheet*& pOutSheet)
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
    if (!pAxis)
    {
        pOutSheet = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }
    // 轴必须是直线型曲线
    if (!wy3d::SketchLine::cast(pAxis) && !wy3d::SketchCenterLine::cast(pAxis))
    {
        pOutSheet = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }

    RevolvedSheet* pSheet = new RevolvedSheet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSheet);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSheet);
        pSheet = nullptr;
        return error;
    }

    error = pSheet->_setSketch(pSketch);
    CHECK_ERROR_FOR_CREATE(error, pSheet);
    error = pSheet->_setAxis(pAxis->getId());
    CHECK_ERROR_FOR_CREATE(error, pSheet);
    error = pSheet->setStartAngle(startAngle);
    CHECK_ERROR_FOR_CREATE(error, pSheet);
    error = pSheet->setEndAngle(endAngle);
    CHECK_ERROR_FOR_CREATE(error, pSheet);

    pOutSheet = pSheet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus RevolvedSheet::_setSketch(const wydb::ElementId& sketchId)
{
    if (sketchId == _sketchId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kRevolvedSheet_sketchId);
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

wy::ErrorStatus RevolvedSheet::_setSketch(wy3d::Sketch* pSketch)
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

wy::ErrorStatus RevolvedSheet::_setAxis(const wydb::ElementId& axisId)
{
    if (axisId == _axisId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kRevolvedSheet_axisId);
    if (wy::ErrorStatus::Ok == error)
    {
        _axisId = axisId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus RevolvedSheet::setAxis(const wy3d::SketchCurve* pAxis)
{
    if (!pAxis)
    {
        return wy::ErrorStatus::NullElementPointer;
    }
    if (!wy3d::SketchLine::cast(pAxis) && !wy3d::SketchCenterLine::cast(pAxis))
    {
        return wy::ErrorStatus::InvalidInput;
    }
    return this->_setAxis(pAxis->getId());
}

wy::ErrorStatus RevolvedSheet::setStartAngle(double startAngle)
{
    if (startAngle == _startAngle)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kRevolvedSheet_startAngle, wydb::ElementDataPieceType::Shape);
    if (wy::ErrorStatus::Ok == error)
    {
        _startAngle = startAngle;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus RevolvedSheet::setEndAngle(double endAngle)
{
    if (endAngle == _endAngle)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kRevolvedSheet_endAngle, wydb::ElementDataPieceType::Shape);
    if (wy::ErrorStatus::Ok == error)
    {
        _endAngle = endAngle;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

void RevolvedSheet::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::REVOLUTION_PARAM_START_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::REVOLUTION_PARAM_END_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr RevolvedSheet::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == RevolvedSheet::classInfo()->className())
    {
        if (ParamNames::REVOLUTION_PARAM_START_ANGLE == paramName)
        {
            return wydb::ParameterValue::createDouble(_startAngle);
        }
        else if (ParamNames::REVOLUTION_PARAM_END_ANGLE == paramName)
        {
            return wydb::ParameterValue::createDouble(_endAngle);
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus RevolvedSheet::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == RevolvedSheet::classInfo()->className())
    {
        if (ParamNames::REVOLUTION_PARAM_START_ANGLE == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setStartAngle(paramValue.asDouble());
        }
        else if (ParamNames::REVOLUTION_PARAM_END_ANGLE == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setEndAngle(paramValue.asDouble());
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool RevolvedSheet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kRevolvedSheet_sketchId.value():
        value = _sketchId;
        return true;
    case kRevolvedSheet_axisId.value():
        value = _axisId;
        return true;
    case kRevolvedSheet_startAngle.value():
        value = _startAngle;
        return true;
    case kRevolvedSheet_endAngle.value():
        value = _endAngle;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool RevolvedSheet::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kRevolvedSheet_sketchId.value():
        _sketchId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kRevolvedSheet_axisId.value():
        _axisId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kRevolvedSheet_startAngle.value():
        _startAngle = std::any_cast<double>(value);
        return true;
    case kRevolvedSheet_endAngle.value():
        _endAngle = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus RevolvedSheet::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _sketchId << _axisId << _startAngle << _endAngle;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus RevolvedSheet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _sketchId >> _axisId >> _startAngle >> _endAngle;
    return wy::ErrorStatus::Ok;
}

void RevolvedSheet::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_sketchId.isNull()) { dependencies.insert(_sketchId); }
    if (!_axisId.isNull()) { dependencies.insert(_axisId); }
}

bool RevolvedSheet::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    if (!_sketchId.isNull() && erasedDependencies.find(_sketchId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->_setSketch(wydb::ElementId::kNull);
        this->_setAxis(wydb::ElementId::kNull);
        return true;
    }
    else if (!_axisId.isNull() && erasedDependencies.find(_axisId) != erasedDependencies.cend())
    {
        this->_setAxis(wydb::ElementId::kNull);
        return true;
    }
    else
    {
        return __baseClass::onDependenciesErased(erasedDependencies);
    }
}

TopoDS_Shape RevolvedSheet::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
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

    std::pair<ErrorCode, gp_Ax1> axisResult = computeRevolutionAxis(pDb, _axisId);
    if (axisResult.first != ErrorCode::NoError)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(axisResult.first));
        return TopoDS_Shape();
    }
    gp_Ax1 axis = axisResult.second;

    SketchProfileForSheet profileForSheet(pSketch);
    if (!profileForSheet.check())
    {
        unsigned int errorCode = static_cast<unsigned int>(ErrorCode::PROFILE_InvalidProfile);
        if (profileForSheet.getError()) errorCode = static_cast<unsigned int>(profileForSheet.getError()->type);
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
    for (const SketchProfile::LoopSPtr pLoop : loops)
    {
        assert(pLoop);
        gp_Trsf identityTrsf;
        std::vector<TopoUtil::WireInfo> wireInfos;
        ErrorCode ec = TopoUtil::makeWires(pSketch, {pLoop}, identityTrsf, wireInfos);
        if (ec != ErrorCode::NoError || wireInfos.size() != 1)
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<unsigned int>(ec != ErrorCode::NoError ? ec : ErrorCode::TOPOSHAPE_GenerateShapeError));
            return TopoDS_Shape();
        }

        assert(1 == wireInfos.size());
        const TopoUtil::WireInfo& wireInfo = wireInfos.front();
        TopoDS_Wire wire = wireInfo.wire;
        std::vector<TopoUtil::EdgeNamingInfo> edgeNameInfos = wireInfo.edgeNameInfos;

        double totalAngle = _endAngle - _startAngle;
        if (totalAngle >= wy3d::TWO_PI) { totalAngle = wy3d::TWO_PI; }
        else if (totalAngle <= -wy3d::TWO_PI) { totalAngle = -wy3d::TWO_PI; }
        else if (_startAngle != 0.0)
        {
            gp_Trsf trsf;
            trsf.SetRotation(axis, _startAngle);
            BRepBuilderAPI_Transform transformer(wire, trsf);
            wire = TopoDS::Wire(transformer.Shape());
            if (wire.IsNull())
            {
                assert(false);
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_NullShapeError));
                return TopoDS_Shape();
            }
            for (TopoUtil::EdgeNamingInfo& edgeNameInfo : edgeNameInfos)
            {
                TopoDS_Shape modifiedShape = transformer.ModifiedShape(edgeNameInfo.edge);
                assert(!modifiedShape.IsNull());
                edgeNameInfo.edge = TopoDS::Edge(modifiedShape);
            }
        }

        BRepPrimAPI_MakeRevol makeRevol(wire, axis, totalAngle, Standard_True);
        makeRevol.Build();
        if (makeRevol.IsDone())
        {
            TopoDS_Shape revolShape = makeRevol.Shape();
            assert(TopAbs_ShapeEnum::TopAbs_SHELL == revolShape.ShapeType());
            shapes.emplace_back(revolShape);

            unsigned int idValue = this->getId().value();
            TopoNamingUtil::naming(wire, makeRevol, edgeNameInfos, idValue, *pTopoNaming);

#ifdef _DEBUG
            char szFileName[100] = { 0 };
            sprintf_s(szFileName, 100, "D:/logs/%d.txt", idValue);
            pTopoNaming->print(szFileName, revolShape);
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
