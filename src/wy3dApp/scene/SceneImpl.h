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

#ifndef WY3DAPP_SCENE_IMPL_H
#define WY3DAPP_SCENE_IMPL_H

#include <vector>
#include <list>
#include <map>
#include <set>
#include <cassert>
#include <memory>

#include <osg/BoundingSphere>
#include <osg/Geode>
#include <osg/Group>
#include <osgViewer/View>
#include <osg/AutoTransform>
#include <osg/LightSource>

#include <wydbElementId.h>
#include <wydbDatabase.h>
#include <wyapSelManager.h>
#include <wy3dPlane.h>
#include <wy3dSolid.h>

#include "snap/SnapSystemBase.h"
#include "select/BoxSelectRectangle.h"
#include "scene/ViewCube.h"
#include "RTree/RTree.h"

#include <wy3dApp/Scene.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class ElementNode;
class OsgGizmoNode;
class SketchEnvironment;
class Sketch3DEnvironment;

// Scene 的具体实现。只在 wy3dApp DLL 内部使用，不导出。
class SceneImpl final : public Scene, public wyap::SnapSystemReactor
{
public:
    explicit SceneImpl(wyap::Document* pDoc);
    ~SceneImpl() override;
    SceneImpl(const SceneImpl&) = delete;
    SceneImpl& operator=(const SceneImpl&) = delete;
    SceneImpl(SceneImpl&&) = delete;
    SceneImpl& operator=(SceneImpl&&) = delete;

    // ── Scene 公共接口 ──
    osg::Group* getRoot() const override { return _pRoot.get(); }
    osg::Group* getGizmosGroup() const override { return _pGizmosGroup.get(); }
    osg::Group* getOsgTestRoot() const { return _pOsgTestRoot.get(); }

    osg::Group* getTransientRoot() const override { return _pTransientGroup.get(); }
    void addTransient(osg::Node* pNode) override
    {
        if (pNode)
        {
            _pTransientGroup->addChild(pNode);
        }
    }
    void removeTransient(osg::Node* pNode) override
    {
        if (pNode)
        {
            _pTransientGroup->removeChild(pNode);
        }
    }
    void clearTransients() override
    {
        _pTransientGroup->removeChildren(0, _pTransientGroup->getNumChildren());
    }

    osg::Group* getSnapObjectsRoot() const override { return _pSnapObjectsGroup.get(); }
    void addSnapObject(osg::Node* pNode) override
    {
        if (pNode)
        {
            _pSnapObjectsGroup->addChild(pNode);
        }
    }
    void removeSnapObject(osg::Node* pNode) override
    {
        if (pNode)
        {
            _pSnapObjectsGroup->removeChild(pNode);
        }
    }
    void clearSnapObjects() override
    {
        _pSnapObjectsGroup->removeChildren(0, _pSnapObjectsGroup->getNumChildren());
    }

    ElementNode* getElementNode(const wydb::ElementId& id) const override;

    DisplayMode getDisplayMode() const override { return _displayMode; }
    void setDisplayMode(DisplayMode mode) override;
    osg::BoundingSphere getFitViewBoundingSphere() const override;

    // ── DLL 内部接口 ──
    osg::BoundingSphere getElementsBoundingSphere() const;
    void initLight(osgViewer::View* pView);
    void initBoxSelectRect(osgViewer::View* pView);
    BoxSelectRectangle* getBoxSelectRectNode() const { return _pBoxSelectRect.get(); }
    void initViewCube(osgViewer::View* pView);
    ViewCube* getViewCubeNode() const { return _pViewCube.get(); }

    std::list<wydb::ElementId> pickByNormalBox(osg::Polytope& polytope, unsigned int allowedElemTypes) const;
    std::list<wydb::ElementId> pickByCrossBox(osg::Polytope& polytope, unsigned int allowedElemTypes) const;
    wyap::GizmoSPtr pickGizmo(osg::Polytope& polytope) const;

    void updateDatumPlaneVisualSize(const wydb::Database* pDb);
    void updateDatumPlaneVisualSize(const wydb::Database* pDb, const wydb::ElementId& datumPlnId);

    void endNoBatchRender();

    bool isWCSVisible() const;
    void showWCS();
    void hideWCS();

    void showSketchCSYS(const wy3d::Plane& sketchPlane);
    void hideSketchCSYS();

    // ── 虚拟回调 ──
    virtual void onDatabaseChanged(
        const wydb::Database* pDb,
        const wydb::Transaction* pTransaction,
        const wydb::DatabaseChangeInfo& changeInfo) override;

    virtual void onTransactionStarted(wydb::Transaction* pTrans) override;
    virtual void onTransactionEnded(wydb::Transaction* pTrans) override;
    virtual void onTransactionAborted(wydb::Transaction* pTrans) override;
    virtual void onTransactionUndone(wydb::Transaction* pTrans) override;
    virtual void onTransactionRedone(wydb::Transaction* pTrans) override;

