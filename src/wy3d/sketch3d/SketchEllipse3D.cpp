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
#include <wy3dSketchEllipse3D.h>
#include <wy3dImpl.h>
#include <wy3dSketch3DParamNames.h>
#include "utils/Util.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(SketchEllipse3D)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchEllipse3D, _centerPnt)
    REGISTER_FIELD(SketchEllipse3D, _normal)
    REGISTER_FIELD(SketchEllipse3D, _xDir)
    REGISTER_FIELD(SketchEllipse3D, _majorRadius)
    REGISTER_FIELD(SketchEllipse3D, _radiusRatio)
END_FIELD_REGISTRATION()

SketchEllipse3D::SketchEllipse3D() : wy3d::SketchCurve3D()
    , _centerPnt(), _normal(wy::Vector3::kZAxis), _xDir(wy::Vector3::kXAxis)
    , _majorRadius(0.0), _radiusRatio(1.0)
{
}

SketchEllipse3D::~SketchEllipse3D()
{
}

wy::ErrorStatus SketchEllipse3D::create(
    wydb::Transaction* pTrans,
    const wy::Vector3& center,
    const wy::Vector3& normal,
    const wy::Vector3& xDir,
    double majorRadius,
    double radiusRatio,
    SketchEllipse3D*& pOut)
{
    pOut = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;

    SketchEllipse3D* pSketchEllipse3D = new SketchEllipse3D();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchEllipse3D);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSketchEllipse3D);
        return error;
    }

    error = pSketchEllipse3D->setCenter(center);
    CHECK_ERROR_FOR_CREATE(error, pSketchEllipse3D)
    error = pSketchEllipse3D->setNormalImpl(normal);
    CHECK_ERROR_FOR_CREATE(error, pSketchEllipse3D)
    error = pSketchEllipse3D->setXDirImpl(xDir);
    CHECK_ERROR_FOR_CREATE(error, pSketchEllipse3D)
    error = pSketchEllipse3D->setMajorRadius(majorRadius);
    CHECK_ERROR_FOR_CREATE(error, pSketchEllipse3D)
    error = pSketchEllipse3D->setRadiusRatio(radiusRatio);
    CHECK_ERROR_FOR_CREATE(error, pSketchEllipse3D)

    pOut = pSketchEllipse3D;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchEllipse3D::setCenter(const wy::Vector3& centerPnt)
{
    if (centerPnt == _centerPnt) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipse3D_centerPnt);
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

wy::ErrorStatus SketchEllipse3D::setNormalImpl(const wy::Vector3& normal)
{
    if (normal.length() < 0.5) return wy::ErrorStatus::InvalidInput;
    wy::Vector3 normalized = normal.normalized();
    if (normalized == _normal) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipse3D_normal);
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

wy::ErrorStatus SketchEllipse3D::setXDirImpl(const wy::Vector3& xDir)
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
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipse3D_xDir);
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

wy::ErrorStatus SketchEllipse3D::setPlane(const wy::Vector3& normal, const wy::Vector3& xDir)
{
    wy::ErrorStatus error = this->setNormalImpl(normal);
    if (wy::ErrorStatus::Ok == error)
    {
        error = this->setXDirImpl(xDir);
    }
    return error;
}

wy::ErrorStatus SketchEllipse3D::setMajorRadius(double majorRadius)
{
    if (majorRadius < wy3d::kMinValue || majorRadius > wy3d::kMaxValue) return wy::ErrorStatus::InvalidInput;
    if (majorRadius == _majorRadius) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipse3D_majorRadius);
    if (wy::ErrorStatus::Ok == error)
    {
        _majorRadius = majorRadius;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchEllipse3D::setRadiusRatio(double radiusRatio)
{
    if (radiusRatio < 1e-5 || radiusRatio > 1.0) return wy::ErrorStatus::InvalidInput;
    if (radiusRatio == _radiusRatio) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEllipse3D_radiusRatio);
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

wy::ErrorStatus SketchEllipse3D::setMinorRadius(double minorRadius)
{
    if (minorRadius <= 0.0 || minorRadius > _majorRadius) return wy::ErrorStatus::InvalidInput;
    return this->setRadiusRatio(minorRadius / _majorRadius);
}

double SketchEllipse3D::polarToParametricAngle(double polarAngle) const
{
    return wy3d::ellipsePolarAngleToParametricAngle(polarAngle, _majorRadius, this->getMinorRadius());
}

wy::Vector3 SketchEllipse3D::getStartPoint() const
{
    return this->getPointAt(0.0);
}

wy::Vector3 SketchEllipse3D::getEndPoint() const
{
    return this->getPointAt(1.0);
}

wy::Vector3 SketchEllipse3D::getPointAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    // 退化时椭圆收缩为中心点(角度转换助手要求长短半轴均为正)
    if (_majorRadius <= 0.0 || this->getMinorRadius() <= 0.0)
    {
        return _centerPnt;
    }

    double angle = this->polarToParametricAngle(wy3d::TWO_PI * t);
    wy::Vector3 yDir = _normal.cross(_xDir);
    return _centerPnt
        + _xDir * (_majorRadius * std::cos(angle))
        + yDir * (this->getMinorRadius() * std::sin(angle));
}

