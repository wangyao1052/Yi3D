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

#ifndef WY3D_OCC_UTIL_H
#define WY3D_OCC_UTIL_H

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pln.hxx>
#include <TopoDS_Shape.hxx>

#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dDefs.h>
#include <wy3dPlane.h>

NS_WY3D_BEG

class OccUtil
{
public:
    static inline gp_Pnt toPnt(const wy::Vector3& pnt)
    {
        return gp_Pnt(pnt.x(), pnt.y(), pnt.z());
    }

    static inline gp_Vec toVec(const wy::Vector3& vec)
    {
        return gp_Vec(vec.x(), vec.y(), vec.z());
    }

    static inline gp_Dir toDir(const wy::Vector3& dir)
    {
        return gp_Dir(dir.x(), dir.y(), dir.z());
    }

    static gp_Ax2 toAx2(const wy3d::Plane& plane);

    static TopoDS_Shape transformShape(
        const TopoDS_Shape& shape,
        const wy::Vector3& position,
        const wy::Vector3& rotation);
};

NS_WY3D_END

#endif // WY3D_OCC_UTIL_H