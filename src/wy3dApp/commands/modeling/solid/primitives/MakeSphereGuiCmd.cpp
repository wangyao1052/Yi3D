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

#include "commands/modeling/solid/primitives/MakeSphereGuiCmd.h"

#include <QCoreApplication>
#include <QCursor>
#include <QString>
#include <cmath>
#include <osg/LineSegment>
#include <osg/ref_ptr>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dImpl.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "scene/Scene.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

constexpr double kHoverPopupDelaySeconds = 0.45;


MakeSphereGuiCmd::MakeSphereGuiCmd() : MakePrimitiveGuiCmd(),
    _uv1(), _radius(0.0),
    _pXYPopup(nullptr),
    _pRadiusPopup(nullptr),
    _hoverPopupState()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

MakeSphereGuiCmd::~MakeSphereGuiCmd()
{
}

void MakeSphereGuiCmd::reset()
{
    __baseClass::reset();
    this->cleanup();
}

void MakeSphereGuiCmd::cleanup()
{
    __baseClass::cleanup();
    this->hidePopup();

    _uv1.set(0.0, 0.0);
    _radius = 0.0;

    _pMakeSphere = nullptr;
    _pRadiusTransient = nullptr;
    _hoverPopupState.resetValue();
}

bool MakeSphereGuiCmd::finishStep(unsigned int step)
{
    switch (step)
    {
    case Step::SpecifyWorkingPlane: // 工作平面
    {
        return MakePrimitiveGuiCmd::finishStep(step);
    }
    break;

    case Step::SpecifyPnt1: // 确定球心
    {
        // 绘制半径
        _pRadiusTransient = std::make_shared<RadiusTransient>();
        wy::Vector3 pnt = _workPln.value(_uv1);
        _pRadiusTransient->update(pnt, pnt);

        // 创建MakeSphere
        _pMakeSphere = std::make_shared<MakeSphere>(this);
        if (!_pMakeSphere->init(_workPln, _uv1))
        {
            assert(false);
            _pMakeSphere = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pMakeSphere->collectElements(_excludeIds);

        // next step
        this->gotoStep(Step::SpecifyPnt2);
        return true;
    }
    break;

    case Step::SpecifyPnt2: // 确定半径
    {
        // 更新半径
        if (_pMakeSphere)
        {
            if (!_pMakeSphere->update(_radius)) // 半径过小时会返回false(比如输入0)
            {
                return false;
            }

            // 提交
            _pMakeSphere->commit();
            _pMakeSphere = nullptr;
        }

        // 销毁绘制的半径
        _pRadiusTransient = nullptr;

        // 退出
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

void MakeSphereGuiCmd::gotoStepImpl(unsigned int step)
{
    this->hidePopup();
    _hoverPopupState.resetValue();

    switch (step)
    {
    case Step::SpecifyWorkingPlane: // 工作平面
    {
        return MakePrimitiveGuiCmd::gotoStepImpl(step);
    }
    break;

    case Step::SpecifyPnt1: // 确定球心
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeSphereGuiCmd",
            "Specify the center point; you can directly input the coordinate values."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPnt2: // 确定半径
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeSphereGuiCmd",
            "Specify the radius; you can directly input the value."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    default:
    {
        assert(false);
        Application::instance().getStatusBar()->setTips("");
        assert(false);
    }
    break;
    }
}

void MakeSphereGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void MakeSphereGuiCmd::onMouseMove(const MouseEvent& event)
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
    case Step::SpecifyWorkingPlane: // 工作平面
    {
        return MakePrimitiveGuiCmd::onMouseMove(event);
    }
    break;

    case Step::SpecifyPnt1: // 确定球心
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        wy::Vector2 uv = _workPln.uv(pnt);
        _hoverPopupState.point = uv;
        return;
    }
    break;

    case Step::SpecifyPnt2: // 确定半径
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        double radius = (_uv1 - _workPln.uv(pnt)).length();
        _hoverPopupState.radius = radius;
        {
            if (_pRadiusTransient) _pRadiusTransient->update(_workPln.value(_uv1), pnt);
            if (_pMakeSphere) _pMakeSphere->update(radius);
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

void MakeSphereGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    this->hidePopup();
    _hoverPopupState.lastMouseX = event.x;
    _hoverPopupState.lastMouseY = event.y;
    _hoverPopupState.lastMouseMoveTime = event.time;

    switch (_step)
    {
    case Step::SpecifyWorkingPlane: // 工作平面
    {
        unsigned int prevStep = _step;
    MakePrimitiveGuiCmd::onLeftMouseDown(event);
    if (prevStep != _step)
        this->simulateMouseMoveFromPopup();
    }
    break;

    case Step::SpecifyPnt1: // 确定球心
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        _uv1 = _workPln.uv(pnt);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyPnt2: // 确定半径
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        _radius = (_uv1 - _workPln.uv(pnt)).length();
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
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

void MakeSphereGuiCmd::initializePopups()
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
    if (!_pRadiusPopup)
    {
        _pRadiusPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("MakeSphereGuiCmd", "Radius"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pRadiusPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pRadiusPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pRadiusPopup->hide();
    }
}

void MakeSphereGuiCmd::showPopup()
{
    if (_step != Step::SpecifyPnt1 && _step != Step::SpecifyPnt2)
    {
        return;
    }
    if (!_pXYPopup || !_pRadiusPopup)
    {
        this->initializePopups();
    }

    if (_step == Step::SpecifyPnt1)
    {
        if (!_pXYPopup)
        {
            return;
        }
        _pXYPopup->setValues(_hoverPopupState.point.x(), _hoverPopupState.point.y());
        _pXYPopup->showAtGlobal(QCursor::pos());
    }
    else
    {
        if (!_pRadiusPopup)
        {
            return;
        }
        _pRadiusPopup->setValue(_hoverPopupState.radius);
        _pRadiusPopup->showAtGlobal(QCursor::pos());
    }
}

void MakeSphereGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pRadiusPopup && _pRadiusPopup->isVisible())
    {
        _pRadiusPopup->hide();
    }
}

void MakeSphereGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyPnt1 && _step != Step::SpecifyPnt2)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pRadiusPopup && _pRadiusPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void MakeSphereGuiCmd::onPopupEnterKey()
{
    if (_step == Step::SpecifyPnt1)
    {
        if (!_pXYPopup)
        {
            return;
        }
        double x(0.0), y(0.0);
        if (!parseDoubleText(_pXYPopup->getRow1Text(), x) ||
            !parseDoubleText(_pXYPopup->getRow2Text(), y))
        {
            return;
        }
        _uv1.set(x, y);
    }
    else if (_step == Step::SpecifyPnt2)
    {
        if (!_pRadiusPopup)
        {
            return;
        }
        double radius(0.0);
        if (!parseDoubleText(_pRadiusPopup->getRowText(), radius))
        {
            return;
        }
        _radius = std::fabs(radius);
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

void MakeSphereGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void MakeSphereGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void MakeSphere::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pSphere) idSet.insert(_pSphere->getId());
}

bool MakeSphere::init(const wy3d::SketchPlane& workPln, const wy::Vector2& uv)
{
    if (!_pDb || !_pTopTrans || _pSphere || _isFinished)
    {
        return false;
    }

    // 半径
    double radius(wy3d::kMinValue);

    // 确定球心
    wy::Vector3 sphereOrigin = workPln.value(uv);

    // 创建Sphere
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Sphere* pSphere(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::Sphere::create(pTrans, radius, pSphere) || !pSphere)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != pSphere->setPosition(sphereOrigin)) goto ABORT_TRANS;
    _pDb->getTransactionManager()->endTransaction();
    _pSphere = pSphere;
    _initOrigin = pSphere->getPosition();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pSphere = nullptr;
    return false;
}

bool MakeSphere::update(double radius)
{
    if (!_pDb || !_pTopTrans || !_pSphere || _isFinished)
    {
        return false;
    }
    if (std::fabs(radius) < wy3d::kMinValue || std::fabs(radius) > wy3d::kMaxValue)
    {
        return false;
    }

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    {
        _pSphere->upgradeForWrite();
        _pSphere->setRadius(std::fabs(radius));
    }
    if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
    {
        pTransMgr->mergeTransaction();
    }
    return true;
}

RadiusTransient::RadiusTransient()
{
    _geom = new osg::Geometry();
    _geom->setNodeMask(~PICK_MASK);
    _geom->setDataVariance(osg::Object::DYNAMIC);
    _geom->setUseDisplayList(false);
    _geom->setUseVertexBufferObjects(true);
    // 顶点数组
    _vertices = new osg::Vec3Array();
    _vertices->resize(2);
    (*_vertices)[0] = osg::Vec3(0.0f, 0.0f, 0.0f);
    (*_vertices)[1] = osg::Vec3(1.0f, 0.0f, 0.0f);
    _geom->setVertexArray(_vertices);
    // 法向数组
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    _geom->setNormalArray(normals, osg::Array::Binding::BIND_OVERALL);
    // 颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    _geom->setColorArray(colors, osg::Array::Binding::BIND_OVERALL);
    // 索引数组
    osg::ref_ptr<osg::UShortArray> indices = new osg::UShortArray();
    indices->resize(2);
    (*indices)[0] = 0;
    (*indices)[1] = 1;
    // GL_LINES
    _geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices->begin(), indices->end()));

    // 添加到根节点
    _root->addChild(_geom.get());
}

RadiusTransient::~RadiusTransient()
{
}

//void RadiusTransient::show()
//{
//    _geom->setNodeMask(~PICK_MASK);
//}
//
//void RadiusTransient::hide()
//{
//    _geom->setNodeMask(0);
//}

void RadiusTransient::update(const wy::Vector3& pnt1, const wy::Vector3& pnt2)
{
    (*_vertices)[0].set(pnt1.x(), pnt1.y(), pnt1.z());
    (*_vertices)[1].set(pnt2.x(), pnt2.y(), pnt2.z());
    _vertices->dirty();
    _geom->dirtyBound();
}
