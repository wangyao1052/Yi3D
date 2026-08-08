///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_BOX_H
#define WY3D_BOX_H

#include <wy3dDefs.h>
#include <wy3dPrimitive.h>

NS_WY3D_BEG

class WY3D_EXPORT Box : public wy3d::Primitive
{
    WYDB_DECLARE_MEMBERS(Box, wy3d::Box, wy3d::Primitive)
public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, double length, double width, double height, Box*& pOut);

    double getLength() const { return _length; }
    wy::ErrorStatus setLength(double length);
    double getWidth() const { return _width; }
    wy::ErrorStatus setWidth(double width);
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
    double _length, _width, _height;
};

NS_WY3D_END

#endif // WY3D_BOX_H