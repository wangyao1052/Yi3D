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
#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dImpl.h>
#include <wy3dSketchParamNames.h>
#include <wy3dSketchCurveIntersectUtil.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SketchCenterLine)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchCenterLine, _startPnt)
    REGISTER_FIELD(SketchCenterLine, _endPnt)
END_FIELD_REGISTRATION()

SketchCenterLine::SketchCenterLine() : wy3d::SketchCurve(), _startPnt(), _endPnt()
{
}

SketchCenterLine::~SketchCenterLine()
{
}

wy::ErrorStatus SketchCenterLine::create(wydb::Transaction* pTrans, const wy::Vector2& sp, const wy::Vector2& ep, SketchCenterLine*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }

    SketchCenterLine* pSketchCenterLine = new SketchCenterLine();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchCenterLine);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSketchCenterLine); pSketchCenterLine = nullptr; return error; }

    error = pSketchCenterLine->setStartPoint(sp); CHECK_ERROR_FOR_CREATE(error, pSketchCenterLine)
    error = pSketchCenterLine->setEndPoint(ep); CHECK_ERROR_FOR_CREATE(error, pSketchCenterLine)

    pOut = pSketchCenterLine;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchCenterLine::setStartPoint(const wy::Vector2& sp)
{
    if (sp == _startPnt) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchCenterLine_startPnt);
    if (wy::ErrorStatus::Ok == error)
    {
        _startPnt = sp;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchCenterLine::setEndPoint(const wy::Vector2& ep)
{
    if (ep == _endPnt) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchCenterLine_endPnt);
    if (wy::ErrorStatus::Ok == error)
    {
        _endPnt = ep;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::Vector2 SketchCenterLine::getPointAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    return _startPnt + (_endPnt - _startPnt) * t;
}

wy::Vector2 SketchCenterLine::getDirection() const
{
    wy::Vector2 d = _endPnt - _startPnt; d.normalize(); return d;
}

bool SketchCenterLine::isDegenerate(double tol) const { return this->getLength() < tol; }

wy::ErrorStatus SketchCenterLine::translate(const wy::Vector2& v)
{
    if (v == wy::Vector2::kZero) return wy::ErrorStatus::Ok;
    wy::ErrorStatus e = this->setStartPoint(_startPnt + v);
    if (e != wy::ErrorStatus::Ok) return e;
    return this->setEndPoint(_endPnt + v);
}

wy::ErrorStatus SketchCenterLine::rotateAround(const wy::Vector2& center, double angle)
{
    if (angle == 0.0) return wy::ErrorStatus::Ok;
    double c = std::cos(angle), s = std::sin(angle);
    wy::Vector2 ns = SketchEntity::rotateAround(_startPnt, center, c, s);
    wy::Vector2 ne = SketchEntity::rotateAround(_endPnt, center, c, s);
    wy::ErrorStatus e = this->setStartPoint(ns);
    if (e != wy::ErrorStatus::Ok) return e;
    return this->setEndPoint(ne);
}

wy::ErrorStatus SketchCenterLine::transform(const wy3d::Matrix3& matrix)
{
    wy::ErrorStatus e = this->setStartPoint(_startPnt * matrix);
    if (e != wy::ErrorStatus::Ok) return e;
    return this->setEndPoint(_endPnt * matrix);
}

unsigned int SketchCenterLine::intersectWith(const SketchCurve& other, std::vector<wy::Vector2>& out) const
{
    const SketchCurve* pO = &other;
    if (const auto* pL = dynamic_cast<const SketchLine*>(pO)) return SketchCurveIntersectUtil::intersect(pL, this, out);
    else if (const auto* pCL = dynamic_cast<const SketchCenterLine*>(pO)) return SketchCurveIntersectUtil::intersect(pCL, this, out);
    else if (const auto* pC = dynamic_cast<const SketchCircle*>(pO)) return SketchCurveIntersectUtil::intersect(this, pC, out);
    else if (const auto* pA = dynamic_cast<const SketchArc*>(pO)) return SketchCurveIntersectUtil::intersect(this, pA, out);
    else if (const auto* pE = dynamic_cast<const SketchEllipse*>(pO)) return SketchCurveIntersectUtil::intersect(this, pE, out);
    else if (const auto* pEA = dynamic_cast<const SketchEllipseArc*>(pO)) return SketchCurveIntersectUtil::intersect(this, pEA, out);
    else if (const auto* pS = dynamic_cast<const SketchSpline*>(pO)) return SketchCurveIntersectUtil::intersect(this, pS, out);
    else { assert(false); return 0; }
}


void SketchCenterLine::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_CENTER_LINE_PARAM_START_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_CENTER_LINE_PARAM_START_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_CENTER_LINE_PARAM_END_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_CENTER_LINE_PARAM_END_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_LINE_PARAM_LENGTH;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_LINE_PARAM_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr SketchCenterLine::getParameterValue(const std::string& className, const std::string& n) const
{
    if (className == SketchCenterLine::classInfo()->className()) {
        if (SketchParamNames::SKETCH_CENTER_LINE_PARAM_START_X == n) return wydb::ParameterValue::createDouble(_startPnt.x());
        if (SketchParamNames::SKETCH_CENTER_LINE_PARAM_START_Y == n) return wydb::ParameterValue::createDouble(_startPnt.y());
        if (SketchParamNames::SKETCH_CENTER_LINE_PARAM_END_X == n) return wydb::ParameterValue::createDouble(_endPnt.x());
        if (SketchParamNames::SKETCH_CENTER_LINE_PARAM_END_Y == n) return wydb::ParameterValue::createDouble(_endPnt.y());
        if (SketchParamNames::SKETCH_LINE_PARAM_LENGTH == n) return wydb::ParameterValue::createDouble((_endPnt - _startPnt).length());
        if (SketchParamNames::SKETCH_LINE_PARAM_ANGLE == n)
        {
            double angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, _endPnt - _startPnt);
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(angle));
        }
    }
    return __baseClass::getParameterValue(className, n);
}

