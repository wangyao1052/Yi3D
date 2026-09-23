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
#include <Geom2dAPI_Interpolate.hxx>
#include <Geom2dAPI_ProjectPointOnCurve.hxx>
#include <TColgp_HArray1OfPnt2d.hxx>
#include <TColgp_Array1OfVec2d.hxx>
#include <TColStd_HArray1OfBoolean.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec2d.hxx>
#include <Precision.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <GCPnts_AbscissaPoint.hxx>

#include <wyVector2.h>
#include <wy3dMath.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSketchSpline.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchParamNames.h>
#include <utils/wy3dSketchCurveIntersectUtil.h>
#include "utils/FilerUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SketchSpline)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchSpline, _mode)
    REGISTER_FIELD(SketchSpline, _degree)
    REGISTER_FIELD(SketchSpline, _points)
    REGISTER_FIELD(SketchSpline, _knots)
    REGISTER_FIELD(SketchSpline, _multiplicities)
    REGISTER_FIELD(SketchSpline, _tangents)
    REGISTER_FIELD(SketchSpline, _pBSpline)
END_FIELD_REGISTRATION()

namespace
{
    bool isClosedFit(const std::vector<wy::Vector2>& points)
    {
        return points.size() >= 4 && points.front() == points.back();
    }

    double spanChordLength(const std::vector<wy::Vector2>& points, std::size_t index)
    {
        const std::size_t count = points.size();
        if (index >= count) return 1.0;

        const double leaving = (index + 1 < count)
            ? (points[index + 1] - points[index]).length() : 0.0;
        if (leaving > wy3d::TOL) return leaving;

        const double arriving = (index > 0)
            ? (points[index] - points[index - 1]).length() : 0.0;
        if (arriving > wy3d::TOL) return arriving;

        return 1.0;
    }

    SketchSpline::Tangent chordTangentAt(const std::vector<wy::Vector2>& points, std::size_t index)
    {
        SketchSpline::Tangent tangent;
        tangent.magnitude = spanChordLength(points, index);
        const std::size_t count = points.size();
        if (count < 2 || index >= count) return tangent;

        const bool isClosed = isClosedFit(points);
        const std::size_t distinctCount = isClosed ? count - 1 : count;
        const std::size_t at = index;

        const std::size_t prev = isClosed
            ? (at + distinctCount - 1) % distinctCount
            : (at > 0 ? at - 1 : 0);
        const std::size_t next = isClosed
            ? (at + 1) % distinctCount
            : (at + 1 < count ? at + 1 : count - 1);

        wy::Vector2 direction = (prev == next) ? wy::Vector2::kZero : (points[next] - points[prev]);
        if (direction.length() <= wy3d::TOL)
        {
            direction = (next > at) ? (points[next] - points[at]) : (points[at] - points[prev]);
            if (direction.length() <= wy3d::TOL) return tangent;
        }

        tangent.angle = wy3d::normalizeRadian(std::atan2(direction.y(), direction.x()));
        return tangent;
    }

    bool isUsableTangent(const SketchSpline::Tangent& tangent)
    {
        return std::isfinite(tangent.angle)
            && std::isfinite(tangent.magnitude)
            && tangent.magnitude > wy3d::TOL;
    }
}

SketchSpline::SketchSpline() : wy3d::SketchCurve()
    , _mode(SplineMode::Undefined)
    , _degree(0),
    _pBSpline(nullptr)
{
}

SketchSpline::~SketchSpline()
{
}

