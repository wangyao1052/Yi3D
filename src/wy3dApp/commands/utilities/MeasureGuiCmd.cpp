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

#include "commands/utilities/MeasureGuiCmd.h"

#include <QAction>
#include <QOpenGLWidget>

#include <Standard_Failure.hxx>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>

#include "application/Application.h"
#include "scene/Colors.h"
#include "scene/nodes/ElementNodeType.h"
#include "select/filters/CommonSelFilters.h"
#include "snap/SketchSnapSystem.h"
#include "utils/MeasureUtil.h"
#include "widgets/frame/MainWindow.h"

namespace
{
// 防御:subPath非纯数字时返回false,避免stoul抛异常直接terminate
bool parseSubPathIndex(const std::string& subPath, unsigned int& index)
{
    if (subPath.empty()) return false;

    for (char c : subPath)
    {
        if (c < '0' || c > '9') return false;
    }

    index = std::stoul(subPath);
    return true;
}
}


MeasureGuiCmd::MeasureGuiCmd() : OsgGuiCommand(), _step(Step::Undefined), _pMeasureGuiCmdCtrls(nullptr),
    _mode(MeasureMode::PointToPoint), _tempUnhighlightedSel(wydb::ElementId::kNull), _pCmdPanel(nullptr)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

wyap::CmdExecution::StartResult MeasureGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 命令控件
    _pMeasureGuiCmdCtrls = dynamic_cast<MeasureGuiCmdControls*>(_pControls.get());
    assert(_pMeasureGuiCmdCtrls);

    // 测量面板
    if (!this->createCmdPanel())
    {
        this->requestAbort(AbortCause::ErrorTerminate);
        return wyap::CmdExecution::StartResult::Failed;
    }

    // 初始化(默认点到点)
    this->gotoStep(Step::SpecifyStartPnt);

    // 鼠标样式
    Application::instance().setCursor(CursorType::Locate);

    return wyap::CmdExecution::StartResult::Succeeded;
}
void MeasureGuiCmd::onEnd()
{
    __baseClass::onEnd();

    this->destroyCmdPanel();
    _pLineTransient = nullptr;
    _pPreview = nullptr;
    _pRowPreview = nullptr;
    _pRowFacePreviewHighlightor = nullptr;
    _pMeasuredHighlightor = nullptr;
    _measuredSet.clear();
    _tempUnhighlightedSel = wyap::Selection(wydb::ElementId::kNull);
    _pMeasureLines.clear();
    _pP2pHoverLine = nullptr;
}
void MeasureGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    __baseClass::onAbort(cause);

    this->destroyCmdPanel();
    _pLineTransient = nullptr;
    _pPreview = nullptr;
    _pRowPreview = nullptr;
    _pRowFacePreviewHighlightor = nullptr;
    _pMeasuredHighlightor = nullptr;
    _measuredSet.clear();
    _tempUnhighlightedSel = wyap::Selection(wydb::ElementId::kNull);
    _pMeasureLines.clear();
    _pP2pHoverLine = nullptr;
}

void MeasureGuiCmd::reset()
{
    _step = Step::Undefined;
    _startPnt.set(0.0, 0.0, 0.0);
    _startPnt2d.set(0.0, 0.0);
    _endPnt.set(0.0, 0.0, 0.0);
    _endPnt2d.set(0.0, 0.0);
    _pSketchSnapContext = nullptr;
    _pLineTransient = nullptr;

    if (_pMeasureGuiCmdCtrls)
    {
        _pMeasureGuiCmdCtrls->hideLength();
        _pMeasureGuiCmdCtrls->setLength(0.0);
    }

    this->gotoStep(Step::SpecifyStartPnt);
}

void MeasureGuiCmd::onEscapeKey()
{
    if (MeasureMode::PointToPoint != _mode)
    {
        this->requestAbort(AbortCause::UserCancel);
        return;
    }

    if (_step == Step::SpecifyStartPnt || _step == Step::Undefined)
    {
        this->requestAbort(AbortCause::UserCancel);
    }
    else
    {
        this->reset();
    }
}

