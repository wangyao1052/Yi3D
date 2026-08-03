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

#include <array>
#include <cassert>
#include <wyVector2.h>
#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dImpl.h>
#include <wy3dSketchParamNames.h>
#include <wy3dSketchCurveIntersectUtil.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SketchEllipseArc)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchEllipseArc, _center)
    REGISTER_FIELD(SketchEllipseArc, _majorAxis)
    REGISTER_FIELD(SketchEllipseArc, _radiusRatio)
    REGISTER_FIELD(SketchEllipseArc, _startAngle)
    REGISTER_FIELD(SketchEllipseArc, _endAngle)
END_FIELD_REGISTRATION()

SketchEllipseArc::SketchEllipseArc() : wy3d::SketchCurve(),
    _center(), _majorAxis(1.0, 0.0), _radiusRatio(1.0), _startAngle(0.0), _endAngle(0.0)
{
}

SketchEllipseArc::~SketchEllipseArc()
{
}

wy::ErrorStatus SketchEllipseArc::create(wydb::Transaction* pTrans, const wy::Vector2& center,
    const wy::Vector2& majorAxis, double radiusRatio, double startAngle, double endAngle, SketchEllipseArc*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }

    SketchEllipseArc* pSketchEllipseArc = new SketchEllipseArc();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchEllipseArc);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSketchEllipseArc); pSketchEllipseArc = nullptr; return error; }

    error = pSketchEllipseArc->setCenter(center); CHECK_ERROR_FOR_CREATE(error, pSketchEllipseArc)
    error = pSketchEllipseArc->setMajorAxis(majorAxis); CHECK_ERROR_FOR_CREATE(error, pSketchEllipseArc)
    error = pSketchEllipseArc->setRadiusRatio(radiusRatio); CHECK_ERROR_FOR_CREATE(error, pSketchEllipseArc)
    error = pSketchEllipseArc->setStartAngle(startAngle); CHECK_ERROR_FOR_CREATE(error, pSketchEllipseArc)
    error = pSketchEllipseArc->setEndAngle(endAngle); CHECK_ERROR_FOR_CREATE(error, pSketchEllipseArc)

    pOut = pSketchEllipseArc;
    return wy::ErrorStatus::Ok;
}

static inline double geometricToParametricAngle(double phi, double a, double b)
{
    return std::atan2(a * std::sin(phi), b * std::cos(phi));
}

wy::Vector2 SketchEllipseArc::getPointAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }

    double angle = geometricToParametricAngle(_startAngle + this->getTotalAngle() * t,
        this->getMajorRadius(), this->getMinorRadius());
    double x = this->getMajorRadius() * std::cos(angle);
    double y = this->getMinorRadius() * std::sin(angle);

    double majorAxisAngle = std::atan2(_majorAxis.y(), _majorAxis.x());
    double cosTheta = std::cos(majorAxisAngle);
    double sinTheta = std::sin(majorAxisAngle);
    double worldX = cosTheta * x - sinTheta * y;
    double worldY = sinTheta * x + cosTheta * y;

    return wy::Vector2(_center.x() + worldX, _center.y() + worldY);
}

wy::Vector2 SketchEllipseArc::getDirectionAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }

    double parametricAngle = geometricToParametricAngle(_startAngle + this->getTotalAngle() * t,
        this->getMajorRadius(), this->getMinorRadius());
    double dx = -this->getMajorRadius() * std::sin(parametricAngle);
    double dy = this->getMinorRadius() * std::cos(parametricAngle);

    double majorAxisAngle = std::atan2(_majorAxis.y(), _majorAxis.x());
    double cosTheta = std::cos(majorAxisAngle);
    double sinTheta = std::sin(majorAxisAngle);
    double worldDx = cosTheta * dx - sinTheta * dy;
    double worldDy = sinTheta * dx + cosTheta * dy;

    wy::Vector2 dir(worldDx, worldDy);
    if (dir.length() <= wy3d::EPS) return wy::Vector2::kZero;
    dir.normalize();
    return dir;
}

bool SketchEllipseArc::isDegenerate(double tol) const
{
    return _majorAxis.length() < tol || this->getTotalAngle() < tol;
}

double SketchEllipseArc::getLength() const
{
    double a = this->getMajorRadius();
    double b = this->getMinorRadius();
    int numSteps = 500;
    double dt = this->getTotalAngle() / numSteps;
    double length = 0.0;
    for (int i = 0; i <= numSteps; i++)
    {
        double theta = _startAngle + i * dt;
        double weight = (i == 0 || i == numSteps) ? 1.0 : (i % 2 == 0 ? 4.0 : 2.0);
        double integrand = std::sqrt(a * a * std::sin(theta) * std::sin(theta)
            + b * b * std::cos(theta) * std::cos(theta));
        length += weight * integrand;
    }
    return length * dt / 3.0;
}

