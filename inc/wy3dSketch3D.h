///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH3D_H
#define WY3D_SKETCH3D_H

#include <memory>
#include <vector>

#include <wyIterator.h>
#include <wy3dDefs.h>
#include <wy3dFeature.h>

NS_WY3D_BEG

class SketchEntity3D;
class Sketch3DElementIterator;

class WY3D_EXPORT Sketch3D : public wy3d::Feature
{
    WYDB_DECLARE_MEMBERS(Sketch3D, wy3d::Sketch3D, wy3d::Feature)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, Sketch3D*& pOutSketch3D);

    virtual wydb::ElementId getParent() const override { return _parentId; }
    wy::ErrorStatus setParent(const wydb::ElementId& parent);

    virtual std::vector<wydb::ElementId> getChildren() const override { return _entities; }

    wy::Iterator<wydb::ElementId> createIterator() const;

    wy::ErrorStatus addEntity(wy3d::SketchEntity3D* pEntity);

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies) override;

private:
    wy::ErrorStatus setEntitiesImpl(std::vector<wydb::ElementId>&& entities);

private:
    wydb::ElementId _parentId;
    std::vector<wydb::ElementId> _entities;

    friend class Sketch3DElementIterator;
};

class Sketch3DElementIterator : public wy::IteratorImpl<wydb::ElementId>
{
public:
    Sketch3DElementIterator(const Sketch3D* pSketch3D);
    virtual bool isDone() const override;
    virtual void moveNext() override;
    virtual wydb::ElementId current() const override;
private:
    const Sketch3D* _pSketch3D;
    std::vector<wydb::ElementId>::const_iterator _iter;
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_H
