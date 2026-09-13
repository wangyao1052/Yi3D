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
#include <GeomAPI_Interpolate.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GCPnts_AbscissaPoint.hxx>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSketchSpline3D.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketch3DParamNames.h>
#include "utils/FilerUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(SketchSpline3D)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchSpline3D, _mode)
    REGISTER_FIELD(SketchSpline3D, _degree)
    REGISTER_FIELD(SketchSpline3D, _points)
    REGISTER_FIELD(SketchSpline3D, _knots)
    REGISTER_FIELD(SketchSpline3D, _multiplicities)
    REGISTER_FIELD(SketchSpline3D, _pBSpline)
END_FIELD_REGISTRATION()

SketchSpline3D::SketchSpline3D() : wy3d::SketchCurve3D()
    , _mode(SplineMode::Undefined), _degree(0), _pBSpline(nullptr)
{
}

SketchSpline3D::~SketchSpline3D()
{
}

wy::ErrorStatus SketchSpline3D::create(wydb::Transaction* pTrans, const std::vector<wy::Vector3>& fitPoints,
    SketchSpline3D*& pOut)
{
    pOut = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (fitPoints.size() < 2) return wy::ErrorStatus::InvalidInput;

    SketchSpline3D* pSketchSpline3D = new SketchSpline3D();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchSpline3D);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSketchSpline3D);
        return error;
    }

    error = pSketchSpline3D->setMode(SplineMode::InterpolationPoints);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline3D)
    error = pSketchSpline3D->setPoints(fitPoints);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline3D)

    pOut = pSketchSpline3D;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchSpline3D::create(wydb::Transaction* pTrans, std::uint32_t degree,
    const std::vector<wy::Vector3>& controlPoints, SketchSpline3D*& pOut)
{
    pOut = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (degree < 1 || degree > 8) return wy::ErrorStatus::InvalidInput;
    if (controlPoints.size() < 2) return wy::ErrorStatus::InvalidInput;

    SketchSpline3D* pSketchSpline3D = new SketchSpline3D();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchSpline3D);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSketchSpline3D);
        return error;
    }

    error = pSketchSpline3D->setMode(SplineMode::ControlPoints);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline3D)
    error = pSketchSpline3D->setDegree(degree);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline3D)
    error = pSketchSpline3D->setPoints(controlPoints);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline3D)

    pOut = pSketchSpline3D;
    return wy::ErrorStatus::Ok;
}

namespace
{
// Validity of a custom knot vector: degree/pole count bounds, knot multiplicities,
// the knot sequence rule (sum(mults) = nbPoles + degree + 1) and strictly increasing knots.
bool isValidKnotVector(std::uint32_t degree, std::size_t numPoles,
    const std::vector<double>& knots, const std::vector<std::uint32_t>& multiplicities)
{
    if (degree < 1 || degree > 8) return false;
    if (numPoles < degree + 1) return false;
    if (knots.size() != multiplicities.size() || knots.size() < 2) return false;

    std::size_t multSum = 0;
    for (const std::uint32_t mult : multiplicities)
    {
        if (mult < 1 || mult > degree + 1) return false;
        multSum += mult;
    }
    if (multSum != numPoles + degree + 1) return false;

    for (std::size_t i = 1; i < knots.size(); ++i)
    {
        if (!(knots[i] > knots[i - 1])) return false;
    }
    return true;
}
}

wy::ErrorStatus SketchSpline3D::create(wydb::Transaction* pTrans, std::uint32_t degree,
    const std::vector<wy::Vector3>& controlPoints, const std::vector<double>& knots,
    const std::vector<std::uint32_t>& multiplicities, SketchSpline3D*& pOut)
{
    pOut = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!isValidKnotVector(degree, controlPoints.size(), knots, multiplicities))
        return wy::ErrorStatus::InvalidInput;

    SketchSpline3D* pSketchSpline3D = new SketchSpline3D();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketchSpline3D);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSketchSpline3D);
        return error;
    }

    error = pSketchSpline3D->setMode(SplineMode::ControlPoints);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline3D)
    error = pSketchSpline3D->setDegree(degree);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline3D)
    error = pSketchSpline3D->setPoints(controlPoints);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline3D)
    error = pSketchSpline3D->setKnots(knots);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline3D)
    error = pSketchSpline3D->setMultiplicities(multiplicities);
    CHECK_ERROR_FOR_CREATE(error, pSketchSpline3D)

    pOut = pSketchSpline3D;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchSpline3D::setMode(SplineMode mode)
{
    if (SplineMode::InterpolationPoints != mode && SplineMode::ControlPoints != mode) return wy::ErrorStatus::InvalidInput;
    if (mode == _mode) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline3D_mode);
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