wy3d::BoundingBox2 SketchEllipseArc::getBoundingBox() const
{
    double a = this->getMajorRadius();
    double b = a * _radiusRatio;

    double majorAxisAngle = std::atan2(_majorAxis.y(), _majorAxis.x());
    double cosTheta = std::cos(majorAxisAngle);
    double sinTheta = std::sin(majorAxisAngle);

    double startAngleParam = wy3d::ellipsePolarAngleToParametricAngle(_startAngle, a, b);
    startAngleParam = wy3d::normalizeRadian(startAngleParam);
    wy::Vector2 startPoint = this->getStartPoint();
    double xMin = startPoint.x(), yMin = startPoint.y();
    double xMax = startPoint.x(), yMax = startPoint.y();

    double endAngleParam = wy3d::ellipsePolarAngleToParametricAngle(_endAngle, a, b);
    endAngleParam = wy3d::normalizeRadian(endAngleParam);
    if (endAngleParam < startAngleParam) endAngleParam += wy3d::TWO_PI;
    wy::Vector2 endPoint = this->getEndPoint();
    xMin = std::min(endPoint.x(), xMin); yMin = std::min(endPoint.y(), yMin);
    xMax = std::max(endPoint.x(), xMax); yMax = std::max(endPoint.y(), yMax);

    double tx1 = wy3d::PI_2;
    if (std::fabs(cosTheta) > wy3d::EPS)
        tx1 = std::atan(-_radiusRatio * sinTheta / cosTheta);
    double tx2 = tx1 + wy3d::PI;
    double txCandidates[2] = { wy3d::normalizeRadian(tx1), wy3d::normalizeRadian(tx2) };
    for (int i = 0; i < 2; i++)
    {
        double t = txCandidates[i];
        if (t < startAngleParam) t += wy3d::TWO_PI;
        if (t >= endAngleParam) continue;
        double x = cosTheta * (a * std::cos(t)) - sinTheta * (b * std::sin(t)) + _center.x();
        xMin = std::min(x, xMin); xMax = std::max(x, xMax);
    }

    double ty1 = wy3d::PI_2;
    if (std::fabs(sinTheta) > wy3d::EPS)
        ty1 = std::atan(_radiusRatio * cosTheta / sinTheta);
    double ty2 = ty1 + wy3d::PI;
    double tyCandidates[2] = { wy3d::normalizeRadian(ty1), wy3d::normalizeRadian(ty2) };
    for (int i = 0; i < 2; i++)
    {
        double t = tyCandidates[i];
        if (t < startAngleParam) t += wy3d::TWO_PI;
        if (t >= endAngleParam) continue;
        double y = sinTheta * (a * std::cos(t)) + cosTheta * (b * std::sin(t)) + _center.y();
        yMin = std::min(y, yMin); yMax = std::max(y, yMax);
    }

    return wy3d::BoundingBox2(
        wy::Vector2(xMin - wy3d::TOL, yMin - wy3d::TOL),
        wy::Vector2(xMax + wy3d::TOL, yMax + wy3d::TOL));
}

wy::ErrorStatus SketchEllipseArc::setCenter(const wy::Vector2& center)
{
    if (_center == center) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipseArc_center);
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

wy::ErrorStatus SketchEllipseArc::setMajorAxis(const wy::Vector2& majorAxis)
{
    if (majorAxis.length() < wy3d::kMinValue || majorAxis.length() > wy3d::kMaxValue)
        return wy::ErrorStatus::InvalidInput;
    if (majorAxis == _majorAxis) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipseArc_majorAxis);
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

wy::Vector2 SketchEllipseArc::getMinorAxis() const
{
    double cosTheta = std::cos(wy3d::PI_2);
    double sinTheta = std::sin(wy3d::PI_2);
    return SketchEntity::rotateAround(_majorAxis, wy::Vector2::kZero, cosTheta, sinTheta) * _radiusRatio;
}

wy::ErrorStatus SketchEllipseArc::setMajorRadius(double majorRadius)
{
    if (majorRadius < wy3d::kMinValue || majorRadius > wy3d::kMaxValue)
        return wy::ErrorStatus::InvalidInput;
    if (majorRadius == _majorAxis.length()) return wy::ErrorStatus::Ok;
    wy::Vector2 majorDir = _majorAxis; majorDir.normalize();
    return this->setMajorAxis(majorDir * majorRadius);
}

wy::ErrorStatus SketchEllipseArc::setMinorRadius(double minorRadius)
{
    if (minorRadius <= 0.0) return wy::ErrorStatus::InvalidInput;
    if (minorRadius > this->getMajorRadius()) return wy::ErrorStatus::InvalidInput;
    return this->setRadiusRatio(minorRadius / this->getMajorRadius());
}

