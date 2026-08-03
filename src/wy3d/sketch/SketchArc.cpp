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

#include <cmath>
#include <cassert>
#include <wyVector2.h>
#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchArc.h>
#include <wy3dImpl.h>
#include <wy3dSketchParamNames.h>
#include <wy3dSketchCurveIntersectUtil.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SketchArc)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchArc, _centerPnt)
    REGISTER_FIELD(SketchArc, _radius)
    REGISTER_FIELD(SketchArc, _startAngle)
    REGISTER_FIELD(SketchArc, _endAngle)
END_FIELD_REGISTRATION()

SketchArc::SketchArc() : wy3d::SketchCurve(), _centerPnt(), _radius(0.0), _startAngle(0.0), _endAngle(0.0)
{
}

SketchArc::~SketchArc()
{
}

wy::ErrorStatus SketchArc::create(wydb::Transaction* pTrans, const wy::Vector2& center, double radius,
    double startAngle, double endAngle, SketchArc*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }

    SketchArc* pSketchArc = new SketchArc();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchArc);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSketchArc); pSketchArc = nullptr; return error; }

    error = pSketchArc->setCenter(center); CHECK_ERROR_FOR_CREATE(error, pSketchArc)
    error = pSketchArc->setRadius(radius); CHECK_ERROR_FOR_CREATE(error, pSketchArc)
    error = pSketchArc->setStartAngle(startAngle); CHECK_ERROR_FOR_CREATE(error, pSketchArc)
    error = pSketchArc->setEndAngle(endAngle); CHECK_ERROR_FOR_CREATE(error, pSketchArc)

    pOut = pSketchArc;
    return wy::ErrorStatus::Ok;
}

wy::Vector2 SketchArc::getStartPoint() const
{ return wy::Vector2(_centerPnt.x() + _radius * std::cos(_startAngle), _centerPnt.y() + _radius * std::sin(_startAngle)); }

wy::Vector2 SketchArc::getEndPoint() const
{ return wy::Vector2(_centerPnt.x() + _radius * std::cos(_endAngle), _centerPnt.y() + _radius * std::sin(_endAngle)); }

wy::Vector2 SketchArc::getPointAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    double angle = this->getTotalAngle() * t + _startAngle;
    return wy::Vector2(_centerPnt.x() + _radius * std::cos(angle), _centerPnt.y() + _radius * std::sin(angle));
}

wy::Vector2 SketchArc::getDirectionAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    double angle = this->getTotalAngle() * t + _startAngle;
    wy::Vector2 dir = wy::Vector2(-std::sin(angle), std::cos(angle));
    dir.normalize();
    return dir;
}

bool SketchArc::isDegenerate(double tol) const { return _radius < tol || this->getLength() < tol; }

double SketchArc::getLength() const
{
    assert(this->getTotalAngle() >= 0.0);
    assert(this->getTotalAngle() < wy3d::PI * 2);
    return _radius * this->getTotalAngle();
}

bool SketchArc::isAngleInArc(double angle) const
{
    if (angle < _startAngle)
    {
        double twoPI = wy3d::PI * 2;
        while (angle < _startAngle) angle += twoPI;
    }
    else if (angle > _startAngle)
    {
        double twoPI = wy3d::PI * 2;
        while ((angle - _startAngle) >= twoPI) angle -= twoPI;
    }
    else return true;

    assert((angle - _startAngle) >= 0.0);
    assert((angle - _startAngle) < wy3d::PI * 2);
    return (angle - _startAngle) <= this->getTotalAngle();
}

wy3d::BoundingBox2 SketchArc::getBoundingBox() const
{
    wy3d::BoundingBox2 bbox;
    bbox.merge(this->getStartPoint());
    bbox.merge(this->getEndPoint());

    static double angles[4] = { 0, wy3d::PI_2, wy3d::PI, wy3d::PI_2 * 3 };
    for (int i = 0; i < 4; ++i)
    {
        double angle = angles[i];
        if (this->isAngleInArc(angle))
        {
            bbox.merge(_centerPnt + wy::Vector2(_radius * std::cos(angle), _radius * std::sin(angle)));
        }
    }
    bbox.expand(wy3d::TOL);
    return bbox;
}

