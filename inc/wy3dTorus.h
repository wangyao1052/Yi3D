///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_TORUS_H
#define WY3D_TORUS_H

#include <wy3dDefs.h>
#include <wy3dPrimitive.h>

NS_WY3D_BEG

class WY3D_EXPORT Torus : public wy3d::Primitive
{
    WYDB_DECLARE_MEMBERS(Torus, wy3d::Torus, wy3d::Primitive)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, double majorRadius, double minorRadius, Torus*& pOut);
    double getMajorRadius() const { return _majorRadius; }
    wy::ErrorStatus setMajorRadius(double radius);
    double getMinorRadius() const { return _minorRadius; }
    wy::ErrorStatus setMinorRadius(double radius);

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
    double _majorRadius, _minorRadius;
};

NS_WY3D_END

#endif // WY3D_TORUS_H