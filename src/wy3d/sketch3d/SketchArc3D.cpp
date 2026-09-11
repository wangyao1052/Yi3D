///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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
#include <cmath>
#include <wyVector3.h>
#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchArc3D.h>
#include <wy3dImpl.h>
#include <wy3dSketch3DParamNames.h>
#include "utils/Util.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(SketchArc3D)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchArc3D, _centerPnt)
    REGISTER_FIELD(SketchArc3D, _normal)
    REGISTER_FIELD(SketchArc3D, _xDir)
    REGISTER_FIELD(SketchArc3D, _radius)
    REGISTER_FIELD(SketchArc3D, _startAngle)
    REGISTER_FIELD(SketchArc3D, _endAngle)
END_FIELD_REGISTRATION()

SketchArc3D::SketchArc3D() : wy3d::SketchCurve3D()
    , _centerPnt(), _normal(wy::Vector3::kZAxis), _xDir(wy::Vector3::kXAxis)
    , _radius(0.0), _startAngle(0.0), _endAngle(0.0)
{
}

SketchArc3D::~SketchArc3D()
{
}

wy::ErrorStatus SketchArc3D::create(
    wydb::Transaction* pTrans,
    const wy::Vector3& center,
    const wy::Vector3& normal,
    const wy::Vector3& xDir,
    double radius,
    double startAngle,
    double endAngle,
    SketchArc3D*& pOut)
{
    pOut = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;

    SketchArc3D* pSketchArc3D = new SketchArc3D();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchArc3D);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSketchArc3D);
        return error;
    }

    error = pSketchArc3D->setCenter(center);
    CHECK_ERROR_FOR_CREATE(error, pSketchArc3D)
    error = pSketchArc3D->setNormalImpl(normal);
    CHECK_ERROR_FOR_CREATE(error, pSketchArc3D)
    error = pSketchArc3D->setXDirImpl(xDir);
    CHECK_ERROR_FOR_CREATE(error, pSketchArc3D)
    error = pSketchArc3D->setRadius(radius);
    CHECK_ERROR_FOR_CREATE(error, pSketchArc3D)
    error = pSketchArc3D->setStartAngle(startAngle);
    CHECK_ERROR_FOR_CREATE(error, pSketchArc3D)
    error = pSketchArc3D->setEndAngle(endAngle);
    CHECK_ERROR_FOR_CREATE(error, pSketchArc3D)

    pOut = pSketchArc3D;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchArc3D::setCenter(const wy::Vector3& centerPnt)
{
    if (centerPnt == _centerPnt) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchArc3D_centerPnt);
    if (wy::ErrorStatus::Ok == error)
    {
        _centerPnt = centerPnt;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchArc3D::setNormalImpl(const wy::Vector3& normal)
{
    if (normal.length() < 0.5) return wy::ErrorStatus::InvalidInput;
    wy::Vector3 normalized = normal.normalized();
    if (normalized == _normal) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchArc3D_normal);
    if (wy::ErrorStatus::Ok == error)
    {
        _normal = normalized;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchArc3D::setXDirImpl(const wy::Vector3& xDir)
{
    wy::Vector3 actualXDir = xDir;
    if (actualXDir.length() < 0.5)
    {
        // No xDir given: derive from the world axes (X, then Y, then Z)
        const wy::Vector3 worldAxes[3] = { wy::Vector3::kXAxis, wy::Vector3::kYAxis, wy::Vector3::kZAxis };
        bool found(false);
        for (const wy::Vector3& axis : worldAxes)
        {
            wy::Vector3 ortho = axis - _normal * axis.dot(_normal);
            if (ortho.length() >= 0.5)
            {
                ortho.normalize();
                actualXDir = ortho;
                found = true;
                break;
            }
        }
        if (!found) return wy::ErrorStatus::InvalidInput; // unreachable with a unit normal
    }
    else
    {
        // The caller-provided xDir may not be perpendicular to the plane
        actualXDir = actualXDir - _normal * actualXDir.dot(_normal);
        if (actualXDir.length() < 1e-5) return wy::ErrorStatus::InvalidInput;
        actualXDir.normalize();
    }

    if (actualXDir == _xDir) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchArc3D_xDir);
    if (wy::ErrorStatus::Ok == error)
    {
        _xDir = actualXDir;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchArc3D::setPlane(const wy::Vector3& normal, const wy::Vector3& xDir)
{
    wy::ErrorStatus error = this->setNormalImpl(normal);
    if (wy::ErrorStatus::Ok == error)
    {
        error = this->setXDirImpl(xDir);
    }
    return error;
}

wy::ErrorStatus SketchArc3D::setRadius(double radius)
{
    if (radius < wy3d::kMinValue || radius > wy3d::kMaxValue) return wy::ErrorStatus::InvalidInput;
    if (_radius == radius) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchArc3D_radius);
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

wy::ErrorStatus SketchArc3D::setStartAngle(double startAngle)
{
    if (_startAngle == startAngle) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchArc3D_startAngle);
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

wy::ErrorStatus SketchArc3D::setEndAngle(double endAngle)
{
    if (_endAngle == endAngle) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchArc3D_endAngle);
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

double SketchArc3D::getTotalAngle() const
{
    return wy3d::normalizeRadian(_endAngle - _startAngle);
}

wy::Vector3 SketchArc3D::getStartPoint() const
{
    return this->getPointAt(0.0);
}

wy::Vector3 SketchArc3D::getEndPoint() const
{
    return this->getPointAt(1.0);
}

wy::Vector3 SketchArc3D::getPointAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    double angle = _startAngle + this->getTotalAngle() * t;
    wy::Vector3 yDir = _normal.cross(_xDir);
    return _centerPnt + _xDir * (std::cos(angle) * _radius) + yDir * (std::sin(angle) * _radius);
}

wy::Vector3 SketchArc3D::getDirectionAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    double angle = _startAngle + this->getTotalAngle() * t;
    wy::Vector3 yDir = _normal.cross(_xDir);
    wy::Vector3 dir = yDir * std::cos(angle) - _xDir * std::sin(angle);
    dir.normalize();
    return dir;
}

wy::Vector3 SketchArc3D::getMiddlePoint() const
{
    return this->getPointAt(0.5);
}

bool SketchArc3D::isDegenerate(double tol) const
{
    return _radius < tol || this->getLength() < tol;
}

double SketchArc3D::getLength() const
{
    return _radius * this->getTotalAngle();
}

void SketchArc3D::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_CENTER_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_CENTER_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_CENTER_Z;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_NORMAL_X;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_NORMAL_Y;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_NORMAL_Z;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_XDIR_X;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_XDIR_Y;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_XDIR_Z;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_START_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_END_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_TOTAL_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ARC3D_PARAM_LENGTH;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr SketchArc3D::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == SketchArc3D::classInfo()->className())
    {
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_CENTER_X == paramName)
            return wydb::ParameterValue::createDouble(_centerPnt.x());
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_CENTER_Y == paramName)
            return wydb::ParameterValue::createDouble(_centerPnt.y());
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_CENTER_Z == paramName)
            return wydb::ParameterValue::createDouble(_centerPnt.z());
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_NORMAL_X == paramName)
            return wydb::ParameterValue::createDouble(_normal.x());
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_NORMAL_Y == paramName)
            return wydb::ParameterValue::createDouble(_normal.y());
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_NORMAL_Z == paramName)
            return wydb::ParameterValue::createDouble(_normal.z());
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_XDIR_X == paramName)
            return wydb::ParameterValue::createDouble(_xDir.x());
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_XDIR_Y == paramName)
            return wydb::ParameterValue::createDouble(_xDir.y());
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_XDIR_Z == paramName)
            return wydb::ParameterValue::createDouble(_xDir.z());
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_RADIUS == paramName)
            return wydb::ParameterValue::createDouble(_radius);
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_START_ANGLE == paramName)
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_startAngle));
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_END_ANGLE == paramName)
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_endAngle));
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_TOTAL_ANGLE == paramName)
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(this->getTotalAngle()));
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_LENGTH == paramName)
            return wydb::ParameterValue::createDouble(this->getLength());
        return nullptr;
    }
    else
    {
        return __baseClass::getParameterValue(className, paramName);
    }
}

