///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_DATUM_PLANE_H
#define WY3D_DATUM_PLANE_H

#include <string>
#include <wy3dDefs.h>
#include <wy3dDatum.h>
#include <wy3dSketchPlane.h>

NS_WY3D_BEG

class WY3D_EXPORT DatumPlane : public wy3d::Datum
{
    WYDB_DECLARE_MEMBERS(DatumPlane, wy3d::DatumPlane, wy3d::Datum)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, const wy3d::SketchPlane& plane, DatumPlane*& pOut);

    const wy3d::SketchPlane& getPlane() const { return _plane; }
    wy::ErrorStatus setPlane(const wy3d::SketchPlane& plane);

    const std::string& getName() const { return _name; }
    wy::ErrorStatus setName(const std::string& name);

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;

private:
    wy3d::SketchPlane _plane;
    std::string _name;
};

NS_WY3D_END

#endif // WY3D_DATUM_PLANE_H