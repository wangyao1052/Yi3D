///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_H
#define WY3D_SKETCH_H

#include <memory>
#include <vector>
#include <wyIterator.h>
#include <wy3dSketchPlane.h>
#include <wy3dDefs.h>
#include <wy3dFeature.h>

NS_WY3D_BEG

class SketchEntity;
class SketchElementIterator;

class WY3D_EXPORT Sketch : public wy3d::Feature
{
    WYDB_DECLARE_MEMBERS(Sketch, wy3d::Sketch, wy3d::Feature)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, const wy3d::SketchPlane& plane, Sketch*& pOutSketch);

    virtual wydb::ElementId getParent() const override { return _ownerId; }
    wy::ErrorStatus setOwner(const wydb::ElementId& owner);
    virtual std::vector<wydb::ElementId> getChildren() const { return _entities; }

    const wy3d::SketchPlane& getPlane() const { return _plane; }
    wy::ErrorStatus setPlane(const wy3d::SketchPlane& plane);

    wy::Iterator<wydb::ElementId> createIterator() const;

    wy::ErrorStatus addEntity(wy3d::SketchEntity* pEntity);

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies) override;

private:
    wy::ErrorStatus _setEntities(std::vector<wydb::ElementId>&& entities);

private:
    wydb::ElementId _ownerId;
    wy3d::SketchPlane _plane;
    std::vector<wydb::ElementId> _entities;
    friend class SketchElementIterator;
};

class SketchElementIterator : public wy::IteratorImpl<wydb::ElementId>
{
public:
    SketchElementIterator(const Sketch* pSketch);
    virtual bool isDone() const override;
    virtual void moveNext() override;
    virtual wydb::ElementId current() const override;
private:
    const Sketch* _pSketch;
    std::vector<wydb::ElementId>::const_iterator _iter;
};

NS_WY3D_END

#endif // WY3D_SKETCH_H