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

#include "TestCommands.h"

#include <osgDB/ReadFile>

#include <wyVector2.h>
#include <geom/wy3dLineSegment2.h>
#include <utils/wy3dCurveIntersectionUtil.h>

#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchCurve.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "view/OsgView.h"
#include "OsgUtils.h"
#include "environments/sketch/SketchEnvironment.h"


int OsgNewBoxCommand::run()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return -1;
    osg::Group* pOsgTestRoot = pActiveScene->getOsgTestRoot();
    if (!pOsgTestRoot) return -1;

    osg::Node* pNode = OsgUtils::createShapeDrawable_Box(
        10.0, 20.0, 30.0, osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    pOsgTestRoot->addChild(pNode);

    return 0;
}


int OpenGLNewBoxCommand::run()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return -1;
    osg::Group* pOsgTestRoot = pActiveScene->getOsgTestRoot();
    if (!pOsgTestRoot) return -1;

    osg::Node* pNode = OsgUtils::createBox();
    pOsgTestRoot->addChild(pNode);

    return 0;
}


int OsgNewCylinderCommand::run()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return -1;
    osg::Group* pOsgTestRoot = pActiveScene->getOsgTestRoot();
    if (!pOsgTestRoot) return -1;

    osg::Node* pNode = OsgUtils::createShapeDrawable_Cylinder(
        10.0, 60.0, osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    pOsgTestRoot->addChild(pNode);

    return 0;
}


int OsgNewSphereCommand::run()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return -1;
    osg::Group* pOsgTestRoot = pActiveScene->getOsgTestRoot();
    if (!pOsgTestRoot) return -1;

    osg::Node* pNode = OsgUtils::createShapeDrawable_Sphere(15.0, osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    pOsgTestRoot->addChild(pNode);

    return 0;
}


int OsgNewCowCommand::run()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return -1;
    osg::Group* pOsgTestRoot = pActiveScene->getOsgTestRoot();
    if (!pOsgTestRoot) return -1;

    osg::ref_ptr<osg::Node> pCowNode = osgDB::readRefNodeFile("D:\\dev\\OSG-3.6.4\\data\\cow.osgt");
    pOsgTestRoot->addChild(pCowNode);

    return 0;
}


int SketchTestIntersectionCommand::run()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    assert(pDb);
    if (!pDb) return 0;
    wyap::Environment* pEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pEnv);
    if (!pSketchEnv) return 0;
    const wy3d::Sketch* pConstSketch = wy3d::Sketch::cast(pDb->getElement(pSketchEnv->getSketchId()));
    if (!pConstSketch) return 0;

    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.getCount() != 2) return 0;
    std::vector<const wy3d::SketchCurve*> curves;
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current().getElementId();
        const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
        if (!pCurve) continue;
        curves.emplace_back(pCurve);
    }
    if (curves.size() != 2) return 0;
    const wy3d::SketchCurve* pCurve1 = curves[0];
    const wy3d::SketchCurve* pCurve2 = curves[1];
    assert(pCurve1);
    assert(pCurve2);    std::vector<wy::Vector2> intPnts;
    pCurve1->intersectWith(*pCurve2, intPnts);
    if (intPnts.empty()) return 0;

    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return 0;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(pSketchEnv->getSketchId()));
    if (!pSketch)
    {
        pDb->getTransactionManager()->abortTransaction();
        return 0;
    }
    for (const wy::Vector2& intPnt : intPnts)
    {
        wy3d::SketchLine* pSketchLine(nullptr);
        wy3d::SketchLine::create(pTrans, wy::Vector2::kZero, intPnt, pSketchLine);
        pSketch->addEntity(pSketchLine);
    }
    pDb->getTransactionManager()->endTransaction();

    return 0;
}


int SketchTestBoundingBoxCommand::run()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    assert(pDb);
    if (!pDb) return 0;
    wyap::Environment* pEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pEnv);
    if (!pSketchEnv) return 0;
    const wy3d::Sketch* pConstSketch = wy3d::Sketch::cast(pDb->getElement(pSketchEnv->getSketchId()));
    if (!pConstSketch) return 0;

    std::vector<const wy3d::SketchCurve*> curveElements;
    curveElements.reserve(100);
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.isEmpty())
    {
        for (auto iter = pConstSketch->createIterator(); !iter.isDone(); iter.moveNext())
        {
            wydb::ElementId id = iter.current();
            const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
            if (!pCurve) continue;
            curveElements.emplace_back(pCurve);
        }
    }
    else
    {
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            wydb::ElementId id = iter.current().getElementId();
            const wy3d::SketchCurve* pCurve = wy3d::SketchCurve::cast(pDb->getElement(id));
            if (!pCurve) continue;
            curveElements.emplace_back(pCurve);
        }
    }

    std::vector<wy3d::geom::BoundingBox2> bboxs;
    bboxs.reserve(curveElements.size());
    for (const wy3d::SketchCurve* pCurve : curveElements)
    {
        if (!pCurve) continue;
        bboxs.emplace_back(pCurve->getBoundingBox());
    }
    if (bboxs.empty()) return 0;

    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return 0;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(pSketchEnv->getSketchId()));
    if (!pSketch)
    {
        pDb->getTransactionManager()->abortTransaction();
        return 0;
    }
    for (const wy3d::geom::BoundingBox2& bbox : bboxs)
    {
        if (bbox.isEmpty()) continue;
        double minX = bbox.min().x();
        if (std::isnan(minX) || std::isinf(minX) || minX == DBL_MAX || minX == -DBL_MAX) continue;
        double minY = bbox.min().y();
        if (std::isnan(minY) || std::isinf(minY) || minY == DBL_MAX || minY == -DBL_MAX) continue;
        double maxX = bbox.max().x();
        if (std::isnan(maxX) || std::isinf(maxX) || maxX == DBL_MAX || maxX == -DBL_MAX) continue;
        double maxY = bbox.max().y();
        if (std::isnan(maxY) || std::isinf(maxY) || maxY == DBL_MAX || maxY == -DBL_MAX) continue;

        {
            wy3d::SketchLine* pSketchLine(nullptr);
            wy3d::SketchLine::create(pTrans, wy::Vector2(minX, minY), wy::Vector2(minX, maxY), pSketchLine);
            pSketchLine->setConstruction(true);
            pSketch->addEntity(pSketchLine);
        }
        {
            wy3d::SketchLine* pSketchLine(nullptr);
            wy3d::SketchLine::create(pTrans, wy::Vector2(minX, minY), wy::Vector2(maxX, minY), pSketchLine);
            pSketchLine->setConstruction(true);
            pSketch->addEntity(pSketchLine);
        }
        {
            wy3d::SketchLine* pSketchLine(nullptr);
            wy3d::SketchLine::create(pTrans, wy::Vector2(maxX, maxY), wy::Vector2(maxX, minY), pSketchLine);
            pSketchLine->setConstruction(true);
            pSketch->addEntity(pSketchLine);
        }
        {
            wy3d::SketchLine* pSketchLine(nullptr);
            wy3d::SketchLine::create(pTrans, wy::Vector2(maxX, maxY), wy::Vector2(minX, maxY), pSketchLine);
            pSketchLine->setConstruction(true);
            pSketch->addEntity(pSketchLine);
        }
    }
    pDb->getTransactionManager()->endTransaction();

    return 0;
}