wy::ErrorStatus SketchSpline3D::setDegree(std::uint32_t degree)
{
    if (SplineMode::ControlPoints != _mode) return wy::ErrorStatus::NotCurrentlyAllowed;
    return this->_setDegree(degree);
}

wy::ErrorStatus SketchSpline3D::_setDegree(std::uint32_t degree)
{
    if (degree < 1 || degree > 8) return wy::ErrorStatus::InvalidInput;
    if (degree == _degree) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline3D_degree);
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

wy::ErrorStatus SketchSpline3D::setPoints(const std::vector<wy::Vector3>& points)
{
    if (points.empty()) return wy::ErrorStatus::InvalidInput;
    if (points == _points) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline3D_points);
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

wy::ErrorStatus SketchSpline3D::setKnots(const std::vector<double>& knots)
{
    if (SplineMode::ControlPoints != _mode) return wy::ErrorStatus::NotCurrentlyAllowed;
    return this->_setKnots(knots);
}

wy::ErrorStatus SketchSpline3D::_setKnots(const std::vector<double>& knots)
{
    if (knots == _knots) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline3D_knots);
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

wy::ErrorStatus SketchSpline3D::setMultiplicities(const std::vector<std::uint32_t>& multiplicities)
{
    if (SplineMode::ControlPoints != _mode) return wy::ErrorStatus::NotCurrentlyAllowed;
    return this->_setMultiplicities(multiplicities);
}

wy::ErrorStatus SketchSpline3D::_setMultiplicities(const std::vector<std::uint32_t>& multiplicities)
{
    if (multiplicities == _multiplicities) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline3D_multiplicities);
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

Handle(Geom_BSplineCurve) SketchSpline3D::getOccSpline() const
{
    if (!_pBSpline.IsNull()) return _pBSpline;
    return this->computeCurve();
}

void SketchSpline3D::setOccSplineImpl(const Handle(Geom_BSplineCurve)& pBSpline)
{
    if (pBSpline == _pBSpline) return;
    wy::ErrorStatus error = this->prepareForFieldChange(kSketchSpline3D_pBSpline, wydb::ElementDataPieceType::None);
    assert(wy::ErrorStatus::Ok == error);
    _pBSpline = pBSpline;
}

Handle(Geom_BSplineCurve) SketchSpline3D::computeCurve() const
{
    switch (_mode)
    {
    case SplineMode::InterpolationPoints:
        return this->newInterpolatedCurve(_points);

    case SplineMode::ControlPoints:
        if (!_knots.empty() && !_multiplicities.empty() && _knots.size() == _multiplicities.size())
        {
            return this->newControlPointCurve(_degree + 1, _points, _knots, _multiplicities);
        }
        return this->newControlPointCurve(_degree + 1, _points);

    default:
        assert(false);
        return nullptr;
    }
}

wy::Vector3 SketchSpline3D::getStartPoint() const
{
    return this->getPointAt(0.0);
}

wy::Vector3 SketchSpline3D::getEndPoint() const
{
    return this->getPointAt(1.0);
}

wy::Vector3 SketchSpline3D::getPointAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    else { if (t < 0.0) { assert(false); t = 0.0; } else if (t > 1.0) { assert(false); t = 1.0; } }

    Handle(Geom_BSplineCurve) pCurve = this->getOccSpline();
    if (pCurve.IsNull()) { assert(false); return wy::Vector3::kZero; }

    const double firstParam = pCurve->FirstParameter();
    const double lastParam = pCurve->LastParameter();
    gp_Pnt point = pCurve->Value(firstParam + t * (lastParam - firstParam));
    return wy::Vector3(point.X(), point.Y(), point.Z());
}

