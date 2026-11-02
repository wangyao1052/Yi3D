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

#ifndef WY3DAPP_ELEMENT_NODE_H
#define WY3DAPP_ELEMENT_NODE_H

#include <cassert>
#include <memory>

#include <osg/BoundingBox>
#include <osg/Group>

#include <wydbElementId.h>
#include <wy3dSelectionType.h>
#include <wyapSelection.h>
#include <wy3dApp/ElementNodeType.h>
#include <wy3dApp/ElementRenderData.h>
#include <wy3dApp/ElementRenderObject.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace osg
{
    class Drawable;
    class Polytope;
}

namespace wydb
{
    class Database;
    class Element;
}

namespace wy3dApp {

class ElementNodeImpl;
class Scene;
class SceneImpl;

class WY3DAPP_EXPORT ElementNode
{
protected:
    explicit ElementNode(const wydb::ElementId& id);
    virtual ~ElementNode();

public:
    virtual ElementNodeType getNodeType() const = 0;

    const wydb::ElementId& getElementId() const { return _id; }

    const osg::BoundingBox& getBoundingBox() const { return _boundBox; }

    // point select
    virtual wyap::Selection createSelectionFromPick(
        osg::Drawable* pDrawable,
        unsigned int primitiveIndex,
        wy3d::SelectionType selType) const;

    // box select
    virtual bool selectByWindow(osg::Polytope& polytope) const;
    virtual bool selectByCrossing(osg::Polytope& polytope) const;

public:
    bool isHighlighted() const;
    bool isHidden() const;
    bool isActive() const;
    bool isPreviewed() const;

    void highlight(bool flag);
    void preview(bool flag);
    void activate(bool flag);
    void hide(bool flag);

    // 获取Osg节点
    osg::Group* getOsgNode() const { return _osgNode.get(); }

protected:
    bool regenerate(Scene* pScene, wydb::Database* pDb);

    virtual std::unique_ptr<ElementRenderData> generateRenderData(
        Scene* pScene,
        const wydb::Element* pElement) = 0;

    virtual osg::BoundingBox computeBoundingBox(
        const ElementRenderData* pRenderData,
        const ElementRenderObject* pRenderObject) const;

    virtual std::unique_ptr<ElementRenderObject> generateRenderObject(
        Scene* pScene,
        const wydb::Element* pElem) = 0;

    bool updateAppearance(wydb::Database* pDb);

    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const { return true; }

    template <typename T>
    T* getRenderDataAs() const
    {
        assert(!_renderData || dynamic_cast<T*>(_renderData.get()));
        return static_cast<T*>(_renderData.get());
    }

    template <typename T>
    T* getRenderObjectAs() const
    {
        assert(!_renderObject || dynamic_cast<T*>(_renderObject.get()));
        return static_cast<T*>(_renderObject.get());
    }

protected:
    wydb::ElementId _id;
    std::unique_ptr<ElementNodeImpl> _pImpl;

    std::unique_ptr<ElementRenderData> _renderData;
    std::unique_ptr<ElementRenderObject> _renderObject;
    osg::BoundingBox _boundBox;
    osg::ref_ptr<osg::Group> _osgNode;

    friend class SceneImpl;
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_NODE_H
