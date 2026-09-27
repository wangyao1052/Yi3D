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

#include "OsgGuiEventDispatcher.h"
#include <cassert>
#include <osgViewer/View>
#include <osgUtil/LineSegmentIntersector>
#include "select/OsgSelectHandler.h"
#include "application/Application.h"
#include "scene/Scene.h"
#include "OsgSelectUtils.h"
#include <wyapGizmoManager.h>
#include <wyapCmdManager.h>
#include <wyapCommand.h>
#include "commands/OsgGuiCommand.h"
#include "gizmo/BaseGizmo.h"
#include "gizmo/renderer/OsgGizmoRenderer.h"
#include "commands/GuiCommand.h" // MouseEvent

static BaseGizmo* getActiveOsgGizmo()
{
    wyap::GizmoSPtr pActiveGizmo = Application::instance().getGizmoManager()->getActiveGizmo();
    return pActiveGizmo ? dynamic_cast<BaseGizmo*>(pActiveGizmo.get()) : nullptr;
}

OsgGuiEventDispatcher::OsgGuiEventDispatcher()
{
    _pSelectHandler = std::make_shared<OsgSelectHandler>();
}

bool OsgGuiEventDispatcher::handle(
    const osgGA::GUIEventAdapter& ea,
    osgGA::GUIActionAdapter& aa,
    osg::Object* object,
    osg::NodeVisitor* nv)
{
    // The ViewCube is a screen space overlay and comes before gizmo, selection and modal
    // commands. It has to come first: the PUSH branch of handleGizmoEvent unconditionally
    // picks and may activate a gizmo, so a click on a cube corner would be stolen by it.
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        if (ViewCube* pViewCube = pScene->getViewCubeNode())
        {
            if (pViewCube->handleEvent(ea, aa))
            {
                return true;
            }
        }
    }

    // Gizmo
    if (handleGizmoEvent(ea, aa))
    {
        return true;
    }

    // 当前有激活的Gizmo则不走其它事件处理器
    if (Application::instance().getGizmoManager()->getActiveGizmo())
    {
        return true;
    }

    // Select
    if (_pSelectHandler)
    {
        auto* pOsgSelHandler = static_cast<OsgSelectHandler*>(_pSelectHandler.get());
        if (pOsgSelHandler->handle(ea, aa, object, nv))
        {
            return true;
        }
    }

    // ── Modal Command ──
    wyap::CmdManager* pCmdMgr = Application::instance().getCmdManager();
    if (pCmdMgr)
    {
        wyap::CmdExecution* pCmdExec = pCmdMgr->getCurrentModalCmdExecution();
        if (pCmdExec)
        {
            OsgGuiCommand* pGuiCmd = dynamic_cast<OsgGuiCommand*>(pCmdExec);
            if (pGuiCmd)
            {
                return pGuiCmd->handle(ea, aa);
            }
        }
    }

    return false;
}

bool OsgGuiEventDispatcher::handleGizmoEvent(
    const osgGA::GUIEventAdapter& ea,
    osgGA::GUIActionAdapter& aa)
{
    if (!Application::instance().getGizmoManager()->hasGizmos())
    {
        return false;
    }

    switch (ea.getEventType())
    {
    case osgGA::GUIEventAdapter::PUSH:
    {
        if (ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        {
            osgViewer::View* pView = dynamic_cast<osgViewer::View*>(&aa);
            assert(pView);
            osg::Camera* pCamera = pView->getCamera();
            assert(pCamera);
            float xMin = ea.getX() - 2;
            float xMax = ea.getX() + 2;
            float yMin = ea.getY() - 2;
            float yMax = ea.getY() + 2;
            osg::Polytope polytope;
            OsgSelectUtils::initPolytope(pCamera, xMin, yMin, xMax, yMax, polytope);

            wyap::GizmoSPtr pickRet = Application::instance().getActiveScene()->pickGizmo(polytope);
            wy::ErrorStatus gizmoRet = Application::instance().getGizmoManager()->setActiveGizmo(pickRet);
            assert(wy::ErrorStatus::Ok == gizmoRet);

            BaseGizmo* pGizmo = getActiveOsgGizmo();
            if (pGizmo)
            {
                auto* pRenderer = static_cast<OsgGizmoRenderer*>(pGizmo->getRenderer());
                if (pRenderer) pRenderer->setOsgView(pView);
                pGizmo->onLeftMouseDown({ea.getX(), ea.getY(), ea.getTime()});
                return true;
            }
        }
    }
    break;

    case osgGA::GUIEventAdapter::DRAG:
    {
        BaseGizmo* pGizmo = getActiveOsgGizmo();
        if (pGizmo)
        {
            osgViewer::View* pView = dynamic_cast<osgViewer::View*>(&aa);
            auto* pRenderer = static_cast<OsgGizmoRenderer*>(pGizmo->getRenderer());
            if (pRenderer) pRenderer->setOsgView(pView);
            pGizmo->onLeftMouseDrag({ea.getX(), ea.getY(), ea.getTime()});
            return true;
        }
    }
    break;

    case osgGA::GUIEventAdapter::RELEASE:
    {
        if (ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        {
            BaseGizmo* pGizmo = getActiveOsgGizmo();
            if (pGizmo)
            {
                osgViewer::View* pView = dynamic_cast<osgViewer::View*>(&aa);
                auto* pRenderer = static_cast<OsgGizmoRenderer*>(pGizmo->getRenderer());
                if (pRenderer) pRenderer->setOsgView(pView);
                pGizmo->onLeftMouseUp({ea.getX(), ea.getY(), ea.getTime()});

                Application::instance().getGizmoManager()->setActiveGizmo(nullptr);
                Application::instance().getSnapSystem()->clearSnapResult();
                return true;
            }
            assert(Application::instance().getGizmoManager()->getActiveGizmo() == nullptr);
        }
    }
    break;

    case osgGA::GUIEventAdapter::KEYDOWN:
    {
        if (ea.getKey() == osgGA::GUIEventAdapter::KEY_Escape)
        {
            BaseGizmo* pGizmo = getActiveOsgGizmo();
            if (pGizmo)
            {
                osgViewer::View* pView = dynamic_cast<osgViewer::View*>(&aa);
                auto* pRenderer = static_cast<OsgGizmoRenderer*>(pGizmo->getRenderer());
                if (pRenderer) pRenderer->setOsgView(pView);
                pGizmo->onEscapeKey();

                Application::instance().getGizmoManager()->setActiveGizmo(nullptr);
                return true;
            }
        }
        return false;
    }
    }

    return false;
}
