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
#include <gp_Lin2d.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <TopoDS_Edge.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dHelix.h>
#include <wy3dParamNames.h>
#include <wy3dSketch.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchCenterLine.h>

#include "utils/OccUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Helix)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Helix, _sketchId)
    REGISTER_FIELD(Helix, _pitch)
    REGISTER_FIELD(Helix, _turns)
    REGISTER_FIELD(Helix, _startAngle)
    REGISTER_FIELD(Helix, _helixFlags)
END_FIELD_REGISTRATION()

Helix::Helix() : wy3d::Curve(), _sketchId(wydb::ElementId::kNull), _pitch(0.0), _turns(0.0), _startAngle(0.0), _helixFlags(0)
{
}

Helix::~Helix()
{
}

wy::ErrorStatus Helix::create(wydb::Transaction* pTrans, wy3d::Sketch* pSketch, double pitch, double turns, double startAngle, Helix*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }
    if (!pSketch) { pOut = nullptr; return wy::ErrorStatus::NullElementPointer; }

    Helix* pHelix = new Helix();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pHelix);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pHelix); pHelix = nullptr; return error; }

    error = pHelix->_setSketch(pSketch); CHECK_ERROR_FOR_CREATE(error, pHelix);
    error = pHelix->setPitch(pitch); CHECK_ERROR_FOR_CREATE(error, pHelix);
    error = pHelix->setTurns(turns); CHECK_ERROR_FOR_CREATE(error, pHelix);
    error = pHelix->setStartAngle(startAngle); CHECK_ERROR_FOR_CREATE(error, pHelix);

    pOut = pHelix;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Helix::_setSketch(const wydb::ElementId& sketchId)
{
    if (sketchId == _sketchId) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kHelix_sketchId);
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

wy::ErrorStatus Helix::_setSketch(wy3d::Sketch* pSketch)
{
    assert(_sketchId.isNull());
    if (!pSketch) return wy::ErrorStatus::NullElementPointer;
    if (!pSketch->getParent().isNull()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->_setSketch(pSketch->getId());
    if (error != wy::ErrorStatus::Ok) return error;

    error = pSketch->setOwner(this->getId());
    return error;
}

wy::ErrorStatus Helix::setPitch(double pitch)
{
    if (pitch < 0.0 || pitch < wy3d::kMinValue || pitch > wy3d::kMaxHelixPitch)
        return wy::ErrorStatus::InvalidInput;
    if (pitch == _pitch) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kHelix_pitch);
    if (wy::ErrorStatus::Ok == error)
    {
        _pitch = pitch;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Helix::setTurns(double turns)
{
    if (turns < 0.0 || turns < wy3d::kMinValue || turns > wy3d::kMaxHelixTurns)
        return wy::ErrorStatus::InvalidInput;
    if (turns == _turns) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kHelix_turns);
    if (wy::ErrorStatus::Ok == error)
    {
        _turns = turns;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Helix::setStartAngle(double startAngle)
{
    if (startAngle == _startAngle) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kHelix_startAngle);
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

wy::ErrorStatus Helix::setClockWise(bool clockwise)
{
    if (this->hasHelixFlag(HelixFlag::ClockWise) == clockwise) return wy::ErrorStatus::Ok;
    if (clockwise) return this->enableHelixFlag(HelixFlag::ClockWise);
    else return this->disableHelixFlag(HelixFlag::ClockWise);
}

wy::ErrorStatus Helix::setReversed(bool reversed)
{
    if (this->hasHelixFlag(HelixFlag::Reversed) == reversed) return wy::ErrorStatus::Ok;
    if (reversed) return this->enableHelixFlag(HelixFlag::Reversed);
    else return this->disableHelixFlag(HelixFlag::Reversed);
}

bool Helix::hasHelixFlag(HelixFlag flag) const
{
    return _helixFlags & static_cast<std::uint32_t>(flag);
}

wy::ErrorStatus Helix::enableHelixFlag(HelixFlag flag)
{
    if (this->hasHelixFlag(flag)) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kHelix_helixFlags);
    if (wy::ErrorStatus::Ok == error)
    {
        _helixFlags |= static_cast<std::uint32_t>(flag);
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Helix::disableHelixFlag(HelixFlag flag)
{
    if (!this->hasHelixFlag(flag)) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kHelix_helixFlags);
    if (wy::ErrorStatus::Ok == error)
    {
        _helixFlags &= (~static_cast<std::uint32_t>(flag));
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void Helix::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::HELIX_IS_CLOCKWISE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::HELIX_IS_REVERSED;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::HELIX_PITCH;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::HELIX_START_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::HELIX_TURNS;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Helix::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Helix::classInfo()->className()) {
        if (ParamNames::HELIX_PITCH == paramName) return wydb::ParameterValue::createDouble(_pitch);
        if (ParamNames::HELIX_TURNS == paramName) return wydb::ParameterValue::createDouble(_turns);
        if (ParamNames::HELIX_START_ANGLE == paramName) return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_startAngle));
        if (ParamNames::HELIX_IS_CLOCKWISE == paramName) return wydb::ParameterValue::createBoolean(this->isClockWise());
        if (ParamNames::HELIX_IS_REVERSED == paramName) return wydb::ParameterValue::createBoolean(this->isReversed());
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus Helix::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Helix::classInfo()->className()) {
        if (ParamNames::HELIX_PITCH == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setPitch(paramValue.asDouble()); }
        if (ParamNames::HELIX_TURNS == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setTurns(paramValue.asDouble()); }
        if (ParamNames::HELIX_START_ANGLE == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setStartAngle(wy3d::degreesToRadians(paramValue.asDouble())); }
        if (ParamNames::HELIX_IS_CLOCKWISE == paramName)
        { if (!paramValue.isBoolean()) return wy::ErrorStatus::InvalidInput; return this->setClockWise(paramValue.asBoolean()); }
        if (ParamNames::HELIX_IS_REVERSED == paramName)
        { if (!paramValue.isBoolean()) return wy::ErrorStatus::InvalidInput; return this->setReversed(paramValue.asBoolean()); }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Helix::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kHelix_sketchId.value():
        value = _sketchId;
        return true;
    case kHelix_pitch.value():
        value = _pitch;
        return true;
    case kHelix_turns.value():
        value = _turns;
        return true;
    case kHelix_startAngle.value():
        value = _startAngle;
        return true;
    case kHelix_helixFlags.value():
        value = _helixFlags;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Helix::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kHelix_sketchId.value():
        _sketchId = std::any_cast<wydb::ElementId>(value);
        return true;
    case kHelix_pitch.value():
        _pitch = std::any_cast<double>(value);
        return true;
    case kHelix_turns.value():
        _turns = std::any_cast<double>(value);
        return true;
    case kHelix_startAngle.value():
        _startAngle = std::any_cast<double>(value);
        return true;
    case kHelix_helixFlags.value():
        _helixFlags = std::any_cast<std::uint32_t>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Helix::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _sketchId << _pitch << _turns << _startAngle << _helixFlags;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Helix::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _sketchId >> _pitch >> _turns >> _startAngle >> _helixFlags;
    return wy::ErrorStatus::Ok;
}

void Helix::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_sketchId.isNull()) dependencies.insert(_sketchId);
}