bool MeasureGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyStartPnt:
    {
        // 显示直线段
        _pLineTransient = std::make_shared<LineTransient>(nullptr, new osg::LineWidth(3.0f), Colors::kEdge_Highlight);
        _pLineTransient->update(_startPnt, _startPnt);

        // 下一步
        this->gotoStep(Step::SpecifyEndPnt);
        return true;
    }
    break;

    case Step::SpecifyEndPnt:
    {
        // 刷新直线段
        if (_pLineTransient)
        {
            _pLineTransient->update(_startPnt, _endPnt);
        }

        // 持久测量线(绿色),与结果行同生命周期
        {
            LineTransientSPtr pLine = std::make_shared<LineTransient>(nullptr, new osg::LineWidth(3.0f), Colors::kEdge_Highlight);
            if (_sketchInfo.pSketchSnapSys)
                pLine->update(_sketchInfo.sketchPlane, _startPnt2d, _endPnt2d);
            else
                pLine->update(_startPnt, _endPnt);
            _pMeasureLines.append(pLine);
        }

        // 销毁直线段
        _pLineTransient = nullptr;

        // 追加结果到面板
        if (_pCmdPanel)
        {
            double length = 0.0;
            if (_sketchInfo.pSketchSnapSys)
                length = (_endPnt2d - _startPnt2d).length();
            else
                length = (_endPnt - _startPnt).length();

            QVector<MeasureValue> values;
            values.append({ MeasureValueKind::Distance, length });
            _pCmdPanel->addResult(wyap::Selection(wydb::ElementId::kNull), values);
        }

        // 循环执行第一步
        this->gotoStep(Step::SpecifyStartPnt);
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

void MeasureGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SpecifyStartPnt:
    {
        // 隐藏长度标签控件
        if (_pMeasureGuiCmdCtrls)
        {
            _pMeasureGuiCmdCtrls->hideLength();
        }

        // 禁用文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MeasureGuiCmd",
            "Specify the start point."));

        // 草图捕捉系统
        if (_sketchInfo.pSketchSnapSys)
        {
            if (_sketchInfo.pSketchSnapSys)
            {
                _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
            }
            _pSketchSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
        }
    }
    break;

    case Step::SpecifyEndPnt:
    {
        // 显示长度标签控件
        if (_pMeasureGuiCmdCtrls)
        {
            _pMeasureGuiCmdCtrls->setLength(0.0);
            _pMeasureGuiCmdCtrls->showLength();
        }

        // 禁用文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MeasureGuiCmd",
            "Specify the end point."));

        // 草图捕捉系统
        if (_sketchInfo.pSketchSnapSys)
        {
            SketchDrawLineContextSPtr pDrawLineContext = std::make_shared<SketchDrawLineContext>(
                wydb::ElementId::kNull, _startPnt2d);
            _pSketchSnapContext = pDrawLineContext;
        }
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        assert(false);
    }
    break;
    }
}

void MeasureGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (MeasureMode::PointToPoint != _mode)
    {
        // 悬停仅预览高亮,不计算数值,点击后计算
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        // 鼠标回到视口:行悬停预览让位(互斥)
        if (_pRowPreview || _pRowFacePreviewHighlightor)
        {
            _pRowPreview = nullptr;
            _pRowFacePreviewHighlightor = nullptr;
            this->restoreTempUnhighlightedSel();
            this->refreshAccumulatedHighlight();
        }

        if (_pCmdPanel)
        {
            if (_pPreview)
                _pCmdPanel->setHoveredRowBySel(_pPreview->getSelection());
            else
                _pCmdPanel->setHoveredRowBySel(wyap::Selection(wydb::ElementId::kNull));
        }
        return;
    }

    switch (_step)
    {
    case Step::SpecifyStartPnt:
    {
        if (_sketchInfo.pSketchSnapSys)
        {
            wy::Vector2 pnt2d = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSketchSnapContext, _sketchInfo.pSketchSnapSys);
        }
        else
        {
            auto ret = this->computePosition3d(event.x, event.y, _workPln, _snapExcludeIds);
            wy::Vector3 pnt3d = ret.first;
            if (ret.second)
            {
                pnt3d = ret.second->getPosition();
            }
        }
        return;
    }
    break;

    case Step::SpecifyEndPnt:
    {
        if (_sketchInfo.pSketchSnapSys)
        {
            wy::Vector2 endPnt2d = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSketchSnapContext, _sketchInfo.pSketchSnapSys);
            if (_pLineTransient) _pLineTransient->update(_sketchInfo.sketchPlane, _startPnt2d, endPnt2d);
            if (_pMeasureGuiCmdCtrls) _pMeasureGuiCmdCtrls->setLength((endPnt2d - _startPnt2d).length());
        }
        else
        {
            auto ret = this->computePosition3d(event.x, event.y, wy3d::SketchPlane(), _snapExcludeIds);
            wy::Vector3 endPnt = ret.first;
            if (ret.second)
            {
                endPnt = ret.second->getPosition();
            }
            if (_pLineTransient) _pLineTransient->update(_startPnt, endPnt);
            if (_pMeasureGuiCmdCtrls) _pMeasureGuiCmdCtrls->setLength((endPnt - _startPnt).length());
        }
        return;
    }
    break;

    default:
    {
        assert(false);
        return;
    }
    break;
    }

    return;
}

void MeasureGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    if (MeasureMode::PointToPoint != _mode)
    {
        this->commitMeasure(event);
        return;
    }

    switch (_step)
    {
    case Step::SpecifyStartPnt:
    {
        if (_sketchInfo.pSketchSnapSys)
        {
            _startPnt2d = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSketchSnapContext, _sketchInfo.pSketchSnapSys);
        }
        else
        {
            auto ret = this->computePosition3d(event.x, event.y, wy3d::SketchPlane(), _snapExcludeIds);
            _startPnt = ret.first;
            if (ret.second)
            {
                _startPnt = ret.second->getPosition();
            }
        }
        this->finishStep(_step);
        return;
    }
    break;

    case Step::SpecifyEndPnt:
    {
        if (_sketchInfo.pSketchSnapSys)
        {
            _endPnt2d = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSketchSnapContext, _sketchInfo.pSketchSnapSys);
        }
        else
        {
            auto ret = this->computePosition3d(event.x, event.y, wy3d::SketchPlane(), _snapExcludeIds);
            _endPnt = ret.first;
            if (ret.second)
            {
                _endPnt = ret.second->getPosition();
            }
        }
        this->finishStep(_step);
        return;
    }
    break;

    default:
    {
        assert(false);
        return;
    }
    break;
    }

    return;
}

GuiCmdControlsSPtr MeasureGuiCmd::initControls()
{
    return std::make_shared<MeasureGuiCmdControls>();
}

void MeasureGuiCmd::setMode(MeasureMode mode)
{
    if (_mode == mode) return;

    _mode = mode;
    if (_pCmdPanel) _pCmdPanel->setMode(mode);

    // 清理当前模式状态(切换即清空)
    _pPreview = nullptr;
    _pRowPreview = nullptr;
    _pRowFacePreviewHighlightor = nullptr;
    _pLineTransient = nullptr;
    _pMeasuredHighlightor = nullptr;
    _measuredSet.clear();
    _tempUnhighlightedSel = wyap::Selection(wydb::ElementId::kNull);
    _pMeasureLines.clear();
    _pP2pHoverLine = nullptr;
    if (_pCmdPanel) _pCmdPanel->clearResults();

    if (MeasureMode::PointToPoint == mode)
    {
        this->reset();

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    else
    {
        this->applyPickOptionForMode();

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 提示信息
        const char* tip = QT_TR_NOOP("Select an edge.");
        if (MeasureMode::Face == mode) tip = QT_TR_NOOP("Select a face.");
        else if (MeasureMode::Body == mode) tip = QT_TR_NOOP("Select a body.");
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MeasureGuiCmd", tip));
    }
}

void MeasureGuiCmd::cycleMode()
{
    this->setMode(static_cast<MeasureMode>((static_cast<int>(_mode) + 1) % kMeasureModeCount));
}

void MeasureGuiCmd::applyPickOptionForMode()
{
    _pointPickOption.pSelPreFilter = nullptr;
    _pointPickOption.pSelFilter = nullptr;

    switch (_mode)
    {
    case MeasureMode::Edge:
    {
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid)
            | static_cast<unsigned int>(ElementNodeType::Sheet);
        _pointPickOption.selType = wy3d::SelectionType::SolidEdge;
        _pointPickOption.acceptElement = false;
    }
    break;

    case MeasureMode::Face:
    {
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid)
            | static_cast<unsigned int>(ElementNodeType::Sheet);
        _pointPickOption.selType = wy3d::SelectionType::SolidFace;
        _pointPickOption.acceptElement = false;
    }
    break;

    case MeasureMode::Body:
    {
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
        _pointPickOption.selType = wy3d::SelectionType::SolidBody;
        _pointPickOption.acceptElement = true;
        // 仅实体有体积,过滤掉其他元素类
        _pointPickOption.pSelPreFilter = std::make_shared<CommonPreSelFilterForPointPick>(
            wy3d::Solid::classInfo());
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }
}

void MeasureGuiCmd::commitMeasure(const MouseEvent& event)
{
    const wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);
    if (sel.getElementId().isNull()) return;

    if (_pCmdPanel && _pCmdPanel->hasResult(sel))
    {
        // 已测过:移除(高亮+结果行+累计同步扣减)
        _measuredSet.remove(sel);
        _pCmdPanel->removeResult(sel);
        this->refreshAccumulatedHighlight();
    }
    else
    {
        // 未测过:新增
        const QVector<MeasureValue> values = this->measureSelectionValues(sel);
        if (values.isEmpty()) return;

        if (_pCmdPanel) _pCmdPanel->addResult(sel, values);
        _measuredSet.add(sel);
        this->refreshAccumulatedHighlight();
    }
}

