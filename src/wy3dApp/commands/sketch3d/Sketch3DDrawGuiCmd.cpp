///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "Sketch3DDrawGuiCmd.h"

#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QCoreApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QColor>
#include <QLabel>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>

#include <wyVector3.h>
#include <wyapSelManager.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "snap/SnapObject.h"
#include "view/BaseView.h"
#include "utils/GuiCommandUtil.h"
#include "widgets/frame/MainWindow.h"

class Sketch3DPlaneLabel : public QLabel
{
public:
    explicit Sketch3DPlaneLabel(QWidget* pParent)
        : QLabel(pParent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowTransparentForInput)
    {
        this->setAttribute(Qt::WA_ShowWithoutActivating, true);
        this->setAttribute(Qt::WA_TranslucentBackground, true); // 圆角外区域真正透明
        this->setFocusPolicy(Qt::NoFocus);
        QFont font = this->font();
        font.setPixelSize(24);
        font.setBold(true);
        this->setFont(font);

        if (pParent)
        {
            pParent->installEventFilter(this);
        }

        this->setPlaneName(QStringLiteral("XY"));
        this->show();
    }

    virtual ~Sketch3DPlaneLabel()
    {
        if (QWidget* pParent = this->parentWidget())
        {
            pParent->removeEventFilter(this);
        }
    }

    // 更新平面名(重算尺寸并贴左下角)
    void setPlaneName(const QString& name)
    {
        // 平面名->暗色:XY红 / YZ绿 / ZX蓝
        if (name == QStringLiteral("XY"))
        {
            _color = QColor(0xB3, 0x00, 0x00);
        }
        else if (name == QStringLiteral("YZ"))
        {
            _color = QColor(0x00, 0x64, 0x00);
        }
        else if (name == QStringLiteral("ZX"))
        {
            _color = QColor(0x00, 0x00, 0x8B);
        }
        else
        {
            _color = QColor(0x55, 0x55, 0x55);
        }

        this->setText(name);
        this->update();
        this->anchorToBottomLeft();
    }

protected:
    virtual bool eventFilter(QObject* pWatched, QEvent* pEvent) override
    {
        if (pWatched == this->parentWidget() && pEvent)
        {
            if (QEvent::Move == pEvent->type() || QEvent::Resize == pEvent->type())
            {
                this->anchorToBottomLeft();
            }
        }
        return QLabel::eventFilter(pWatched, pEvent);
    }

    virtual void paintEvent(QPaintEvent* pEvent) override
    {
        // 白底圆角 + 深灰描边,使其明显浮于场景之上
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(QColor(0x4a, 0x4a, 0x4a), 2.0));
        painter.setBrush(QColor(0xff, 0xff, 0xff));
        painter.drawRoundedRect(box, 5.0, 5.0);

        painter.setPen(_color);
        painter.drawText(QRectF(this->rect()).adjusted(10.0, 0.0, -10.0, 0.0), Qt::AlignCenter, this->text());
    }

private:
    void anchorToBottomLeft()
    {
        QWidget* pParent = this->parentWidget();
        if (!pParent)
        {
            return;
        }

        const int margin = 12;
        QFontMetrics fm(this->font());
        this->resize(fm.horizontalAdvance(this->text()) + 24, fm.height() + 12);
        this->move(pParent->mapToGlobal(QPoint(margin, pParent->height() - this->height() - margin)));
    }

private:
    QColor _color = QColor(0x55, 0x55, 0x55);
};

Sketch3DWorkingPlane::Sketch3DWorkingPlane()
    : _workingPlane(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis)
    , _xDir(wy::Vector3::kXAxis)
    , _yDir(wy::Vector3::kYAxis)
    , _zDir(wy::Vector3::kZAxis)
    , _isValid(true)
    , _i(0)
    , _pOriginSnapObject(nullptr)
    , _pPlaneLabel(nullptr)
{
    _xDir = _workingPlane.getXDir();
    _yDir = _workingPlane.getYDir();
    _zDir = _workingPlane.getNormal();

    this->show();
}

Sketch3DWorkingPlane::Sketch3DWorkingPlane(const wy3d::SketchPlane& plane)
    : _workingPlane(plane)
    , _xDir(wy::Vector3::kXAxis)
    , _yDir(wy::Vector3::kYAxis)
    , _zDir(wy::Vector3::kZAxis)
    , _isValid(true)
    , _i(0)
    , _pOriginSnapObject(nullptr)
{
    if (_workingPlane.isValid())
    {
        _xDir = _workingPlane.getXDir();
        _yDir = _workingPlane.getYDir();
        _zDir = _workingPlane.getNormal();

        this->show();
    }
    else
    {
        _isValid = false;
    }
}

Sketch3DWorkingPlane::~Sketch3DWorkingPlane()
{
    if (!_isValid)
    {
        assert(false);
        return;
    }

    wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
    if (pSnapSys && _pOriginSnapObject)
    {
        pSnapSys->beginChange();
        pSnapSys->removeResidentSnapObject(_pOriginSnapObject);
        pSnapSys->endChange();
        _pOriginSnapObject = nullptr;
    }

    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->hideSketchCSYS();
    }
    else
    {
        assert(false);
    }
}

void Sketch3DWorkingPlane::show()
{
    if (!_isValid)
    {
        assert(false);
        return;
    }

    this->showImpl(_workingPlane);
}

void Sketch3DWorkingPlane::moveTo(const wy::Vector3& pos)
{
    if (!_isValid)
    {
        assert(false);
        return;
    }

    _workingPlane.setOrigin(pos);
    this->showImpl(_workingPlane);
}