wy::ErrorStatus SketchArc::setCenter(const wy::Vector2& centerPoint)
{
    if (_centerPnt == centerPoint) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchArc_centerPnt);
    if (wy::ErrorStatus::Ok == error)
    {
        _centerPnt = centerPoint;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchArc::setRadius(double radius)
{
    if (radius < wy3d::kMinValue || radius > wy3d::kMaxValue) return wy::ErrorStatus::InvalidInput;
    if (_radius == radius) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchArc_radius);
    if (wy::ErrorStatus::Ok == error)
    {
        _radius = radius;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchArc::setStartAngle(double startAngle)
{
    if (_startAngle == startAngle) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchArc_startAngle);
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

wy::ErrorStatus SketchArc::setEndAngle(double endAngle)
{
    if (_endAngle == endAngle) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchArc_endAngle);
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

double SketchArc::getTotalAngle() const { return wy3d::normalizeRadian(_endAngle - _startAngle); }

wy::Vector2 SketchArc::getMiddlePoint() const
{
    double angle = _startAngle + this->getTotalAngle() / 2;
    return _centerPnt + wy::Vector2(_radius * std::cos(angle), _radius * std::sin(angle));
}

wy::ErrorStatus SketchArc::translate(const wy::Vector2& vector)
{
    if (vector == wy::Vector2::kZero) return wy::ErrorStatus::Ok;
    return this->setCenter(_centerPnt + vector);
}

wy::ErrorStatus SketchArc::rotateAround(const wy::Vector2& center, double angle)
{
    if (angle == 0.0) return wy::ErrorStatus::Ok;

    double cosTheta = std::cos(angle);
    double sinTheta = std::sin(angle);
    wy::Vector2 newCenterPnt = SketchEntity::rotateAround(_centerPnt, center, cosTheta, sinTheta);
    wy::Vector2 newStartPnt = SketchEntity::rotateAround(this->getStartPoint(), center, cosTheta, sinTheta);
    double newStartAngle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), newStartPnt - newCenterPnt);
    double newEndAngle = this->getTotalAngle() + newStartAngle;

    wy::ErrorStatus error = this->setCenter(newCenterPnt);
    if (wy::ErrorStatus::Ok != error) return error;
    error = this->setStartAngle(newStartAngle);
    if (wy::ErrorStatus::Ok != error) return error;
    return this->setEndAngle(newEndAngle);
}

wy::ErrorStatus SketchArc::transform(const wy3d::Matrix3& matrix)
{
    bool isMirrored = (matrix.determinant() < 0);
    double scaleX = matrix.get(0, 0);
    double scaleY = matrix.get(1, 1);
    bool isScaled = std::fabs(scaleX - scaleY) <= wy3d::TOL && std::fabs(scaleX - 1.0) > wy3d::TOL;

    wy::Vector2 newCenterPnt = _centerPnt * matrix;
    double totalAngle = this->getTotalAngle();
    double newStartAngle;
    if (isMirrored)
        newStartAngle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), this->getEndPoint() * matrix - newCenterPnt);
    else
        newStartAngle = wy::Vector2::rotationAngle(wy::Vector2(1.0, 0.0), this->getStartPoint() * matrix - newCenterPnt);

    if (isScaled)
    {
        wy::ErrorStatus error = this->setRadius(_radius * scaleX);
        if (wy::ErrorStatus::Ok != error) return error;
    }

    wy::ErrorStatus error = this->setCenter(newCenterPnt);
    if (wy::ErrorStatus::Ok != error) return error;
    error = this->setStartAngle(newStartAngle);
    if (wy::ErrorStatus::Ok != error) return error;
    return this->setEndAngle(newStartAngle + totalAngle);
}

unsigned int SketchArc::intersectWith(const SketchCurve& other, std::vector<wy::Vector2>& out) const
{
    const SketchCurve* pO = &other;
    if (const auto* pL = dynamic_cast<const SketchLine*>(pO))
        return SketchCurveIntersectUtil::intersect(pL, this, out);
    else if (const auto* pCL = dynamic_cast<const SketchCenterLine*>(pO))
        return SketchCurveIntersectUtil::intersect(pCL, this, out);
    else if (const auto* pC = dynamic_cast<const SketchCircle*>(pO))
        return SketchCurveIntersectUtil::intersect(pC, this, out);
    else if (const auto* pA = dynamic_cast<const SketchArc*>(pO))
        return SketchCurveIntersectUtil::intersect(this, pA, out);
    else if (const auto* pE = dynamic_cast<const SketchEllipse*>(pO))
        return SketchCurveIntersectUtil::intersect(this, pE, out);
    else if (const auto* pEA = dynamic_cast<const SketchEllipseArc*>(pO))
        return SketchCurveIntersectUtil::intersect(this, pEA, out);
    else if (const auto* pS = dynamic_cast<const SketchSpline*>(pO))
        return SketchCurveIntersectUtil::intersect(this, pS, out);
    else { assert(false); return 0; }
}


