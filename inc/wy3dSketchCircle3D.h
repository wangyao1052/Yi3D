///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_CIRCLE3D_H
#define WY3D_SKETCH_CIRCLE3D_H

#include <wyVector3.h>
#include <wy3dDefs.h>
#include <wy3dSketchCurve3D.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchCircle3D : public wy3d::SketchCurve3D
{
    WYDB_DECLARE_MEMBERS(SketchCircle3D, wy3d::SketchCircle3D, wy3d::SketchCurve3D)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        const wy::Vector3& center,
        const wy::Vector3& normal,
        const wy::Vector3& xDir,
        double radius,
        SketchCircle3D*& pOutSketchCircle3D);

    const wy::Vector3& getCenter() const { return _centerPnt; }
    wy::ErrorStatus setCenter(const wy::Vector3& centerPnt);

    const wy::Vector3& getNormal() const { return _normal; }
    const wy::Vector3& getXDir() const { return _xDir; }
    wy::ErrorStatus setPlane(const wy::Vector3& normal, const wy::Vector3& xDir);

    double getRadius() const { return _radius; }
    wy::ErrorStatus setRadius(double radius);

    virtual wy::Vector3 getStartPoint() const override;
    virtual wy::Vector3 getEndPoint() const override;
    virtual wy::Vector3 getPointAt(double t, bool clamp = true) const override;
    virtual wy::Vector3 getDirectionAt(double t, bool clamp = true) const override;
    virtual bool isClosed() const override { return true; }
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

private:
    wy::ErrorStatus setNormalImpl(const wy::Vector3& normal);
    wy::ErrorStatus setXDirImpl(const wy::Vector3& xDir);

private:
    wy::Vector3 _centerPnt;
    wy::Vector3 _normal;
    wy::Vector3 _xDir;
    double _radius;
};

NS_WY3D_END

#endif // WY3D_SKETCH_CIRCLE3D_H