wy::ErrorStatus SketchCenterLine::setParameterValue(const std::string& className, const std::string& n, const wydb::ParameterValue& v)
{
    if (className == SketchCenterLine::classInfo()->className()) {
        if (SketchParamNames::SKETCH_LINE_PARAM_LENGTH == n)
        {
            if (!v.isDouble()) return wy::ErrorStatus::InvalidInput;
            double length = v.asDouble();
            if (length <= 0.0) return wy::ErrorStatus::InvalidInput;
            wy::Vector2 dir = _endPnt - _startPnt; dir.normalize();
            if (dir.length() < 0.5) dir.set(1.0, 0.0);
            return this->setEndPoint(_startPnt + length * dir);
        }
        if (SketchParamNames::SKETCH_LINE_PARAM_ANGLE == n)
        {
            if (!v.isDouble()) return wy::ErrorStatus::InvalidInput;
            double length = (_endPnt - _startPnt).length();
            double angle = wy3d::degreesToRadians(v.asDouble());
            wy::Vector2 dir(std::cos(angle), std::sin(angle));
            return this->setEndPoint(_startPnt + dir * length);
        }
        if (!v.isDouble()) return wy::ErrorStatus::InvalidInput; double d = v.asDouble();
        if (SketchParamNames::SKETCH_CENTER_LINE_PARAM_START_X == n) return this->setStartPoint(wy::Vector2(d, _startPnt.y()));
        if (SketchParamNames::SKETCH_CENTER_LINE_PARAM_START_Y == n) return this->setStartPoint(wy::Vector2(_startPnt.x(), d));
        if (SketchParamNames::SKETCH_CENTER_LINE_PARAM_END_X == n) return this->setEndPoint(wy::Vector2(d, _endPnt.y()));
        if (SketchParamNames::SKETCH_CENTER_LINE_PARAM_END_Y == n) return this->setEndPoint(wy::Vector2(_endPnt.x(), d));
    }
    return __baseClass::setParameterValue(className, n, v);
}

bool SketchCenterLine::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchCenterLine_startPnt.value():
        value = _startPnt;
        return true;
    case kSketchCenterLine_endPnt.value():
        value = _endPnt;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchCenterLine::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchCenterLine_startPnt.value():
        _startPnt = std::any_cast<const wy::Vector2&>(value);
        return true;
    case kSketchCenterLine_endPnt.value():
        _endPnt = std::any_cast<const wy::Vector2&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchCenterLine::writeToFiler(wydb::OutFiler& f) const { __baseClass::writeToFiler(f); f << _startPnt << _endPnt; return wy::ErrorStatus::Ok; }
wy::ErrorStatus SketchCenterLine::readFromFiler(wydb::InFiler& f) { __baseClass::readFromFiler(f); f >> _startPnt >> _endPnt; return wy::ErrorStatus::Ok; }

NS_WY3D_END