void SketchArc::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ARC_PARAM_CENTER_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ARC_PARAM_CENTER_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ARC_PARAM_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ARC_PARAM_START_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ARC_PARAM_END_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ARC_PARAM_TOTAL_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ARC_PARAM_LENGTH;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr SketchArc::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == SketchArc::classInfo()->className()) {
        if (SketchParamNames::SKETCH_ARC_PARAM_CENTER_X == paramName) return wydb::ParameterValue::createDouble(_centerPnt.x());
        if (SketchParamNames::SKETCH_ARC_PARAM_CENTER_Y == paramName) return wydb::ParameterValue::createDouble(_centerPnt.y());
        if (SketchParamNames::SKETCH_ARC_PARAM_RADIUS == paramName) return wydb::ParameterValue::createDouble(_radius);
        if (SketchParamNames::SKETCH_ARC_PARAM_START_ANGLE == paramName) return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_startAngle));
        if (SketchParamNames::SKETCH_ARC_PARAM_END_ANGLE == paramName) return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_endAngle));
        if (SketchParamNames::SKETCH_ARC_PARAM_TOTAL_ANGLE == paramName) return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(this->getTotalAngle()));
        if (SketchParamNames::SKETCH_ARC_PARAM_LENGTH == paramName) return wydb::ParameterValue::createDouble(this->getTotalAngle() * _radius);
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus SketchArc::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == SketchArc::classInfo()->className()) {
        if (SketchParamNames::SKETCH_ARC_PARAM_CENTER_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector2(paramValue.asDouble(), _centerPnt.y())); }
        if (SketchParamNames::SKETCH_ARC_PARAM_CENTER_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector2(_centerPnt.x(), paramValue.asDouble())); }
        if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
        double d = paramValue.asDouble();
        if (SketchParamNames::SKETCH_ARC_PARAM_RADIUS == paramName) return this->setRadius(d);
        if (SketchParamNames::SKETCH_ARC_PARAM_START_ANGLE == paramName) return this->setStartAngle(wy3d::degreesToRadians(d));
        if (SketchParamNames::SKETCH_ARC_PARAM_END_ANGLE == paramName) return this->setEndAngle(wy3d::degreesToRadians(d));
        if (SketchParamNames::SKETCH_ARC_PARAM_TOTAL_ANGLE == paramName)
        { if (d <= 0.0) return wy::ErrorStatus::InvalidInput; d = wy3d::degreesToRadians(d); if (d >= wy3d::TWO_PI) return wy::ErrorStatus::InvalidInput; return this->setEndAngle(_startAngle + d); }
        if (SketchParamNames::SKETCH_ARC_PARAM_LENGTH == paramName)
        { if (d <= 0.0) return wy::ErrorStatus::InvalidInput; if (_radius <= wy3d::TOL) return wy::ErrorStatus::InvalidInput; double ta = d / _radius; if (ta >= wy3d::TWO_PI) return wy::ErrorStatus::InvalidInput; return this->setEndAngle(_startAngle + ta); }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchArc::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchArc_centerPnt.value():
        value = _centerPnt;
        return true;
    case kSketchArc_radius.value():
        value = _radius;
        return true;
    case kSketchArc_startAngle.value():
        value = _startAngle;
        return true;
    case kSketchArc_endAngle.value():
        value = _endAngle;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchArc::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchArc_centerPnt.value():
        _centerPnt = std::any_cast<const wy::Vector2&>(value);
        return true;
    case kSketchArc_radius.value():
        _radius = std::any_cast<double>(value);
        return true;
    case kSketchArc_startAngle.value():
        _startAngle = std::any_cast<double>(value);
        return true;
    case kSketchArc_endAngle.value():
        _endAngle = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchArc::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _centerPnt << _radius << _startAngle << _endAngle;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchArc::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _centerPnt >> _radius >> _startAngle >> _endAngle;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