wy::ErrorStatus SketchEllipseArc::setRadiusRatio(double radiusRatio, bool allowGreaterThanOne)
{
    if (radiusRatio < 1e-5 || radiusRatio > 1e5) return wy::ErrorStatus::InvalidInput;
    if (!allowGreaterThanOne && radiusRatio > 1.0) return wy::ErrorStatus::InvalidInput;
    if (radiusRatio == _radiusRatio) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipseArc_radiusRatio);
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

wy::ErrorStatus SketchEllipseArc::setStartAngle(double startAngle)
{
    if (_startAngle == startAngle) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipseArc_startAngle);
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

wy::ErrorStatus SketchEllipseArc::setEndAngle(double endAngle)
{
    if (_endAngle == endAngle) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipseArc_endAngle);
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

double SketchEllipseArc::getTotalAngle() const
{
    return wy3d::normalizeRadian(_endAngle - _startAngle);
}

wy::ErrorStatus SketchEllipseArc::translate(const wy::Vector2& vector)
{
    if (vector == wy::Vector2::kZero) return wy::ErrorStatus::Ok;
    return this->setCenter(_center + vector);
}

wy::ErrorStatus SketchEllipseArc::rotateAround(const wy::Vector2& center, double angle)
{
    if (angle == 0.0) return wy::ErrorStatus::Ok;

    double cosTheta = std::cos(angle), sinTheta = std::sin(angle);
    wy::Vector2 newCenter = SketchEntity::rotateAround(_center, center, cosTheta, sinTheta);
    wy::Vector2 newMajorAxis = SketchEntity::rotateAround(_majorAxis, center, cosTheta, sinTheta)
        - SketchEntity::rotateAround(wy::Vector2::kZero, center, cosTheta, sinTheta);
    wy::Vector2 newStartPoint = SketchEntity::rotateAround(this->getStartPoint(), center, cosTheta, sinTheta);
    double newStartAngle = wy::Vector2::rotationAngle(newMajorAxis, newStartPoint - newCenter);
    double newEndAngle = this->getTotalAngle() + newStartAngle;

    wy::ErrorStatus error = this->setCenter(newCenter);
    if (wy::ErrorStatus::Ok != error) return error;
    error = this->setMajorAxis(newMajorAxis);
    if (wy::ErrorStatus::Ok != error) return error;
    error = this->setStartAngle(newStartAngle);
    if (wy::ErrorStatus::Ok != error) return error;
    return this->setEndAngle(newEndAngle);
}

wy::ErrorStatus SketchEllipseArc::transform(const wy3d::Matrix3& matrix)
{
    wy::Vector2 newCenter = _center * matrix;
    wy::Vector2 newMajorAxis = (_center + _majorAxis) * matrix - newCenter;

    wy::Vector2 newStartPoint = this->getStartPoint() * matrix;
    double newStartAngle = wy::Vector2::rotationAngle(newMajorAxis, newStartPoint - newCenter);

    wy::Vector2 newMidPoint = this->getPointAt(0.5) * matrix;
    double newMidAngle = wy::Vector2::rotationAngle(newMajorAxis, newMidPoint - newCenter);
    if (newMidAngle < newStartAngle) newMidAngle += wy3d::TWO_PI;

    wy::Vector2 newEndPoint = this->getEndPoint() * matrix;
    double newEndAngle = wy::Vector2::rotationAngle(newMajorAxis, newEndPoint - newCenter);
    if (newEndAngle < newStartAngle) newEndAngle += wy3d::TWO_PI;

    if (newMidAngle > newEndAngle)
    {
        std::swap(newStartAngle, newEndAngle);
        newStartAngle = wy3d::normalizeRadian(newStartAngle);
        if (newEndAngle < newStartAngle) newEndAngle += wy3d::TWO_PI;
    }

    wy::ErrorStatus error = this->setCenter(newCenter);
    if (wy::ErrorStatus::Ok != error) return error;
    error = this->setMajorAxis(newMajorAxis);
    if (wy::ErrorStatus::Ok != error) return error;
    error = this->setStartAngle(newStartAngle);
    if (wy::ErrorStatus::Ok != error) return error;
    return this->setEndAngle(newEndAngle);
}

