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

#include "OsgSelectHandler.h"
#include <cassert>
#include <wy3dApp/Application.h>
#include <osg/Camera>
#include <osgUtil/PolytopeIntersector>
#include "OsgSelectUtils.h"
#include <wy3dApp/Scene.h>
#include "scene/nodes/ElementNodeType.h"
#include "select/PointPick.h"
#include "select/BoxPick.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

OsgSelectHandler::OsgSelectHandler() : SelectHandler(), _pOsgView(nullptr)
{
}

OsgSelectHandler::~OsgSelectHandler()
{
}

bool OsgSelectHandler::handle(const osgGA::GUIEventAdapter& ea,
                               osgGA::GUIActionAdapter& aa,
                               osg::Object* object,
                               osg::NodeVisitor* nv)
{
    if (!_supportsPointSelect && !_supportsBoxSelect)
        return false;

    _pOsgView = dynamic_cast<osgViewer::View*>(&aa);

    switch (ea.getEventType())
    {
    case osgGA::GUIEventAdapter::MOVE:
        this->onMouseMove(ea.getX(), ea.getY());
        break;

    case osgGA::GUIEventAdapter::PUSH:
        if (ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
            this->onMouseDown(ea.getX(), ea.getY());
        return false;

    case osgGA::GUIEventAdapter::RELEASE:
        if (ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        {
            bool isCtrlDown = (ea.getModKeyMask() & osgGA::GUIEventAdapter::MODKEY_CTRL) != 0;
            this->onMouseUp(ea.getX(), ea.getY(), isCtrlDown);
        }
        return false;

    case osgGA::GUIEventAdapter::DRAG:
        if (ea.getButtonMask() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
            this->onMouseDrag(ea.getX(), ea.getY());
        return false;

    default:
        return false;
    }

    return false;
}

// ── 拾取实现 ──

wyap::Selection OsgSelectHandler::pointPick(float x, float y)
{
    static PointPickOption option;
    option.allowedElemTypes = _allowedElemTypes;
    option.selType = _selType;
    option.pSelPreFilter = _pPreSelFilter;
    option.pSelFilter = _pSelFilter;
    return PointPick::pick(Application::instance().getActiveDatabase(),
                           _pOsgView.get(), x, y, option);
}

wyap::SelectionSet OsgSelectHandler::boxPick_cross(
    float xMin, float yMin,
    float xMax, float yMax)
{
    static BoxPickOption option;
    option.allowedElemTypes = _allowedElemTypes;
    option.selType = _selType;
    option.pSelPreFilter = _pPreSelFilter;
    option.pSelFilter = _pSelFilter;
    return BoxPick::pick(Application::instance().getActiveDatabase(),
                         _pOsgView.get(), xMin, yMin, xMax, yMax, option);
}

wyap::SelectionSet OsgSelectHandler::boxPick_window(
    float xMin, float yMin,
    float xMax, float yMax)
{
    osgViewer::View* pView = _pOsgView.get();
    if (!pView) return wyap::SelectionSet();

    osg::Camera* pCamera = pView->getCamera();
    if (!pCamera)
    {
        assert(false);
        return wyap::SelectionSet();
    }

    // 构造视锥体
    osg::Polytope polytope;
    OsgSelectUtils::initPolytope(pCamera, xMin, yMin, xMax, yMax, polytope);

    // 窗口框选
    Scene* pScene = Application::instance().getActiveScene();
    std::list<wydb::ElementId> pickedIdsList;
    pickedIdsList = pScene->pickByNormalBox(polytope, _allowedElemTypes == 0 ? ElementNodeType::All : _allowedElemTypes);

    wyap::SelectionSet ss;
    for (const wydb::ElementId& id : pickedIdsList)
        ss.add(wyap::Selection(id));

    return ss;
}

// ── 框选矩形 ──

void OsgSelectHandler::drawBoxRect(float xMin, float yMin,
                                    float xMax, float yMax)
{
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return;
    BoxSelectRectangle* pSelRect = pScene->getBoxSelectRectNode();
    if (!pSelRect) return;
    pSelRect->show();
    pSelRect->update(osg::Vec2(xMin, yMin), osg::Vec2(xMax, yMax));
}

void OsgSelectHandler::clearBoxRect()
{
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return;
    BoxSelectRectangle* pSelRect = pScene->getBoxSelectRectNode();
    if (!pSelRect) return;
    pSelRect->hide();
}

} // namespace wy3dApp
