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

#ifndef WY3DAPP_ELEM_SNAP_OBJECT_CREATOR_H
#define WY3DAPP_ELEM_SNAP_OBJECT_CREATOR_H

#include <list>
#include <osg/Vec3d>
#include <osg/Matrix>
#include <TopoDS_Shape.hxx>
#include <wyVector3.h>
#include "../SnapSystemBase.h"
#include <wy3dSketchEntity.h>
#include <wy3dPlane.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 元素捕捉对象工厂
class ElemSnapObjectCreator
{
public:
    virtual ~ElemSnapObjectCreator() {}

    // 创建捕捉对象
    virtual std::list<wyap::SnapObjectSPtr> createSnapObjects(const wydb::Element* pElem) = 0;

protected:
    std::list<wyap::SnapObjectSPtr> newSnapObjects(const wydb::ElementId& id, const TopoDS_Shape& shape) const;

    bool getSketchPlane(const wy3d::SketchEntity* pSketchEntity, wy3d::Plane& outSketchPlane) const;

protected:
    template<typename T>
    wyap::SnapObjectSPtr newSnapPoint(const wydb::ElementId& id, double x, double y, double z) const
    {
        return std::make_shared<T>(id, wy::Vector3(x, y, z));
    }

    template<typename T>
    wyap::SnapObjectSPtr newSnapPoint(const wydb::ElementId& id, const wy::Vector3& pnt) const
    {
        return std::make_shared<T>(id, pnt);
    }

    template<typename T>
    wyap::SnapObjectSPtr newSnapPoint(const wydb::ElementId& id, double x, double y, double z, const osg::Matrix& matrix) const
    {
        osg::Vec3d pnt(x, y, z);
        pnt = pnt * matrix;
        return std::make_shared<T>(id, wy::Vector3(pnt.x(), pnt.y(), pnt.z()));
    }
};

class EmptySnapObjectCreator : public ElemSnapObjectCreator
{
public:
    // 创建捕捉对象
    virtual std::list<wyap::SnapObjectSPtr> createSnapObjects(const wydb::Element* pElem) override
    {
        return std::list<wyap::SnapObjectSPtr>();
    }
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEM_SNAP_OBJECT_CREATOR_H
