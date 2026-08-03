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
#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchCircle.h>
#include <wy3dImpl.h>
#include <wy3dSketchParamNames.h>
#include <wy3dSketchCurveIntersectUtil.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SketchCircle)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchCircle, _centerPnt)
    REGISTER_FIELD(SketchCircle, _radius)
END_FIELD_REGISTRATION()

SketchCircle::SketchCircle() : wy3d::SketchCurve(), _centerPnt(), _radius(0.0)
{
}

SketchCircle::~SketchCircle()
{
}

wy::ErrorStatus SketchCircle::create(wydb::Transaction* pTrans, const wy::Vector2& center, double radius, SketchCircle*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }

    SketchCircle* pSketchCircle = new SketchCircle();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchCircle);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSketchCircle); pSketchCircle = nullptr; return error; }

    error = pSketchCircle->setCenter(center); CHECK_ERROR_FOR_CREATE(error, pSketchCircle)
    error = pSketchCircle->setRadius(radius); CHECK_ERROR_FOR_CREATE(error, pSketchCircle)

    pOut = pSketchCircle;
    return wy::ErrorStatus::Ok;
}

wy::Vector2 SketchCircle::getStartPoint() const
{
    return wy::Vector2(_centerPnt.x() + _radius, _centerPnt.y());
}

wy::Vector2 SketchCircle::getEndPoint() const
{
    return wy::Vector2(_centerPnt.x() + _radius, _centerPnt.y());
}

wy::Vector2 SketchCircle::getPointAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    double angle = wy3d::TWO_PI * t;
    return wy::Vector2(_centerPnt.x() + _radius * std::cos(angle), _centerPnt.y() + _radius * std::sin(angle));
}

wy::Vector2 SketchCircle::getDirectionAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    double angle = wy3d::TWO_PI * t;
    wy::Vector2 dir = wy::Vector2(-std::sin(angle), std::cos(angle));
    dir.normalize();
    return dir;
}

bool SketchCircle::isDegenerate(double tol) const { return _radius < tol; }

double SketchCircle::getLength() const { assert(_radius); return wy3d::PI * 2 * _radius; }

wy3d::BoundingBox2 SketchCircle::getBoundingBox() const
{
    return wy3d::BoundingBox2(
        wy::Vector2(_centerPnt.x() - _radius - wy3d::TOL, _centerPnt.y() - _radius - wy3d::TOL),
        wy::Vector2(_centerPnt.x() + _radius + wy3d::TOL, _centerPnt.y() + _radius + wy3d::TOL));
}

wy::ErrorStatus SketchCircle::setCenter(const wy::Vector2& centerPoint)
{
    if (_centerPnt == centerPoint) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchCircle_centerPnt);
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

wy::ErrorStatus SketchCircle::setRadius(double radius)
{
    if (radius < wy3d::kMinValue || radius > wy3d::kMaxValue) return wy::ErrorStatus::InvalidInput;
    if (_radius == radius) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchCircle_radius);
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

wy::ErrorStatus SketchCircle::translate(const wy::Vector2& vector)
{
    if (vector == wy::Vector2::kZero) return wy::ErrorStatus::Ok;
    return this->setCenter(_centerPnt + vector);
}

wy::ErrorStatus SketchCircle::rotateAround(const wy::Vector2& center, double angle)
{
    if (angle == 0.0) return wy::ErrorStatus::Ok;
    return this->setCenter(SketchEntity::rotateAround(_centerPnt, center, std::cos(angle), std::sin(angle)));
}

wy::ErrorStatus SketchCircle::transform(const wy3d::Matrix3& matrix)
{
    wy::ErrorStatus error = this->setCenter(_centerPnt * matrix);
    if (wy::ErrorStatus::Ok != error) return error;

    const double a = matrix.get(0, 0), b = matrix.get(0, 1), c = matrix.get(1, 0), d = matrix.get(1, 1);
    const double lenCol0 = std::sqrt(a * a + c * c);
    const double lenCol1 = std::sqrt(b * b + d * d);
    const double dotCol = a * b + c * d;
    if (std::fabs(lenCol0 - lenCol1) <= wy3d::TOL && std::fabs(dotCol) <= wy3d::TOL)
    {
        if (std::fabs(lenCol0 - 1.0) > wy3d::TOL) return this->setRadius(_radius * lenCol0);
        return wy::ErrorStatus::Ok;
    }

    assert(false);
    return wy::ErrorStatus::InvalidInput;
}

unsigned int SketchCircle::intersectWith(const SketchCurve& other, std::vector<wy::Vector2>& out) const
{
    const SketchCurve* pO = &other;
    if (const auto* pL = dynamic_cast<const SketchLine*>(pO))
        return SketchCurveIntersectUtil::intersect(pL, this, out);
    else if (const auto* pCL = dynamic_cast<const SketchCenterLine*>(pO))
        return SketchCurveIntersectUtil::intersect(pCL, this, out);
    else if (const auto* pC = dynamic_cast<const SketchCircle*>(pO))
        return SketchCurveIntersectUtil::intersect(this, pC, out);
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


void SketchCircle::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_CIRCLE_PARAM_CENTER_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_CIRCLE_PARAM_CENTER_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_CIRCLE_PARAM_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_CIRCLE_PARAM_DIAMETER;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_CIRCLE_PARAM_PERIMETER;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_CIRCLE_PARAM_AREA;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr SketchCircle::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == SketchCircle::classInfo()->className()) {
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_CENTER_X == paramName)
            return wydb::ParameterValue::createDouble(_centerPnt.x());
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_CENTER_Y == paramName)
            return wydb::ParameterValue::createDouble(_centerPnt.y());
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_RADIUS == paramName)
            return wydb::ParameterValue::createDouble(_radius);
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_DIAMETER == paramName)
            return wydb::ParameterValue::createDouble(2 * _radius);
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_PERIMETER == paramName)
            return wydb::ParameterValue::createDouble(wy3d::TWO_PI * _radius);
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_AREA == paramName)
            return wydb::ParameterValue::createDouble(wy3d::PI * _radius * _radius);
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus SketchCircle::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == SketchCircle::classInfo()->className()) {
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_CENTER_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector2(paramValue.asDouble(), _centerPnt.y())); }
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_CENTER_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector2(_centerPnt.x(), paramValue.asDouble())); }
        if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
        double d = paramValue.asDouble();
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_RADIUS == paramName) return this->setRadius(d);
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_DIAMETER == paramName) return this->setRadius(d / 2);
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_PERIMETER == paramName) return this->setRadius(d / wy3d::TWO_PI);
        if (SketchParamNames::SKETCH_CIRCLE_PARAM_AREA == paramName)
        { if (d <= 0.0) return wy::ErrorStatus::InvalidInput; return this->setRadius(std::sqrt(d / wy3d::PI)); }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchCircle::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchCircle_centerPnt.value():
        value = _centerPnt;
        return true;
    case kSketchCircle_radius.value():
        value = _radius;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchCircle::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchCircle_centerPnt.value():
        _centerPnt = std::any_cast<const wy::Vector2&>(value);
        return true;
    case kSketchCircle_radius.value():
        _radius = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchCircle::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _centerPnt << _radius;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchCircle::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _centerPnt >> _radius;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