wy::Vector3 SketchSpline3D::getDirectionAt(double t, bool clamp) const
{
    if (clamp) { if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    else { if (t < 0.0) { assert(false); t = 0.0; } else if (t > 1.0) { assert(false); t = 1.0; } }

    Handle(Geom_BSplineCurve) pCurve = this->getOccSpline();
    if (pCurve.IsNull()) { assert(false); return wy::Vector3::kZero; }

    try
    {
        const double firstParam = pCurve->FirstParameter();
        const double lastParam = pCurve->LastParameter();
        gp_Pnt point;
        gp_Vec tangent;
        pCurve->D1(firstParam + t * (lastParam - firstParam), point, tangent);
        const double magnitude = tangent.Magnitude();
        if (magnitude <= wy3d::EPS) return wy::Vector3::kZero;
        tangent.Normalize();
        return wy::Vector3(tangent.X(), tangent.Y(), tangent.Z());
    }
    catch (const Standard_Failure&)
    {
        return wy::Vector3::kZero;
    }
}

bool SketchSpline3D::isClosed() const
{
    Handle(Geom_BSplineCurve) pCurve = this->getOccSpline();
    if (!pCurve.IsNull())
    {
        if (pCurve->IsPeriodic()) return true;
        else return pCurve->IsClosed();
    }

    switch (_mode)
    {
    case SplineMode::InterpolationPoints:
    {
        if (_points.size() <= 3) return false;
        return _points.front() == _points.back();
    }
    case SplineMode::ControlPoints:
    {
        if (_points.size() <= 4) return false;
        return _points.front() == _points.back();
    }
    default:
    {
        assert(false);
        return false;
    }
    }
}

bool SketchSpline3D::isDegenerate(double tol) const
{
    return this->getLength() <= tol;
}

double SketchSpline3D::getLength() const
{
    Handle(Geom_BSplineCurve) pCurve = this->getOccSpline();
    if (pCurve.IsNull())
    {
        assert(false);
        return 0.0;
    }

    try
    {
        GeomAdaptor_Curve curveAdaptor(pCurve);
        return GCPnts_AbscissaPoint::Length(curveAdaptor);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return 0.0;
    }
}

void SketchSpline3D::onChainUpdate(
    const wydb::ElementDataPiece& dirtyDataPiece,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector,
    wydb::ChainUpdateCallbackManager& callbackManager)
{
    switch (dirtyDataPiece.getType())
    {
    case wydb::ElementDataPieceType::Completion:
    case wydb::ElementDataPieceType::Shape:
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

void SketchSpline3D::updateGeometry()
{
    Handle(Geom_BSplineCurve) pCurve = this->computeCurve();
    this->setOccSplineImpl(pCurve);
    // 次数回读实际曲线:插值式由插值器决定,控制点式在点数不足时会被降次
    if (!pCurve.IsNull()) this->_setDegree(static_cast<std::uint32_t>(pCurve->Degree()));
    if (SplineMode::InterpolationPoints == _mode)
    {
        this->_setKnots({});
        this->_setMultiplicities({});
    }
}

Handle(Geom_BSplineCurve) SketchSpline3D::newInterpolatedCurve(const std::vector<wy::Vector3>& points) const
{
    try
    {
        if (points.size() < 2) { assert(false); return nullptr; }
        std::vector<wy::Vector3> fitPoints = points;

        bool isClosed = false;
        if (fitPoints.size() >= 4 && fitPoints.front() == fitPoints.back())
        {
            isClosed = true;
            fitPoints.pop_back();
        }

        const size_t numPoints = fitPoints.size();
        Handle(TColgp_HArray1OfPnt) occPoints = new TColgp_HArray1OfPnt(1, static_cast<Standard_Integer>(numPoints));
        for (size_t i = 0; i < numPoints; ++i)
        {
            const wy::Vector3& pnt = fitPoints[i];
            occPoints->SetValue(static_cast<Standard_Integer>(i) + 1, gp_Pnt(pnt.x(), pnt.y(), pnt.z()));
        }

        GeomAPI_Interpolate interpolator(occPoints, isClosed, wy3d::TOL);
        interpolator.Perform();

        if (!interpolator.IsDone()) { assert(false); return nullptr; }
        return interpolator.Curve();
    }
    catch (const Standard_Failure&)
    {
        return nullptr;
    }
}

Handle(Geom_BSplineCurve) SketchSpline3D::newControlPointCurve(std::uint32_t order, const std::vector<wy::Vector3>& points) const
{
    try
    {
        if (order < 2 || order > 10) return nullptr;
        if (points.size() < 2) return nullptr;

        std::uint32_t degree = order - 1;

        bool isClosed = false;
        if (points.size() >= 5 && points.front() == points.back()) isClosed = true;

        std::vector<wy::Vector3> controlPoints = points;
        if (isClosed && !controlPoints.empty()) controlPoints.pop_back();
        const std::uint32_t numDistinct = static_cast<std::uint32_t>(controlPoints.size());

        // 可用极点数撑不起该次数时降次,而不是返回空缓存(空缓存会让整个草图的 isDegenerate 成立);
        // 闭合情形要按去重后的极点数算,否则闭合重复点会把可用点数算多一个
        if (numDistinct - 1 < degree) degree = numDistinct - 1;
        order = degree + 1;
        if (numDistinct < order) return nullptr; // 上面的降次已保证,留作兜底

        if (isClosed)
        {
            // 周期曲线:极点表补 degree 个回绕极点(周期 = 去重后的极点数),
            // 节点重数全为 1,节点数 = 极点数 + 1(K 序列规则 sum(mults) = nbPoles + 1)
            controlPoints.reserve(controlPoints.size() + degree);
            for (std::uint32_t i = 0; i < degree; ++i)
            {
                controlPoints.push_back(controlPoints[i]);
            }
        }

        const std::uint32_t numControlPoints = static_cast<std::uint32_t>(controlPoints.size());
        TColgp_Array1OfPnt occControlPoints(1, numControlPoints);
        for (std::uint32_t i = 0; i < numControlPoints; ++i)
        {
            const wy::Vector3& pnt = controlPoints[i];
            occControlPoints.SetValue(i + 1, gp_Pnt(pnt.x(), pnt.y(), pnt.z()));
        }

        TColStd_Array1OfReal occKnots;
        TColStd_Array1OfInteger occMults;
        if (isClosed)
        {
            const std::uint32_t totalKnots = numControlPoints + 1;
            occKnots = TColStd_Array1OfReal(1, totalKnots);
            occMults = TColStd_Array1OfInteger(1, totalKnots);
            const double knotSpacing = 1.0 / (totalKnots - 1);
            for (std::uint32_t i = 1; i <= totalKnots; ++i)
            {
                occKnots.SetValue(i, (i - 1) * knotSpacing);
                occMults.SetValue(i, 1);
            }
            occKnots.SetValue(totalKnots, 1.0);
        }
        else
        {
            // 非周期:均匀 clamped(两端重数 = order)
            const std::uint32_t numUniqueKnots = numControlPoints - degree + 1;
            if (numUniqueKnots < 2) { assert(false); return nullptr; }
            occKnots = TColStd_Array1OfReal(1, numUniqueKnots);
            occMults = TColStd_Array1OfInteger(1, numUniqueKnots);
            const double knotSpacing = 1.0 / (numUniqueKnots - 1);
            for (std::uint32_t i = 1; i <= numUniqueKnots; ++i)
            {
                occKnots.SetValue(i, (i - 1) * knotSpacing);
                occMults.SetValue(i, 1);
            }
            occKnots.SetValue(numUniqueKnots, 1.0);
            occMults.SetValue(1, order);
            occMults.SetValue(numUniqueKnots, order);
        }

        return new Geom_BSplineCurve(occControlPoints, occKnots, occMults, degree, isClosed);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
}

Handle(Geom_BSplineCurve) SketchSpline3D::newControlPointCurve(std::uint32_t order,
    const std::vector<wy::Vector3>& points, const std::vector<double>& knots,
    const std::vector<std::uint32_t>& multiplicities) const
{
    try
    {
        // One above the highest degree the entity accepts, i.e. degree 8 is order 9.
        if (order < 2 || order > 9) return nullptr;
        const std::uint32_t degree = order - 1;
        if (!isValidKnotVector(degree, points.size(), knots, multiplicities)) return nullptr;

        const std::uint32_t numControlPoints = static_cast<std::uint32_t>(points.size());
        TColgp_Array1OfPnt occControlPoints(1, numControlPoints);
        for (std::uint32_t i = 0; i < numControlPoints; ++i)
        {
            const wy::Vector3& pnt = points[i];
            occControlPoints.SetValue(i + 1, gp_Pnt(pnt.x(), pnt.y(), pnt.z()));
        }

        const std::uint32_t numKnots = static_cast<std::uint32_t>(knots.size());
        TColStd_Array1OfReal occKnots(1, numKnots);
        TColStd_Array1OfInteger occMults(1, numKnots);
        for (std::uint32_t i = 0; i < numKnots; ++i)
        {
            occKnots.SetValue(i + 1, knots[i]);
            occMults.SetValue(i + 1, static_cast<Standard_Integer>(multiplicities[i]));
        }

        return new Geom_BSplineCurve(occControlPoints, occKnots, occMults, degree);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
}

void SketchSpline3D::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_SPLINE3D_PARAM_ORDER;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr SketchSpline3D::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == SketchSpline3D::classInfo()->className())
    {
        if (Sketch3DParamNames::SKETCH_SPLINE3D_PARAM_ORDER == paramName)
            return wydb::ParameterValue::createInteger(static_cast<int>(_degree + 1));
        return nullptr;
    }
    else
    {
        return __baseClass::getParameterValue(className, paramName);
    }
}

wy::ErrorStatus SketchSpline3D::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == SketchSpline3D::classInfo()->className())
    {
        if (Sketch3DParamNames::SKETCH_SPLINE3D_PARAM_ORDER == paramName)
        {
            if (!paramValue.isInteger()) return wy::ErrorStatus::InvalidInput;
            const int order = paramValue.asInteger();
            if (order < 2) return wy::ErrorStatus::InvalidInput;
            return this->setDegree(static_cast<std::uint32_t>(order - 1));
        }
        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool SketchSpline3D::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchSpline3D_mode.value():
        value = _mode;
        return true;
    case kSketchSpline3D_degree.value():
        value = _degree;
        return true;
    case kSketchSpline3D_points.value():
        value = _points;
        return true;
    case kSketchSpline3D_knots.value():
        value = _knots;
        return true;
    case kSketchSpline3D_multiplicities.value():
        value = _multiplicities;
        return true;
    case kSketchSpline3D_pBSpline.value():
        value = _pBSpline;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchSpline3D::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchSpline3D_mode.value():
        _mode = std::any_cast<SplineMode>(value);
        return true;
    case kSketchSpline3D_degree.value():
        _degree = std::any_cast<std::uint32_t>(value);
        return true;
    case kSketchSpline3D_points.value():
        _points = std::any_cast<const std::vector<wy::Vector3>&>(value);
        return true;
    case kSketchSpline3D_knots.value():
        _knots = std::any_cast<const std::vector<double>&>(value);
        return true;
    case kSketchSpline3D_multiplicities.value():
        _multiplicities = std::any_cast<const std::vector<std::uint32_t>&>(value);
        return true;
    case kSketchSpline3D_pBSpline.value():
        _pBSpline = std::any_cast<Handle(Geom_BSplineCurve)>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchSpline3D::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << static_cast<std::int32_t>(_mode) << _degree;
    FilerUtil::writeVector(filer, _points);
    if (SplineMode::ControlPoints == _mode)
    {
        FilerUtil::writeVector(filer, _knots);
        FilerUtil::writeVector(filer, _multiplicities);
    }
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchSpline3D::readFromFiler(wydb::InFiler& filer)
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
    return wy::ErrorStatus::Ok;
}

NS_WY3D_END
