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

#include "SetColorGuiCmd.h"

#include <QCoreApplication>
#include <QOpenGLWidget>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>

#include "application/Application.h"
#include "environments/sketch/SketchEnvironment.h"
#include "commands/dialogs/SetColorCmdPanel.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/filters/CommonSelFilters.h"
#include "widgets/frame/MainWindow.h"


SetColorGuiCmd::SetColorGuiCmd()
    : OsgGuiCommand()
    , _sessionTransStatus(SessionTransStatus::Uninitialized)
    , _targetColor(168, 107, 224)
    , _pPanel(nullptr)
    , _lastHoverElementId(wydb::ElementId::kNull)
    , _pHoverPreview(nullptr)
    , _pointPickOption()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

SetColorGuiCmd::~SetColorGuiCmd()
{
}

wyap::CmdExecution::StartResult SetColorGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    wyap::Environment* pEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    if (dynamic_cast<SketchEnvironment*>(pEnv))
    {
        return wyap::CmdExecution::StartResult::Rejected;
    }

    // 开启会话级事务
    if (!this->beginSessionTransaction())
    {
        return wyap::CmdExecution::StartResult::Failed;
    }

    // 点选选项
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid | ElementNodeType::Sheet);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.pSelFilter = std::make_shared<MultiClassSelFilter>(
        std::vector<wyrx::ClassInfo*>{wy3d::Solid::classInfo(), wy3d::Sheet::classInfo()});

    // 鼠标样式
    Application::instance().setCursor(CursorType::SelectElements);

    // 创建对话框
    if (!this->createPanel())
    {
        this->tryAbortSessionTransaction();
        return wyap::CmdExecution::StartResult::Failed;
    }

    // 提示信息
    Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SetColorGuiCmd",
        "Click solid elements to apply the selected color."));

    return wyap::CmdExecution::StartResult::Succeeded;
}
void SetColorGuiCmd::onEnd()
{
    __baseClass::onEnd();

    this->tryAbortSessionTransaction();
    this->destroyPanel();
}
void SetColorGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    __baseClass::onAbort(cause);

    this->tryAbortSessionTransaction();
    this->destroyPanel();
}

void SetColorGuiCmd::cleanup()
{
    _lastHoverElementId = wydb::ElementId::kNull;
    _pHoverPreview = nullptr;
    if (_pPanel)
    {
        _pPanel->hide();
    }
}

void SetColorGuiCmd::onEscapeKey()
{
    this->requestAbort(AbortCause::UserCancel);
}

void SetColorGuiCmd::onMouseMove(const MouseEvent& event)
{
    wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);
    if (sel.getElementId() == _lastHoverElementId)
    {
        return;
    }
    _lastHoverElementId = sel.getElementId();

    this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pHoverPreview);
    return;
}

void SetColorGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    wydb::ElementId solidId = wydb::ElementId::kNull;
    if (_pHoverPreview && !_pHoverPreview->getSelection().getElementId().isNull())
    {
        solidId = _pHoverPreview->getSelection().getElementId();
    }
    else
    {
        const std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _pointPickOption);
        solidId = pickRet.first;
    }

    _pHoverPreview = nullptr;
    if (solidId.isNull())
    {
        return;
    }

    this->applyColorToSolid(solidId);
    return;
}

bool SetColorGuiCmd::beginSessionTransaction()
{
    assert(SessionTransStatus::Uninitialized == _sessionTransStatus);
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }
    wydb::Transaction* pSessionTransGroup = pDb->getTransactionManager()->startTransactionGroup();
    if (!pSessionTransGroup)
    {
        assert(false);
        return false;
    }

    _sessionTransStatus = SessionTransStatus::Started;
    return true;
}

void SetColorGuiCmd::tryCommitSessionTransaction()
{
    if (SessionTransStatus::Started != _sessionTransStatus)
    {
        return;
    }

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }
    wy::ErrorStatus error = pDb->getTransactionManager()->endTransaction();
    assert(wy::ErrorStatus::Ok == error);
    _sessionTransStatus = SessionTransStatus::Commited;
}

void SetColorGuiCmd::tryAbortSessionTransaction()
{
    if (SessionTransStatus::Started != _sessionTransStatus)
    {
        return;
    }

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }
    wy::ErrorStatus error = pDb->getTransactionManager()->abortTransaction();
    assert(wy::ErrorStatus::Ok == error);
    _sessionTransStatus = SessionTransStatus::Aborted;
}

bool SetColorGuiCmd::createPanel()
{
    assert(!_pPanel);

    QOpenGLWidget* pParentWidget = nullptr;
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (pMainWindow)
    {
        pParentWidget = pMainWindow->findChild<QOpenGLWidget*>();
    }
    if (!pParentWidget)
    {
        return false;
    }

    _pPanel = new SetColorCmdPanel(pParentWidget);
    _pPanel->setColor(_targetColor);
    QObject::connect(_pPanel, &SetColorCmdPanel::colorChanged, [this](const QColor& color)
    {
        _targetColor = color;
    });
    QObject::connect(_pPanel, &SetColorCmdPanel::accepted, [this]()
    {
        this->tryCommitSessionTransaction();
        this->userEnd(); // 退出命令
    });
    QObject::connect(_pPanel, &SetColorCmdPanel::canceled, [this]()
    {
        this->tryAbortSessionTransaction();
        this->userAbort(); // 退出命令
    });
    _pPanel->show();
    return true;
}

void SetColorGuiCmd::destroyPanel()
{
    if (_pPanel)
    {
        _pPanel->hide();
        delete _pPanel;
        _pPanel = nullptr;
    }
}

wy3d::Color SetColorGuiCmd::getTargetColor() const
{
    return wy3d::Color(
        static_cast<unsigned char>(_targetColor.red()),
        static_cast<unsigned char>(_targetColor.green()),
        static_cast<unsigned char>(_targetColor.blue()));
}

bool SetColorGuiCmd::applyColorToSolid(const wydb::ElementId& solidId)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    if (solidId.isNull())
    {
        return false;
    }
    const wydb::Element* pElem = pDb->getElement(solidId);
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
    const wy3d::Sheet* pSheet = pSolid ? nullptr : wy3d::Sheet::cast(pElem);
    if (!pSolid && !pSheet)
    {
        return false;
    }

    const wy3d::Color targetColor = this->getTargetColor();
    const wy3d::Color currentColor = pSolid ? pSolid->getColor() : pSheet->getColor();
    if (currentColor == targetColor)
    {
        return true;
    }

    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("");
    if (!pTrans)
    {
        assert(false);
        return false;
    }
    wydb::Element* pWriteElem = pTrans->getElementForWrite(solidId);
    wy3d::Solid* pSolidWrite = wy3d::Solid::cast(pWriteElem);
    wy3d::Sheet* pSheetWrite = pSolidWrite ? nullptr : wy3d::Sheet::cast(pWriteElem);
    if (!pSolidWrite && !pSheetWrite)
    {
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    const wy::ErrorStatus setColorError = pSolidWrite
        ? pSolidWrite->setColor(targetColor) : pSheetWrite->setColor(targetColor);
    if (wy::ErrorStatus::Ok != setColorError)
    {
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    const wy::ErrorStatus endError = pDb->getTransactionManager()->endTransaction();
    if (wy::ErrorStatus::Ok != endError)
    {
        assert(false);
        return false;
    }

    return true;
}
