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

#include <wyVector3.h>
#include "SnapObject.h"
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Point>
#include "SnapContext.h"
#include "SnapConsts.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

bool SnapPoint::snap(osg::Polytope& polytope, wy::Vector3& position) const
{
    if (polytope.contains(osg::Vec3(_position.x(), _position.y(), _position.z())))
    {
        position = _position;
        return true;
    }
    else
    {
        return false;
    }
}

osg::ref_ptr<osg::Node> SnapPoint::generateOsgNode() const
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    {
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        vertices->push_back(osg::Vec3(_position.x(), _position.y(), _position.z()));
        geom->setVertexArray(vertices);
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        colors->push_back(SnapConsts::PointColor);
        geom->setColorArray(colors);
        geom->setColorBinding(osg::Geometry::BIND_OVERALL);
    }
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, 1));
    geom->getOrCreateStateSet()->setAttribute(new osg::Point(SnapConsts::PointSize));
    geom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geom);
    return geode;
}

} // namespace wy3dApp
