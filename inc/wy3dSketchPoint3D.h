///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_POINT3D_H
#define WY3D_SKETCH_POINT3D_H

#include <wyVector3.h>
#include <wy3dDefs.h>
#include <wy3dSketchEntity3D.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchPoint3D : public wy3d::SketchEntity3D
{
    WYDB_DECLARE_MEMBERS(SketchPoint3D, wy3d::SketchPoint3D, wy3d::SketchEntity3D)

public:
    // Parameter names
    static constexpr const char* PARAM_ID         = "ID";
    static constexpr const char* PARAM_POSITION_X = "Position X";
    static constexpr const char* PARAM_POSITION_Y = "Position Y";
    static constexpr const char* PARAM_POSITION_Z = "Position Z";

    static wy::ErrorStatus create(wydb::Transaction* pTrans, const wy::Vector3& position, SketchPoint3D*& pOut);

    wy::Vector3 getPosition() const { return _position; }
    wy::ErrorStatus setPosition(const wy::Vector3& position);

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;

private:
    wy::Vector3 _position;
};

NS_WY3D_END

#endif // WY3D_SKETCH_POINT3D_H