QVector<MeasureValue> MeasureGuiCmd::measureSelectionValues(const wyap::Selection& sel)
{
    QVector<MeasureValue> values;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return values;

    const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(sel.getElementId()));
    const wy3d::Sheet* pSheet = nullptr;
    if (!pSolid) pSheet = wy3d::Sheet::cast(pDb->getElement(sel.getElementId()));
    if (!pSolid && !pSheet) return values;
    const TopoDS_Shape& shape = pSolid ? pSolid->getShape() : pSheet->getShape();

    try
    {
        switch (wy3d::UIntToSelectionType(sel.getSelectionType()))
        {
        case wy3d::SelectionType::SolidEdge:
        {
            const std::string& subPath = sel.getSubPath();
            unsigned int edgeIndex = 0;
            if (!parseSubPathIndex(subPath, edgeIndex)) return values;
            const std::pair<bool, double> result = MeasureUtil::edgeLength(shape, edgeIndex);
            if (result.first)
                values.append({ MeasureValueKind::Length, result.second });
        }
        break;

        case wy3d::SelectionType::SolidFace:
        {
            const std::string& subPath = sel.getSubPath();
            unsigned int faceIndex = 0;
            if (!parseSubPathIndex(subPath, faceIndex)) return values;

            const std::pair<bool, double> areaResult = MeasureUtil::faceArea(shape, faceIndex);
            if (areaResult.first)
                values.append({ MeasureValueKind::Area, areaResult.second });
            const std::pair<bool, double> perimeterResult = MeasureUtil::facePerimeter(shape, faceIndex);
            if (perimeterResult.first)
                values.append({ MeasureValueKind::Perimeter, perimeterResult.second });
        }
        break;

        case wy3d::SelectionType::Element:
        {
            // 体模式:前置过滤器已确保是实体
            if (!pSolid) return values;
            const std::pair<bool, double> volumeResult = MeasureUtil::bodyVolume(shape);
            if (volumeResult.first)
                values.append({ MeasureValueKind::Volume, volumeResult.second });
            const std::pair<bool, double> areaResult = MeasureUtil::bodySurfaceArea(shape);
            if (areaResult.first)
                values.append({ MeasureValueKind::SurfaceArea, areaResult.second });
        }
        break;

        default:
        {
            assert(false);
        }
        break;
        }
    }
    catch (const Standard_Failure&)
    {
        // 病态几何上的OCCT异常按测量失败处理,不追加结果行
        return values;
    }

    return values;
}

void MeasureGuiCmd::refreshAccumulatedHighlight()
{
    // 先销毁旧高亮器(清除旧高亮),再应用新集合,避免旧析构覆盖新颜色
    _pMeasuredHighlightor = nullptr;
    _pMeasuredHighlightor = std::make_shared<SelectionSetHighlightor>(_measuredSet);
}

void MeasureGuiCmd::onResultHovered(const wyap::Selection& sel)
{
    // 行悬停预览与视口悬停预览互斥(共用节点预览态)
    _pPreview = nullptr;
    _pRowPreview = nullptr;
    _pRowFacePreviewHighlightor = nullptr;
    this->restoreTempUnhighlightedSel();
    this->refreshAccumulatedHighlight();
    if (sel.getElementId().isNull()) return;

    if (wy3d::SelectionType::SolidFace == wy3d::UIntToSelectionType(sel.getSelectionType()))
    {
        // 面的高亮态会挡住previewFace(节点守卫),改用自定义色高亮器直接覆写预览色
        wyap::SelectionSet previewSet;
        previewSet.add(sel);
        _pRowFacePreviewHighlightor = std::make_shared<SelectionSetHighlightor>(previewSet, Colors::kSolidFace_Preview);
    }
    else
    {
        // 边/体的高亮态会挡住预览(节点守卫),临时移出测量集让预览生效,离行恢复
        if (_measuredSet.remove(sel))
        {
            _tempUnhighlightedSel = sel;
            this->refreshAccumulatedHighlight();
        }
        _pRowPreview = std::make_shared<SelectPreview>(sel);
    }
}

void MeasureGuiCmd::restoreTempUnhighlightedSel()
{
    if (_tempUnhighlightedSel.getElementId().isNull()) return;

    _measuredSet.add(_tempUnhighlightedSel);
    _tempUnhighlightedSel = wyap::Selection(wydb::ElementId::kNull);
}

