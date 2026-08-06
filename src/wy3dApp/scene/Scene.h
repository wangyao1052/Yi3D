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

#ifndef WY3DAPP_SCENE_H
#define WY3DAPP_SCENE_H

#include <vector>
#include <list>
#include <map>
#include <set>
#include <cassert>

#include <osg/BoundingSphere>
#include <osg/Geode>
#include <osg/Group>
#include <osgViewer/View>
#include <osg/AutoTransform>
#include <osg/LightSource>

#include <wydbElementId.h>
#include <wydbDatabase.h>
#include <wyapSelManager.h>
#include <wyapScene.h>
#include <wy3dSketchPlane.h>
#include "snap/SnapSystemBase.h"
#include <wy3dSolid.h>

#include "select/BoxSelectRectangle.h"
#include "RTree/RTree.h"

namespace wyap
{
    class Document;
}

class ElementNode;
class OsgGizmoNode;
class SketchEnvironment;

// 场景
// 每个文档对应一个场景
// 场景的生命周期由SceneManager来管理
class Scene : public wyap::Scene, public wyap::SnapSystemReactor
{
public:
    Scene(wyap::Document* pDoc);
    virtual ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;

    // 获取元素外包围盒
    osg::BoundingSphere getElementsBoundingBox() const;

    // 根节点
    osg::Group* getRoot() const { return _pRoot.get(); }
    // Gizmo 组节点
    osg::Group* getGizmosGroup() const { return _pGizmosGroup.get(); }
    // osg测试根节点
    osg::Group* getOsgTestRoot() const { return _pOsgTestRoot.get(); }

    // 临时渲染对象根节点
    osg::Group* getTransientRoot() const { return _pTransientGroup.get(); }
    // 添加临时渲染对象
    void addTransient(osg::Node* pNode)
    {
        if (pNode)
        {
            _pTransientGroup->addChild(pNode);
        }
    }
    // 删除临时渲染对象
    void removeTransient(osg::Node* pNode)
    {
        if (pNode)
        {
            _pTransientGroup->removeChild(pNode);
        }
    }
    // 清空临时渲染对象
    void clearTransients()
    {
        _pTransientGroup->removeChildren(0, _pTransientGroup->getNumChildren());
    }

    // 捕捉对象根节点
    osg::Group* getSnapObjectsRoot() const { return _pSnapObjectsGroup.get(); }
    // 添加捕捉渲染对象
    void addSnapObject(osg::Node* pNode)
    {
        if (pNode)
        {
            _pSnapObjectsGroup->addChild(pNode);
        }
    }
    // 删除捕捉渲染对象
    void removeSnapObject(osg::Node* pNode)
    {
        if (pNode)
        {
            _pSnapObjectsGroup->removeChild(pNode);
        }
    }
    // 清空捕捉渲染对象
    void clearSnapObjects()
    {
        _pSnapObjectsGroup->removeChildren(0, _pSnapObjectsGroup->getNumChildren());
    }

    // 初始化灯光
    void initLight(osgViewer::View* pView);

    // 初始化框选矩形
    void initBoxSelectRect(osgViewer::View* pView);
    // 框选矩形节点
    BoxSelectRectangle* getBoxSelectRectNode() const { return _pBoxSelectRect.get(); }

    // 默认框选(完全框住才选中)
    std::list<wydb::ElementId> pickByNormalBox(osg::Polytope& polytope, unsigned int pickMask) const;
    // 交叉框选(任何部分有交集就选中)
    std::list<wydb::ElementId> pickByCrossBox(osg::Polytope& polytope, unsigned int pickMask) const;

    // pick gizmo
    wyap::GizmoSPtr pickGizmo(osg::Polytope& polytope) const;

    // 刷新基准面显示大小
    void updateDatumPlaneVisualSize(const wydb::Database* pDb);
    // 刷新具体基准面显示大小
    void updateDatumPlaneVisualSize(const wydb::Database* pDb, const wydb::ElementId& datumPlnId);
    
    // 显示模式
    enum class DisplayMode { Shaded = 0, Wireframe = 1 };
    DisplayMode getDisplayMode() const { return _displayMode; }
    void setDisplayMode(DisplayMode mode);

    // 结束非批次渲染
    // 在选择面&边时会启动元素节点的非批次渲染(例如:每个面对应一个PrimitiveSet以方便高亮);
    // 但这会损耗渲染性能;需要在适当的时候调用该接口结束非批次渲染
    void endNoBatchRender();

    // 世界坐标系是否可见
    bool isWCSVisible() const;
    // 显示世界坐标系
    void showWCS();
    // 隐藏世界坐标系
    void hideWCS();

    // 显示草图坐标系
    void showSketchCSYS(const wy3d::SketchPlane& sketchPlane);
    // 隐藏草图坐标系
    void hideSketchCSYS();

public:
    // 数据库变化响应
    virtual void onDatabaseChanged(
        const wydb::Database* pDb,
        const wydb::Transaction* pTransaction,
        const wydb::DatabaseChangeInfo& changeInfo) override;

    virtual void onTransactionStarted(wydb::Transaction* pTrans) override;
    virtual void onTransactionEnded(wydb::Transaction* pTrans) override;
    virtual void onTransactionAborted(wydb::Transaction* pTrans) override;
    virtual void onTransactionUndone(wydb::Transaction* pTrans) override;
    virtual void onTransactionRedone(wydb::Transaction* pTrans) override;
    
