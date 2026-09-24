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

#include "Sketch3DEdgesTransient.h"

#include <cmath>
#include <vector>

#include <BRepAdaptor_Curve.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Edge.hxx>
#include <gp_Pnt.hxx>
#include <osg/LineWidth>
#include <wyVector3.h>

#include "scene/RenderConst.h"

// 采样段数(圆/弧足够圆滑,直线多点无碍)
static const int kSampleSegments = 64;

Sketch3DEdgesTransient::Sketch3DEdgesTransient(const std::vector<TopoDS_Edge>& edges)
    : GuiCmdTransient()
{
    for (const TopoDS_Edge& edge : edges)
    {
        if (edge.IsNull())
        {
            continue;
        }

        // One unusable edge must not cost the whole preview: skip it and draw the rest.
        try
        {
            BRepAdaptor_Curve curve(edge);
            const double first = curve.FirstParameter();
            const double last = curve.LastParameter();
            if (!std::isfinite(first) || !std::isfinite(last)) continue;
            if (last - first <= 0.0) continue;

            std::vector<wy::Vector3> points;
            points.reserve(kSampleSegments + 1);
            for (int i = 0; i <= kSampleSegments; ++i)
            {
                const gp_Pnt pnt = curve.Value(first + (last - first) * i / kSampleSegments);
                points.emplace_back(pnt.X(), pnt.Y(), pnt.Z());
            }

            // A closed edge ends where it started, so its closing segment comes for free.
            this->addLineGeometry(points, false);
        }
        catch (const Standard_Failure&)
        {
            continue;
        }
    }
}

Sketch3DEdgesTransient::~Sketch3DEdgesTransient()
{
}

void Sketch3DEdgesTransient::addLineGeometry(const std::vector<wy::Vector3>& points, bool closed)
{
    if (points.size() < 2)
    {
        return;
    }

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    geom->setNodeMask(~PICK_MASK); // 不可拾取

    // 顶点数组
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    vertices->reserve(points.size());
    for (const wy::Vector3& pnt : points)
    {
        vertices->push_back(osg::Vec3(pnt.x(), pnt.y(), pnt.z()));
    }
    geom->setVertexArray(vertices);

    // 颜色:橙色(与2D草图参考高亮一致)
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(1.0f, 0.392f, 0.039f, 1.0f));
    geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);

    // 相邻点组成GL_LINES线段,闭合曲线补尾首段
    std::vector<unsigned int> indices;
    indices.reserve(points.size() * 2);
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        indices.push_back(static_cast<unsigned int>(i));
        indices.push_back(static_cast<unsigned int>(i + 1));
    }
    if (closed)
    {
        indices.push_back(static_cast<unsigned int>(points.size() - 1));
        indices.push_back(0);
    }
    geom->addPrimitiveSet(new osg::DrawElementsUInt(GL_LINES, indices.cbegin(), indices.cend()));

    // 加粗
    geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(3.0));

    _root->addChild(geom.get());
}
