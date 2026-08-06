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
#include <BRepPrimAPI_MakeRevol.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>
#include <BRepBuilderAPI_Transform.hxx>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dRevolution.h>
#include <wy3dParamNames.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchProfile.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "utils/Util.h"
#include "topo/SketchTopoBuilder.h"
#include "topo/TopoNamingUtil.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Revolution)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Revolution, _sketchId)
    REGISTER_FIELD(Revolution, _axisId)
    REGISTER_FIELD(Revolution, _startAngle)
    REGISTER_FIELD(Revolution, _endAngle)
END_FIELD_REGISTRATION()

Revolution::Revolution()
    : wy3d::Solid(),
      _sketchId(wydb::ElementId::kNull),
      _axisId(wydb::ElementId::kNull),
      _startAngle(0.0), _endAngle(wy3d::TWO_PI)
{
}

Revolution::~Revolution()
{
}

wy::ErrorStatus Revolution::create(
    wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch, const wy3d::SketchCurve* pAxis, double startAngle, double endAngle,
    Revolution*& pOutRevolution)
{
    if (!pTrans)
    {
        pOutRevolution = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }
    if (!pSketch || !pAxis)
    {
        pOutRevolution = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }
    Revolution* pRevolution = new Revolution();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pRevolution);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pRevolution);
        pRevolution = nullptr;
        return error;
    }

    error = pRevolution->_setSketch(pSketch);
    CHECK_ERROR_FOR_CREATE(error, pRevolution);
    error = pRevolution->setAxis(pAxis);
    CHECK_ERROR_FOR_CREATE(error, pRevolution);
    error = pRevolution->setStartAngle(startAngle);
    CHECK_ERROR_FOR_CREATE(error, pRevolution);
    error = pRevolution->setEndAngle(endAngle);
    CHECK_ERROR_FOR_CREATE(error, pRevolution);

    pOutRevolution = pRevolution;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Revolution::createCut(
    wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch, const wy3d::SketchCurve* pAxis, double startAngle, double endAngle,
    wy3d::Solid* pSolidToCut,
    Revolution*& pOutRevolution)
{
    if (!pTrans)
    {
        pOutRevolution = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }
    if (!pSketch || !pAxis)
    {
        pOutRevolution = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }
    Revolution* pRevolution = new Revolution();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pRevolution);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pRevolution);
        pRevolution = nullptr;
        return error;
    }

    error = pRevolution->_setSketch(pSketch);
    CHECK_ERROR_FOR_CREATE(error, pRevolution);
    error = pRevolution->setAxis(pAxis);
    CHECK_ERROR_FOR_CREATE(error, pRevolution);
    error = pRevolution->setStartAngle(startAngle);
    CHECK_ERROR_FOR_CREATE(error, pRevolution);
    error = pRevolution->setEndAngle(endAngle);
    CHECK_ERROR_FOR_CREATE(error, pRevolution);
    error = pRevolution->setCut(true);
    CHECK_ERROR_FOR_CREATE(error, pRevolution);

    if (pSolidToCut)
    {
        error = pSolidToCut->addModification(pRevolution);
        CHECK_ERROR_FOR_CREATE(error, pRevolution);
    }

    pOutRevolution = pRevolution;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Revolution::_setSketch(const wydb::ElementId& sketchId)
{
    if (sketchId == _sketchId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kRevolution_sketchId);
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

wy::ErrorStatus Revolution::_setSketch(wy3d::Sketch* pSketch)
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

wy::ErrorStatus Revolution::_setAxis(const wydb::ElementId& axisId)
{
    if (axisId == _axisId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kRevolution_axisId);
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

wy::ErrorStatus Revolution::setAxis(const wy3d::SketchCurve* pAxis)
{
    if (!pAxis)
    {
        return wy::ErrorStatus::NullElementPointer;
    }

    // 轴必须是直线型曲线（SketchLine 或 SketchCenterLine）
    if (!wy3d::SketchLine::cast(pAxis) && !wy3d::SketchCenterLine::cast(pAxis))
    {
        return wy::ErrorStatus::InvalidInput;
    }

    return this->_setAxis(pAxis->getId());
}

wy::ErrorStatus Revolution::setStartAngle(double startAngle)
{
    if (startAngle == _startAngle)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kRevolution_startAngle);
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

wy::ErrorStatus Revolution::setEndAngle(double endAngle)
{
    if (endAngle == _endAngle)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kRevolution_endAngle);
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


void Revolution::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::REVOLUTION_PARAM_AXIS;
        pParamSchema->addParameterDefinition(def);
    }
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
wydb::ParameterValueUPtr Revolution::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Revolution::classInfo()->className())
    {
        if (ParamNames::REVOLUTION_PARAM_AXIS == paramName)
        {
            return wydb::ParameterValue::createElementId(_axisId);
        }
        else if (ParamNames::REVOLUTION_PARAM_START_ANGLE == paramName)
        {
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_startAngle));
        }
        else if (ParamNames::REVOLUTION_PARAM_END_ANGLE == paramName)
        {
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_endAngle));
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus Revolution::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Revolution::classInfo()->className())
    {
        if (ParamNames::REVOLUTION_PARAM_AXIS == paramName)
        {
            if (!paramValue.isElementId()) return wy::ErrorStatus::InvalidInput;
            wydb::ElementId axisId = paramValue.asElementId();
            if (axisId.isNull()) return wy::ErrorStatus::InvalidInput;
            const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(getDatabase()->getElement(axisId));
            if (!pCurve) return wy::ErrorStatus::InvalidInput;
            return this->setAxis(pCurve);
        }
        else if (ParamNames::REVOLUTION_PARAM_START_ANGLE == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setStartAngle(wy3d::degreesToRadians(paramValue.asDouble()));
        }
        else if (ParamNames::REVOLUTION_PARAM_END_ANGLE == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setEndAngle(wy3d::degreesToRadians(paramValue.asDouble()));
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Revolution::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kRevolution_sketchId.value():
        value = _sketchId;
        return true;
    case kRevolution_axisId.value():
        value = _axisId;
        return true;
    case kRevolution_startAngle.value():
        value = _startAngle;
        return true;
    case kRevolution_endAngle.value():
        value = _endAngle;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Revolution::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kRevolution_sketchId.value():
        _sketchId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kRevolution_axisId.value():
        _axisId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kRevolution_startAngle.value():
        _startAngle = std::any_cast<double>(value);
        return true;
    case kRevolution_endAngle.value():
        _endAngle = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Revolution::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _sketchId << _axisId << _startAngle << _endAngle;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Revolution::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _sketchId >> _axisId >> _startAngle >> _endAngle;
    return wy::ErrorStatus::Ok;
}

void Revolution::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_sketchId.isNull()) { dependencies.insert(_sketchId); }
    if (!_axisId.isNull()) { dependencies.insert(_axisId); }
}

