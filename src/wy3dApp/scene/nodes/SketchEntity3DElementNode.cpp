///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "SketchEntity3DElementNode.h"

#include <cassert>

#include <Standard_Failure.hxx>
#include <osg/LineWidth>
#include <osg/StateSet>
#include <OsgUtils.h>

#include <wy3dSketchEntity3D.h>

#include "scene/SketchEntity3DLinearization.h"
#include "scene/RenderConst.h"
#include <wy3dApp/Colors.h>
#include "scene/nodes/LineRenderObject.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchEntity3DElementNode::SketchEntity3DElementNode(const wydb::ElementId& id) : ElementNode(id)
{
}

std::unique_ptr<ElementRenderObject> SketchEntity3DElementNode::generateRenderObject(
    Scene* pScene, const wydb::Element* pElem)
{
    (void)pScene;
    assert(pElem);
    const wy3d::SketchEntity3D* pSketchEntity3D = wy3d::SketchEntity3D::cast(pElem);
    if (!pSketchEntity3D)
    {
        assert(false);
        return nullptr;
    }

    LineRenderData* data = this->renderData();
    if (!data || !data->vertices || !data->lineIndices)
    {
        assert(false);
        return nullptr;
    }

    LineRenderStyle style;
    style.nodeType = this->getNodeType();
    style.color = Colors::sketchEntity.normal;
    style.highlightColor = Colors::sketchEntity.highlight;
    style.previewColor = Colors::sketchEntity.preview;
    style.lineWidth = 2.0f;
    style.highlightLineWidth = 2.0f;
    style.changeLineWidthOnHighlight = false;
    style.depthTest = false;
    style.highlightDepthTest = false;
    style.changeDepthTestOnHighlight = false;
    style.lighting = false;
    style.applyStateToRoot = true;
    style.renderBin = RenderBinNumers::SketchEntity3D;
    // 点图元没有线索引,按 GL_POINTS 绘制
    style.pointsWhenNoLines = true;
    style.pointSize = 8.0f;
    style.highlightPointSize = 8.0f;
    style.changePointSizeOnHighlight = false;

    auto renderObject = std::make_unique<LineRenderObject>(
        pElem->getId(), this->getNodeType(), *data, *data->lineIndices, style);
    renderObject->build();
    return renderObject;
}

LineRenderObject* SketchEntity3DElementNode::renderObject() const
{
    return dynamic_cast<LineRenderObject*>(_renderObject.get());
}

std::unique_ptr<ElementRenderData> SketchEntity3DElementNode::generateRenderData(
    Scene* pScene, const wydb::Element* pElement) noexcept
{
    (void)pScene;

    try
    {
        auto data = std::make_unique<LineRenderData>();

        assert(pElement);
        const wy3d::SketchEntity3D* pSketchEntity3D = wy3d::SketchEntity3D::cast(pElement);
        if (!pSketchEntity3D)
        {
            assert(false);
            return nullptr;
        }

        std::shared_ptr<SketchEntity3DLinearization> pEntityLinear = std::make_shared<SketchEntity3DLinearization>(pSketchEntity3D);
        data->vertices->reserve(pEntityLinear->getVertices().size());
        data->lineIndices->reserve(pEntityLinear->getIndices().size());
        unsigned int baseIndex = data->vertices->size();
        for (const wy::Vector3& pnt : pEntityLinear->getVertices())
        {
            data->vertices->push_back(osg::Vec3(pnt.x(), pnt.y(), pnt.z()));
        }
        for (unsigned int index : pEntityLinear->getIndices())
        {
            data->lineIndices->push_back(baseIndex + index);
        }

        return data;
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return nullptr;
    }
    catch (...)
    {
        assert(false);
        return nullptr;
    }
}

} // namespace wy3dApp
