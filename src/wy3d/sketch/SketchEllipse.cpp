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
#include <wy3dSketchEllipse.h>
#include <wy3dImpl.h>
#include <wy3dSketchParamNames.h>
#include <wy3dSketchCurveIntersectUtil.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SketchEllipse)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchEllipse, _center)
    REGISTER_FIELD(SketchEllipse, _majorAxis)
    REGISTER_FIELD(SketchEllipse, _radiusRatio)
END_FIELD_REGISTRATION()

SketchEllipse::SketchEllipse() : wy3d::SketchCurve(), _center(), _majorAxis(1.0, 0.0), _radiusRatio(1.0)
{
}

SketchEllipse::~SketchEllipse()
{
}

wy::ErrorStatus SketchEllipse::create(wydb::Transaction* pTrans, const wy::Vector2& center,
    const wy::Vector2& majorAxis, double radiusRatio, SketchEllipse*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }

    SketchEllipse* pSketchEllipse = new SketchEllipse();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchEllipse);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSketchEllipse); pSketchEllipse = nullptr; return error; }

    error = pSketchEllipse->setCenter(center); CHECK_ERROR_FOR_CREATE(error, pSketchEllipse)
    error = pSketchEllipse->setMajorAxis(majorAxis); CHECK_ERROR_FOR_CREATE(error, pSketchEllipse)
    error = pSketchEllipse->setRadiusRatio(radiusRatio); CHECK_ERROR_FOR_CREATE(error, pSketchEllipse)

    pOut = pSketchEllipse;
    return wy::ErrorStatus::Ok;
}

static inline double geometricToParametricAngle(double phi, double a, double b)
{
    return std::atan2(a * std::sin(phi), b * std::cos(phi));
}

wy::Vector2 SketchEllipse::getPointAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }

    double angle = geometricToParametricAngle(wy3d::PI * 2 * t, this->getMajorRadius(), this->getMinorRadius());
    double x = this->getMajorRadius() * std::cos(angle);
    double y = this->getMinorRadius() * std::sin(angle);

    double majorAxisAngle = std::atan2(_majorAxis.y(), _majorAxis.x());
    double cosTheta = std::cos(majorAxisAngle);
    double sinTheta = std::sin(majorAxisAngle);
    double worldX = cosTheta * x - sinTheta * y;
    double worldY = sinTheta * x + cosTheta * y;

    return wy::Vector2(_center.x() + worldX, _center.y() + worldY);
}

wy::Vector2 SketchEllipse::getDirectionAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }

    double parametricAngle = geometricToParametricAngle(wy3d::TWO_PI * t,
        this->getMajorRadius(), this->getMinorRadius());
    double dx_dtheta = -this->getMajorRadius() * std::sin(parametricAngle);
    double dy_dtheta = this->getMinorRadius() * std::cos(parametricAngle);

    double majorAxisAngle = std::atan2(_majorAxis.y(), _majorAxis.x());
    double cosTheta = std::cos(majorAxisAngle);
    double sinTheta = std::sin(majorAxisAngle);
    double worldDx = cosTheta * dx_dtheta - sinTheta * dy_dtheta;
    double worldDy = sinTheta * dx_dtheta + cosTheta * dy_dtheta;

    wy::Vector2 dir(worldDx, worldDy);
    if (dir.length() <= wy3d::EPS) return wy::Vector2::kZero;
    dir.normalize();
    return dir;
}

bool SketchEllipse::isDegenerate(double tol) const { return _majorAxis.length() < tol; }

double SketchEllipse::getLength() const
{
    double a = this->getMajorRadius();
    double b = this->getMinorRadius();
    double h = std::pow((a - b) / (a + b), 2);
    return wy3d::PI * (a + b) * (1 + (3 * h) / (10 + std::sqrt(4 - 3 * h)));
}

