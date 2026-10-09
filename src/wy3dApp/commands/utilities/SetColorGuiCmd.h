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

#ifndef WY3DAPP_SET_COLOR_GUI_CMD_H
#define WY3DAPP_SET_COLOR_GUI_CMD_H

#include <QColor>
#include <wy3dColor.h>

#include "commands/OsgGuiCommand.h"

class SetColorCmdPanel;

class SetColorGuiCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(SetColorGuiCmd, wy3dApp::SolidColorGuiCmd, OsgGuiCommand)
public:
    SetColorGuiCmd();
    virtual ~SetColorGuiCmd();

    void userEnd()
    {
        this->requestEnd();
    }
    void userAbort()
    {
        this->requestAbort(AbortCause::UserCancel);
    }

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;
    virtual void onEnd() override;
    virtual void onAbort(wyap::CmdExecution::AbortCause cause) override;
    virtual void cleanup() override;
    virtual void onEscapeKey() override;
    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

private:
    bool beginSessionTransaction();
    void tryCommitSessionTransaction();
    void tryAbortSessionTransaction();

    bool createPanel();
    void destroyPanel();
    wy3d::Color getTargetColor() const;
    bool applyColorToSolid(const wydb::ElementId& solidId);

private:
    enum class SessionTransStatus
    {
        Uninitialized = 0,
        Started = 1,
        Commited = 2,
        Aborted = 3,
    };
    SessionTransStatus _sessionTransStatus;

    QColor _targetColor;
    SetColorCmdPanel* _pPanel;

    PointPickOption _pointPickOption;
    wydb::ElementId _lastHoverElementId;
    SelectPreview _hoverPreview;
};

#endif // WY3DAPP_SET_COLOR_GUI_CMD_H