void MeasureGuiCmd::onResultRowHovered(int row)
{
    // 恢复上一条被预览的测量线
    if (_pP2pHoverLine)
    {
        _pP2pHoverLine->setColor(Colors::kEdge_Highlight);
        _pP2pHoverLine = nullptr;
    }

    if (MeasureMode::PointToPoint != _mode) return;
    if (row < 0 || row >= _pMeasureLines.size()) return;

    _pP2pHoverLine = _pMeasureLines.at(row);
    _pP2pHoverLine->setColor(Colors::kEdge_Preview);
}

void MeasureGuiCmd::clearResults()
{
    // 清除全部:几何高亮+结果行
    _pMeasuredHighlightor = nullptr;
    _pRowPreview = nullptr;
    _pRowFacePreviewHighlightor = nullptr;
    _measuredSet.clear();
    _tempUnhighlightedSel = wyap::Selection(wydb::ElementId::kNull);
    _pMeasureLines.clear();
    _pP2pHoverLine = nullptr;
    if (_pCmdPanel) _pCmdPanel->clearResults();
}

GuiCmdMenu* MeasureGuiCmd::initContextMenu()
{
    return new MeasureGuiCmdMenu(this);
}

bool MeasureGuiCmdMenu::initCustomHeaderActions(QMenu* menu)
{
    assert(menu);
    assert(_pCmd);

    // 清空测量结果
    QAction* pActionClearResults = new QAction(tr("Clear Measure Results"), menu);
    menu->addAction(pActionClearResults);
    this->connect(pActionClearResults, &QAction::triggered, this, &MeasureGuiCmdMenu::onClearMeasureResults);
    return true;
}

void MeasureGuiCmdMenu::onClearMeasureResults()
{
    MeasureGuiCmd* pCmd = dynamic_cast<MeasureGuiCmd*>(_pCmd);
    if (pCmd) pCmd->clearResults();
}

bool MeasureGuiCmd::createCmdPanel()
{
    QOpenGLWidget* pParentWidget = nullptr;
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (pMainWindow)
        pParentWidget = pMainWindow->findChild<QOpenGLWidget*>();
    if (!pParentWidget) return false;

    _pCmdPanel = new MeasureCmdPanel(pParentWidget);

    QObject::connect(_pCmdPanel, &MeasureCmdPanel::modeChanged,
        _pCmdPanel, [this](MeasureMode mode) { this->setMode(mode); });
    QObject::connect(_pCmdPanel, &MeasureCmdPanel::tabPressed,
        _pCmdPanel, [this]() { this->cycleMode(); });
    QObject::connect(_pCmdPanel, &MeasureCmdPanel::escapePressed,
        _pCmdPanel, [this]() { this->onEscapeKey(); });
    QObject::connect(_pCmdPanel, &MeasureCmdPanel::resultHovered,
        _pCmdPanel, [this](const wyap::Selection& sel) { this->onResultHovered(sel); });
    QObject::connect(_pCmdPanel, &MeasureCmdPanel::resultRowHovered,
        _pCmdPanel, [this](int row) { this->onResultRowHovered(row); });
    QObject::connect(_pCmdPanel, &MeasureCmdPanel::clearRequested,
        _pCmdPanel, [this]() { this->clearResults(); });

    _pCmdPanel->show();
    return true;
}

void MeasureGuiCmd::destroyCmdPanel()
{
    if (_pCmdPanel)
    {
        _pCmdPanel->hide();
        delete _pCmdPanel;
        _pCmdPanel = nullptr;
    }
}

MeasureGuiCmdControls::MeasureGuiCmdControls() : GuiCmdControls(), _pLengthLabel(nullptr)
{
    _pLengthLabel = this->newLabel();
    _pLengthLabel->setText("");
    _pLengthLabel->hide();
}

MeasureGuiCmdControls::~MeasureGuiCmdControls()
{
}

void MeasureGuiCmdControls::showLength()
{
    _pLengthLabel->show();
}

void MeasureGuiCmdControls::hideLength()
{
    _pLengthLabel->hide();
}

void MeasureGuiCmdControls::setLength(double length)
{
    _pLengthLabel->setText(QString::number(length, 'f', 2));
}

void MeasureGuiCmdControls::timerEvent(QTimerEvent* event)
{
    static QPoint delta(12, -12);
    QPoint newPosLocal = _pLengthLabel->parentWidget()->mapFromGlobal(
        QCursor::pos() + delta - QPoint(0, _pLengthLabel->size().height()));
    if (newPosLocal != _pLengthLabel->pos())
    {
        _pLengthLabel->move(newPosLocal);
    }
}
