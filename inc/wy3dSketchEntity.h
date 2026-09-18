///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_ENTITY_H
#define WY3D_SKETCH_ENTITY_H

#include <wyVector2.h>
#include <wy3dVector2.h>
#include <geom/wy3dBoundingBox2.h>
#include <geom/wy3dMatrix3.h>
#include <wydbElement.h>
#include <wy3dDefs.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchEntity : public wydb::Element
{
    WYDB_DECLARE_ABSTRACT_MEMBERS(SketchEntity, wy3d::SketchEntity, wydb::Element)

public:
    virtual wydb::ElementId getParent() const override { return _ownerId; }
    wy::ErrorStatus setOwner(const wydb::ElementId& ownerId);

    virtual wy::ErrorStatus translate(const wy::Vector2& vector) { return wy::ErrorStatus::NotImplementedYet; }
    virtual wy3d::geom::BoundingBox2 getBoundingBox() const { return wy3d::geom::BoundingBox2(); }

    virtual wy::ErrorStatus rotateAround(const wy::Vector2& center, double angle) { return wy::ErrorStatus::NotImplementedYet; }
    virtual wy::ErrorStatus transform(const wy3d::geom::Matrix3& matrix) { return wy::ErrorStatus::NotImplementedYet; }

protected:

    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer);
    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies);

    static inline wy::Vector2 rotateAround(const wy::Vector2& pnt, const wy::Vector2& center, double cosTheta, double sinTheta);

private:
    wydb::ElementId _ownerId;
};

inline wy::Vector2 SketchEntity::rotateAround(const wy::Vector2& pnt, const wy::Vector2& center, double cosTheta, double sinTheta)
{
    double dx=pnt.x()-center.x(); double dy=pnt.y()-center.y();
    return wy::Vector2(cosTheta*dx-sinTheta*dy+center.x(), sinTheta*dx+cosTheta*dy+center.y());
}

NS_WY3D_END

#endif // WY3D_SKETCH_ENTITY_H