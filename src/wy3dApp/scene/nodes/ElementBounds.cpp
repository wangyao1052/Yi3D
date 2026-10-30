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

#include "scene/nodes/ElementBounds.h"

namespace wy3dApp {

osg::BoundingBox ElementBounds::compute(const osg::Vec3Array& vertices)
{
    osg::BoundingBox bbox;
    for (const osg::Vec3& vertex : vertices)
    {
        bbox.expandBy(vertex);
    }
    return bbox;
}

void ElementBounds::transform(osg::BoundingBox& bbox, const osg::Matrix& matrix)
{
    if (!bbox.valid()) return;
    if (matrix.isIdentity()) return;

    const osg::Vec3 corners[8] = {
        osg::Vec3(bbox.xMin(), bbox.yMin(), bbox.zMin()),
        osg::Vec3(bbox.xMin(), bbox.yMin(), bbox.zMax()),
        osg::Vec3(bbox.xMin(), bbox.yMax(), bbox.zMin()),
        osg::Vec3(bbox.xMin(), bbox.yMax(), bbox.zMax()),
        osg::Vec3(bbox.xMax(), bbox.yMin(), bbox.zMin()),
        osg::Vec3(bbox.xMax(), bbox.yMin(), bbox.zMax()),
        osg::Vec3(bbox.xMax(), bbox.yMax(), bbox.zMin()),
        osg::Vec3(bbox.xMax(), bbox.yMax(), bbox.zMax()),
    };

    bbox.init();
    for (const osg::Vec3& corner : corners)
    {
        bbox.expandBy(corner * matrix);
    }
}

} // namespace wy3dApp
