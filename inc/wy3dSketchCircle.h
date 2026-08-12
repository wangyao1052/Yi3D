///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_CIRCLE_H
#define WY3D_SKETCH_CIRCLE_H

#include <wyVector2.h>
#include <wy3dDefs.h>
#include <wy3dSketchCurve.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchCircle : public wy3d::SketchCurve
{
    WYDB_DECLARE_MEMBERS(SketchCircle, wy3d::SketchCircle, wy3d::SketchCurve)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, const wy::Vector2& center, double radius, SketchCircle*& pOut);

    virtual wy::Vector2 getStartPoint() const override;
    virtual wy::Vector2 getEndPoint() const override;
    virtual wy::Vector2 getPointAt(double t, bool clamp=true) const override;
    virtual wy::Vector2 getDirectionAt(double t, bool clamp=true) const override;
    virtual bool isClosed() const override { return true; }
    virtual bool isDegenerate(double tol) const override;
    virtual double getLength() const override;
    virtual wy3d::BoundingBox2 getBoundingBox() const override;

    const wy::Vector2& getCenter() const { return _centerPnt; }
    wy::ErrorStatus setCenter(const wy::Vector2& centerPoint);

    double getRadius() const { return _radius; }
    wy::ErrorStatus setRadius(double radius);

    virtual wy::ErrorStatus translate(const wy::Vector2& vector) override;
    virtual wy::ErrorStatus rotateAround(const wy::Vector2& center, double angle) override;
    virtual wy::ErrorStatus transform(const wy3d::Matrix3& matrix) override;
    virtual unsigned int intersectWith(const SketchCurve& other, std::vector<wy::Vector2>& outIntPnts) const override;

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;

private:
    wy::Vector2 _centerPnt;
    double _radius;
};

NS_WY3D_END

#endif // WY3D_SKETCH_CIRCLE_H