wy::ErrorStatus SketchSpline::create(
    wydb::Transaction* pTrans,
    const std::vector<wy::Vector2>& fitPoints,
    SketchSpline*& pOut)
{
    pOut = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (fitPoints.size() < 2) return wy::ErrorStatus::InvalidInput;

    SketchSpline* pSketchSpline = new SketchSpline();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchSpline);
    if (error != wy::ErrorStatus::Ok)
    {
        wydb::deleteElement(pSketchSpline);
        pSketchSpline = nullptr;
        return error;
    }

    error = pSketchSpline->setMode(SplineMode::InterpolationPoints);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline);
    error = pSketchSpline->setPoints(fitPoints);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline);
    std::vector<Tangent> tangents;
    tangents.resize(fitPoints.size());
    error = pSketchSpline->setTangents(tangents);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline);

    pOut = pSketchSpline;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchSpline::create(
    wydb::Transaction* pTrans,
    std::uint32_t degree,
    const std::vector<wy::Vector2>& controlPoints,
    SketchSpline*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }
    if (degree < 1 || degree > 8) { pOut = nullptr; return wy::ErrorStatus::InvalidInput; }
    if (controlPoints.size() < 2) { pOut = nullptr; return wy::ErrorStatus::InvalidInput; }

    SketchSpline* pSketchSpline = new SketchSpline();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchSpline);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSketchSpline); pSketchSpline = nullptr; return error; }

    error = pSketchSpline->setMode(SplineMode::ControlPoints); CHECK_ERROR_FOR_CREATE(error, pSketchSpline);
    error = pSketchSpline->setDegree(degree); CHECK_ERROR_FOR_CREATE(error, pSketchSpline);
    error = pSketchSpline->setPoints(controlPoints); CHECK_ERROR_FOR_CREATE(error, pSketchSpline);

    pOut = pSketchSpline;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchSpline::create(wydb::Transaction* pTrans, std::uint32_t degree,
    const std::vector<wy::Vector2>& controlPoints, const std::vector<double>& knots,
    const std::vector<std::uint32_t>& multiplicities, SketchSpline*& pOut)
{
    if (!pTrans) { pOut = nullptr; return wy::ErrorStatus::NullDatabasePointer; }
    if (degree < 1 || degree > 8) { pOut = nullptr; return wy::ErrorStatus::InvalidInput; }
    if (controlPoints.size() < 2 || knots.size() < 2 || multiplicities.size() < 2 || knots.size() != multiplicities.size())
    { pOut = nullptr; return wy::ErrorStatus::InvalidInput; }

    SketchSpline* pSketchSpline = new SketchSpline();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchSpline);
    if (error != wy::ErrorStatus::Ok) { wydb::deleteElement(pSketchSpline); pSketchSpline = nullptr; return error; }

    error = pSketchSpline->setMode(SplineMode::ControlPoints); CHECK_ERROR_FOR_CREATE(error, pSketchSpline);
    error = pSketchSpline->setDegree(degree); CHECK_ERROR_FOR_CREATE(error, pSketchSpline);
    error = pSketchSpline->setPoints(controlPoints); CHECK_ERROR_FOR_CREATE(error, pSketchSpline);
    error = pSketchSpline->setKnots(knots); CHECK_ERROR_FOR_CREATE(error, pSketchSpline);
    error = pSketchSpline->setMultiplicities(multiplicities); CHECK_ERROR_FOR_CREATE(error, pSketchSpline);

    pOut = pSketchSpline;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchSpline::setMode(SplineMode mode)
{
    if (SplineMode::InterpolationPoints != mode && SplineMode::ControlPoints != mode) return wy::ErrorStatus::InvalidInput;
    if (mode == _mode) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline_mode);
    if (wy::ErrorStatus::Ok == error)
    {
        _mode = mode;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchSpline::setDegree(std::uint32_t degree)
{
    if (SplineMode::ControlPoints != _mode) return wy::ErrorStatus::NotCurrentlyAllowed;
    return this->_setDegree(degree);
}

wy::ErrorStatus SketchSpline::_setDegree(std::uint32_t degree)
{
    if (degree < 1 || degree > 8) return wy::ErrorStatus::InvalidInput;
    if (degree == _degree) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline_degree);
    if (wy::ErrorStatus::Ok == error)
    {
        _degree = degree;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchSpline::setPoints(const std::vector<wy::Vector2>& points)
{
    if (points.empty()) return wy::ErrorStatus::InvalidInput;
    if (points == _points) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline_points);
    if (wy::ErrorStatus::Ok == error)
    {
        _points = points;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchSpline::setTangents(const std::vector<Tangent>& tangents)
{
    if (SplineMode::InterpolationPoints != _mode) return wy::ErrorStatus::NotCurrentlyAllowed;
    if (tangents.size() != _points.size()) return wy::ErrorStatus::InvalidInput;
    for (const Tangent& tangent : tangents)
    {
        if (!isUsableTangent(tangent)) return wy::ErrorStatus::InvalidInput;
    }

    if (tangents == _tangents) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline_tangents);
    if (wy::ErrorStatus::Ok == error)
    {
        _tangents = tangents;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

void SketchSpline::refreshTangents()
{
    if (!_pBSpline) return;
    if (SplineMode::InterpolationPoints != _mode || _points.size() < 2) return;
    if (_tangents.size() != _points.size())
    {
        assert(false);
        return;
    }

    bool isClosed = isClosedFit(_points);
    const std::size_t count = isClosed ? _points.size() - 1 : _points.size();

    bool isAllDriving = true;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!_tangents[i].isDriving)
        {
            isAllDriving = false;
            break;
        }
    }
    if (isAllDriving) return;

    std::vector<double> params(count, 0.0);
    for (std::size_t i = 1; i < count; ++i)
    {
        params[i] = params[i - 1] + (_points[i] - _points[i - 1]).length();
    }

    struct SplineParam
    {
        bool isPeriodic;
        double firstParam;
        double lastParam;
        double paramSpan;
    } splineParam;
    splineParam.firstParam = _pBSpline->FirstParameter();
    splineParam.lastParam = _pBSpline->LastParameter();
    splineParam.isPeriodic = _pBSpline->IsPeriodic();
    splineParam.paramSpan = splineParam.lastParam - splineParam.firstParam;

    auto derivativeAtFitPoint = [&splineParam, &params, this](std::size_t index, gp_Vec2d& outDerivative) -> bool
    {
        try
        {
            double curveParam(0.0);
            if (splineParam.isPeriodic)
            {
                Geom2dAPI_ProjectPointOnCurve projector(
                    gp_Pnt2d(_points[index].x(), _points[index].y()), _pBSpline);
                if (projector.NbPoints() < 1) return false;
                curveParam = projector.LowerDistanceParameter();
            }
            else
            {
                if (params.back() <= wy3d::TOL) return false;
                curveParam = splineParam.firstParam + splineParam.paramSpan * (params[index] / params.back());
            }

            if (curveParam < splineParam.firstParam) curveParam = splineParam.firstParam;
            else if (curveParam > splineParam.lastParam) curveParam = splineParam.lastParam;

            gp_Pnt2d pntOnCurve;
            _pBSpline->D1(curveParam, pntOnCurve, outDerivative);
            return outDerivative.Magnitude() > wy3d::TOL;
        }
        catch (const Standard_Failure&)
        {
            return false;
        }
    };

    std::vector<Tangent> tangents = _tangents;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (tangents[i].isDriving) continue;

        gp_Vec2d derivative;
        if (derivativeAtFitPoint(i, derivative))
        {
            tangents[i].angle = wy3d::normalizeRadian(std::atan2(derivative.Y(), derivative.X()));
            tangents[i].magnitude = derivative.Magnitude() * spanChordLength(_points, i);
        }
        else
        {
            tangents[i] = chordTangentAt(_points, i);
        }
    }
    this->setTangents(tangents);
}

wy::ErrorStatus SketchSpline::setKnots(const std::vector<double>& knots)
{
    if (SplineMode::ControlPoints != _mode) return wy::ErrorStatus::NotCurrentlyAllowed;
    return this->_setKnots(knots);
}

wy::ErrorStatus SketchSpline::_setKnots(const std::vector<double>& knots)
{
    if (knots == _knots) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline_knots);
    if (wy::ErrorStatus::Ok == error)
    {
        _knots = knots;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchSpline::setMultiplicities(const std::vector<std::uint32_t>& multiplicities)
{
    if (SplineMode::ControlPoints != _mode) return wy::ErrorStatus::NotCurrentlyAllowed;
    return this->_setMultiplicities(multiplicities);
}

wy::ErrorStatus SketchSpline::_setMultiplicities(const std::vector<std::uint32_t>& multiplicities)
{
    if (multiplicities == _multiplicities) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline_multiplicities);
    if (wy::ErrorStatus::Ok == error)
    {
        _multiplicities = multiplicities;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SketchSpline::_setOccSpline(Handle(Geom2d_BSplineCurve) pBSpline)
{
    if (pBSpline == _pBSpline) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline_pBSpline, wydb::ElementDataPieceType::None);
    if (wy::ErrorStatus::Ok == error)
    {
        _pBSpline = pBSpline;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::Vector2 SketchSpline::getPointAt(double t, bool clamp) const
{
    if (_pBSpline.IsNull()) { assert(false); return wy::Vector2(); }
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    else { if (t < 0.0) { assert(false); t = 0.0; } else if (t > 1.0) { assert(false); t = 1.0; } }
    try
    {
        double firstParam = _pBSpline->FirstParameter();
        double lastParam = _pBSpline->LastParameter();
        double curveParam = firstParam + t * (lastParam - firstParam);
        gp_Pnt2d point = _pBSpline->Value(curveParam);
        return wy::Vector2(point.X(), point.Y());
    }
    catch (const Standard_Failure&) { return wy::Vector2(); }
}

wy::Vector2 SketchSpline::getDirectionAt(double t, bool clamp) const
{
    if (_pBSpline.IsNull()) { assert(false); return wy::Vector2(1.0, 0.0); }
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    else { if (t < 0.0) { assert(false); t = 0.0; } else if (t > 1.0) { assert(false); t = 1.0; } }
    try
    {
        const double firstParam = _pBSpline->FirstParameter();
        const double lastParam = _pBSpline->LastParameter();
        const double curveParam = firstParam + t * (lastParam - firstParam);
        gp_Pnt2d point;
        gp_Vec2d tangent;
        _pBSpline->D1(curveParam, point, tangent);
        tangent.Normalize();
        return wy::Vector2(tangent.X(), tangent.Y());
    }
    catch (const Standard_Failure&) { assert(false); return wy::Vector2(1.0, 0.0); }
}

bool SketchSpline::isClosed() const
{
    if (_pBSpline)
    {
        if (_pBSpline->IsPeriodic()) return true;
        else return _pBSpline->IsClosed();
    }

    switch (_mode)
    {
    case SplineMode::InterpolationPoints:
    {
        return isClosedFit(_points);
    }
    case SplineMode::ControlPoints:
    {
        if (_points.size() <= 4) return false;
        const wy::Vector2& firstPnt = _points.front();
        const wy::Vector2& lastPnt = _points.back();
        return firstPnt.x() == lastPnt.x() && firstPnt.y() == lastPnt.y();
    }
    default:
    {
        assert(false);
        return false;
    }
    }
}

bool SketchSpline::isDegenerate(double tol) const { return this->getLength() <= tol; }

double SketchSpline::getLength() const
{
    if (_pBSpline.IsNull()) { assert(false); return 0.0; }
    try
    {
        Geom2dAdaptor_Curve curveAdaptor(_pBSpline);
        return GCPnts_AbscissaPoint::Length(curveAdaptor);
    }
    catch (const Standard_Failure&) { assert(false); return 0.0; }
}

wy3d::geom::BoundingBox2 SketchSpline::getBoundingBox() const
{
    wy3d::geom::BoundingBox2 bbox;
    if (_pBSpline.IsNull()) { assert(false); return bbox; }
    Standard_Integer nbPoles = _pBSpline->NbPoles();
    for (Standard_Integer i = 1; i <= nbPoles; i++)
    {
        gp_Pnt2d pole = _pBSpline->Pole(i);
        bbox.merge(wy::Vector2(pole.X(), pole.Y()));
    }
    return bbox;
}

wy::ErrorStatus SketchSpline::translate(const wy::Vector2& vector)
{
    if (vector == wy::Vector2::kZero) return wy::ErrorStatus::Ok;
    std::vector<wy::Vector2> newPoints;
    newPoints.reserve(_points.size());
    for (const wy::Vector2& pnt : _points) { newPoints.emplace_back(pnt + vector); }
    return this->setPoints(newPoints);
}

wy::ErrorStatus SketchSpline::rotateAround(const wy::Vector2& center, double angle)
{
    if (angle == 0.0) return wy::ErrorStatus::Ok;

    double cosTheta = std::cos(angle);
    double sinTheta = std::sin(angle);
    std::vector<wy::Vector2> newPoints;
    newPoints.reserve(_points.size());
    for (const wy::Vector2& pnt : _points)
    {
        newPoints.emplace_back(SketchEntity::rotateAround(pnt, center, cosTheta, sinTheta));
    }
    wy::ErrorStatus error = this->setPoints(newPoints);
    if (wy::ErrorStatus::Ok != error) return error;

    if (SplineMode::InterpolationPoints == _mode)
    {
        std::vector<Tangent> newTangents = _tangents;
        for (Tangent& tangent : newTangents)
        {
            tangent.angle = wy3d::normalizeRadian(tangent.angle + angle);
        }
        return this->setTangents(newTangents);
    }
    else
    {
        return wy::ErrorStatus::Ok;
    }
}

wy::ErrorStatus SketchSpline::transform(const wy3d::geom::Matrix3& matrix)
{
    std::vector<wy::Vector2> newPoints;
    newPoints.reserve(_points.size());
    for (const wy::Vector2& pnt : _points) { newPoints.emplace_back(pnt * matrix); }

    wy::ErrorStatus error = this->setPoints(newPoints);
    if (wy::ErrorStatus::Ok != error) return error;

    if (SplineMode::InterpolationPoints == _mode)
    {
        // The linear part of the matrix turns the direction, a mirror without a special case, and the
        // sign of a magnitude, which is part of the direction it stands for, comes through untouched.
        // A magnitude is also as long as the span it is measured against, so it takes the stretch the
        // matrix puts on the tangent: a rotation or a mirror leaves it, a scaling multiplies it.
        const double m00 = matrix.get(0, 0), m01 = matrix.get(0, 1);
        const double m10 = matrix.get(1, 0), m11 = matrix.get(1, 1);

        std::vector<Tangent> newTangents = _tangents;
        for (Tangent& tangent : newTangents)
        {
            const double x = std::cos(tangent.angle);
            const double y = std::sin(tangent.angle);
            const double newX = x * m00 + y * m10;
            const double newY = x * m01 + y * m11;
            const double stretch = std::sqrt(newX * newX + newY * newY);
            if (stretch <= wy3d::TOL) continue;
            tangent.angle = wy3d::normalizeRadian(std::atan2(newY, newX));
            tangent.magnitude *= stretch;
        }
        return this->setTangents(newTangents);
    }
    else
    {
        return wy::ErrorStatus::Ok;
    }
}

std::uint32_t SketchSpline::intersectWith(const SketchCurve& other, std::vector<wy::Vector2>& out) const
{
    const SketchCurve* pO = &other;
    if (const auto* pL = dynamic_cast<const SketchLine*>(pO)) return SketchCurveIntersectUtil::intersect(pL, this, out);
    else if (const auto* pCL = dynamic_cast<const SketchCenterLine*>(pO)) return SketchCurveIntersectUtil::intersect(pCL, this, out);
    else if (const auto* pC = dynamic_cast<const SketchCircle*>(pO)) return SketchCurveIntersectUtil::intersect(pC, this, out);
    else if (const auto* pA = dynamic_cast<const SketchArc*>(pO)) return SketchCurveIntersectUtil::intersect(pA, this, out);
    else if (const auto* pE = dynamic_cast<const SketchEllipse*>(pO)) return SketchCurveIntersectUtil::intersect(pE, this, out);
    else if (const auto* pEA = dynamic_cast<const SketchEllipseArc*>(pO)) return SketchCurveIntersectUtil::intersect(pEA, this, out);
    else if (const auto* pS = dynamic_cast<const SketchSpline*>(pO)) return SketchCurveIntersectUtil::intersect(pS, this, out);
    else { assert(false); return 0; }
}

void SketchSpline::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = SketchParamNames::SKETCH_SPLINE_ORDER;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr SketchSpline::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == SketchSpline::classInfo()->className()) {
        if (SketchParamNames::SKETCH_SPLINE_ORDER == paramName) return wydb::ParameterValue::createInteger(_degree + 1);
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus SketchSpline::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == SketchSpline::classInfo()->className()) {
        if (SketchParamNames::SKETCH_SPLINE_ORDER == paramName)
        { if (!paramValue.isInteger()) return wy::ErrorStatus::InvalidInput; return this->setDegree(paramValue.asInteger() - 1); }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchSpline::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchSpline_mode.value():
        value = _mode;
        return true;
    case kSketchSpline_degree.value():
        value = _degree;
        return true;
    case kSketchSpline_points.value():
        value = _points;
        return true;
    case kSketchSpline_knots.value():
        value = _knots;
        return true;
    case kSketchSpline_multiplicities.value():
        value = _multiplicities;
        return true;
    case kSketchSpline_tangents.value():
        value = _tangents;
        return true;
    case kSketchSpline_pBSpline.value():
        value = _pBSpline;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchSpline::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchSpline_mode.value():
        _mode = std::any_cast<SplineMode>(value);
        return true;
    case kSketchSpline_degree.value():
        _degree = std::any_cast<std::uint32_t>(value);
        return true;
    case kSketchSpline_points.value():
        _points = std::any_cast<const std::vector<wy::Vector2>&>(value);
        return true;
    case kSketchSpline_knots.value():
        _knots = std::any_cast<const std::vector<double>&>(value);
        return true;
    case kSketchSpline_multiplicities.value():
        _multiplicities = std::any_cast<const std::vector<std::uint32_t>&>(value);
        return true;
    case kSketchSpline_tangents.value():
        _tangents = std::any_cast<const std::vector<Tangent>&>(value);
        return true;
    case kSketchSpline_pBSpline.value():
        _pBSpline = std::any_cast<Handle(Geom2d_BSplineCurve)>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchSpline::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);

    filer << static_cast<std::int32_t>(_mode) << _degree;
    FilerUtil::writeVector(filer, _points);
    if (SplineMode::ControlPoints == _mode)
    {
        FilerUtil::writeVector(filer, _knots);
        FilerUtil::writeVector(filer, _multiplicities);
    }
    else if (SplineMode::InterpolationPoints == _mode)
    {
        filer << static_cast<std::uint32_t>(_tangents.size());
        for (const Tangent& tangent : _tangents)
        {
            filer << tangent.angle << tangent.magnitude << tangent.isDriving;
        }
    }
    else
    {
        assert(false);
    }

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchSpline::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);

    std::int32_t mode(0);
    filer >> mode;
    _mode = static_cast<SplineMode>(mode);
    filer >> _degree;
    FilerUtil::readVector(filer, _points);

    if (SplineMode::ControlPoints == _mode)
    {
        FilerUtil::readVector(filer, _knots);
        FilerUtil::readVector(filer, _multiplicities);
    }
    else if (SplineMode::InterpolationPoints == _mode)
    {
        if (filer.getFileVersion() >= wydb::FileVersion(0, 20))
        {
            std::uint32_t numTangent(0);
            filer >> numTangent;
            _tangents.resize(numTangent);
            for (Tangent& tangent : _tangents)
            {
                filer >> tangent.angle >> tangent.magnitude >> tangent.isDriving;
            }
        }
        else
        {
            _tangents.resize(_points.size());
        }
    }
    else
    {
        assert(false);
    }

    return wy::ErrorStatus::Ok;
}

void SketchSpline::onChainUpdate(
    const wydb::ElementDataPiece& dirtyDataPiece,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector,
    wydb::ChainUpdateCallbackManager& callbackManager)
{
    switch (dirtyDataPiece.getType())
    {
    case wydb::ElementDataPieceType::Completion:
    {
        this->updateGeometry();
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }
}

void SketchSpline::updateGeometry()
{
    switch (_mode)
    {
    case SplineMode::InterpolationPoints:
    {
        Handle(Geom2d_BSplineCurve) pBSpline = this->newBSpline(_points, _tangents);
        this->_setOccSpline(pBSpline);
        this->refreshTangents();
        if (pBSpline) this->_setDegree(pBSpline->Degree());
        this->_setKnots({});
        this->_setMultiplicities({});
    }
    break;

    case SplineMode::ControlPoints:
    {
        Handle(Geom2d_BSplineCurve) pBSpline;
        if (!_knots.empty() && !_multiplicities.empty() && _knots.size() == _multiplicities.size())
        {
            pBSpline = this->newBSpline(_degree + 1, _points, _knots, _multiplicities);
        }
        else
        {
            assert(_knots.empty());
            assert(_multiplicities.empty());
            pBSpline = this->newBSpline(_degree + 1, _points);
        }
        this->_setOccSpline(pBSpline);
    }
    break;

    default:
    {
        assert(false);
        this->_setOccSpline(nullptr);
    }
    break;
    }
}

Handle(Geom2d_BSplineCurve) SketchSpline::newBSpline(
    const std::vector<wy::Vector2>& points,
    const std::vector<Tangent>& tangents) const
{
    if (points.size() < 2)
    {
        assert(false);
        return nullptr;
    }

    if (points.size() != tangents.size())
    {
        assert(false);
        return nullptr;
    }

    try
    {
        bool isClosed = isClosedFit(points);
        size_t originalNumPoints = points.size();
        size_t numPoints = isClosed ? originalNumPoints - 1 : originalNumPoints;

        Handle(TColgp_HArray1OfPnt2d) occPoints =
            new TColgp_HArray1OfPnt2d(1, static_cast<Standard_Integer>(numPoints));
        for (std::size_t i = 0; i < numPoints; ++i)
        {
            occPoints->SetValue(static_cast<Standard_Integer>(i) + 1,
                gp_Pnt2d(points[i].x(), points[i].y()));
        }
        
        Geom2dAPI_Interpolate interpolator(occPoints, isClosed, wy3d::TOL);

        bool hasDrivingTangent = false;
        for (size_t i = 0; i < numPoints; ++i)
        {
            if (tangents[i].isDriving)
            {
                hasDrivingTangent = true;
                break;
            }
        }
        if (hasDrivingTangent)
        {
            TColgp_Array1OfVec2d occTangents(1, static_cast<Standard_Integer>(numPoints));
            Handle(TColStd_HArray1OfBoolean) occTangentFlags =
                new TColStd_HArray1OfBoolean(1, static_cast<Standard_Integer>(numPoints), Standard_False);
            for (size_t i = 0; i < numPoints; ++i)
            {
                const Tangent tangent = tangents[i];
                double speed = tangent.magnitude / spanChordLength(points, i);
                if (std::fabs(speed) <= wy3d::TOL) speed = 1.0;
                occTangents.SetValue(static_cast<Standard_Integer>(i) + 1,
                    gp_Vec2d(speed * std::cos(tangent.angle), speed * std::sin(tangent.angle)));
                occTangentFlags->SetValue(static_cast<Standard_Integer>(i) + 1,
                    tangent.isDriving ? Standard_True : Standard_False);
            }
            interpolator.Load(occTangents, occTangentFlags, Standard_False);
        }
        interpolator.Perform();
        if (!interpolator.IsDone())
        {
            return nullptr;
        }
        return interpolator.Curve();
    }
    catch (const Standard_Failure&)
    {
        return nullptr;
    }
}

Handle(Geom2d_BSplineCurve) SketchSpline::newBSpline(std::uint32_t order, const std::vector<wy::Vector2>& points) const
{
    try
    {
        if (order < 2 || order > 10) return nullptr;
        std::uint32_t degree = static_cast<std::int32_t>(order) - 1;

        bool isClosed = false;
        if (points.size() >= 5)
        {
            const wy::Vector2& firstPnt = points.front();
            const wy::Vector2& lastPnt = points.back();
            if (firstPnt.x() == lastPnt.x() && firstPnt.y() == lastPnt.y()) isClosed = true;
        }

        std::vector<wy::Vector2> controlPoints = points;
        if (isClosed && !controlPoints.empty()) { controlPoints.pop_back(); }
        std::uint32_t numControlPoints = static_cast<std::uint32_t>(controlPoints.size());

        if (isClosed)
        {
            if (numControlPoints < degree + 1) return nullptr;
            controlPoints.reserve(static_cast<size_t>(numControlPoints) + degree);
            for (unsigned int i = 0; i < degree; ++i) { controlPoints.push_back(controlPoints[i]); }
            numControlPoints = static_cast<std::uint32_t>(controlPoints.size());
        }

        if (numControlPoints < order) return nullptr;

        TColgp_Array1OfPnt2d occControlPoints(1, numControlPoints);
        for (unsigned int i = 0; i < numControlPoints; ++i)
        {
            occControlPoints.SetValue(i + 1, gp_Pnt2d(controlPoints[i].x(), controlPoints[i].y()));
        }

        TColStd_Array1OfReal occKnots;
        TColStd_Array1OfInteger occMults;
        if (isClosed)
        {
            const unsigned int totalKnots = numControlPoints + degree + 1;
            occKnots = TColStd_Array1OfReal(1, totalKnots);
            occMults = TColStd_Array1OfInteger(1, totalKnots);
            if (totalKnots < 2) { assert(false); return nullptr; }
            const double knotSpacing = 1.0 / (totalKnots - 1);
            for (unsigned int i = 1; i <= totalKnots; ++i)
            {
                occKnots.SetValue(i, (i - 1) * knotSpacing);
                occMults.SetValue(i, 1);
            }
            occKnots.SetValue(totalKnots, 1.0);
        }
        else
        {
            const unsigned int numUniqueKnots = numControlPoints - degree + 1;
            occKnots = TColStd_Array1OfReal(1, numUniqueKnots);
            occMults = TColStd_Array1OfInteger(1, numUniqueKnots);
            if (numUniqueKnots < 2) { assert(false); return nullptr; }
            const double knotSpacing = 1.0 / (numUniqueKnots - 1);
            for (unsigned int i = 1; i <= numUniqueKnots; ++i)
            {
                occKnots.SetValue(i, (i - 1) * knotSpacing);
                occMults.SetValue(i, 1);
            }
            occKnots.SetValue(numUniqueKnots, 1.0);
            occMults.SetValue(1, order);
            occMults.SetValue(numUniqueKnots, order);
        }

        Handle(Geom2d_BSplineCurve) spline = new Geom2d_BSplineCurve(occControlPoints, occKnots, occMults, degree);
        if (isClosed) { spline->SetPeriodic(); if (!spline->IsPeriodic()) assert(false); }
        return spline;
    }
    catch (const Standard_Failure&) { assert(false); return nullptr; }
}

Handle(Geom2d_BSplineCurve) SketchSpline::newBSpline(std::uint32_t order,
    const std::vector<wy::Vector2>& controlPoints, const std::vector<double>& knots,
    const std::vector<std::uint32_t>& multiplicities) const
{
    try
    {
        if (order < 2) return nullptr;
        std::uint32_t degree = static_cast<std::int32_t>(order) - 1;
        if (knots.empty() || multiplicities.empty() || knots.size() != multiplicities.size()) return nullptr;

        const std::uint32_t numControlPoints = static_cast<std::uint32_t>(controlPoints.size());
        if (numControlPoints < order) return nullptr;

        TColgp_Array1OfPnt2d occControlPoints(1, numControlPoints);
        for (unsigned int i = 0; i < numControlPoints; ++i)
        {
            occControlPoints.SetValue(i + 1, gp_Pnt2d(controlPoints[i].x(), controlPoints[i].y()));
        }

        std::uint32_t numKnots = static_cast<std::uint32_t>(knots.size());
        TColStd_Array1OfReal occKnots(1, numKnots);
        for (unsigned int i = 0; i < numKnots; ++i) { occKnots.SetValue(i + 1, knots[i]); }

        std::uint32_t numMults = static_cast<std::uint32_t>(multiplicities.size());
        TColStd_Array1OfInteger occMults(1, numMults);
        for (unsigned int i = 0; i < numMults; ++i) { occMults.SetValue(i + 1, multiplicities[i]); }

        return new Geom2d_BSplineCurve(occControlPoints, occKnots, occMults, degree);
    }
    catch (const Standard_Failure&) { return nullptr; }
}

NS_WY3D_END