wy3d::BoundingBox2 SketchEllipse::getBoundingBox() const
{
    double aSquared = getMajorRadius();
    double bSquared = aSquared * _radiusRatio;
    aSquared = aSquared * aSquared;
    bSquared = bSquared * bSquared;

    double theta = std::atan2(_majorAxis.y(), _majorAxis.x());
    double cosThetaSquared = std::cos(theta); cosThetaSquared = cosThetaSquared * cosThetaSquared;
    double sinThetaSquared = std::sin(theta); sinThetaSquared = sinThetaSquared * sinThetaSquared;

    double dx = std::sqrt(aSquared * cosThetaSquared + bSquared * sinThetaSquared);
    double dy = std::sqrt(aSquared * sinThetaSquared + bSquared * cosThetaSquared);

    return wy3d::BoundingBox2(
        wy::Vector2(_center.x() - dx - wy3d::TOL, _center.y() - dy - wy3d::TOL),
        wy::Vector2(_center.x() + dx + wy3d::TOL, _center.y() + dy + wy3d::TOL));
}

wy::ErrorStatus SketchEllipse::setCenter(const wy::Vector2& center)
{
    if (_center == center) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipse_center);
    if (wy::ErrorStatus::Ok == error)
    {
        _center = center;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchEllipse::setMajorAxis(const wy::Vector2& majorAxis)
{
    if (majorAxis.length() < wy3d::kMinValue || majorAxis.length() > wy3d::kMaxValue) return wy::ErrorStatus::InvalidInput;
    if (majorAxis == _majorAxis) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipse_majorAxis);
    if (wy::ErrorStatus::Ok == error)
    {
        _majorAxis = majorAxis;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::Vector2 SketchEllipse::getMinorAxis() const
{
    double cosTheta = std::cos(wy3d::PI_2);
    double sinTheta = std::sin(wy3d::PI_2);
    return SketchEntity::rotateAround(_majorAxis, wy::Vector2::kZero, cosTheta, sinTheta) * _radiusRatio;
}

wy::ErrorStatus SketchEllipse::setMajorRadius(double majorRadius)
{
    if (majorRadius < wy3d::kMinValue || majorRadius > wy3d::kMaxValue) return wy::ErrorStatus::InvalidInput;
    if (majorRadius == _majorAxis.length()) return wy::ErrorStatus::Ok;
    wy::Vector2 majorDir = _majorAxis; majorDir.normalize();
    return this->setMajorAxis(majorDir * majorRadius);
}

wy::ErrorStatus SketchEllipse::setMinorRadius(double minorRadius)
{
    if (minorRadius <= 0.0) return wy::ErrorStatus::InvalidInput;
    if (minorRadius > this->getMajorRadius()) return wy::ErrorStatus::InvalidInput;
    return this->setRadiusRatio(minorRadius / this->getMajorRadius());
}

wy::ErrorStatus SketchEllipse::setRadiusRatio(double radiusRatio, bool allowGreaterThanOne)
{
    if (radiusRatio < 1e-5 || radiusRatio > 1e5) return wy::ErrorStatus::InvalidInput;
    if (!allowGreaterThanOne && radiusRatio > 1.0) return wy::ErrorStatus::InvalidInput;
    if (radiusRatio == _radiusRatio) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipse_radiusRatio);
    if (wy::ErrorStatus::Ok == error)
    {
        _radiusRatio = radiusRatio;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchEllipse::translate(const wy::Vector2& vector)
{
    if (vector == wy::Vector2::kZero) return wy::ErrorStatus::Ok;
    return this->setCenter(_center + vector);
}

wy::ErrorStatus SketchEllipse::rotateAround(const wy::Vector2& center, double angle)
{
    if (angle == 0.0) return wy::ErrorStatus::Ok;
    double cosTheta = std::cos(angle), sinTheta = std::sin(angle);
    wy::Vector2 newCenter = SketchEntity::rotateAround(_center, center, cosTheta, sinTheta);
    wy::Vector2 newMajorAxis = SketchEntity::rotateAround(this->getStartPoint(), center, cosTheta, sinTheta);
    newMajorAxis -= newCenter;
    wy::ErrorStatus error = this->setCenter(newCenter);
    if (wy::ErrorStatus::Ok != error) return error;
    return this->setMajorAxis(newMajorAxis);
}

wy::ErrorStatus SketchEllipse::transform(const wy3d::Matrix3& matrix)
{
    wy::Vector2 newCenter = _center * matrix;
    wy::Vector2 newMajorAxis = (_center + _majorAxis) * matrix - newCenter;
    wy::ErrorStatus error = this->setCenter(newCenter);
    if (wy::ErrorStatus::Ok != error) return error;
    return this->setMajorAxis(newMajorAxis);
}

unsigned int SketchEllipse::intersectWith(const SketchCurve& other, std::vector<wy::Vector2>& out) const
{
    const SketchCurve* pO = &other;
    if (const auto* pL = dynamic_cast<const SketchLine*>(pO))
        return SketchCurveIntersectUtil::intersect(pL, this, out);
    else if (const auto* pCL = dynamic_cast<const SketchCenterLine*>(pO))
        return SketchCurveIntersectUtil::intersect(pCL, this, out);
    else if (const auto* pC = dynamic_cast<const SketchCircle*>(pO))
        return SketchCurveIntersectUtil::intersect(pC, this, out);
    else if (const auto* pA = dynamic_cast<const SketchArc*>(pO))
        return SketchCurveIntersectUtil::intersect(pA, this, out);
    else if (const auto* pE = dynamic_cast<const SketchEllipse*>(pO))
        return SketchCurveIntersectUtil::intersect(this, pE, out);
    else if (const auto* pEA = dynamic_cast<const SketchEllipseArc*>(pO))
        return SketchCurveIntersectUtil::intersect(this, pEA, out);
    else if (const auto* pS = dynamic_cast<const SketchSpline*>(pO))
        return SketchCurveIntersectUtil::intersect(this, pS, out);
    else { assert(false); return 0; }
}


void SketchEllipse::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_PARAM_CENTER_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_PARAM_CENTER_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_PARAM_MAJOR_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_PARAM_MINOR_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_PARAM_RADIUS_RATIO;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_PARAM_MAJOR_AXIS_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_PARAM_PERIMETER;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_PARAM_AREA;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr SketchEllipse::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == SketchEllipse::classInfo()->className()) {
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_CENTER_X == paramName) return wydb::ParameterValue::createDouble(_center.x());
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_CENTER_Y == paramName) return wydb::ParameterValue::createDouble(_center.y());
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_MAJOR_RADIUS == paramName) return wydb::ParameterValue::createDouble(this->getMajorRadius());
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_MINOR_RADIUS == paramName) return wydb::ParameterValue::createDouble(this->getMinorRadius());
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_RADIUS_RATIO == paramName) return wydb::ParameterValue::createDouble(_radiusRatio);
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_MAJOR_AXIS_ANGLE == paramName)
        { double angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, _majorAxis); return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(angle)); }
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_PERIMETER == paramName) return wydb::ParameterValue::createDouble(this->getLength());
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_AREA == paramName) return wydb::ParameterValue::createDouble(wy3d::PI * this->getMajorRadius() * this->getMinorRadius());

        return nullptr;
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus SketchEllipse::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == SketchEllipse::classInfo()->className()) {
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_CENTER_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector2(paramValue.asDouble(), _center.y())); }
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_CENTER_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector2(_center.x(), paramValue.asDouble())); }
        if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
        double d = paramValue.asDouble();
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_MAJOR_RADIUS == paramName) return this->setMajorRadius(d);
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_MINOR_RADIUS == paramName) return this->setMinorRadius(d);
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_RADIUS_RATIO == paramName) { if (d > 1.0) return wy::ErrorStatus::InvalidInput; return this->setRadiusRatio(d); }
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_MAJOR_AXIS_ANGLE == paramName)
        { double angle = wy3d::degreesToRadians(d); return this->setMajorAxis(this->getMajorRadius() * wy::Vector2(std::cos(angle), std::sin(angle))); }
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_PERIMETER == paramName) return wy::ErrorStatus::ParameterReadonly;
        if (SketchParamNames::SKETCH_ELLIPSE_PARAM_AREA == paramName) return wy::ErrorStatus::ParameterReadonly;

        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchEllipse::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchEllipse_center.value():
        value = _center;
        return true;
    case kSketchEllipse_majorAxis.value():
        value = _majorAxis;
        return true;
    case kSketchEllipse_radiusRatio.value():
        value = _radiusRatio;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchEllipse::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchEllipse_center.value():
        _center = std::any_cast<const wy::Vector2&>(value);
        return true;
    case kSketchEllipse_majorAxis.value():
        _majorAxis = std::any_cast<const wy::Vector2&>(value);
        return true;
    case kSketchEllipse_radiusRatio.value():
        _radiusRatio = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchEllipse::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _center << _majorAxis << _radiusRatio;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchEllipse::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _center >> _majorAxis >> _radiusRatio;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
