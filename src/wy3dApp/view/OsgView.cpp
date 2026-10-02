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

#include "OsgView.h"
#include <wyVector3.h>
#include <wyapDocument.h>
#include "scene/Scene.h"
#include "commands/OsgGuiEventDispatcher.h"
#include "OsgSelectUtils.h"
#include "snap/SnapObject.h"
#include "snap/SnapResult.h"
#include "snap/SnapConsts.h"
#include "CameraManipulator3d.h"
#include "scene/RenderConst.h"
#include "application/Application.h"
#include "ViewUtil.h"

#define REAL_SCENE(pScene) dynamic_cast<Scene*>(pScene)

OsgView::OsgView(wyap::Document* pDoc, osgViewer::View* pOsgView) : BaseView(pDoc), _pOsgView(pOsgView), _pGuiEventDispatcher(nullptr)
{
    assert(_pOsgView);

    // 缓存 OsgGuiEventDispatcher 指针，避免每次 get 时遍历
    osgViewer::View::EventHandlers& eventHandlers = _pOsgView->getEventHandlers();
    for (const osg::ref_ptr<osgGA::EventHandler>& pEventHandler : eventHandlers)
    {
        OsgGuiEventDispatcher* pDispatcher = dynamic_cast<OsgGuiEventDispatcher*>(pEventHandler.get());
        if (pDispatcher)
        {
            _pGuiEventDispatcher = pDispatcher;
            break;
        }
    }
    assert(_pGuiEventDispatcher);
}

OsgView::~OsgView()
{
}

void OsgView::initialize()
{
    assert(_pOsgView);
    wyap::Scene* pScene = Application::instance().getSceneManager()->getScene(this->getDocument());
    if (pScene)
    {
        _pOsgView->setSceneData(REAL_SCENE(pScene)->getRoot());
        REAL_SCENE(pScene)->initBoxSelectRect(_pOsgView);
        REAL_SCENE(pScene)->initViewCube(_pOsgView);
        REAL_SCENE(pScene)->initLight(_pOsgView);
    }
}

std::list<wydb::ElementId> OsgView::snapElements(double x, double y) const
{
    std::list<wydb::ElementId> snappedElemsIds;
    if (!_pOsgView)
    {
        assert(false);
        return snappedElemsIds;
    }

    // 构造多面体用于捕捉
    osg::Camera* pCamera = _pOsgView->getCamera();
    assert(pCamera);
    double xMin = x - SnapConsts::PickSize;
    double xMax = x + SnapConsts::PickSize;
    double yMin = y - SnapConsts::PickSize;
    double yMax = y + SnapConsts::PickSize;
    osg::Polytope polytope;
    OsgSelectUtils::initPolytope(pCamera, xMin, yMin, xMax, yMax, polytope);

    // 场景选择
    Scene* pScene = REAL_SCENE(Application::instance().getSceneManager()->getScene(this->getDocument()));
    if (!pScene)
    {
        assert(false);
        return snappedElemsIds;
    }

    // 交叉框选
    snappedElemsIds = pScene->pickByCrossBox(polytope, PICK_MASK);
    return snappedElemsIds;
}

wyap::SnapResultSPtr OsgView::snapObject(double x, double y, const std::list<wyap::SnapObjectSPtr>& snapObjects) const
{
    if (!_pOsgView)
    {
        assert(false);
        return nullptr;
    }

    // 构造多面体用于捕捉
    osg::Camera* pCamera = _pOsgView->getCamera();
    assert(pCamera);
    double xMin = x - SnapConsts::PickSize;
    double xMax = x + SnapConsts::PickSize;
    double yMin = y - SnapConsts::PickSize;
    double yMax = y + SnapConsts::PickSize;
    osg::Polytope polytope;
    OsgSelectUtils::initPolytope(pCamera, xMin, yMin, xMax, yMax, polytope);

    // 捕捉
    wy::Vector3 outPos;
    for (const wyap::SnapObjectSPtr& pSnapObject : snapObjects)
    {
        if (!pSnapObject) continue;
        SnapObject* pRealSnapObj = dynamic_cast<SnapObject*>(pSnapObject.get());
        if (!pRealSnapObj) continue;
        if (pRealSnapObj->snap(polytope, outPos))
        {
            return std::make_shared<SnapResultPoint>(outPos, pSnapObject);
        }
    }

    return nullptr;
}

