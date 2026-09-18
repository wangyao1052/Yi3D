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

#include "SketchRectArrayGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <cmath>
#include <cfloat>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include "snap/SnapSystemBase.h"
#include <wyapClipboard.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>
#include <wy3dImpl.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "commands/sketch/dialogs/SketchRectArrayDialog.h"
#include "commands/transient/BasicTransient.h"
#include "scene/nodes/ElementNode.h"
#include "scene/RenderConst.h"
#include "scene/Scene.h"
#include "select/filters/CommonSelFilters.h"
#include "snap/SketchSnapSystem.h"
#include "utils/MathUtils.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}


bool SketchRectArrayGuiCmd::isValidRowsCols(unsigned int cols, unsigned int rows)
{
    if (cols == 0 || rows == 0) return false;
    if (cols == 1 && rows == 1) return false;
    size_t num = static_cast<size_t>(cols) * static_cast<size_t>(rows) - 1;
    if (num == 0 || num >= 10000)
    {
        return false;
    }

    return true;
}

SketchRectArrayGuiCmd::SketchRectArrayGuiCmd()
    : OsgGuiCommand(),
    _step(Step::Undefined),
    _ids(),
    _cols(4),
    _rows(3),
    _colSpacingStartPnt(),
    _colSpacing(100.0),
    _rowSpacingStartPnt(),
    _rowSpacing(100.0),
    _pSnapContext(),
    _pRectArray(),
    _pLineTransient(),
    _pColSpacingPopup(),
    _pRowSpacingPopup(),
    _hoverPopupState()
{
    _options.pointSelect = true;
    _options.boxSelect = true;
    _options.selectionType = wy3d::SelectionType::Element;
    _options.pickMask = static_cast<unsigned int>(ElementNodeType::SketchEntity);
    _options.filter = std::make_shared<SingleClassSelFilter>(wy3d::SketchEntity::classInfo());
    _options.preview = true;
}

SketchRectArrayGuiCmd::~SketchRectArrayGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchRectArrayGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 初始化
    _pLineTransient = std::make_shared<LineTransient>();
    _pLineTransient->hide();
    if (Application::instance().getSelManager()->getSelections().isEmpty())
    {
        this->gotoStep(Step::Step1_SelectElements);
    }
    else
    {
        this->finishStep(Step::Step1_SelectElements);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}

void SketchRectArrayGuiCmd::cleanup()
{
    this->hidePopup();

    _step = Step::Undefined;
    _ids.clear();
    _cols = 4;
    _rows = 3;
    _colSpacingStartPnt.set(0.0, 0.0);
    _colSpacing = 100.0;
    _rowSpacingStartPnt.set(0.0, 0.0);
    _rowSpacing = 100.0;
    _pSnapContext = nullptr;
    _hoverPopupState.resetValue();
    _pRectArray = nullptr;
    _pLineTransient = nullptr;
}

void SketchRectArrayGuiCmd::reset()
{
    this->cleanup();
    this->gotoStep(Step::Step1_SelectElements);
}

void SketchRectArrayGuiCmd::onEscapeKey()
{
    this->hidePopup();

    if (_step == Step::Step1_SelectElements || _step == Step::Undefined)
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else
    {
        this->reset();
        this->simulateMouseMoveFromPopup();
    }
}

