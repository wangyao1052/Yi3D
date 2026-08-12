///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_SPLINE_H
#define WY3D_SKETCH_SPLINE_H

#include <vector>
#include <Geom2d_BSplineCurve.hxx>
#include <wyVector2.h>
#include <wy3dDefs.h>
#include <wy3dSketchCurve.h>
#include <wy3dImpl.h>

NS_WY3D_BEG

enum class SplineMode : std::int32_t { Undefined=0, InterpolationPoints=1, ControlPoints=2 };

class WY3D_EXPORT SketchSpline : public wy3d::SketchCurve
{
    WYDB_DECLARE_MEMBERS(SketchSpline, wy3d::SketchSpline, wy3d::SketchCurve)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, const std::vector<wy::Vector2>& fitPoints, SketchSpline*& pOut);
    static wy::ErrorStatus create(wydb::Transaction* pTrans, std::uint32_t degree, const std::vector<wy::Vector2>& controlPoints, SketchSpline*& pOut);
    static wy::ErrorStatus create(wydb::Transaction* pTrans, std::uint32_t degree, const std::vector<wy::Vector2>& controlPoints, const std::vector<double>& knots, const std::vector<std::uint32_t>& multiplicities, SketchSpline*& pOut);

    SplineMode getMode() const { return _mode; }
    wy::ErrorStatus setMode(SplineMode mode);
    std::uint32_t getDegree() const { return _degree; }
    wy::ErrorStatus setDegree(std::uint32_t degree);
    const std::vector<wy::Vector2>& getPoints() const { return _points; }
    wy::ErrorStatus setPoints(const std::vector<wy::Vector2>& points);
    const std::vector<double>& getKnots() const { return _knots; }
    wy::ErrorStatus setKnots(const std::vector<double>& knots);
    const std::vector<std::uint32_t> getMultiplicities() const { return _multiplicities; }
    wy::ErrorStatus setMultiplicities(const std::vector<std::uint32_t>& multiplicities);

    Handle(Geom2d_BSplineCurve) getOccSpline() const { return _pBSpline; }
    wy::ErrorStatus _setOccSpline(Handle(Geom2d_BSplineCurve) pBSpline);

    virtual wy::Vector2 getStartPoint() const override { return getPointAt(0.0); }
    virtual wy::Vector2 getEndPoint() const override { return getPointAt(1.0); }
    virtual wy::Vector2 getPointAt(double t, bool clamp=true) const override;
    virtual wy::Vector2 getDirectionAt(double t, bool clamp=true) const override;
    virtual bool isClosed() const override;
    virtual bool isDegenerate(double tol) const override;
    virtual double getLength() const override;
    virtual wy3d::BoundingBox2 getBoundingBox() const override;

    virtual wy::ErrorStatus translate(const wy::Vector2& vector) override;
    virtual wy::ErrorStatus rotateAround(const wy::Vector2& center, double angle) override;
    virtual wy::ErrorStatus transform(const wy3d::Matrix3& matrix) override;
    virtual std::uint32_t intersectWith(const SketchCurve& other, std::vector<wy::Vector2>& outIntPnts) const override;

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;
    void updateGeometry();

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
    wy::ErrorStatus _setDegree(std::uint32_t degree);
    wy::ErrorStatus _setKnots(const std::vector<double>& knots);
    wy::ErrorStatus _setMultiplicities(const std::vector<std::uint32_t>& multiplicities);
    Handle(Geom2d_BSplineCurve) newBSpline(const std::vector<wy::Vector2>& fitPoints) const;
    Handle(Geom2d_BSplineCurve) newBSpline(std::uint32_t order, const std::vector<wy::Vector2>& controlPoints) const;
    Handle(Geom2d_BSplineCurve) newBSpline(std::uint32_t order, const std::vector<wy::Vector2>& controlPoints, const std::vector<double>& knots, const std::vector<std::uint32_t>& multiplicities) const;

private:
    SplineMode _mode;
    std::uint32_t _degree;
    std::vector<wy::Vector2> _points;
    std::vector<double> _knots;
    std::vector<std::uint32_t> _multiplicities;
    Handle(Geom2d_BSplineCurve) _pBSpline;
};

NS_WY3D_END

#endif // WY3D_SKETCH_SPLINE_H