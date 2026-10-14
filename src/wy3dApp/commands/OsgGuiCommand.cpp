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

#include <wy3dApp/OsgGuiCommand.h>
#include <cassert>

#include <osg/Camera>
#include <osg/GraphicsContext>
#include <osg/LineSegment>
#include <osgViewer/View>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>

#include <wy3dApp/Application.h>
#include "commands/GuiEventDispatcher.h"
#include "commands/OsgCoordUtil.h"
#include "snap/SketchSnapSystem.h"
#include "select/PointPick.h"
#include "select/SelectHandler.h"
#include "select/SelectFilterFunctors.h"
#include "view/BaseView.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

OsgGuiCommand::OsgGuiCommand() : GuiCommand(), _pOsgView(nullptr)
{
}

bool OsgGuiCommand::handle(
    const osgGA::GUIEventAdapter& ea,
    osgGA::GUIActionAdapter& aa)
{
    _pOsgView = dynamic_cast<osgViewer::View*>(&aa);

    switch (ea.getEventType())
    {
    case osgGA::GUIEventAdapter::EventType::PUSH:
    {
        switch (ea.getButton())
        {
        case osgGA::GUIEventAdapter::MouseButtonMask::LEFT_MOUSE_BUTTON:
        {
            this->onLeftMouseDown({ea.getX(), ea.getY(), ea.getTime()}); return true;
        }
        break;

        case osgGA::GUIEventAdapter::MouseButtonMask::MIDDLE_MOUSE_BUTTON:
        {
            this->onMiddleMouseDown({ea.getX(), ea.getY(), ea.getTime()}); return false;
        }
        break;

        case osgGA::GUIEventAdapter::MouseButtonMask::RIGHT_MOUSE_BUTTON:
        {
            this->onRightMouseDown({ea.getX(), ea.getY(), ea.getTime()}); return false;
        }
        break;
        }
    }
    break;

    case osgGA::GUIEventAdapter::EventType::RELEASE:
    {
        switch (ea.getButton())
        {
        case osgGA::GUIEventAdapter::MouseButtonMask::LEFT_MOUSE_BUTTON:
        {
            this->onLeftMouseUp({ea.getX(), ea.getY(), ea.getTime()}); return true;
        }
        break;

        case osgGA::GUIEventAdapter::MouseButtonMask::MIDDLE_MOUSE_BUTTON:
        {
            this->onMiddleMouseUp({ea.getX(), ea.getY(), ea.getTime()}); return false;
        }
        break;

        case osgGA::GUIEventAdapter::MouseButtonMask::RIGHT_MOUSE_BUTTON:
        {
            this->onRightMouseUp({ea.getX(), ea.getY(), ea.getTime()}); return false;
        }
        break;
        }
    }
    break;

    case osgGA::GUIEventAdapter::EventType::MOVE:
    {
        this->onMouseMove({ea.getX(), ea.getY(), ea.getTime()}); return true;
    }
    break;

    case osgGA::GUIEventAdapter::EventType::DRAG:
    {
    }
    break;

    case osgGA::GUIEventAdapter::EventType::FRAME:
    {
        this->onFrame(ea.getTime());
        return false;
    }
    break;

    case osgGA::GUIEventAdapter::EventType::KEYDOWN:
    {
        this->onKeyDown({ea.getKey(), ea.getModKeyMask(), ea.getTime()}); return true;
    }
    break;

    case osgGA::GUIEventAdapter::EventType::KEYUP:
    {
        this->onKeyUp({ea.getKey(), ea.getModKeyMask(), ea.getTime()}); return true;
    }
    break;

    case osgGA::GUIEventAdapter::EventType::DOUBLECLICK:
    {
        switch (ea.getButton())
        {
        case osgGA::GUIEventAdapter::MouseButtonMask::LEFT_MOUSE_BUTTON:
        {
            this->onLeftMouseDoubleClicked({ea.getX(), ea.getY(), ea.getTime()}); return true;
        }
        break;

        case osgGA::GUIEventAdapter::MouseButtonMask::RIGHT_MOUSE_BUTTON:
        {
            this->onRightMouseDoubleClicked({ea.getX(), ea.getY(), ea.getTime()}); return true;
        }
        break;
        }
    }
    break;
    }

    return false;
}

osgViewer::View* OsgGuiCommand::getOsgView() const
{
    return _pOsgView.get();
}

