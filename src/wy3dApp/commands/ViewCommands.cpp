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

#include "ViewCommands.h"

#include <set>
#include <cassert>
#include <QCoreApplication>
#include <wy3dDatumPlane.h>
#include <wyapSelManager.h>
#include <wyapSelection.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "view/OsgView.h"
#include "view/ViewUtil.h"
#include "environments/sketch/SketchEnvironment.h"
#include "utils/MathUtils.h"


int FitViewCommand::run()
{
    BaseView* pActiveView = Application::instance().getActiveView();
    if (!pActiveView) return -1;
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return -1;
    osg::BoundingSphere bsSphere = pScene->getElementsBoundingBox();
    pActiveView->viewAll(bsSphere);

    return 0;
}


int FitSelectionCommand::run()
{
    BaseView* pActiveView = Application::instance().getActiveView();
    if (!pActiveView) return -1;
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return -1;
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.isEmpty()) return 0;

    osg::BoundingBox mergedBBox;
    mergedBBox.init();
    std::set<wydb::ElementId> ids;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        ids.insert(iter.current().getElementId());
    }
    for (const wydb::ElementId& id : ids)
    {
        ElementNode* pElemNode = pScene->getElementNode(id);
        if (!pElemNode)
        {
            assert(false);
            continue;
        }
        const osg::BoundingBox& bbox = pElemNode->getBoundingBox();
        if (bbox.valid())
        {
            mergedBBox.expandBy(bbox);
        }
    }
    if (!mergedBBox.valid())
    {
        return 0;
    }

    pActiveView->viewAll(osg::BoundingSphere(mergedBBox.center(), mergedBBox.radius()));
    return 0;
}


int IsometricViewCommand::run()
{
    BaseView* pView = Application::instance().getActiveView();
    if (!pView) return -1;
    pView->lookAtISO();

    return 0;
}


int FrontViewCommand::run()
{
    BaseView* pView = Application::instance().getActiveView();
    if (pView)
    {
        pView->lookAtFront();
    }

    return 0;
}


int BackViewCommand::run()
{
    BaseView* pView = Application::instance().getActiveView();
    if (pView)
    {
        pView->lookAtBack();
    }

    return 0;
}


int LeftViewCommand::run()
{
    BaseView* pView = Application::instance().getActiveView();
    if (pView)
    {
        pView->lookAtLeft();
    }

    return 0;
}


int RightViewCommand::run()
{
    BaseView* pView = Application::instance().getActiveView();
    if (pView)
    {
        pView->lookAtRight();
    }

    return 0;
}


int TopViewCommand::run()
{
    BaseView* pView = Application::instance().getActiveView();
    if (pView)
    {
        pView->lookAtTop();
    }

    return 0;
}


int BottomViewCommand::run()
{
    BaseView* pView = Application::instance().getActiveView();
    if (pView)
    {
        pView->lookAtBottom();
    }

    return 0;
}


int OrientToSketchCommand::run()
{
    wyap::Environment* pCurEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pCurEnv);
    if (!pSketchEnv)
    {
        assert(false);
        return 0;
    }
    const wy3d::SketchPlane& sketchPlane = pSketchEnv->getSketchPlane();

    BaseView* pView = Application::instance().getActiveView();
    if (!pView)
    {
        assert(false);
        return 0;
    }
    pView->viewToWorkingPlane(sketchPlane);

    return 0;
}


int ViewNormalToCommand::run()
{
    BaseView* pView = Application::instance().getActiveView();
    if (!pView)
    {
        assert(false);
        return 0;
    }
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.getCount() != 1)
    {
        assert(false);
        return 0;
    }
    wydb::ElementId id = ss.createIterator().current().getElementId();
    if (id.isNull())
    {
        assert(false);
        return 0;
    }
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return 0;
    }
    const wydb::Element* pElem = pDb->getElement(id);
    if (!pElem)
    {
        assert(false);
        return 0;
    }
    const wyrx::ClassInfo* classInfo = pElem->getClassInfo();
    if (classInfo == wy3d::DatumPlane::classInfo())
    {
        const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pElem);
        if (!pDatumPlane)
        {
            assert(false);
            return 0;
        }
        pView->viewToWorkingPlane(pDatumPlane->getPlane());
    }
    else if (classInfo == wy3d::Sketch::classInfo())
    {
        const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem);
        if (!pSketch)
        {
            assert(false);
            return 0;
        }
        pView->viewToWorkingPlane(pSketch->getPlane());
    }
    else
    {
        assert(false);
        return 0;
    }

    return 0;
}


int ShadedWithEdgesDisplayCommand::run()
{
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return -1;
    pScene->setDisplayMode(Scene::DisplayMode::ShadedWithEdges);
    return 0;
}


int ShadedDisplayCommand::run()
{
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return -1;
    pScene->setDisplayMode(Scene::DisplayMode::Shaded);
    return 0;
}


int WireframeDisplayCommand::run()
{
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return -1;
    pScene->setDisplayMode(Scene::DisplayMode::Wireframe);
    return 0;
}