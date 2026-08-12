///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_POINT_H
#define WY3D_SKETCH_POINT_H

#include <wyVector2.h>
#include <wy3dDefs.h>
#include <wy3dSketchEntity.h>
#include <wy3dImpl.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchPoint : public wy3d::SketchEntity
{
    WYDB_DECLARE_MEMBERS(SketchPoint, wy3d::SketchPoint, wy3d::SketchEntity)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, const wy::Vector2& position, SketchPoint*& pOut);

    wy::Vector2 getPosition() const { return _position; }
    wy::ErrorStatus setPosition(const wy::Vector2& position);

    virtual wy::ErrorStatus translate(const wy::Vector2& vector) override { return setPosition(_position+vector); }
    virtual wy::ErrorStatus rotateAround(const wy::Vector2& center, double angle) override;
    virtual wy::ErrorStatus transform(const wy3d::Matrix3& matrix) override { return setPosition(_position*matrix); }
    virtual wy3d::BoundingBox2 getBoundingBox() const override { return wy3d::BoundingBox2(wy::Vector2(_position.x()-wy3d::TOL,_position.y()-wy3d::TOL),wy::Vector2(_position.x()+wy3d::TOL,_position.y()+wy3d::TOL)); }

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;

private:
    wy::Vector2 _position;
};

NS_WY3D_END

#endif // WY3D_SKETCH_POINT_H