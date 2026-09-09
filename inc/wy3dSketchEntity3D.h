///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_ENTITY3D_H
#define WY3D_SKETCH_ENTITY3D_H

#include <wydbElement.h>
#include <wy3dDefs.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchEntity3D : public wydb::Element
{
    WYDB_DECLARE_ABSTRACT_MEMBERS(SketchEntity3D, wy3d::SketchEntity3D, wydb::Element)

public:
    virtual wydb::ElementId getParent() const override { return _ownerId; }
    wy::ErrorStatus setOwner(const wydb::ElementId& ownerId);

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies) override;

private:
    wydb::ElementId _ownerId;
};

NS_WY3D_END

#endif // WY3D_SKETCH_ENTITY3D_H