void Sketch3DWorkingPlane::switchToNext()
{
    if (!_isValid)
    {
        assert(false);
        return;
    }

    _i = (_i + 1) % 3;
    wy::Vector3 origin = _workingPlane.getOrigin();
    switch (_i)
    {
    case 0: // XY
        _workingPlane = wy3d::SketchPlane(origin, _zDir, _xDir);
        break;
    case 1: // YZ
        _workingPlane = wy3d::SketchPlane(origin, _xDir, _yDir);
        break;
    case 2: // ZX
        _workingPlane = wy3d::SketchPlane(origin, _yDir, _zDir);
        break;
    default:
        assert(false);
        _workingPlane = wy3d::SketchPlane(origin, _zDir, _xDir);
        break;
    }

    this->showImpl(_workingPlane);
}

void Sketch3DWorkingPlane::showImpl(const wy3d::SketchPlane& plane)
{
    wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
    if (pSnapSys)
    {
        if (_pOriginSnapObject)
        {
            pSnapSys->beginChange();
            pSnapSys->removeResidentSnapObject(_pOriginSnapObject);
            pSnapSys->endChange();
            _pOriginSnapObject = nullptr;
        }

        _pOriginSnapObject = std::make_shared<SnapCoordinatePoint>(plane.getOrigin());
        pSnapSys->beginChange();
        pSnapSys->addResidentSnapObject(_pOriginSnapObject);
        pSnapSys->endChange();
    }

    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->showSketchCSYS(plane);

        if (!_pPlaneLabel)
        {
            if (MainWindow* pMainWindow = Application::instance().getMainWindow())
            {
                if (QOpenGLWidget* pGLWidget = pMainWindow->findChild<QOpenGLWidget*>())
                {
                    _pPlaneLabel = std::make_unique<Sketch3DPlaneLabel>(pGLWidget);
                }
            }
        }
        static const char* kPlaneNames[3] = { "XY", "YZ", "ZX" };
        if (_pPlaneLabel)
        {
            _pPlaneLabel->setPlaneName(QLatin1String(kPlaneNames[_i % 3]));
        }
    }
    else
    {
        assert(false);
    }
}

Sketch3DDrawGuiCmd::Sketch3DDrawGuiCmd()
    : OsgGuiCommand()
    , _sketch3DInfo()
    , _pWorkPlane(nullptr)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

Sketch3DDrawGuiCmd::~Sketch3DDrawGuiCmd()
{
}

wyap::CmdExecution::StartResult Sketch3DDrawGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    if (!GuiCommandUtil::initSketch3DInfo(_sketch3DInfo))
    {
        assert(false);
        return wyap::CmdExecution::StartResult::Failed;
    }

    _pWorkPlane.reset();
    wy3d::SketchPlane plane;
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.getCount() == 1 &&
        GuiCommandUtil::getWorkingPlane(ss.createIterator().current(), plane) &&
        plane.isValid())
    {
        _pWorkPlane = std::make_unique<Sketch3DWorkingPlane>(plane);
    }
    else
    {
        _pWorkPlane = std::make_unique<Sketch3DWorkingPlane>();
    }

    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    Application::instance().setCursor(CursorType::Locate);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void Sketch3DDrawGuiCmd::onEnd()
{
    GuiCommand::onEnd();

    _pWorkPlane = nullptr;
}

void Sketch3DDrawGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);

    _pWorkPlane = nullptr;
}

void Sketch3DDrawGuiCmd::onSpaceKey()
{
    if (_pWorkPlane)
    {
        _pWorkPlane->switchToNext();
    }
    else
    {
        assert(false);
    }
}

const wy3d::SketchPlane& Sketch3DDrawGuiCmd::getWorkingPlane() const
{
    static const wy3d::SketchPlane kDefaultPlane(wy::Vector3::kZero, wy::Vector3::kZAxis, wy::Vector3::kXAxis);
    if (_pWorkPlane)
    {
        return _pWorkPlane->getWorkingPlane();
    }
    else
    {
        assert(false);
        return kDefaultPlane;
    }
}

void Sketch3DDrawGuiCmd::moveWorkPlaneOriginTo(const wy::Vector3& pnt)
{
    assert(_pWorkPlane);
    if (!_pWorkPlane)
    {
        return;
    }
    _pWorkPlane->moveTo(pnt);
}

GuiCmdMenu* Sketch3DDrawGuiCmd::initContextMenu()
{
    return new Sketch3DDrawGuiCmdMenu(this);
}

bool Sketch3DDrawGuiCmdMenu::initCustomHeaderActions(QMenu* menu)
{
    assert(menu);
    assert(_pCmd);
    Sketch3DDrawGuiCmd* pCmd = dynamic_cast<Sketch3DDrawGuiCmd*>(_pCmd);
    if (!pCmd)
    {
        assert(false);
        return false;
    }

     QAction* pActionNormalToWorkPln = new QAction(tr("View Normal To Working Plane"), menu);
     pActionNormalToWorkPln->setIcon(QIcon(":/images/View_Normal.svg"));
     menu->addAction(pActionNormalToWorkPln);
     this->connect(pActionNormalToWorkPln, &QAction::triggered, this, &Sketch3DDrawGuiCmdMenu::onViewNormalToWorkingPlane);
     return true;
}

void Sketch3DDrawGuiCmdMenu::onViewNormalToWorkingPlane()
{
    assert(_pCmd);
    Sketch3DDrawGuiCmd* pCmd = dynamic_cast<Sketch3DDrawGuiCmd*>(_pCmd);
    if (!pCmd)
    {
        assert(false);
        return;
    }
    BaseView* pView = Application::instance().getActiveView();
    if (!pView)
    {
        assert(false);
        return;
    }
    pView->viewToWorkingPlane(pCmd->getWorkingPlane());
}
