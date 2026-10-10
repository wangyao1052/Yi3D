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
#include "OccUtil.h"
#include <gp_Quaternion.hxx>

NS_WY3D_BEG

gp_Ax2 OccUtil::toAx2(const wy3d::Plane& plane)
{
    return gp_Ax2(toPnt(plane.getOrigin()), toDir(plane.getNormal()), toDir(plane.getXDir()));
}

TopoDS_Shape OccUtil::transformShape(
    const TopoDS_Shape& shape,
    const wy::Vector3& position,
    const wy::Vector3& rotation)
{
    if (position.x() == 0.0 && position.y() == 0.0 && position.z() == 0.0 &&
        rotation.x() == 0.0 && rotation.y() == 0.0 && rotation.z() == 0.0)
    {
        return shape;
    }

    gp_Trsf transform;
    // position
    gp_Vec translation(position.x(), position.y(), position.z());
    transform.SetTranslationPart(translation);
    // rotation
    // euler angles: z-->x-->y
    gp_Quaternion quaternion;
    if (1)
    {
        gp_Quaternion rotZ;
        rotZ.SetVectorAndAngle(gp_Vec(0.0, 0.0, 1.0), rotation.z());
        gp_Quaternion rotX;
        rotX.SetVectorAndAngle(gp_Vec(1.0, 0.0, 0.0), rotation.x());
        gp_Quaternion rotY;
        rotY.SetVectorAndAngle(gp_Vec(0.0, 1.0, 0.0), rotation.y());
        quaternion = rotY * rotX * rotZ;
    }
    else
    {
        // TODO:Ч������,��Ҫ��ʱ����
        quaternion.SetEulerAngles(gp_EulerSequence::gp_Intrinsic_ZXY, rotation.x(), rotation.y(), rotation.z());
    }
    transform.SetRotationPart(quaternion);

    return shape.Located(TopLoc_Location(transform));
}

NS_WY3D_END