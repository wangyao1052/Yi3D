///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_CURVE_H
#define WY3D_SKETCH_CURVE_H

#include <vector>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wy3dDefs.h>
#include <wy3dSketchEntity.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchCurve : public wy3d::SketchEntity
{
    WYDB_DECLARE_ABSTRACT_MEMBERS(SketchCurve, wy3d::SketchCurve, wy3d::SketchEntity)

public:
    // Parameter names
    static constexpr const char* PARAM_IS_CONSTRUCTION = "IsConstruction";

    bool isConstruction() const { return _isConstruction; }
    wy::ErrorStatus setConstruction(bool value);

    virtual wy::Vector2 getStartPoint() const { return wy::Vector2::kZero; }
    virtual wy::Vector2 getEndPoint() const { return wy::Vector2::kZero; }
    virtual wy::Vector2 getPointAt(double t, bool clamp = true) const { return wy::Vector2::kZero; }
    virtual wy::Vector2 getDirectionAt(double t, bool clamp = true) const { return wy::Vector2::kZero; }
    virtual bool isClosed() const { return false; }
    virtual bool isDegenerate(double tol) const { return false; }
    virtual double getLength() const { return 0.0; }
    virtual unsigned int intersectWith(const SketchCurve& other, std::vector<wy::Vector2>& outIntPnts) const { return 0; }

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer);

protected:
    bool _isConstruction;
};

NS_WY3D_END

#endif // WY3D_SKETCH_CURVE_H