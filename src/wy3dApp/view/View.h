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

#ifndef WY3DAPP_VIEW_H
#define WY3DAPP_VIEW_H

#include <wyapView.h>
#include <wy3dPlane.h>
#include <osg/Vec3d>
#include <osg/BoundingSphere>
#include "snap/SnapSystemBase.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class GuiEventDispatcher;

class View : public wyap::View
{
public:
    explicit View(wyap::Document* pDoc) : wyap::View(pDoc) {}
    virtual ~View() = default;

    // 获取事件分发器
    virtual GuiEventDispatcher* getGuiEventDispatcher() const = 0;

    virtual void lookAt(const osg::Vec3d& lookDir, const osg::Vec3d& up) = 0;
    virtual void lookAtISO() = 0;
    virtual void lookAtFront() = 0;
    virtual void lookAtBack() = 0;
    virtual void lookAtLeft() = 0;
    virtual void lookAtRight() = 0;
    virtual void lookAtTop() = 0;
    virtual void lookAtBottom() = 0;

    virtual void ortho() = 0;
    virtual void perspective() = 0;

    virtual void viewToWorkingPlane(const wy3d::Plane& workPln) = 0;

    virtual void viewAll(const osg::BoundingSphere& bdSphere) = 0;

    // 捕捉元素（从 wyap::View 迁移至此）
    virtual std::list<wydb::ElementId> snapElements(double x, double y) const = 0;
    // 捕捉对象（从 wyap::View 迁移至此）
    virtual wyap::SnapResultSPtr snapObject(double x, double y, const std::list<wyap::SnapObjectSPtr>& snapObjects) const = 0;
    virtual wyap::SnapResultSPtr snapObject(double x, double y, const std::set<wyap::SnapObjectSPtr>& snapObjects) const = 0;
};

} // namespace wy3dApp

#endif // WY3DAPP_VIEW_H