wyap::SnapResultSPtr OsgView::snapObject(double x, double y, const std::set<wyap::SnapObjectSPtr>& snapObjects) const
{
    if (!_pOsgView)
    {
        assert(false);
        return nullptr;
    }

    // 构造多面体用于捕捉
    osg::Camera* pCamera = _pOsgView->getCamera();
    assert(pCamera);
    double xMin = x - SnapConsts::PickSize;
    double xMax = x + SnapConsts::PickSize;
    double yMin = y - SnapConsts::PickSize;
    double yMax = y + SnapConsts::PickSize;
    osg::Polytope polytope;
    OsgSelectUtils::initPolytope(pCamera, xMin, yMin, xMax, yMax, polytope);

    // 捕捉
    wy::Vector3 outPos;
    for (const wyap::SnapObjectSPtr& pSnapObject : snapObjects)
    {
        if (!pSnapObject) continue;
        SnapObject* pRealSnapObj = dynamic_cast<SnapObject*>(pSnapObject.get());
        if (!pRealSnapObj) continue;
        if (pRealSnapObj->snap(polytope, outPos))
        {
            return std::make_shared<SnapResultPoint>(outPos, pSnapObject);
        }
    }

    return nullptr;
}

void OsgView::lookAt(const osg::Vec3d& lookDir, const osg::Vec3d& up)
{
    ViewUtil::viewTo(_pOsgView, lookDir, up);
}

void OsgView::lookAtISO()
{
    ViewUtil::viewToISO(_pOsgView);
}

void OsgView::lookAtFront()
{
    ViewUtil::viewToFront(_pOsgView);
}

void OsgView::lookAtBack()
{
    ViewUtil::viewToBack(_pOsgView);
}

void OsgView::lookAtLeft()
{
    ViewUtil::viewToLeft(_pOsgView);
}

void OsgView::lookAtRight()
{
    ViewUtil::viewToRight(_pOsgView);
}

void OsgView::lookAtTop()
{
    ViewUtil::viewToTop(_pOsgView);
}

void OsgView::lookAtBottom()
{
    ViewUtil::viewToBottom(_pOsgView);
}

void OsgView::ortho()
{
    ViewUtil::ortho(_pOsgView);
}

void OsgView::perspective()
{
    ViewUtil::perspective(_pOsgView);
}

void OsgView::viewToWorkingPlane(const wy3d::SketchPlane& workPln)
{
    ViewUtil::viewToWorkingPlane(_pOsgView, workPln);
}

void OsgView::viewAll(const osg::BoundingSphere& bdSphere)
{
    ViewUtil::viewAll(_pOsgView, bdSphere);
}

GuiEventDispatcher* OsgView::getGuiEventDispatcher() const
{
    return _pGuiEventDispatcher;
}

void OsgView::onDatabaseChanged(
    const wydb::Database* pDb,
    const wydb::Transaction* pTransaction,
    const wydb::DatabaseChangeInfo& changeInfo)
{
    if (!_pOsgView || !this->getDocument())
    {
        assert(false);
        return;
    }

    wyap::Scene* pScene = Application::instance().getSceneManager()->getScene(this->getDocument());

    // 动态设置ModelSize
    CameraManipulator3d* pCamMan3d = dynamic_cast<CameraManipulator3d*>(_pOsgView->getCameraManipulator());
    if (!pCamMan3d) return;
    osg::BoundingSphere boundSphere = REAL_SCENE(pScene)->getElementsBoundingSphere();
    if (boundSphere.radius() > 0)
    {
        pCamMan3d->setModelSize(boundSphere.radius());
    }
    else
    {
        pCamMan3d->setModelSize(100.0);
    }
}