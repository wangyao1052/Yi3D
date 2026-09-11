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

#ifndef WY3D_SKETCH_SPLINE3D_H
#define WY3D_SKETCH_SPLINE3D_H

#include <cstdint>
#include <vector>
#include <wyVector3.h>
#include <wy3dDefs.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchSpline.h>
#include <Geom_BSplineCurve.hxx>

NS_WY3D_BEG

class WY3D_EXPORT SketchSpline3D : public wy3d::SketchCurve3D
{
    WYDB_DECLARE_MEMBERS(SketchSpline3D, wy3d::SketchSpline3D, wy3d::SketchCurve3D)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        const std::vector<wy::Vector3>& fitPoints,
        SketchSpline3D*& pOutSketchSpline3D);
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        std::uint32_t degree,
        const std::vector<wy::Vector3>& controlPoints,
        SketchSpline3D*& pOutSketchSpline3D);

    SplineMode getMode() const { return _mode; }
    wy::ErrorStatus setMode(SplineMode mode);

    std::uint32_t getDegree() const { return _degree; }
    wy::ErrorStatus setDegree(std::uint32_t degree);

    const std::vector<wy::Vector3>& getPoints() const { return _points; }
    wy::ErrorStatus setPoints(const std::vector<wy::Vector3>& points);

    const std::vector<double>& getKnots() const { return _knots; }
    wy::ErrorStatus setKnots(const std::vector<double>& knots);

    const std::vector<std::uint32_t>& getMultiplicities() const { return _multiplicities; }
    wy::ErrorStatus setMultiplicities(const std::vector<std::uint32_t>& multiplicities);

    Handle(Geom_BSplineCurve) getOccSpline() const;

    virtual wy::Vector3 getStartPoint() const override;
    virtual wy::Vector3 getEndPoint() const override;
    virtual wy::Vector3 getPointAt(double t, bool clamp = true) const override;
    virtual wy::Vector3 getDirectionAt(double t, bool clamp = true) const override;
    virtual bool isClosed() const override;
    virtual bool isDegenerate(double tol) const override;
    virtual double getLength() const override;

public:
    virtual wydb::ParameterValueUPtr getParameterValue(
        const std::string& className,
        const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(
        const std::string& className,
        const std::string& paramName,
        const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual void onChainUpdate(
        const wydb::ElementDataPiece& dirtyDataPiece,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector,
        wydb::ChainUpdateCallbackManager& callbackManager) override;

private:
    void setOccSplineImpl(const Handle(Geom_BSplineCurve)& pBSpline);
    void updateGeometry();
    Handle(Geom_BSplineCurve) computeCurve() const;
    Handle(Geom_BSplineCurve) newInterpolatedCurve(const std::vector<wy::Vector3>& points) const;
    Handle(Geom_BSplineCurve) newControlPointCurve(std::uint32_t order, const std::vector<wy::Vector3>& points) const;

    wy::ErrorStatus _setDegree(std::uint32_t degree);
    wy::ErrorStatus _setKnots(const std::vector<double>& knots);
    wy::ErrorStatus _setMultiplicities(const std::vector<std::uint32_t>& multiplicities);

private:
    SplineMode _mode;
    std::uint32_t _degree;
    std::vector<wy::Vector3> _points;
    std::vector<double> _knots;
    std::vector<std::uint32_t> _multiplicities;
    Handle(Geom_BSplineCurve) _pBSpline;
};

NS_WY3D_END

#endif // WY3D_SKETCH_SPLINE3D_H