wy::ErrorStatus SketchArc3D::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == SketchArc3D::classInfo()->className())
    {
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_CENTER_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector3(paramValue.asDouble(), _centerPnt.y(), _centerPnt.z())); }
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_CENTER_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector3(_centerPnt.x(), paramValue.asDouble(), _centerPnt.z())); }
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_CENTER_Z == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector3(_centerPnt.x(), _centerPnt.y(), paramValue.asDouble())); }
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_NORMAL_X == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_NORMAL_Y == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_NORMAL_Z == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_XDIR_X == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_XDIR_Y == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_XDIR_Z == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
        double d = paramValue.asDouble();
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_RADIUS == paramName) return this->setRadius(d);
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_START_ANGLE == paramName) return this->setStartAngle(wy3d::degreesToRadians(d));
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_END_ANGLE == paramName) return this->setEndAngle(wy3d::degreesToRadians(d));
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_TOTAL_ANGLE == paramName)
        { if (d <= 0.0) return wy::ErrorStatus::InvalidInput; d = wy3d::degreesToRadians(d); if (d >= wy3d::TWO_PI) return wy::ErrorStatus::InvalidInput; return this->setEndAngle(_startAngle + d); }
        if (Sketch3DParamNames::SKETCH_ARC3D_PARAM_LENGTH == paramName)
        { if (d <= 0.0) return wy::ErrorStatus::InvalidInput; if (_radius <= wy3d::TOL) return wy::ErrorStatus::InvalidInput; double ta = d / _radius; if (ta >= wy3d::TWO_PI) return wy::ErrorStatus::InvalidInput; return this->setEndAngle(_startAngle + ta); }
        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchArc3D::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchArc3D_centerPnt.value():
        value = _centerPnt;
        return true;
    case kSketchArc3D_normal.value():
        value = _normal;
        return true;
    case kSketchArc3D_xDir.value():
        value = _xDir;
        return true;
    case kSketchArc3D_radius.value():
        value = _radius;
        return true;
    case kSketchArc3D_startAngle.value():
        value = _startAngle;
        return true;
    case kSketchArc3D_endAngle.value():
        value = _endAngle;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchArc3D::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchArc3D_centerPnt.value():
        _centerPnt = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kSketchArc3D_normal.value():
        _normal = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kSketchArc3D_xDir.value():
        _xDir = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kSketchArc3D_radius.value():
        _radius = std::any_cast<double>(value);
        return true;
    case kSketchArc3D_startAngle.value():
        _startAngle = std::any_cast<double>(value);
        return true;
    case kSketchArc3D_endAngle.value():
        _endAngle = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchArc3D::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _centerPnt << _normal << _xDir << _radius << _startAngle << _endAngle;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchArc3D::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _centerPnt >> _normal >> _xDir >> _radius >> _startAngle >> _endAngle;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