unsigned int SketchEllipseArc::intersectWith(const SketchCurve& other, std::vector<wy::Vector2>& out) const
{
    const SketchCurve* pOther = &other;
    if (const auto* pLine = dynamic_cast<const SketchLine*>(pOther))
        return SketchCurveIntersectUtil::intersect(pLine, this, out);
    else if (const auto* pCenterLine = dynamic_cast<const SketchCenterLine*>(pOther))
        return SketchCurveIntersectUtil::intersect(pCenterLine, this, out);
    else if (const auto* pCircle = dynamic_cast<const SketchCircle*>(pOther))
        return SketchCurveIntersectUtil::intersect(pCircle, this, out);
    else if (const auto* pArc = dynamic_cast<const SketchArc*>(pOther))
        return SketchCurveIntersectUtil::intersect(pArc, this, out);
    else if (const auto* pEllipse = dynamic_cast<const SketchEllipse*>(pOther))
        return SketchCurveIntersectUtil::intersect(pEllipse, this, out);
    else if (const auto* pEllipseArc = dynamic_cast<const SketchEllipseArc*>(pOther))
        return SketchCurveIntersectUtil::intersect(this, pEllipseArc, out);
    else if (const auto* pSpline = dynamic_cast<const SketchSpline*>(pOther))
        return SketchCurveIntersectUtil::intersect(this, pSpline, out);
    else { assert(false); return 0; }
}


void SketchEllipseArc::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_CENTER_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_CENTER_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_MAJOR_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_MINOR_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_RADIUS_RATIO;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_MAJOR_AXIS_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_START_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_END_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_PERIMETER;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr SketchEllipseArc::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == SketchEllipseArc::classInfo()->className()) {
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_CENTER_X == paramName)
            return wydb::ParameterValue::createDouble(_center.x());
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_CENTER_Y == paramName)
            return wydb::ParameterValue::createDouble(_center.y());
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_MAJOR_RADIUS == paramName)
            return wydb::ParameterValue::createDouble(this->getMajorRadius());
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_MINOR_RADIUS == paramName)
            return wydb::ParameterValue::createDouble(this->getMinorRadius());
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_RADIUS_RATIO == paramName)
            return wydb::ParameterValue::createDouble(_radiusRatio);
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_MAJOR_AXIS_ANGLE == paramName)
        {
            double angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, _majorAxis);
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(angle));
        }
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_START_ANGLE == paramName)
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_startAngle));
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_END_ANGLE == paramName)
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_endAngle));
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_PERIMETER == paramName)
            return wydb::ParameterValue::createDouble(this->getLength());

        return nullptr;
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus SketchEllipseArc::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == SketchEllipseArc::classInfo()->className()) {
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_CENTER_X == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setCenter(wy::Vector2(paramValue.asDouble(), _center.y()));
        }
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_CENTER_Y == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setCenter(wy::Vector2(_center.x(), paramValue.asDouble()));
        }
        if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
        double d = paramValue.asDouble();
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_MAJOR_RADIUS == paramName) return this->setMajorRadius(d);
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_MINOR_RADIUS == paramName) return this->setMinorRadius(d);
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_RADIUS_RATIO == paramName)
        {
            if (d > 1.0) return wy::ErrorStatus::InvalidInput;
            return this->setRadiusRatio(d);
        }
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_MAJOR_AXIS_ANGLE == paramName)
        {
            double angle = wy3d::degreesToRadians(d);
            return this->setMajorAxis(this->getMajorRadius() * wy::Vector2(std::cos(angle), std::sin(angle)));
        }
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_START_ANGLE == paramName)
            return this->setStartAngle(wy3d::degreesToRadians(d));
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_END_ANGLE == paramName)
            return this->setEndAngle(wy3d::degreesToRadians(d));
        if (SketchParamNames::SKETCH_ELLIPSE_ARC_PARAM_PERIMETER == paramName)
            return wy::ErrorStatus::ParameterReadonly;

        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchEllipseArc::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchEllipseArc_center.value():
        value = _center;
        return true;
    case kSketchEllipseArc_majorAxis.value():
        value = _majorAxis;
        return true;
    case kSketchEllipseArc_radiusRatio.value():
        value = _radiusRatio;
        return true;
    case kSketchEllipseArc_startAngle.value():
        value = _startAngle;
        return true;
    case kSketchEllipseArc_endAngle.value():
        value = _endAngle;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchEllipseArc::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchEllipseArc_center.value():
        _center = std::any_cast<const wy::Vector2&>(value);
        return true;
    case kSketchEllipseArc_majorAxis.value():
        _majorAxis = std::any_cast<const wy::Vector2&>(value);
        return true;
    case kSketchEllipseArc_radiusRatio.value():
        _radiusRatio = std::any_cast<double>(value);
        return true;
    case kSketchEllipseArc_startAngle.value():
        _startAngle = std::any_cast<double>(value);
        return true;
    case kSketchEllipseArc_endAngle.value():
        _endAngle = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchEllipseArc::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _center << _majorAxis << _radiusRatio << _startAngle << _endAngle;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchEllipseArc::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _center >> _majorAxis >> _radiusRatio >> _startAngle >> _endAngle;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