bool Revolution::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
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

TopoDS_Shape Revolution::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(_sketchId));
    if (!pSketch)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }

    if (_axisId.isNull())
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::REVOLUTION_UnspecifiedAxisLine));
        return TopoDS_Shape();
    }
    const wy3d::SketchCurve* pAxisCurve = wy3d::SketchCurve::cast(pDb->getElement(_axisId));
    if (!pAxisCurve)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::REVOLUTION_InvalidRevolutionAxisLine));
        return TopoDS_Shape();
    }
    // 轴必须是直线型曲线
    if (!wy3d::SketchLine::cast(pAxisCurve) && !wy3d::SketchCenterLine::cast(pAxisCurve))
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::REVOLUTION_InvalidRevolutionAxisLine));
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
    wy::Vector3 sketchOrigin = sketchPlane.getOrigin();
    gp_Pln sketchPln(gp_Pnt(sketchOrigin.x(), sketchOrigin.y(), sketchOrigin.z()), sketchNormalDir);

    // 轴可能来自其他草图，用轴所属草图的平面做 2D→3D 转换
    const wy3d::Sketch* pAxisSketch = wy3d::Sketch::cast(pDb->getElement(pAxisCurve->getParent()));
    if (!pAxisSketch)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::REVOLUTION_InvalidRevolutionAxisLine));
        return TopoDS_Shape();
    }
    const wy3d::SketchPlane& axisSketchPlane = pAxisSketch->getPlane();
    if (!axisSketchPlane.isValid())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::REVOLUTION_InvalidRevolutionAxisLine));
        return TopoDS_Shape();
    }
    wy::Vector3 axisStartPnt = axisSketchPlane.value(pAxisCurve->getStartPoint());
    wy::Vector3 axisEndPnt = axisSketchPlane.value(pAxisCurve->getEndPoint());
    wy::Vector3 axisDir = axisEndPnt - axisStartPnt;
    if (axisDir.length() <= wy3d::EPS)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::REVOLUTION_InvalidRevolutionAxisLine));
        return TopoDS_Shape();
    }
    axisDir.normalize();
    gp_Ax1 axis(gp_Pnt(axisStartPnt.x(), axisStartPnt.y(), axisStartPnt.z()), gp_Dir(axisDir.x(), axisDir.y(), axisDir.z()));

    SketchTopoBuilder sketchTopoBuilder(pSketch);

    SketchProfile sketchProfile(pSketch);
    if (!sketchProfile.check())
    {
        assert(false);
        std::shared_ptr<SketchError> pError = sketchProfile.getError();
        if (pError)
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<unsigned int>(pError->type));
        }
        else
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        }
        return TopoDS_Shape();
    }

    std::vector<TopoDS_Shape> shapes;
    const std::vector<SketchProfile::FaceSPtr>& sketchFaces = sketchProfile.getFaces();
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

        double totalAngle = _endAngle - _startAngle;
        if (totalAngle >= wy3d::TWO_PI) { totalAngle = wy3d::TWO_PI; }
        else if (totalAngle <= -wy3d::TWO_PI) { totalAngle = -wy3d::TWO_PI; }
        else if (_startAngle != 0.0)
        {
            gp_Trsf trsf;
            trsf.SetRotation(axis, _startAngle);
            BRepBuilderAPI_Transform transformer(face, trsf);
            face = TopoDS::Face(transformer.Shape());
            if (face.IsNull())
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

        BRepPrimAPI_MakeRevol makeRevol(face, axis, totalAngle);
        makeRevol.Build();
        if (makeRevol.IsDone())
        {
            TopoDS_Shape revol = makeRevol.Shape();
            shapes.emplace_back(revol);
            unsigned int idValue = this->getId().value();
            TopoNamingUtil::naming(face, makeRevol, edgeNameInfos, idValue, *pTopoNaming,
                profileIndex);
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
    else if (shapes.size() == 1)
    {
#ifdef _DEBUG
        char szFileName[100] = { 0 };
        sprintf_s(szFileName, 100, "D:/logs/%d.txt", this->getId().value());
        pTopoNaming->print(szFileName, shapes.front());
#endif
        return shapes.front();
    }
    else
    {
        TopoDS_Compound compound;
        BRep_Builder brepBuilder;
        brepBuilder.MakeCompound(compound);
        for (const TopoDS_Shape& shape : shapes) { brepBuilder.Add(compound, shape); }
#ifdef _DEBUG
        char szFileName[100] = { 0 };
        sprintf_s(szFileName, 100, "D:/logs/%d.txt", this->getId().value());
        pTopoNaming->print(szFileName, compound);
#endif
        return compound;
    }
}

NS_WY3D_END
