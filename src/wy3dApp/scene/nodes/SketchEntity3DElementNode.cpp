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

#include <osg/LineWidth>
#include <OsgUtils.h>

#include <wy3dSketchEntity3D.h>

#include "scene/SketchEntity3DLinearization.h"
#include "scene/RenderConst.h"
#include "scene/Colors.h"

bool SketchEntity3DElementNode::pickByNormalBoxImpl(osg::Polytope& polytope) const
{
    // 是否包含所有点
    for (const osg::Vec3& vertex : *_vertices)
    {
        if (!polytope.contains(vertex))
        {
            return false;
        }
    }

    return true;
}

void SketchEntity3DElementNode::generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem)
{
    assert(pElem);
    const wy3d::SketchEntity3D* pSketchEntity3D = wy3d::SketchEntity3D::cast(pElem);
    if (!pSketchEntity3D)
    {
        assert(false);
        return;
    }

    assert(_vertices);
    assert(_lineIndices);

    // 生成包围盒
    _boundBox = this->computeBoundingBox(*_vertices);

    // 生成渲染对象
    if (!_lineIndices->empty())
    {
        _curvesGeom = new osg::Geometry();
        _curvesGeom->setNodeMask(static_cast<unsigned int>(this->getNodeType()));
        _curvesGeom->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::SketchElement, "RenderBin");
        {
            _curvesGeom->setUseDisplayList(false);
            _curvesGeom->setUseVertexBufferObjects(true);
            _curvesGeom->setVertexArray(_vertices);
            osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
            colors->push_back(Colors::kSketchEntity);
            _curvesGeom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
            _curvesGeom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, _lineIndices->begin(), _lineIndices->end()));
            _curvesGeom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
            _curvesGeom->setUserValue("ElementId", static_cast<unsigned int>(pElem->getId().value()));
            _curvesGeom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(2.0f));
        }
        _osgNode->addChild(_curvesGeom);
    }
}

ElementNode::GenRenderDataRet SketchEntity3DElementNode::generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement)
{
    _lineIndices = new osg::UIntArray();

    assert(pElement);
    const wy3d::SketchEntity3D* pSketchEntity3D = wy3d::SketchEntity3D::cast(pElement);
    if (!pSketchEntity3D)
    {
        assert(false);
        return GenRenderDataRet::Ok_Empty;
    }

    std::shared_ptr<SketchEntity3DLinearization> pEntityLinear = std::make_shared<SketchEntity3DLinearization>(pSketchEntity3D);
    _vertices->reserve(pEntityLinear->getVertices().size());
    _lineIndices->reserve(pEntityLinear->getIndices().size());
    unsigned int baseIndex = _vertices->size();
    for (const wy::Vector3& pnt : pEntityLinear->getVertices())
    {
        _vertices->push_back(osg::Vec3(pnt.x(), pnt.y(), pnt.z()));
    }
    for (unsigned int index : pEntityLinear->getIndices())
    {
        _lineIndices->push_back(baseIndex + index);
    }

    return GenRenderDataRet::Ok;
}

void SketchEntity3DElementNode::highlightImpl(bool flag)
{
    if (_curvesGeom)
    {
        OsgUtils::setNodeColor(_curvesGeom, flag ? Colors::kSketchEntity_Highlight : Colors::kSketchEntity);
    }
}

void SketchEntity3DElementNode::previewImpl(bool flag)
{
    if (this->isHighlighted())
    {
        assert(false);
        return;
    }
    if (_curvesGeom)
    {
        OsgUtils::setNodeColor(_curvesGeom, flag ? Colors::kSketchEntity_Preview : Colors::kSketchEntity);
    }
}

void SketchEntity3DElementNode::setActiveImpl(bool flag)
{
}