    // 选择集变化响应
    virtual void onSelectionChanged(
        const wyap::SelectionSet& addedSS,
        const wyap::SelectionSet& removedSS,
        const wyap::SelectionSet& curSS) override;

    // Gizmo变更响应
    virtual void onGizmoChanged(
        const std::set<wyap::GizmoSPtr>& addedGizmos,
        const std::set<wyap::GizmoSPtr>& removedGizmos,
        const std::set<wyap::GizmoSPtr>& currGizmos) override;
    virtual void onGizmoToBeDeactivated(wyap::GizmoSPtr pGizmo) override;
    virtual void onGizmoToBeActivated(wyap::GizmoSPtr pGizmo) override;
    virtual void onGizmoActivated(wyap::GizmoSPtr pGizmo) override;

    // 捕捉系统变化响应
    virtual void onSnapResultChanged(
        wyap::SnapResultSPtr pCurSnapResult,
        wyap::SnapResultSPtr pLastSnapResult) override;

    // 进入环境
    virtual void onEnvironmentToBeEntered(wyap::Environment * pEnvironment) override;
    virtual void onEnvironmentEntered(wyap::Environment* pEnvironment) override;

    // 退出环境
    virtual void onEnvironmentToBeExited(wyap::Environment* pEnvironment) override;
    virtual void onEnvironmentExited(
        wyap::Environment* pEnvironment,
        wyap::Environment::ExitCode exitCode) override;

public:
    // 获取元素节点
    ElementNode* getElementNode(const wydb::ElementId& id) const;

private:
    // 添加元素节点
    bool addElementNode(const wydb::ElementId& id, bool addSketchEntity = false);
    // 新建实体元素节点
    ElementNode* newElementNodeOfSolid(const wy3d::Solid* pSolid);
    // 移除元素节点
    bool removeElementNode(const wydb::ElementId& id);
    // 修改元素节点
    bool modifyElementNode(
        const wydb::ElementId& id,
        const wydb::DatabaseChangeDetails& details,
        bool isInSketchEnv);

private:
    // 初始化世界坐标系
    osg::ref_ptr<osg::Group> newSketchUpStyleWCS();
    osg::ref_ptr<osg::Group> newWCS();
    // 初始化草图坐标系
    osg::ref_ptr<osg::AutoTransform> newSketchCSYS(float axisHalfLen);
    // 更新草图坐标系
    bool updateSketchCSYS(float axisHalfLen);
    // 添加元素节点
    void addToScene(ElementNode* pElemNode);
    // 移除元素节点
    void removeFromScene(std::map<wydb::ElementId, ElementNode*>::const_iterator iter);
    // 销毁元素节点
    inline void destroyElementNode(ElementNode* pElemNode)
    {
        assert(pElemNode);
        delete pElemNode;
    }

    // 添加到OSG场景中
    void addToOsgScene(const ElementNode* pElemNode);
    // 从OSG场景中移除
    void removeFromOsgScene(const ElementNode* pElemNode);

    // 添加到空间索引树中
    void addToRTree(const ElementNode* pElemNode);
    // 从空间索引树移除
    void removeFromRTree(const ElementNode* pElemNode);

    // 框选
    std::list<wydb::ElementId> pickByBoxImpl(osg::Polytope& polytope, bool isCross, unsigned int pickMask) const;

    // 进入草图环境
    void enterSketchEnvironment(SketchEnvironment* pSketchEnv);
    // 退出草图环境
    void exitSketchEnvironment(
        SketchEnvironment* pSketchEnv,
        wyap::Environment::ExitCode exitCode);

private:
    // 场景根节点
    osg::ref_ptr<osg::Group> _pRoot;
    // 元素根节点
    osg::ref_ptr<osg::Group> _pElemsRoot;
    // 元素节点
    std::map<wydb::ElementId, ElementNode*> _id2ElemNode;
    // osg测试节点
    osg::ref_ptr<osg::Group> _pOsgTestRoot;
    // 世界坐标轴
    osg::ref_ptr<osg::Group> _pWCS;
    // 草图坐标系
    osg::ref_ptr<osg::AutoTransform> _pSketchCsys;
    // 框选矩形
    osg::ref_ptr<BoxSelectRectangle> _pBoxSelectRect;
    // 主光源
    osg::ref_ptr<osg::LightSource> _mainLightSource;
    // Gizmos
    osg::ref_ptr<osg::Group> _pGizmosGroup;
    std::map<OsgGizmoNode*, wyap::GizmoSPtr> _node2Gizmo;
    // 临时渲染对象
    osg::ref_ptr<osg::Group> _pTransientGroup;
    // 捕捉对象
    osg::ref_ptr<osg::Group> _pSnapObjectsGroup;
    // 空间索引树
    RTree<unsigned int, double, 3> _rtree;
    RTree<unsigned int, double, 3> _rtreeDatum; // 基准面的空间索引树(基准面的显示大小需要随着场景大小的变化而动态变化)
    // 显示模式
    DisplayMode _displayMode = DisplayMode::Shaded;
    // 草图环境信息
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

#endif // WY3DAPP_SCENE_H