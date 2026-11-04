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

#include "SlotGuideTransient.h"

#include <cmath>

#include <osg/Geometry>
#include <osg/LineStipple>
#include <osg/LineWidth>
#include <osg/PrimitiveSet>

#include <wy3dVector3.h>

namespace wy3dApp {

namespace {

constexpr double kMinGuideLength = 1.0e-9;
constexpr int kLineStippleFactor = 2;
constexpr unsigned short kLineStipplePattern = 0xAAAA;

} // namespace

SlotGuideTransient::SlotGuideTransient(const wy3d::Plane& plane)
    : GuiCmdTransient()
    , _plane(plane)
    , _vertices(new osg::Vec3Array())
    , _geometry(new osg::Geometry())
{
    _vertices->resize(2);

    _geometry->setDataVariance(osg::Object::DYNAMIC);
    _geometry->setUseDisplayList(false);
    _geometry->setUseVertexBufferObjects(true);
    _geometry->setVertexArray(_vertices.get());

    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(1.0f, 0.392f, 0.039f, 1.0f));
    _geometry->setColorArray(colors.get(), osg::Array::BIND_OVERALL);
    _geometry->addPrimitiveSet(new osg::DrawArrays(
        osg::PrimitiveSet::LINES, 0, 2));
    _geometry->getOrCreateStateSet()->setAttribute(new osg::LineWidth(1.5f));
    _geometry->getOrCreateStateSet()->setAttributeAndModes(
        new osg::LineStipple(kLineStippleFactor, kLineStipplePattern),
        osg::StateAttribute::ON);

    _root->addChild(_geometry.get());
    this->hideLine();
}

void SlotGuideTransient::update(const wy::Vector2& start, const wy::Vector2& end)
{
    if (std::hypot(end.x() - start.x(), end.y() - start.y()) < kMinGuideLength)
    {
        this->hideLine();
        return;
    }

    const wy::Vector3 startWorld = _plane.value(start);
    const wy::Vector3 endWorld = _plane.value(end);
    (*_vertices)[0].set(
        static_cast<float>(startWorld.x()),
        static_cast<float>(startWorld.y()),
        static_cast<float>(startWorld.z()));
    (*_vertices)[1].set(
        static_cast<float>(endWorld.x()),
        static_cast<float>(endWorld.y()),
        static_cast<float>(endWorld.z()));

    _vertices->dirty();
    _geometry->dirtyBound();
    this->show();
}

void SlotGuideTransient::hideLine()
{
    this->hide();
}

} // namespace wy3dApp
