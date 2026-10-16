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

#ifndef WY3DAPP_SKETCH_SNAP_SYSTEM_H
#define WY3DAPP_SKETCH_SNAP_SYSTEM_H

#include <vector>
#include <list>
#include <map>
#include <set>
#include <queue>
#include <memory>
#include <QString>
#include <osg/Node>
#include <RTree/RTree.h>
#include <osgGA/GUIEventHandler>
#include <osgGA/GUIActionAdapter>
#include <osgViewer/View>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include "SnapSystemBase.h"
#include "Snap2dSystem.h"
#include "utils/StlUtil.h"
#include <wyapDocManager.h>
#include <wy3dSketch.h>

#include "commands/transient/SketchCurveTransient.h"
#include "SketchSnapContext.h"
#include "SketchSnapObject.h"
#include "Snap2dResult.h"
#include "select/PointPick.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchSnapSystem : public Snap2dSystem
{
public:
    explicit SketchSnapSystem(const wy3d::Sketch* pSketch);
    SketchSnapSystem(const SketchSnapSystem&) = delete;
    SketchSnapSystem& operator=(const SketchSnapSystem&) = delete;
    SketchSnapSystem(SketchSnapSystem&&) = delete;
    SketchSnapSystem& operator=(SketchSnapSystem&&) = delete;

protected:
    // 初始化
    bool init(const wy3d::Sketch* pSketch);

    const wy3d::Sketch* getSketch() const;

    virtual wydb::ElementId pickElement(
        const wydb::Database* pDb, osgViewer::View* pView,
        double x, double y, const SketchSnapContext* pContext) override;
    virtual Snap2dResultSPtr pickedDispatch(
        const SketchSnapContext* pContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY) override;
    virtual bool refreshPlane(const wydb::Database* pDb) override;
    virtual void rebuildAll() override;
    virtual void appendSnapObjects(const wydb::Database* pDb, const wydb::ElementId& id) override;
    virtual GuiCmdTransientSPtr createHighlightTransient(
        const wydb::Database* pDb, const wydb::ElementId& id) const override;

private:
    // 定位点+捕捉到对象
    Snap2dResultSPtr snapImplPicked(
        const SketchLocateContext* pLocateContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 绘制直线+捕捉到对象
    Snap2dResultSPtr snapImplPicked(
        const SketchDrawLineContext* pDrawLineContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 绘制圆+捕捉到对象
    Snap2dResultSPtr snapImplPicked(
        const SketchDrawCircleContext* pDrawCircleContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 绘制圆弧+捕捉到对象
    Snap2dResultSPtr snapImplPicked(
        const SketchDrawArcContext* pDrawCircleContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 曲线端点+捕捉到对象
    Snap2dResultSPtr snapImplPicked(
        const SketchCurveEndPointContext* pCurveEndPointContext, const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY);

    // 捕捉点
    PointSnapObjectSPtr snapPointSnapObject_Nearest(
        const std::list<PointSnapObjectSPtr>& snapObjs,
        const wy::Vector2& pos, double tol,
        double scaleScreenX, double scaleScreenY)
    {
        assert(tol > 0.0);
        assert(scaleScreenX > 0.0);
        assert(scaleScreenY > 0.0);

        double tolX = std::fabs(tol * scaleScreenX); // transform tol from pixel to sketch
        double tolY = std::fabs(tol * scaleScreenY); // transform tol from pixel to sketch
        double minDist(DBL_MAX), disX(0.0), disY(0.0);
        PointSnapObjectSPtr pRet(nullptr);
        for (const PointSnapObjectSPtr& pSnapObj : snapObjs)
        {
            assert(pSnapObj);
            wy::Vector2 value = pSnapObj->getValue();
            disX = std::fabs(value.x() - pos.x());
            disY = std::fabs(value.y() - pos.y());
            if (disX <= tolX && disY <= tolY)
            {
                if (disX + disY < minDist)
                {
                    minDist = disX + disY;
                    pRet = pSnapObj;
                }
            }
        }

        return pRet;
    }

private:
    // 草图ID
    wydb::ElementId _sketchId;

    // 点选选项
    PointPick::Options _pickOption;
};

typedef std::shared_ptr<SketchSnapSystem> SketchSnapSystemSPtr;

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_SNAP_SYSTEM_H
