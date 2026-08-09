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

#include "GizmoFactory.h"

#include <wy3dSolid.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dCone.h>
#include <wy3dTorus.h>
#include <wy3dTube.h>
#include <wy3dUnion.h>
#include <wy3dDifference.h>
#include <wy3dIntersection.h>
#include <wy3dExtrusion.h>
#include <wy3dRevolution.h>
#include <wy3dSweep.h>
#include <wy3dLoft.h>
#include <wy3dImportedSolid.h>
#include <wy3dSheet.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dRevolvedSheet.h>
#include <wy3dImportedSheet.h>

#include <wy3dSketchPoint.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>

#include "creator/PrimitiveGizmoCreator.h"
#include "creator/SketchPointGizmoCreator.h"
#include "creator/SketchLineGizmoCreator.h"
#include "creator/SketchCenterLineGizmoCreator.h"
#include "creator/SketchCircleGizmoCreator.h"
#include "creator/SketchArcGizmoCreator.h"
#include "creator/SketchEllipseGizmoCreator.h"
#include "creator/SketchEllipseArcGizmoCreator.h"
#include "creator/SketchSplineGizmoCreator.h"

#define REGISTER_CREATOR(CLASS, GIZMO_CREATOR) \
    { \
        static_assert(std::is_base_of_v<ElemGizmoCreator, GIZMO_CREATOR>, \
                 "GIZMO_CREATOR must inherit from ElemGizmoCreator"); \
        assert(!_name2Creator.count(CLASS::className())); \
        _name2Creator[CLASS::className()] = std::make_unique<GIZMO_CREATOR>(); \
    }

GizmoFactory::GizmoFactory()
{
    REGISTER_CREATOR(wy3d::Solid, NullGizmoCreator);
    REGISTER_CREATOR(wy3d::Box, PrimitiveGizmoCreator);
    REGISTER_CREATOR(wy3d::Cylinder, PrimitiveGizmoCreator);
    REGISTER_CREATOR(wy3d::Sphere, PrimitiveGizmoCreator);
    REGISTER_CREATOR(wy3d::Cone, PrimitiveGizmoCreator);
    REGISTER_CREATOR(wy3d::Torus, PrimitiveGizmoCreator);
    REGISTER_CREATOR(wy3d::Tube, PrimitiveGizmoCreator);

    // modified by wangyao 2025.06.03 {
    // 布尔&拉伸&旋转&扫掠&放样不支持Gizmo
    REGISTER_CREATOR(wy3d::Boolean, NullGizmoCreator);
    REGISTER_CREATOR(wy3d::Union, NullGizmoCreator);
    REGISTER_CREATOR(wy3d::Difference, NullGizmoCreator);
    REGISTER_CREATOR(wy3d::Intersection, NullGizmoCreator);
    REGISTER_CREATOR(wy3d::Extrusion, NullGizmoCreator);
    REGISTER_CREATOR(wy3d::Revolution, NullGizmoCreator);
    REGISTER_CREATOR(wy3d::Sweep, NullGizmoCreator);
    REGISTER_CREATOR(wy3d::Loft, NullGizmoCreator);
    // }

    // 导入实体不支持Gizmo
    REGISTER_CREATOR(wy3d::ImportedSolid, NullGizmoCreator);
    REGISTER_CREATOR(wy3d::ImportedSheet, NullGizmoCreator);

    REGISTER_CREATOR(wy3d::SketchPoint, SketchPointGizmoCreator);
    REGISTER_CREATOR(wy3d::SketchLine, SketchLineGizmoCreator);
    REGISTER_CREATOR(wy3d::SketchCenterLine, SketchCenterLineGizmoCreator);
    REGISTER_CREATOR(wy3d::SketchCircle, SketchCircleGizmoCreator);
    REGISTER_CREATOR(wy3d::SketchArc, SketchArcGizmoCreator);
    REGISTER_CREATOR(wy3d::SketchEllipse, SketchEllipseGizmoCreator);
    REGISTER_CREATOR(wy3d::SketchEllipseArc, SketchEllipseArcGizmoCreator);
    REGISTER_CREATOR(wy3d::SketchSpline, SketchSplineGizmoCreator);

    // 曲面 (no gizmo)
    REGISTER_CREATOR(wy3d::Sheet, NullGizmoCreator);
    REGISTER_CREATOR(wy3d::ExtrudedSheet, NullGizmoCreator);
    REGISTER_CREATOR(wy3d::RevolvedSheet, NullGizmoCreator);
}

std::list<wyap::GizmoSPtr> GizmoFactory::createGizmos(const wydb::Element* pElem) const
{
    if (!pElem)
    {
        assert(false);
        return std::list<wyap::GizmoSPtr>();
    }

    auto iter = _name2Creator.find(pElem->getClassInfo()->className());
    if (iter == _name2Creator.cend())
    {
        assert(false);
        return std::list<wyap::GizmoSPtr>();
    }

    return iter->second->createGizmos(pElem);
}

