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
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dImpl.h>
#include <wy3dSketchParamNames.h>
#include <wy3dSketchCurveIntersectUtil.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SketchLine)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchLine, _startPnt)
    REGISTER_FIELD(SketchLine, _endPnt)
END_FIELD_REGISTRATION()

SketchLine::SketchLine() : wy3d::SketchCurve(), _startPnt(), _endPnt()
{
}

SketchLine::~SketchLine()
{
}

wy::ErrorStatus SketchLine::create(wydb::Transaction* pTrans, const wy::Vector2& startPnt, const wy::Vector2& endPnt, SketchLine*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }

    SketchLine* pSketchLine = new SketchLine();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchLine);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSketchLine); pSketchLine = nullptr; return error; }

    error = pSketchLine->setStartPoint(startPnt); CHECK_ERROR_FOR_CREATE(error, pSketchLine)
    error = pSketchLine->setEndPoint(endPnt); CHECK_ERROR_FOR_CREATE(error, pSketchLine)

    pOut = pSketchLine;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchLine::setStartPoint(const wy::Vector2& startPnt)
{
    if (startPnt == _startPnt) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchLine_startPnt);
    if (wy::ErrorStatus::Ok == error)
    {
        _startPnt = startPnt;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchLine::setEndPoint(const wy::Vector2& endPnt)
{
    if (endPnt == _endPnt) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchLine_endPnt);
    if (wy::ErrorStatus::Ok == error)
    {
        _endPnt = endPnt;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::Vector2 SketchLine::getPointAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    return _startPnt + (_endPnt - _startPnt) * t;
}

wy::Vector2 SketchLine::getDirectionAt(double t, bool clamp) const
{
    wy::Vector2 dir = _endPnt - _startPnt;
    dir.normalize();
    return dir;
}

wy::Vector2 SketchLine::getDirection() const
{
    wy::Vector2 dir = _endPnt - _startPnt;
    dir.normalize();
    return dir;
}

bool SketchLine::isDegenerate(double tol) const
{
    return this->getLength() < tol;
}

wy::ErrorStatus SketchLine::translate(const wy::Vector2& vector)
{
    if (vector == wy::Vector2::kZero) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->setStartPoint(_startPnt + vector);
    if (wy::ErrorStatus::Ok != error) return error;
    return this->setEndPoint(_endPnt + vector);
}

wy::ErrorStatus SketchLine::rotateAround(const wy::Vector2& center, double angle)
{
    if (angle == 0.0) return wy::ErrorStatus::Ok;

    double cosTheta = std::cos(angle);
    double sinTheta = std::sin(angle);
    wy::Vector2 newStartPnt = SketchEntity::rotateAround(_startPnt, center, cosTheta, sinTheta);
    wy::Vector2 newEndPnt = SketchEntity::rotateAround(_endPnt, center, cosTheta, sinTheta);

    wy::ErrorStatus error = this->setStartPoint(newStartPnt);
    if (wy::ErrorStatus::Ok != error) return error;
    return this->setEndPoint(newEndPnt);
}

wy::ErrorStatus SketchLine::transform(const wy3d::Matrix3& matrix)
{
    wy::ErrorStatus error = this->setStartPoint(_startPnt * matrix);
    if (wy::ErrorStatus::Ok != error) return error;
    return this->setEndPoint(_endPnt * matrix);
}

unsigned int SketchLine::intersectWith(const SketchCurve& other, std::vector<wy::Vector2>& out) const
{
    const SketchCurve* pOther = &other;
    if (const auto* pL = dynamic_cast<const SketchLine*>(pOther))
        return SketchCurveIntersectUtil::intersect(this, pL, out);
    else if (const auto* pCL = dynamic_cast<const SketchCenterLine*>(pOther))
        return SketchCurveIntersectUtil::intersect(this, pCL, out);
    else if (const auto* pC = dynamic_cast<const SketchCircle*>(pOther))
        return SketchCurveIntersectUtil::intersect(this, pC, out);
    else if (const auto* pA = dynamic_cast<const SketchArc*>(pOther))
        return SketchCurveIntersectUtil::intersect(this, pA, out);
    else if (const auto* pE = dynamic_cast<const SketchEllipse*>(pOther))
        return SketchCurveIntersectUtil::intersect(this, pE, out);
    else if (const auto* pEA = dynamic_cast<const SketchEllipseArc*>(pOther))
        return SketchCurveIntersectUtil::intersect(this, pEA, out);
    else if (const auto* pS = dynamic_cast<const SketchSpline*>(pOther))
        return SketchCurveIntersectUtil::intersect(this, pS, out);
    else { assert(false); return 0; }
}


void SketchLine::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_LINE_PARAM_START_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_LINE_PARAM_START_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_LINE_PARAM_END_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_LINE_PARAM_END_Y;
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
wydb::ParameterValueUPtr SketchLine::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == SketchLine::classInfo()->className()) {
        if (SketchParamNames::SKETCH_LINE_PARAM_START_X == paramName)
            return wydb::ParameterValue::createDouble(_startPnt.x());
        if (SketchParamNames::SKETCH_LINE_PARAM_START_Y == paramName)
            return wydb::ParameterValue::createDouble(_startPnt.y());
        if (SketchParamNames::SKETCH_LINE_PARAM_END_X == paramName)
            return wydb::ParameterValue::createDouble(_endPnt.x());
        if (SketchParamNames::SKETCH_LINE_PARAM_END_Y == paramName)
            return wydb::ParameterValue::createDouble(_endPnt.y());
        if (SketchParamNames::SKETCH_LINE_PARAM_LENGTH == paramName)
            return wydb::ParameterValue::createDouble((_endPnt - _startPnt).length());
        if (SketchParamNames::SKETCH_LINE_PARAM_ANGLE == paramName)
        {
            double angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, _endPnt - _startPnt);
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(angle));
        }
        return nullptr;
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus SketchLine::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == SketchLine::classInfo()->className()) {
        if (SketchParamNames::SKETCH_LINE_PARAM_START_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setStartPoint(wy::Vector2(paramValue.asDouble(), _startPnt.y())); }
        if (SketchParamNames::SKETCH_LINE_PARAM_START_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setStartPoint(wy::Vector2(_startPnt.x(), paramValue.asDouble())); }
        if (SketchParamNames::SKETCH_LINE_PARAM_END_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setEndPoint(wy::Vector2(paramValue.asDouble(), _endPnt.y())); }
        if (SketchParamNames::SKETCH_LINE_PARAM_END_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setEndPoint(wy::Vector2(_endPnt.x(), paramValue.asDouble())); }
        if (SketchParamNames::SKETCH_LINE_PARAM_LENGTH == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            double length = paramValue.asDouble();
            if (length <= 0.0) return wy::ErrorStatus::InvalidInput;
            wy::Vector2 dir = _endPnt - _startPnt; dir.normalize();
            if (dir.length() < 0.5) dir.set(1.0, 0.0);
            return this->setEndPoint(_startPnt + length * dir);
        }
        if (SketchParamNames::SKETCH_LINE_PARAM_ANGLE == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            double length = (_endPnt - _startPnt).length();
            double angle = wy3d::degreesToRadians(paramValue.asDouble());
            wy::Vector2 dir(std::cos(angle), std::sin(angle));
            return this->setEndPoint(_startPnt + dir * length);
        }
        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchLine::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchLine_startPnt.value():
        value = _startPnt;
        return true;
    case kSketchLine_endPnt.value():
        value = _endPnt;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchLine::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchLine_startPnt.value():
        _startPnt = std::any_cast<const wy::Vector2&>(value);
        return true;
    case kSketchLine_endPnt.value():
        _endPnt = std::any_cast<const wy::Vector2&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchLine::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _startPnt << _endPnt;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchLine::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _startPnt >> _endPnt;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
