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

#include <cassert>
#include <osgViewer/View>
#include <osgGA/EventHandler>
#include <wyapView.h>
#include "snap/SnapSystemBase.h"

#include "view/BaseView.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class OsgGuiEventDispatcher;

// 视图
class OsgView : public BaseView
{
public:
    OsgView(wyap::Document* pDoc, osgViewer::View* pOsgView);
    ~OsgView();

    // 渲染视图关联的场景
    virtual void initialize() override;

    // 获取事件分发器
    virtual GuiEventDispatcher* getGuiEventDispatcher() const override;

    // 捕捉元素
    virtual std::list<wydb::ElementId> snapElements(double x, double y) const override;
    // 捕捉对象
    virtual wyap::SnapResultSPtr snapObject(double x, double y, const std::list<wyap::SnapObjectSPtr>& snapObjects) const override;
    virtual wyap::SnapResultSPtr snapObject(double x, double y, const std::set<wyap::SnapObjectSPtr>& snapObjects) const override;

    virtual void lookAt(const osg::Vec3d& lookDir, const osg::Vec3d& up) override;
    virtual void lookAtISO() override;
    virtual void lookAtFront() override;
    virtual void lookAtBack() override;
    virtual void lookAtLeft() override;
    virtual void lookAtRight() override;
    virtual void lookAtTop() override;
    virtual void lookAtBottom() override;

    virtual void ortho() override;
    virtual void perspective() override;

    virtual void viewToWorkingPlane(const wy3d::Plane& workPln) override;

    virtual void viewAll(const osg::BoundingSphere& bdSphere) override;

    // 获取OSG视图
    osgViewer::View* getOsgView() const { return _pOsgView; }

public:
    // 数据库变化响应
    virtual void onDatabaseChanged(
        const wydb::Database* pDb,
        const wydb::Transaction* pTransaction,
        const wydb::DatabaseChangeInfo& changeInfo) override;

private:
    // OSG视图
    osgViewer::View* _pOsgView;
    // GUI事件分发器(缓存)
    GuiEventDispatcher* _pGuiEventDispatcher;
};

} // namespace wy3dApp

#endif // WY3DAPP_VIEW_H
