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

#ifndef WY3DAPP_SKETCH_EXTEND_GUI_CMD_H
#define WY3DAPP_SKETCH_EXTEND_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <map>
#include <memory>
#include <osg/Array>
#include <osg/LineStipple>
#include <wyVector3.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>

#include <wy3dApp/GuiCmdTransient.h>
#include "SketchExtendGraph.h"
#include "SketchTrimGuiCmd.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class ExtendSegmentTransient;

class SketchExtendGuiCmd : public OsgGuiCommand, public wydb::DatabaseReactor
{
    WYRX_DECLARE_MEMBERS(SketchExtendGuiCmd, wy3dApp::SketchExtendGuiCmd, OsgGuiCommand)
public:
    SketchExtendGuiCmd();
    virtual ~SketchExtendGuiCmd();

    virtual void onDatabaseChanged(
        const wydb::Database* pDatabase,
        const wydb::Transaction* pTransaction,
        const wydb::DatabaseChangeInfo& changeInfo) override;

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

private:
    // 拾取延伸段
    void pickExtendSegment(const wydb::ElementId& id, const wy::Vector3& pickPos3d);
    // 延伸
    bool extend(const wydb::ElementId curveId, const SketchExtendKnot& startKnot, const SketchExtendKnot& endKnot);

    // 初始化
    bool initExtendGraph();

private:
    std::unique_ptr<SketchExtendGraph> _pExtendGraph;
    std::shared_ptr<ExtendSegmentTransient> _pTransient;

    // 点选选项
    PointPickOptions _pointPickOption;
};

class ExtendSegmentTransient : public GuiCmdTransient
{
public:
    ExtendSegmentTransient(GuiCommand* pGuiCmd, const wy3d::Plane& sketchPlane, const wy3d::SketchCurve* pSketchCurve, const SketchExtendSegment& segment);
    virtual ~ExtendSegmentTransient();

    wydb::ElementId getId() const { return _id; }
    const SketchExtendSegment& getSegment() const { return _segment; }

private:
    void init(const wy3d::Plane& sketchPlane, const wy3d::SketchCurve* pSketchCurve);
    void initGeom(osg::ref_ptr<osg::Vec3Array> vertices, const std::vector<unsigned int>& indices);

private:
    wydb::ElementId _id;
    SketchExtendSegment _segment;
    osg::ref_ptr<osg::LineStipple> _lineStipple;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_EXTEND_GUI_CMD_H
