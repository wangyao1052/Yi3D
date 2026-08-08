///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SPHERE_H
#define WY3D_SPHERE_H

#include <wy3dDefs.h>
#include <wy3dPrimitive.h>

NS_WY3D_BEG

class WY3D_EXPORT Sphere : public wy3d::Primitive
{
    WYDB_DECLARE_MEMBERS(Sphere, wy3d::Sphere, wy3d::Primitive)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, double radius, Sphere*& pOut);

    double getRadius() const { return _radius; }
    wy::ErrorStatus setRadius(double radius);

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual TopoDS_Shape generateOriginalShape() const override;

private:
    double _radius;
};

NS_WY3D_END

#endif // WY3D_SPHERE_H