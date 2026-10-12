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

#ifndef WY3DAPP_SKETCH_DRAW_LINE_TANGENT_GUI_CMD_H
#define WY3DAPP_SKETCH_DRAW_LINE_TANGENT_GUI_CMD_H

#include <wy3dApp/OsgGuiCommand.h>
#include <vector>
#include <set>
#include <map>
#include <wyVector2.h>
#include <wy3dVector2.h>
#include <wydbElementId.h>
#include <wy3dSketchLine.h>
#include "snap/SketchSnapContext.h"
#include "commands/transient/BasicTransient.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

struct TangentLineIdPair
{
    wydb::ElementId id1st;
    wydb::ElementId id2nd;

    TangentLineIdPair() : id1st(wydb::ElementId::kNull), id2nd(wydb::ElementId::kNull) {}
    TangentLineIdPair(const wydb::ElementId& id1, const wydb::ElementId& id2) : id1st(id1), id2nd(id2)
    {
        if (id1st > id2nd) // 确保id1st <= id2nd
        {
            std::swap(id1st, id2nd);
        }
    }

    bool operator<(const TangentLineIdPair& other) const
    {
        if (id1st < other.id1st)
        {
            return true;
        }
        else if (id1st == other.id1st)
        {
            return id2nd < other.id2nd;
        }
        else
        {
            return false;
        }
    }
};

struct TangentLine
{
    wy::Vector2 startPoint;
    wydb::ElementId startElementId;
    wy::Vector2 endPoint;
    wydb::ElementId endElementId;
};

class SketchDrawLineTangentGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SketchDrawLineTangentGuiCmd, wy3dApp::SketchDrawLineTangentGuiCmd, OsgGuiCommand)
public:
    SketchDrawLineTangentGuiCmd();

protected:
    GuiCmdSketchInfo _sketchInfo;
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;

protected:
    enum class Step
    {
        Undefined = 0,
        SelectStartEntity = 1,
        SelectEndEntity = 2,
    };
    virtual void reset();
    virtual void onEscapeKey() override;
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

private:
    bool createLine(const wy::Vector2& startPnt, const wy::Vector2& endPnt);

    bool findMatchedTangentLine(
        const wydb::ElementId& startElemId, const wy::Vector2& startPickPos,
        const wydb::ElementId& endElemId, const wy::Vector2& endPickPos,
        wy::Vector2& startPnt, wy::Vector2& endPnt);

private:
    Step _step;
    wydb::ElementId _startElementId;
    wy::Vector2 _startPickPos;
    wydb::ElementId _endElementId;
    wy::Vector2 _endPickPos;
    LineTransientSPtr _pLineTransient;

    // 点选选项
    PointPickOption _pointPickOption;
    // 预览
    SelectPreview _preview;
    // 高亮
    SelectionSetHighlightorSPtr _pSelSetHighlightor;
private:
    std::map<TangentLineIdPair, std::vector<TangentLine>> _cachedTangentLines;
};

}

#endif // WY3DAPP_SKETCH_DRAW_LINE_TANGENT_GUI_CMD_H