wy::Vector3 SketchEllipse3D::getDirectionAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    if (_majorRadius <= 0.0 || this->getMinorRadius() <= 0.0)
    {
        return wy::Vector3::kZero;
    }

    double angle = this->polarToParametricAngle(wy3d::TWO_PI * t);
    wy::Vector3 yDir = _normal.cross(_xDir);
    wy::Vector3 dir = yDir * (this->getMinorRadius() * std::cos(angle)) - _xDir * (_majorRadius * std::sin(angle));
    if (dir.length() <= wy3d::EPS)
    {
        return wy::Vector3::kZero;
    }
    dir.normalize();
    return dir;
}

bool SketchEllipse3D::isDegenerate(double tol) const
{
    return _majorRadius < tol;
}

double SketchEllipse3D::getLength() const
{
    double a = _majorRadius;
    double b = this->getMinorRadius();
    if (a <= 0.0 || b <= 0.0) return 0.0;
    double h = std::pow((a - b) / (a + b), 2);
    return wy3d::PI * (a + b) * (1.0 + (3.0 * h) / (10.0 + std::sqrt(4.0 - 3.0 * h)));
}

void SketchEllipse3D::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_CENTER_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_CENTER_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_CENTER_Z;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_NORMAL_X;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_NORMAL_Y;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_NORMAL_Z;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_XDIR_X;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_XDIR_Y;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_XDIR_Z;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_MAJOR_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_MINOR_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_RADIUS_RATIO;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_PERIMETER;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_AREA;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr SketchEllipse3D::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == SketchEllipse3D::classInfo()->className())
    {
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_CENTER_X == paramName)
            return wydb::ParameterValue::createDouble(_centerPnt.x());
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_CENTER_Y == paramName)
            return wydb::ParameterValue::createDouble(_centerPnt.y());
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_CENTER_Z == paramName)
            return wydb::ParameterValue::createDouble(_centerPnt.z());
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_NORMAL_X == paramName)
            return wydb::ParameterValue::createDouble(_normal.x());
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_NORMAL_Y == paramName)
            return wydb::ParameterValue::createDouble(_normal.y());
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_NORMAL_Z == paramName)
            return wydb::ParameterValue::createDouble(_normal.z());
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_XDIR_X == paramName)
            return wydb::ParameterValue::createDouble(_xDir.x());
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_XDIR_Y == paramName)
            return wydb::ParameterValue::createDouble(_xDir.y());
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_XDIR_Z == paramName)
            return wydb::ParameterValue::createDouble(_xDir.z());
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_MAJOR_RADIUS == paramName)
            return wydb::ParameterValue::createDouble(_majorRadius);
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_MINOR_RADIUS == paramName)
            return wydb::ParameterValue::createDouble(this->getMinorRadius());
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_RADIUS_RATIO == paramName)
            return wydb::ParameterValue::createDouble(_radiusRatio);
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_PERIMETER == paramName)
            return wydb::ParameterValue::createDouble(this->getLength());
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_AREA == paramName)
            return wydb::ParameterValue::createDouble(wy3d::PI * _majorRadius * this->getMinorRadius());
        return nullptr;
    }
    else
    {
        return __baseClass::getParameterValue(className, paramName);
    }
}

wy::ErrorStatus SketchEllipse3D::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == SketchEllipse3D::classInfo()->className())
    {
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_CENTER_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector3(paramValue.asDouble(), _centerPnt.y(), _centerPnt.z())); }
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_CENTER_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector3(_centerPnt.x(), paramValue.asDouble(), _centerPnt.z())); }
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_CENTER_Z == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setCenter(wy::Vector3(_centerPnt.x(), _centerPnt.y(), paramValue.asDouble())); }
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_NORMAL_X == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_NORMAL_Y == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_NORMAL_Z == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_XDIR_X == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_XDIR_Y == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_XDIR_Z == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_PERIMETER == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_AREA == paramName)
            return wy::ErrorStatus::ParameterReadonly;
        if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
        double d = paramValue.asDouble();
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_MAJOR_RADIUS == paramName)
        {
            // 长半轴不得小于短半轴
            if (d < this->getMinorRadius()) return wy::ErrorStatus::InvalidInput;
            return this->setMajorRadius(d);
        }
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_MINOR_RADIUS == paramName) return this->setMinorRadius(d);
        if (Sketch3DParamNames::SKETCH_ELLIPSE3D_PARAM_RADIUS_RATIO == paramName) return this->setRadiusRatio(d);
        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchEllipse3D::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchEllipse3D_centerPnt.value():
        value = _centerPnt;
        return true;
    case kSketchEllipse3D_normal.value():
        value = _normal;
        return true;
    case kSketchEllipse3D_xDir.value():
        value = _xDir;
        return true;
    case kSketchEllipse3D_majorRadius.value():
        value = _majorRadius;
        return true;
    case kSketchEllipse3D_radiusRatio.value():
        value = _radiusRatio;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchEllipse3D::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchEllipse3D_centerPnt.value():
        _centerPnt = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kSketchEllipse3D_normal.value():
        _normal = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kSketchEllipse3D_xDir.value():
        _xDir = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kSketchEllipse3D_majorRadius.value():
        _majorRadius = std::any_cast<double>(value);
        return true;
    case kSketchEllipse3D_radiusRatio.value():
        _radiusRatio = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchEllipse3D::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _centerPnt << _normal << _xDir << _majorRadius << _radiusRatio;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchEllipse3D::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _centerPnt >> _normal >> _xDir >> _majorRadius >> _radiusRatio;
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
