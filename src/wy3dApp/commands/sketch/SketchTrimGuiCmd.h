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

#ifndef WY3DAPP_SKETCH_TRIM_GUI_CMD_H
#define WY3DAPP_SKETCH_TRIM_GUI_CMD_H

#include "commands/OsgGuiCommand.h"
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
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>

#include "commands/transient/GuiCmdTransient.h"
#include "SketchTrimGraph.h"

class TrimSegmentTransient;

class SketchTrimGuiCmd : public OsgGuiCommand, public wydb::DatabaseReactor
{
    WYRX_DECLARE_MEMBERS(SketchTrimGuiCmd, SketchTrimGuiCmd, OsgGuiCommand)
public:
    SketchTrimGuiCmd();
    virtual ~SketchTrimGuiCmd();

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
    // 拾取修剪段
    void pickTrimSegment(const wydb::ElementId& id, const wy::Vector3& pickPos3d);

    bool trim(const wydb::ElementId curveId, const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot);

    bool trimLine(
        wydb::Database* pDb, wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, wy3d::SketchLine* pConstLine,
        const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot);
    bool trimCenterLine(
        wydb::Database* pDb, wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, wy3d::SketchCenterLine* pConstLine,
        const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot);
    bool trimCircle(
        wydb::Database* pDb, wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, wy3d::SketchCircle* pCircle,
        const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot);
    bool trimArc(
        wydb::Database* pDb, wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, wy3d::SketchArc* pArc,
        const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot);
    bool trimEllipse(
        wydb::Database* pDb, wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, wy3d::SketchEllipse* pEllipse,
        const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot);
    bool trimEllipseArc(
        wydb::Database* pDb, wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, wy3d::SketchEllipseArc* pEllipseArc,
        const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot);
    bool trimSpline(
        wydb::Database* pDb, wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, wy3d::SketchSpline* pSpline,
        const SketchTrimKnot& startKnot, const SketchTrimKnot& endKnot);

private:
    std::unique_ptr<SketchTrimGraph> _pTrimGraph;
    std::shared_ptr<TrimSegmentTransient> _pTransient;
    std::map<wydb::ElementId, wydb::ElementId> _id2Parent;

    // 点选选项
    PointPickOption _pointPickOption;
};

class TrimSegmentTransient : public GuiCmdTransient
{
public:
    TrimSegmentTransient(GuiCommand* pGuiCmd, const wy3d::Plane& sketchPlane, const wy3d::SketchCurve* pSketchCurve, const SketchTrimSegment& trimSegment);
    virtual ~TrimSegmentTransient();

    wydb::ElementId getId() const { return _id; }
    const SketchTrimSegment& getSegment() const { return _trimSegment; }

private:
    void init(const wy3d::Plane& sketchPlane, const wy3d::SketchCurve* pSketchCurve);
    void initGeom(osg::ref_ptr<osg::Vec3Array> vertices, const std::vector<unsigned int>& indices);

private:
    wydb::ElementId _id;
    SketchTrimSegment _trimSegment;
    osg::ref_ptr<osg::LineStipple> _lineStipple;
};

#endif // WY3DAPP_SKETCH_TRIM_GUI_CMD_H