bool SketchRectArrayGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::Step1_SelectElements:
    {
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (ss.isEmpty()) return false;
        _ids.clear();
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            _ids.insert(iter.current().getElementId());
        }
        if (_ids.empty()) return false;

        // 计算默认的列间距与行间距
        this->computeDefaultColRowSpacing(_ids, _colSpacing, _rowSpacing);

        // next step
        this->gotoStep(Step::Step2_SpecifyRowsCols);
        return true;
    }
    break;

    case Step::Step2_SpecifyRowsCols:
    {
        _pRectArray = nullptr;
        if (wydb::Database* pDb = Application::instance().getActiveDatabase())
        {
            _pRectArray = std::make_shared<SketchRectArrayElements>(this, _sketchInfo.sketchPlane, _sketchInfo.sketchId);
            if (!_pRectArray->init(_ids, _cols, _rows, _colSpacing, _rowSpacing))
            {
                _pRectArray = nullptr;
            }
        }
        if (!_pRectArray)
        {
            this->reset();
            return false;
        }

        // next step
        if (1 == _cols)
            this->gotoStep(Step::Step5_SpecifyRowSpacing_StartPnt); // 列数为1时跳过指定列间隙
        else
            this->gotoStep(Step::Step3_SpecifyColumnSpacing_StartPnt);
        return true;
    }
    break;

    case Step::Step3_SpecifyColumnSpacing_StartPnt:
    {
        if (_pLineTransient)
        {
            _pLineTransient->update(
                _sketchInfo.sketchPlane.value(_colSpacingStartPnt),
                _sketchInfo.sketchPlane.value(_colSpacingStartPnt));
            _pLineTransient->show();
        }

        // next step
        this->gotoStep(Step::Step4_SpecifyColumnSpacing_EndPnt);
        return true;
    }
    break;

    case Step::Step4_SpecifyColumnSpacing_EndPnt:
    {
        if (_rows == 1) // 行数为1时直接执行阵列
        {
            if (_pRectArray)
            {
                if (!_pRectArray->perform(_ids, _cols, _rows, _colSpacing, _rowSpacing))
                {
                    assert(false);
                    return false;
                }
                _pRectArray->commit();
                _pRectArray = nullptr;
            }
            if (_pLineTransient) _pLineTransient->hide();

            // next step
            //this->gotoStep(Step::Step1_SelectElements);
            this->requestEnd();
            return true;
        }
        else 
        {
            if (_pRectArray) _pRectArray->update(_colSpacing, _rowSpacing);
            if (_pLineTransient) _pLineTransient->hide();

            // next step
            this->gotoStep(Step::Step5_SpecifyRowSpacing_StartPnt);
            return true;
        }
    }
    break;

    case Step::Step5_SpecifyRowSpacing_StartPnt:
    {
        if (_pLineTransient)
        {
            _pLineTransient->update(
                _sketchInfo.sketchPlane.value(_rowSpacingStartPnt),
                _sketchInfo.sketchPlane.value(_rowSpacingStartPnt));
            _pLineTransient->show();
        }

        // next step
        this->gotoStep(Step::Step6_SpecifyRowSpacing_EndPnt);
        return true;
    }
    break;

    case Step::Step6_SpecifyRowSpacing_EndPnt:
    {
        if (_pRectArray)
        {
            if (!_pRectArray->perform(_ids, _cols, _rows, _colSpacing, _rowSpacing))
            {
                assert(false);
                return false;
            }
            _pRectArray->commit();
            _pRectArray = nullptr;
        }

        if (_pLineTransient) _pLineTransient->hide();

        // next step
        //this->gotoStep(Step::Step1_SelectElements);
        this->requestEnd();
        return true;
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }

    return false;
}

void SketchRectArrayGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();
    if (step == Step::Step4_SpecifyColumnSpacing_EndPnt)
    {
        _hoverPopupState.colSign = 1;
    }
    else if (step == Step::Step6_SpecifyRowSpacing_EndPnt)
    {
        _hoverPopupState.rowSign = 1;
    }

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();
    // 清空草图捕捉结果
    if (_sketchInfo.pSketchSnapSys)
    {
        _sketchInfo.pSketchSnapSys->clearSnapResult();
    }

    switch (step)
    {
    case Step::Step1_SelectElements:
    {
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchRectArray",
            "Select elements to perform rectangular array; press Enter or Spacebar to confirm; press Esc to cancel."));
        Application::instance().setCursor(CursorType::SelectElements);

        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = true;
        selOptions.boxSelect = true;
        selOptions.selectionType = wy3d::SelectionType::Element;
        selOptions.pickMask = static_cast<unsigned int>(ElementNodeType::SketchEntity);
        selOptions.filter = std::make_shared<SingleClassSelFilter>(wy3d::SketchEntity::classInfo());
        selOptions.preview = true;
        selOptions.selectMode = SelectMode::Incremental;
        this->configSelect(selOptions);

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
    }
    break;

    case Step::Step2_SpecifyRowsCols:
    {
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        SketchRectArrayDialog dialog(4, 3);
        if (QDialog::Accepted != dialog.exec())
        {
            this->reset();
            return;
        }
        _cols = dialog.getColumns();
        _rows = dialog.getRows();
        if (!isValidRowsCols(_cols, _rows))
        {
            assert(false);
            this->reset();
            return;
        }
        this->finishStep(_step);

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
    }
    break;

    case Step::Step3_SpecifyColumnSpacing_StartPnt:
    {
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchRectArray",
            "Specify the start point of column spacing or input the column spacing."));
        Application::instance().setCursor(CursorType::Locate);

        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::Step4_SpecifyColumnSpacing_EndPnt:
    {
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchRectArray",
            "Specify the end point of column spacing or input the column spacing."));
        Application::instance().setCursor(CursorType::Locate);

        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::Step5_SpecifyRowSpacing_StartPnt:
    {
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchRectArray",
            "Specify the start point of row spacing or input the row spacing."));
        Application::instance().setCursor(CursorType::Locate);

        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::Step6_SpecifyRowSpacing_EndPnt:
    {
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchRectArray",
            "Specify the end point of row spacing or input the row spacing."));
        Application::instance().setCursor(CursorType::Locate);

        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    default:
    {
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        Application::instance().getStatusBar()->setTips("");
        assert(false);
    }
    break;
    }
}

void SketchRectArrayGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchRectArrayGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    switch (_step)
    {
    case Step::Step1_SelectElements:
    {
    }
    break;

    case Step::Step2_SpecifyRowsCols:
    {
    }
    break;

    case Step::Step3_SpecifyColumnSpacing_StartPnt:
    {
        this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
    }
    break;

    case Step::Step4_SpecifyColumnSpacing_EndPnt:
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        double rawColSpacing = pnt.x() - _colSpacingStartPnt.x();
        if (std::fabs(rawColSpacing) > wy3d::EPS)
        {
            _hoverPopupState.colSign = (rawColSpacing >= 0.0) ? 1 : -1;
        }
        double colSpacingAbs = std::fabs(rawColSpacing);
        _hoverPopupState.value = colSpacingAbs;
        double colSpacing = _hoverPopupState.colSign * colSpacingAbs;
        {
            if (_pLineTransient) _pLineTransient->update(
                _sketchInfo.sketchPlane.value(_colSpacingStartPnt),
                _sketchInfo.sketchPlane.value(wy::Vector2(pnt.x(), _colSpacingStartPnt.y())));
            if (_pRectArray) _pRectArray->update(colSpacing, _rowSpacing);
        }
    }
    break;

    case Step::Step5_SpecifyRowSpacing_StartPnt:
    {
        this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
    }
    break;

    case Step::Step6_SpecifyRowSpacing_EndPnt:
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        double rawRowSpacing = pnt.y() - _rowSpacingStartPnt.y();
        if (std::fabs(rawRowSpacing) > wy3d::EPS)
        {
            _hoverPopupState.rowSign = (rawRowSpacing >= 0.0) ? 1 : -1;
        }
        double rowSpacingAbs = std::fabs(rawRowSpacing);
        _hoverPopupState.value = rowSpacingAbs;
        double rowSpacing = _hoverPopupState.rowSign * rowSpacingAbs;
        {
            if (_pLineTransient) _pLineTransient->update(
                _sketchInfo.sketchPlane.value(_rowSpacingStartPnt),
                _sketchInfo.sketchPlane.value(wy::Vector2(_rowSpacingStartPnt.x(), pnt.y())));
            if (_pRectArray) _pRectArray->update(_colSpacing, rowSpacing);
        }
    }
    break;

    default:
    {
    }
    break;
    }

    return;
}

void SketchRectArrayGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::Step1_SelectElements:
    break;

    case Step::Step2_SpecifyRowsCols:
    break;

    case Step::Step3_SpecifyColumnSpacing_StartPnt:
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        {
            _colSpacingStartPnt = pnt;
            this->finishStep(_step);
        }
    }
    break;

    case Step::Step4_SpecifyColumnSpacing_EndPnt:
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        double rawColSpacing = pnt.x() - _colSpacingStartPnt.x();
        if (std::fabs(rawColSpacing) > wy3d::EPS)
        {
            _hoverPopupState.colSign = (rawColSpacing >= 0.0) ? 1 : -1;
        }
        double colSpacingAbs = std::fabs(rawColSpacing);
        {
            _colSpacing = _hoverPopupState.colSign * colSpacingAbs;
            this->finishStep(_step);
        }
    }
    break;

    case Step::Step5_SpecifyRowSpacing_StartPnt:
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        {
            _rowSpacingStartPnt = pnt;
            this->finishStep(_step);
        }
    }
    break;

    case Step::Step6_SpecifyRowSpacing_EndPnt:
    {
        wy::Vector2 pnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        double rawRowSpacing = pnt.y() - _rowSpacingStartPnt.y();
        if (std::fabs(rawRowSpacing) > wy3d::EPS)
        {
            _hoverPopupState.rowSign = (rawRowSpacing >= 0.0) ? 1 : -1;
        }
        double rowSpacingAbs = std::fabs(rawRowSpacing);
        {
            _rowSpacing = _hoverPopupState.rowSign * rowSpacingAbs;
            this->finishStep(_step);
        }
    }
    break;

    default:
    {

    }
    break;
    }

    return;
}

void SketchRectArrayGuiCmd::onEnterKey()
{
    if (Step::Step1_SelectElements == _step)
    {
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (!ss.isEmpty())
        {
            this->finishStep(_step);
        }
    }
}

void SketchRectArrayGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool SketchRectArrayGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::Step1_SelectElements == _step;
}

void SketchRectArrayGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool SketchRectArrayGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::Step1_SelectElements == _step;
}

void SketchRectArrayGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::Step1_SelectElements == _step)
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }
}

void SketchRectArrayGuiCmd::initializePopups()
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (!_pColSpacingPopup)
    {
        _pColSpacingPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchRectArray", "Column Spacing"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pColSpacingPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pColSpacingPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pColSpacingPopup->hide();
    }
    if (!_pRowSpacingPopup)
    {
        _pRowSpacingPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("SketchRectArray", "Row Spacing"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRowSpacingPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRowSpacingPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRowSpacingPopup->hide();
    }
}

void SketchRectArrayGuiCmd::showPopup()
{
    if (!_pColSpacingPopup || !_pRowSpacingPopup)
    {
        this->initializePopups();
    }

    GuiCmdHoverInputPopup1* pActivePopup = this->getActivePopup();
    if (!pActivePopup)
    {
        return;
    }

    if (_step == Step::Step4_SpecifyColumnSpacing_EndPnt)
    {
        pActivePopup->setValue(_hoverPopupState.value);
    }
    else if (_step == Step::Step6_SpecifyRowSpacing_EndPnt)
    {
        pActivePopup->setValue(_hoverPopupState.value);
    }
    else
    {
        return;
    }

    pActivePopup->showAtGlobal(QCursor::pos());
}

void SketchRectArrayGuiCmd::hidePopup()
{
    if (_pColSpacingPopup && _pColSpacingPopup->isVisible())
    {
        _pColSpacingPopup->hide();
    }
    if (_pRowSpacingPopup && _pRowSpacingPopup->isVisible())
    {
        _pRowSpacingPopup->hide();
    }
}

GuiCmdHoverInputPopup1* SketchRectArrayGuiCmd::getActivePopup() const
{
    if (_step == Step::Step4_SpecifyColumnSpacing_EndPnt)
    {
        return _pColSpacingPopup.get();
    }
    if (_step == Step::Step6_SpecifyRowSpacing_EndPnt)
    {
        return _pRowSpacingPopup.get();
    }
    return nullptr;
}

void SketchRectArrayGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::Step4_SpecifyColumnSpacing_EndPnt &&
        _step != Step::Step6_SpecifyRowSpacing_EndPnt)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pColSpacingPopup && _pColSpacingPopup->isVisible()) ||
        (_pRowSpacingPopup && _pRowSpacingPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= 0.45)
    {
        this->showPopup();
    }
}

void SketchRectArrayGuiCmd::onPopupEnterKey()
{
    GuiCmdHoverInputPopup1* pActivePopup = this->getActivePopup();
    if (!pActivePopup)
    {
        return;
    }

    if (_step == Step::Step4_SpecifyColumnSpacing_EndPnt)
    {
        double colSpacing(0.0);
        if (!parseDoubleText(pActivePopup->getRowText(), colSpacing))
        {
            return;
        }
        _colSpacing = _hoverPopupState.colSign * std::fabs(colSpacing);
    }
    else if (_step == Step::Step6_SpecifyRowSpacing_EndPnt)
    {
        double rowSpacing(0.0);
        if (!parseDoubleText(pActivePopup->getRowText(), rowSpacing))
        {
            return;
        }
        _rowSpacing = _hoverPopupState.rowSign * std::fabs(rowSpacing);
    }
    else
    {
        return;
    }

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchRectArrayGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchRectArrayGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void SketchRectArrayGuiCmd::computeDefaultColRowSpacing(
    const std::set<wydb::ElementId>& ids, double& colSpacing, double& rowSpacing) const
{
    colSpacing = 50.0;
    rowSpacing = 50.0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    wy3d::geom::BoundingBox2 totalBBox;
    for (const wydb::ElementId& id : ids)
    {
        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem) continue;
        const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem);
        if (!pSketchEntity) continue;
        wy3d::geom::BoundingBox2 bbox = pSketchEntity->getBoundingBox();
        totalBBox.merge(bbox);
    }
    if (!totalBBox.isEmpty())
    {
        double x = totalBBox.width() * 1.5;
        double y = totalBBox.height() * 1.5;
        if (x < wy3d::TOL * 10)
        {
            if (y / 10 > x)
            {
                x = y / 10;
            }
        }
        if (y < wy3d::TOL * 10)
        {
            if (x / 10 > y)
            {
                y = x / 10;
            }
        }
        colSpacing = x;
        rowSpacing = y;
    }
}