bool Helix::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    if (!_sketchId.isNull() && erasedDependencies.find(_sketchId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->_setSketch(wydb::ElementId::kNull);
        return true;
    }

    return responsed;
}

TopoDS_Edge Helix::generateShape(
    wydb::ChainUpdateFeedbackCollector& feedbackCollector) const
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
        return TopoDS_Edge();
    }

    const wy3d::SketchCircle* pCircle(nullptr);
    for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::Element* pElem = pDb->getElement(iter.current());
        if (!pElem) { assert(false); continue; }

        const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pElem);
        if (!pCurve) continue;
        if (pCurve->isConstruction()) continue;

        const wyrx::ClassInfo* classInfo = pCurve->getClassInfo();
        if (classInfo == wy3d::SketchCenterLine::classInfo()) continue;

        if (classInfo != wy3d::SketchCircle::classInfo())
        {
            assert(false);
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::HELIX_InvalidSketch));
            return TopoDS_Edge();
        }

        if (pCircle)
        {
            assert(false);
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::HELIX_InvalidSketch));
            return TopoDS_Edge();
        }
        pCircle = wy3d::SketchCircle::cast(pCurve);
        assert(pCircle);
    }
    if (!pCircle)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::HELIX_InvalidSketch));
        return TopoDS_Edge();
    }

    const wy3d::Plane& sketchPlane = pSketch->getPlane();
    if (!sketchPlane.isValid())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Edge();
    }

    wy::Vector3 origin = sketchPlane.value(pCircle->getCenter());
    wy::Vector3 normal = sketchPlane.getNormal();
    wy::Vector3 xAxis = sketchPlane.getXDir();
    double radius = pCircle->getRadius();
    gp_Ax3 ax3(OccUtil::toPnt(origin), OccUtil::toDir(normal), OccUtil::toDir(xAxis));
    Handle(Geom_CylindricalSurface) cylinderSurf = new Geom_CylindricalSurface(ax3, radius);

    double baseAngleDir = this->isClockWise() ? -wy3d::TWO_PI : wy3d::TWO_PI;
    if (this->isReversed()) baseAngleDir = -baseAngleDir;
    gp_Lin2d line2d(gp_Pnt2d(wy3d::normalizeRadian(_startAngle), 0.0),
        gp_Dir2d(baseAngleDir, _pitch));
    Handle(Geom2d_Line) geomLine2d = new Geom2d_Line(line2d);
    double lineSeg2dLen = std::sqrt(_pitch * _pitch + wy3d::TWO_PI * wy3d::TWO_PI) * _turns;
    if (this->isReversed()) lineSeg2dLen = -lineSeg2dLen;
    Handle(Geom2d_TrimmedCurve) geomLineSeg2d = new Geom2d_TrimmedCurve(geomLine2d, 0.0, lineSeg2dLen);

    BRepBuilderAPI_MakeEdge mkEdge(geomLineSeg2d, cylinderSurf);
    if (!mkEdge.IsDone())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_GenerateShapeError));
        return TopoDS_Edge();
    }
    TopoDS_Edge edge = mkEdge.Edge();

    return edge;
}

NS_WY3D_END