void OsgGuiCommand::screenToWindow(double screenX, double screenY,
                                   double& windowX, double& windowY) const
{
    osgViewer::View* pView = _pOsgView.get();
    if (!pView)
    {
        windowX = screenX;
        windowY = screenY;
        return;
    }

    osg::Camera* camera = pView->getCamera();
    if (!camera || !camera->getGraphicsContext())
    {
        windowX = screenX;
        windowY = screenY;
        return;
    }

    const osg::GraphicsContext::Traits* traits = camera->getGraphicsContext()->getTraits();
    windowX = screenX - traits->x;
    windowY = traits->y + traits->height - screenY;
}

void OsgGuiCommand::configSelect(SelectOptions options)
{
    BaseView* pView = Application::instance().getActiveView();
    if (!pView) return;
    GuiEventDispatcher* pGuiEventDispatcher = pView->getGuiEventDispatcher();
    if (!pGuiEventDispatcher) return;
    SelectHandler* pSelHandler = pGuiEventDispatcher->getSelectHandler();
    if (!pSelHandler)
    {
        assert(false);
        return;
    }
    pSelHandler->reset();
    pSelHandler->enablePointSelect(options.pointSelect);
    pSelHandler->enableBoxSelect(options.boxSelect);
    pSelHandler->setAllowedElemTypes(options.allowedElemTypes);
    pSelHandler->setSelectionType(options.selectionType);
    pSelHandler->setPreFilterFunctor(options.preFilter);
    pSelHandler->setPostFilterFunctor(options.postFilter);
    pSelHandler->enablePreview(options.preview);
    pSelHandler->setSelectMode(options.selectMode);
}

wyap::Selection OsgGuiCommand::pointPick(
    double x, double y,
    const PointPickOption& option)
{
    static wyap::Selection nullSel = wyap::Selection(wydb::ElementId::kNull);
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return nullSel;
    osgViewer::View* pView = _pOsgView.get();
    if (!pView) return nullSel;
    return PointPick::pick(pDb, pView, x, y, option);
}

// ── 坐标计算（转发到 OsgCoordUtil）────────────────────────

std::pair<wy::Vector3, wyap::SnapResultSPtr> OsgGuiCommand::computePosition3d(
    double x, double y,
    const wy3d::Plane& sketchPlane,
    const std::set<wydb::ElementId>& excludeIds,
    bool snap)
{
    return OsgCoordUtil::computePosition3d(getOsgView(), x, y, sketchPlane, excludeIds, snap);
}

wy::Vector2 OsgGuiCommand::computePosition2d(
    double x, double y,
    const wy3d::Plane& sketchPlane,
    const std::set<wydb::ElementId>& excludeIds,
    std::shared_ptr<SketchSnapContext> pSnapContext,
    SketchSnapSystem* pSketchSnapSys,
    bool snap3d)
{
    return OsgCoordUtil::computePosition2d(getOsgView(), x, y, sketchPlane, excludeIds, pSnapContext, pSketchSnapSys, snap3d);
}

wy::Vector2 OsgGuiCommand::computePosition2dWithoutSnap(
    double x, double y,
    const wy3d::Plane& sketchPlane)
{
    return OsgCoordUtil::computePosition2dWithoutSnap(getOsgView(), x, y, sketchPlane);
}

bool OsgGuiCommand::computeHeight(double x, double y, const wy::Vector3& basePnt, double& height,
    const GuiCmdMakeElement* pMakeElement)
{
    return OsgCoordUtil::computeHeight(getOsgView(), x, y, basePnt, height, pMakeElement);
}

bool OsgGuiCommand::computeHeight(double x, double y, const osg::Vec3d& basePnt, double& height,
    const GuiCmdMakeElement* pMakeElement)
{
    return OsgCoordUtil::computeHeight(getOsgView(), x, y, basePnt, height, pMakeElement);
}

bool OsgGuiCommand::computeHeight2(double x, double y,
    const wy3d::Plane& workPln,
    const wy::Vector2& basePnt,
    const std::set<wydb::ElementId>& excludeIds,
    double& height)
{
    return OsgCoordUtil::computeHeight2(getOsgView(), x, y, workPln, basePnt, excludeIds, height);
}

bool OsgGuiCommand::computeRotationAngle(double x, double y,
    const wy3d::Plane& workPln,
    const wy::Vector2& basis,
    const std::set<wydb::ElementId>& excludeIds,
    double& rotationAngle)
{
    return OsgCoordUtil::computeRotationAngle(getOsgView(), x, y, workPln, basis, excludeIds, rotationAngle);
}

} // namespace wy3dApp
