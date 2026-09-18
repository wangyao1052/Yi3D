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

#include "commands/edit/SketchScaleGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <cmath>
#include <list>
#include <set>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapEnvironment.h>
#include <wyapEnvManager.h>
#include <wyapClipboard.h>
#include <wy3dPrimitive.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/Colors.h"
#include "view/OsgView.h"
#include "commands/OsgGuiEventDispatcher.h"
#include "scene/nodes/ElementNode.h"
#include "environments/sketch/SketchEnvironment.h"
#include "snap/SketchSnapSystem.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "commands/transient/BasicTransient.h"
#include "utils/MathUtils.h"
#include "common/osg/OsgUtils.h"
#include "select/filters/CommonSelFilters.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

constexpr double kHoverPopupDelaySeconds = 0.45;


SketchScaleGuiCmd::SketchScaleGuiCmd()
    : OsgGuiCommand()
    , _step(Step::Undefined)
    , _sels()
    , _baseLength(10.0)
    , _basePnt()
    , _scale(1.0)
    , _pSnapContext(nullptr)
    , _pScaleElements(nullptr)
    , _pLineTransient(nullptr)
    , _pXYPopup(nullptr)
    , _pScalePopup(nullptr)
    , _hoverPopupState()
{
    _options.pointSelect = true;
    _options.boxSelect = true;
}

SketchScaleGuiCmd::~SketchScaleGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchScaleGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    _sketchInfo = GuiCommandUtil::initSketchInfo();

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
void SketchScaleGuiCmd::onEnd()
{
    this->hidePopup();

    // 取消缩放
    _pScaleElements = nullptr;

    __baseClass::onEnd();

}
void SketchScaleGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    this->hidePopup();

    // 取消缩放
    _pScaleElements = nullptr;

    __baseClass::onAbort(cause);

}

void SketchScaleGuiCmd::onEscapeKey()
{
    this->hidePopup();
    __baseClass::onEscapeKey();
}

bool SketchScaleGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::Step1_SelectElements:
    {
        // 选择集
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (ss.isEmpty())
        {
            assert(false);
            return false;
        }
        _sels = ss;

        // 计算选择的元素计算基准长度
        _baseLength = this->computeBaseLength(_sels);
        assert(_baseLength > 0.0001);

        // 隐藏缩放参照线
        if (_pLineTransient) _pLineTransient->hide();

        // 下一步
        this->gotoStep(Step::Step2_SpecifyBasePnt);
        return true;
    }
    break;

    case Step::Step2_SpecifyBasePnt:
    {
        // 缩放元素操作对象
        _pScaleElements = std::make_shared<ScaleElements>(this);
        if (!_pScaleElements->init(_sels))
        {
            assert(false);
            _pScaleElements = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        // 显示缩放参照线
        if (_pLineTransient)
        {
            _pLineTransient->update(_sketchInfo.sketchPlane, _basePnt, _basePnt);
            _pLineTransient->show();
        }

        // 下一步
        this->gotoStep(Step::Step3_SpecifyScaleRatio);
        return true;
    }
    break;

    case Step::Step3_SpecifyScaleRatio:
    {
        if (_pScaleElements)
        {
            if (!_pScaleElements->perform(_sels, _sketchInfo.sketchId, _basePnt, _scale))
            {
                assert(false);
                _pScaleElements = nullptr;
                this->requestAbort(AbortCause::ErrorTerminate);
                return false;
            }
            _pScaleElements->commit();
            _pScaleElements = nullptr;
        }

        // 隐藏缩放参照线
        if (_pLineTransient) _pLineTransient->hide();

        // 清空捕捉结果
        Application::instance().getSnapSystem()->clearSnapResult();

        // 结束命令
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

void SketchScaleGuiCmd::gotoStep(Step step)
{
    _step = step;
    this->hidePopup();
    _hoverPopupState.resetValue();

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
        // 选择配置项
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = true;
        selOptions.boxSelect = true;
        selOptions.selectionType = wy3d::SelectionType::Element;
        selOptions.pickMask = static_cast<unsigned int>(ElementNodeType::SketchEntity);
        selOptions.filter = std::make_shared<SingleClassSelFilter>(wy3d::SketchEntity::classInfo());
        selOptions.preview = true;
        selOptions.selectMode = SelectMode::Incremental;
        this->configSelect(selOptions);

        // 禁用文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Scale",
            "Select elements to scale; press Enter or Spacebar to confirm; press Esc to cancel."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::Step2_SpecifyBasePnt:
    {
        // 选择配置项:禁用选择
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Scale",
            "Specify the base point; you can directly input the coordinate values."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 草图捕捉系统
        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::Step3_SpecifyScaleRatio:
    {
        // 选择配置项:禁用选择
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        // 允许文本输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("Scale",
            "Specify the scale ratio."));
        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);

        // 草图捕捉系统
        _pSnapContext = std::make_shared<SketchDrawLineContext>(wydb::ElementId::kNull, _basePnt);
    }
    break;

    default:
    {
        // 选择配置项:禁用选择
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        // 禁用文本输入
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
}

void SketchScaleGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void SketchScaleGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (event.x != _hoverPopupState.lastMouseX ||
        event.y != _hoverPopupState.lastMouseY)
    {
        this->hidePopup();
        _hoverPopupState.lastMouseX = event.x;
        _hoverPopupState.lastMouseY = event.y;
        _hoverPopupState.lastMouseMoveTime = event.time;
    }

    if (_step == Step::Step1_SelectElements)
    {
    }
    else if (_step == Step::Step2_SpecifyBasePnt)
    {
        wy::Vector2 basePnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _hoverPopupState.point = basePnt;
        return;
    }
    else if (_step == Step::Step3_SpecifyScaleRatio)
    {
        wy::Vector2 refPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        double len = (refPnt - _basePnt).length();
        double scale = this->computeScaleRatio(len, _baseLength);
        _hoverPopupState.scale = scale;
        {
            wy::Vector3 startPnt3d = _sketchInfo.sketchPlane.value(_basePnt);
            wy::Vector3 endPnt3d = _sketchInfo.sketchPlane.value(refPnt);
            if (_pLineTransient) _pLineTransient->update(startPnt3d, endPnt3d);
            if (_pScaleElements) _pScaleElements->update(_sketchInfo.sketchPlane, _basePnt, scale);
            return;
        }
    }
    else
    {
    }

    return;
}

void SketchScaleGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;


    if (_step == Step::Step1_SelectElements)
    {
    }
    else if (_step == Step::Step2_SpecifyBasePnt)
    {
        _basePnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    else if (_step == Step::Step3_SpecifyScaleRatio)
    {
        wy::Vector2 refPnt = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, _snapExcludeIds, _pSnapContext, _sketchInfo.pSketchSnapSys);
        double len = (refPnt - _basePnt).length();
        _scale = this->computeScaleRatio(len, _baseLength);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    else
    {
    }

    return;
}

void SketchScaleGuiCmd::onEnterKey()
{
    if (Step::Step1_SelectElements == _step)
    {
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (!ss.isEmpty())
        {
            if (this->finishStep(_step))
            {
                this->simulateMouseMoveFromPopup();
            }
        }
    }
}

void SketchScaleGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool SketchScaleGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::Step1_SelectElements == _step;
}

void SketchScaleGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool SketchScaleGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::Step1_SelectElements == _step;
}

void SketchScaleGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::Step1_SelectElements == _step)
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }
}

void SketchScaleGuiCmd::initializePopups()
{
    MainWindow* pMainWindow = Application::instance().getMainWindow();
    if (!_pXYPopup)
    {
        _pXYPopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QStringLiteral("X"),
            QStringLiteral("Y"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pXYPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pXYPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pXYPopup->hide();
    }
    if (!_pScalePopup)
    {
        _pScalePopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("Scale", "Scale"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pScalePopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pScalePopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pScalePopup->hide();
    }
}

void SketchScaleGuiCmd::showPopup()
{
    if (_step != Step::Step2_SpecifyBasePnt && _step != Step::Step3_SpecifyScaleRatio)
    {
        return;
    }
    if (!_pXYPopup || !_pScalePopup)
    {
        this->initializePopups();
    }

    if (_step == Step::Step2_SpecifyBasePnt)
    {
        if (!_pXYPopup) return;
        _pXYPopup->setValues(_hoverPopupState.point.x(), _hoverPopupState.point.y());
        _pXYPopup->showAtGlobal(QCursor::pos());
    }
    else
    {
        if (!_pScalePopup) return;
        _pScalePopup->setValue(_hoverPopupState.scale);
        _pScalePopup->showAtGlobal(QCursor::pos());
    }
}

void SketchScaleGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pScalePopup && _pScalePopup->isVisible())
    {
        _pScalePopup->hide();
    }
}

void SketchScaleGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::Step2_SpecifyBasePnt && _step != Step::Step3_SpecifyScaleRatio)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pScalePopup && _pScalePopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void SketchScaleGuiCmd::onPopupEnterKey()
{
    if (_step != Step::Step2_SpecifyBasePnt && _step != Step::Step3_SpecifyScaleRatio)
    {
        return;
    }

    if (_step == Step::Step2_SpecifyBasePnt)
    {
        if (!_pXYPopup) return;
        double x(0.0), y(0.0);
        if (!parseDoubleText(_pXYPopup->getRow1Text(), x) ||
            !parseDoubleText(_pXYPopup->getRow2Text(), y))
        {
            return;
        }
        _basePnt.set(x, y);
    }
    else
    {
        if (!_pScalePopup) return;
        double scale(1.0);
        if (!parseDoubleText(_pScalePopup->getRowText(), scale))
        {
            return;
        }
        _scale = scale;
    }

    if (this->finishStep(_step))
    {
        this->simulateMouseMoveFromPopup();
    }
}

void SketchScaleGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void SketchScaleGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

double SketchScaleGuiCmd::computeBaseLength(const wyap::SelectionSet& sels) const
{
    double baseLen = 1.0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return baseLen;
    }

    wy3d::geom::BoundingBox2 totalBBox;
    for (auto iter = sels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current().getElementId();
        const wy3d::SketchEntity* pSketchEnt = wy3d::SketchEntity::cast(pDb->getElement(id));
        if (!pSketchEnt)
        {
            assert(false);
            continue;
        }
        wy3d::geom::BoundingBox2 bbox = pSketchEnt->getBoundingBox();
        totalBBox.merge(bbox);
    }
    if (totalBBox.isEmpty())
    {
        assert(false);
        return baseLen;
    }

    double width = totalBBox.width();
    double height = totalBBox.height();
    baseLen = width > height ? width / 2 : height / 2;
    return baseLen;
}

double SketchScaleGuiCmd::computeScaleRatio(double len, double baseLen) const
{
    if (baseLen < wy3d::kMinValue) // 0.001
    {
        return 1.0;
    }
    double scale = std::fabs(len) / baseLen;

    // 范围:0.001~1000
    if (scale < wy3d::kMinValue)
    {
        return wy3d::kMinValue;
    }
    else if (scale > 1000.0)
    {
        return 1000.0;
    }
    else
    {
        return scale;
    }
}
