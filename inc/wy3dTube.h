///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_TUBE_H
#define WY3D_TUBE_H

#include <wy3dDefs.h>
#include <wy3dPrimitive.h>

NS_WY3D_BEG

class WY3D_EXPORT Tube : public wy3d::Primitive
{
    WYDB_DECLARE_MEMBERS(Tube, wy3d::Tube, wy3d::Primitive)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, double outerRadius, double innerRadius, double height, Tube*& pOut);
    double getOuterRadius() const { return _outerRadius; }
    wy::ErrorStatus setOuterRadius(double radius);
    double getInnerRadius() const { return _innerRadius; }
    wy::ErrorStatus setInnerRadius(double radius);
    double getHeight() const { return _height; }
    wy::ErrorStatus setHeight(double height);

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
    double _outerRadius, _innerRadius, _height;
};

NS_WY3D_END

#endif // WY3D_TUBE_H