    virtual void onSelectionChanged(
        const wyap::SelectionSet& addedSS,
        const wyap::SelectionSet& removedSS,
        const wyap::SelectionSet& curSS) override;

    virtual void onGizmoChanged(
        const std::set<wyap::GizmoSPtr>& addedGizmos,
        const std::set<wyap::GizmoSPtr>& removedGizmos,
        const std::set<wyap::GizmoSPtr>& currGizmos) override;
    virtual void onGizmoToBeDeactivated(wyap::GizmoSPtr pGizmo) override;
    virtual void onGizmoToBeActivated(wyap::GizmoSPtr pGizmo) override;
    virtual void onGizmoActivated(wyap::GizmoSPtr pGizmo) override;

    virtual void onSnapResultChanged(
        wyap::SnapResultSPtr pCurSnapResult,
        wyap::SnapResultSPtr pLastSnapResult) override;

    virtual void onEnvironmentToBeEntered(wyap::Environment* pEnvironment) override;
    virtual void onEnvironmentEntered(wyap::Environment* pEnvironment) override;
    virtual void onEnvironmentToBeExited(wyap::Environment* pEnvironment) override;
    virtual void onEnvironmentExited(
        wyap::Environment* pEnvironment,
        wyap::Environment::ExitCode exitCode) override;

private:
    bool addElementNode(const wydb::ElementId& id, bool addSketchEntity = false);
    ElementNode* newElementNodeOfSolid(const wy3d::Solid* pSolid);
    bool removeElementNode(const wydb::ElementId& id);
    bool modifyElementNode(
        const wydb::ElementId& id,
        const wydb::DatabaseChangeDetails& details,
        bool isInSketchEnv);

    osg::ref_ptr<osg::Group> newSketchUpStyleWCS();
    osg::ref_ptr<osg::Group> newWCS();
    osg::ref_ptr<osg::AutoTransform> newSketchCSYS(float axisHalfLen);
    bool updateSketchCSYS(float axisHalfLen);
    void addToScene(ElementNode* pElemNode);
    void removeFromScene(std::map<wydb::ElementId, ElementNode*>::const_iterator iter);
    void destroyElementNode(ElementNode* pElemNode);

    void addToOsgScene(const ElementNode* pElemNode);
    void removeFromOsgScene(const ElementNode* pElemNode);

    void addToRTree(const ElementNode* pElemNode);
    void removeFromRTree(const ElementNode* pElemNode);

    std::list<wydb::ElementId> pickByBoxImpl(osg::Polytope& polytope, bool isCross, unsigned int allowedElemTypes) const;

    void enterSketchEnvironment(SketchEnvironment* pSketchEnv);
    void exitSketchEnvironment(
        SketchEnvironment* pSketchEnv,
        wyap::Environment::ExitCode exitCode);

    void enterSketch3DEnvironment(Sketch3DEnvironment* pSketch3DEnv);
    void exitSketch3DEnvironment(
        Sketch3DEnvironment* pSketch3DEnv,
        wyap::Environment::ExitCode exitCode);

private:
    osg::ref_ptr<osg::Group> _pRoot;
    osg::ref_ptr<osg::Group> _pElemsRoot;
    std::map<wydb::ElementId, ElementNode*> _id2ElemNode;
    osg::ref_ptr<osg::Group> _pOsgTestRoot;
    osg::ref_ptr<osg::Group> _pWCS;
    osg::ref_ptr<osg::AutoTransform> _pSketchCsys;
    osg::ref_ptr<BoxSelectRectangle> _pBoxSelectRect;
    osg::ref_ptr<ViewCube> _pViewCube;
    osg::ref_ptr<osg::LightSource> _mainLightSource;
    osg::ref_ptr<osg::Group> _pGizmosGroup;
    std::map<OsgGizmoNode*, wyap::GizmoSPtr> _node2Gizmo;
    osg::ref_ptr<osg::Group> _pTransientGroup;
    osg::ref_ptr<osg::Group> _pSnapObjectsGroup;
    RTree<unsigned int, double, 3> _rtree;
    RTree<unsigned int, double, 3> _rtreeDatum;
    DisplayMode _displayMode = DisplayMode::ShadedWithEdges;

    struct SketchEnvInfo
    {
        wydb::ElementId sketchId;
        ElementNode* pSketchElemNode;

        SketchEnvInfo()
        {
            sketchId = wydb::ElementId::kNull;
            pSketchElemNode = nullptr;
        }
    };
    std::shared_ptr<SketchEnvInfo> _pSketchEnvInfo;
};

} // namespace wy3dApp

#endif // WY3DAPP_SCENE_IMPL_H
