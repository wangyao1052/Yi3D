///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_CENTER_LINE_H
#define WY3D_SKETCH_CENTER_LINE_H

#include <wyVector2.h>
#include <wy3dDefs.h>
#include <wy3dSketchCurve.h>
#include <wy3dImpl.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchCenterLine : public wy3d::SketchCurve
{
    WYDB_DECLARE_MEMBERS(SketchCenterLine, wy3d::SketchCenterLine, wy3d::SketchCurve)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, const wy::Vector2& startPnt, const wy::Vector2& endPnt, SketchCenterLine*& pOut);

    virtual wy::Vector2 getStartPoint() const override { return _startPnt; }
    wy::ErrorStatus setStartPoint(const wy::Vector2& startPnt);
    virtual wy::Vector2 getEndPoint() const override { return _endPnt; }
    wy::ErrorStatus setEndPoint(const wy::Vector2& endPnt);
    virtual wy::Vector2 getPointAt(double t, bool clamp=true) const override;
    wy::Vector2 getDirection() const;
    virtual bool isClosed() const override { return false; }
    virtual bool isDegenerate(double tol) const override;
    virtual double getLength() const override { return (_endPnt-_startPnt).length(); }
    virtual wy3d::BoundingBox2 getBoundingBox() const override
    { double minX=std::min(_startPnt.x(),_endPnt.x()),maxX=std::max(_startPnt.x(),_endPnt.x()),minY=std::min(_startPnt.y(),_endPnt.y()),maxY=std::max(_startPnt.y(),_endPnt.y()); return wy3d::BoundingBox2(wy::Vector2(minX-wy3d::TOL,minY-wy3d::TOL),wy::Vector2(maxX+wy3d::TOL,maxY+wy3d::TOL)); }

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
    wy::Vector2 _startPnt;
    wy::Vector2 _endPnt;
};

NS_WY3D_END

#endif // WY3D_SKETCH_CENTER_LINE_H