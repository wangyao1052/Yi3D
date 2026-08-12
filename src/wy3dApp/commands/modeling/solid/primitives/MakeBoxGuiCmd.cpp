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

#include "commands/modeling/solid/primitives/MakeBoxGuiCmd.h"

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
#include <wy3dDatumPlane.h>

#include "application/Application.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "scene/Scene.h"
#include "utils/MathUtils.h"
#include "utils/TopoShapeUtil.h"

#include "application/Application.h"
#include "snap/SketchSnapSystem.h"
#include "select/SketchPlaneSelFilter.h"
#include "utils/MathUtils.h"
#include "utils/TopoShapeUtil.h"
#include "environments/sketch/SketchEnvironment.h"
#include "scene/nodes/ElementNodeType.h"
#include "scene/Scene.h"
#include "widgets/frame/MainWindow.h"

static bool parseDoubleText(const QString& text, double& value)
{
    bool ok(false);
    value = text.trimmed().toDouble(&ok);
    return ok;
}

constexpr double kHoverPopupDelaySeconds = 0.45;


MakeBoxGuiCmd::MakeBoxGuiCmd() : MakePrimitiveGuiCmd(),
    _uv1(), _uv2(), _height(0.0),
    _pXYPopup(nullptr),
    _pLengthWidthPopup(nullptr),
    _pHeightPopup(nullptr),
    _hoverPopupState()
{
}

MakeBoxGuiCmd::~MakeBoxGuiCmd()
{
}

void MakeBoxGuiCmd::reset()
{
    __baseClass::reset();   // 先清基类状态
    this->cleanup();        // 再清本类状态
}

void MakeBoxGuiCmd::cleanup()
{
    __baseClass::cleanup(); // 防御式调用

    this->hidePopup();

    _uv1.set(0.0, 0.0);
    _uv2.set(0.0, 0.0);
    _height = 0.0;

    _pMakeBox = nullptr;
    _pRectTransient = nullptr;
    _hoverPopupState.resetValue();
}

bool MakeBoxGuiCmd::finishStep(unsigned int step)
{
    switch (step)
    {
    case Step::SpecifyWorkingPlane: // 工作平面
    {
        return MakePrimitiveGuiCmd::finishStep(step);
    }
    break;

    case Step::SpecifyPnt1: // 确定起点
    {
        // 绘制矩形
        _pRectTransient = std::make_shared<RectTransient>();
        _pRectTransient->update(_workPln, _uv1, _uv1);

        // next step
        this->gotoStep(Step::SpecifyPnt2);
        return true;
    }
    break;

    case Step::SpecifyPnt2: // 确定长宽
    {
        // 创建MakeBox
        _pMakeBox = std::make_shared<MakeBox>(this);
        if (!_pMakeBox->init(_workPln, _uv1, _uv2)) // 长宽过小时会返回false(比如输入0,0)
        {
            _pMakeBox = nullptr;
            return false;
        }
        _pMakeBox->collectElements(_excludeIds);

        // 销毁矩形
        _pRectTransient = nullptr;

        // next step
        this->gotoStep(Step::SpecifyPnt3);
        return true;
    }
    break;

    case Step::SpecifyPnt3: // 确定高度
    {
        // 更新高度
        if (_pMakeBox)
        {
            if (!_pMakeBox->update(_height)) // 高度过小时会返回false(比如输入0)
            {
                return false;
            }

            // 提交
            _pMakeBox->commit();
            _pMakeBox = nullptr;
        }

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

void MakeBoxGuiCmd::gotoStepImpl(unsigned int step)
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

    case Step::SpecifyPnt1: // 确定起点
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeBoxGuiCmd",
            "Specify the starting point; you can directly input the coordinate values."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPnt2: // 确定长宽
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeBoxGuiCmd",
            "Specify length and width; you can directly input the values."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPnt3: // 确定高度
    {
        // 允许输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakeBoxGuiCmd",
            "Specify the height; you can directly input the value."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    default:
    {
        assert(false);
        Application::instance().getStatusBar()->setTips("");
    }
    break;
    }
}

void MakeBoxGuiCmd::onFrame(double time)
{
    this->tryShowPopupOnHover(time);
}

void MakeBoxGuiCmd::onMouseMove(const MouseEvent& event)
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

    case Step::SpecifyPnt1: // 确定起点
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        wy::Vector2 uv = _workPln.uv(pnt);
        _hoverPopupState.point = uv;
        return;
    }
    break;

    case Step::SpecifyPnt2: // 确定长宽
    {
        wy::Vector3 pnt = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        wy::Vector2 uv = _workPln.uv(pnt);
        double deltaLength = uv.x() - _uv1.x();
        double deltaWidth = uv.y() - _uv1.y();
        _hoverPopupState.lengthSign = deltaLength < 0.0 ? -1 : 1;
        _hoverPopupState.widthSign = deltaWidth < 0.0 ? -1 : 1;
        _hoverPopupState.length = std::fabs(deltaLength);
        _hoverPopupState.width = std::fabs(deltaWidth);
        {
            if (_pRectTransient) _pRectTransient->update(_workPln, _uv1, uv);
        }
        return;
    }
    break;

    case Step::SpecifyPnt3: // 确定高度
    {
        double height(0.0);
        if (this->computeHeight2(event.x, event.y, _workPln, _uv2, _excludeIds, height))
        {
            _hoverPopupState.heightSign = height < 0.0 ? -1 : 1;
            _hoverPopupState.height = std::fabs(height);
            {
                if (_pMakeBox) _pMakeBox->update(height);
            }
        }
        else
        {
            assert(false);
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

void MakeBoxGuiCmd::onLeftMouseDown(const MouseEvent& event)
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

    case Step::SpecifyPnt1: // 确定起点
    {
        wy::Vector3 pnt1 = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        _uv1 = _workPln.uv(pnt1);
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyPnt2: // 确定长宽
    {
        wy::Vector3 pnt2 = this->computePosition3d(event.x, event.y, _workPln, _excludeIds).first;
        _uv2 = _workPln.uv(pnt2);
        _hoverPopupState.lengthSign = (_uv2.x() - _uv1.x()) < 0.0 ? -1 : 1;
        _hoverPopupState.widthSign = (_uv2.y() - _uv1.y()) < 0.0 ? -1 : 1;
        if (this->finishStep(_step))
        {
            this->simulateMouseMoveFromPopup();
        }
        return;
    }
    break;

    case Step::SpecifyPnt3: // 确定高度
    {
        double height(0.0);
        if (this->computeHeight2(event.x, event.y, _workPln, _uv2, _excludeIds, height))
        {
            _hoverPopupState.heightSign = height < 0.0 ? -1 : 1;
            _height = height;
            if (this->finishStep(_step))
            {
                //this->simulateMouseMoveFromPopup();
            }
        }
        else
        {
            assert(false);
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

void MakeBoxGuiCmd::initializePopups()
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
    if (!_pLengthWidthPopup)
    {
        _pLengthWidthPopup = std::make_unique<GuiCmdHoverInputPopup2>(
            QCoreApplication::translate("MakeBoxGuiCmd", "Length"),
            QCoreApplication::translate("MakeBoxGuiCmd", "Width"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pLengthWidthPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pLengthWidthPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pLengthWidthPopup->hide();
    }
    if (!_pHeightPopup)
    {
        _pHeightPopup = std::make_unique<GuiCmdHoverInputPopup1>(
            QCoreApplication::translate("MakeBoxGuiCmd", "Height"),
            QStringLiteral("-1234.56"),
            pMainWindow);
        _pHeightPopup->setAcceptHandler([this]() { this->onPopupEnterKey(); });
        _pHeightPopup->setCancelHandler([this]() { this->onPopupEscapeKey(); });
        _pHeightPopup->hide();
    }
}

void MakeBoxGuiCmd::showPopup()
{
    if (_step != Step::SpecifyPnt1 &&
        _step != Step::SpecifyPnt2 &&
        _step != Step::SpecifyPnt3)
    {
        return;
    }
    if (!_pXYPopup || !_pLengthWidthPopup || !_pHeightPopup)
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
    else if (_step == Step::SpecifyPnt2)
    {
        if (!_pLengthWidthPopup)
        {
            return;
        }
        _pLengthWidthPopup->setValues(_hoverPopupState.length, _hoverPopupState.width);
        _pLengthWidthPopup->showAtGlobal(QCursor::pos());
    }
    else
    {
        if (!_pHeightPopup)
        {
            return;
        }
        _pHeightPopup->setValue(_hoverPopupState.height);
        _pHeightPopup->showAtGlobal(QCursor::pos());
    }
}

void MakeBoxGuiCmd::hidePopup()
{
    if (_pXYPopup && _pXYPopup->isVisible())
    {
        _pXYPopup->hide();
    }
    if (_pLengthWidthPopup && _pLengthWidthPopup->isVisible())
    {
        _pLengthWidthPopup->hide();
    }
    if (_pHeightPopup && _pHeightPopup->isVisible())
    {
        _pHeightPopup->hide();
    }
}

void MakeBoxGuiCmd::tryShowPopupOnHover(double time)
{
    if (_step != Step::SpecifyPnt1 &&
        _step != Step::SpecifyPnt2 &&
        _step != Step::SpecifyPnt3)
    {
        return;
    }
    if (_hoverPopupState.lastMouseMoveTime < 0.0)
    {
        return;
    }
    if ((_pXYPopup && _pXYPopup->isVisible()) ||
        (_pLengthWidthPopup && _pLengthWidthPopup->isVisible()) ||
        (_pHeightPopup && _pHeightPopup->isVisible()))
    {
        return;
    }
    if (time - _hoverPopupState.lastMouseMoveTime >= kHoverPopupDelaySeconds)
    {
        this->showPopup();
    }
}

void MakeBoxGuiCmd::onPopupEnterKey()
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
        if (!_pLengthWidthPopup)
        {
            return;
        }
        double length(0.0), width(0.0);
        if (!parseDoubleText(_pLengthWidthPopup->getRow1Text(), length) ||
            !parseDoubleText(_pLengthWidthPopup->getRow2Text(), width))
        {
            return;
        }
        length = std::fabs(length);
        width = std::fabs(width);
        _uv2 = _uv1 + wy::Vector2(
            static_cast<double>(_hoverPopupState.lengthSign) * length,
            static_cast<double>(_hoverPopupState.widthSign) * width);
    }
    else if (_step == Step::SpecifyPnt3)
    {
        if (!_pHeightPopup)
        {
            return;
        }
        double height(0.0);
        if (!parseDoubleText(_pHeightPopup->getRowText(), height))
        {
            return;
        }
        height = std::fabs(height);
        _height = static_cast<double>(_hoverPopupState.heightSign) * height;
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

void MakeBoxGuiCmd::onPopupEscapeKey()
{
    this->onEscapeKey();
}

void MakeBoxGuiCmd::simulateMouseMoveFromPopup()
{
    if (_hoverPopupState.lastMouseX == DBL_MAX || _hoverPopupState.lastMouseY == DBL_MAX)
        return;
    this->onMouseMove({static_cast<float>(_hoverPopupState.lastMouseX),
                       static_cast<float>(_hoverPopupState.lastMouseY),
                       _hoverPopupState.lastMouseMoveTime});
}

void MakeBox::collectElements(std::set<wydb::ElementId>& idSet) const
{
    if (_pBox) idSet.insert(_pBox->getId());
}

bool MakeBox::init(const wy::Vector2& pnt1, const wy::Vector2& pnt2, double z)
{
    if (!_pDb || !_pTopTrans || _pBox || _isFinished)
    {
        return false;
    }

    // 计算Box长与宽
    double xMin = pnt1.x();
    double xMax = pnt2.x();
    if (xMin > xMax) std::swap(xMin, xMax);
    double yMin = pnt1.y();
    double yMax = pnt2.y();
    if (yMin > yMax) std::swap(yMin, yMax);
    double length = xMax - xMin;
    double width = yMax - yMin;
    if (length < wy3d::kMinValue || length > wy3d::kMaxValue || width < wy3d::kMinValue || width > wy3d::kMaxValue)
    {
        return false;
    }
    double height(wy3d::kMinValue);

    // 确定Box起始坐标
    wy::Vector3 boxOrigin(xMin, yMin, z);

    // 创建Box
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Box* pBox(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::Box::create(pTrans, length, width, height, pBox) || !pBox)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != pBox->setPosition(boxOrigin))
    {
        goto ABORT_TRANS;
    }
    _pDb->getTransactionManager()->endTransaction();
    _pBox = pBox;
    _initOrigin = pBox->getPosition();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pBox = nullptr;
    return false;
}

bool MakeBox::init(const osg::Vec3d& pnt1, const osg::Vec3d& pnt2)
{
    return this->init(wy::Vector2(pnt1.x(), pnt1.y()), wy::Vector2(pnt2.x(), pnt2.y()), pnt1.z());
}

bool MakeBox::init(const wy3d::SketchPlane& workPln, const wy::Vector2& uv1, const wy::Vector2& uv2)
{
    if (!_pDb || !_pTopTrans || _pBox || _isFinished)
    {
        return false;
    }

    // 计算长&宽
    double xMin = uv1.x();
    double xMax = uv2.x();
    if (xMin > xMax) std::swap(xMin, xMax);

    double yMin = uv1.y();
    double yMax = uv2.y();
    if (yMin > yMax) std::swap(yMin, yMax);

    double length = xMax - xMin;
    double width = yMax - yMin;
    if (length < wy3d::kMinValue || length > wy3d::kMaxValue ||
        width < wy3d::kMinValue || width > wy3d::kMaxValue)
    {
        return false;
    }

    // 默认高度
    double height(wy3d::kMinValue);

    // 计算旋转欧拉角
    if (!workPln.isValid())
    {
        return false;
    }
    wy::Vector3 rotation = MathUtils::computeEulerZXY(workPln);

    // 确定Box起始坐标
    wy::Vector3 boxOrigin = workPln.value(xMin, yMin);

    // 创建Box
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Box* pBox(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::Box::create(pTrans, length, width, height, pBox) || !pBox)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != pBox->setRotation(rotation)) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != pBox->setPosition(boxOrigin)) goto ABORT_TRANS;
    _pDb->getTransactionManager()->endTransaction();
    _pBox = pBox;
    _initOrigin = pBox->getPosition();
    _zAxis = workPln.getNormal();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    _pBox = nullptr;
    return false;
}

bool MakeBox::update(double height)
{
    if (!_pDb || !_pTopTrans || !_pBox || _isFinished)
    {
        return false;
    }
    if (std::fabs(height) < wy3d::kMinValue || std::fabs(height) > wy3d::kMaxValue)
    {
        return false;
    }

    wydb::TransactionManager* pTransMgr = _pDb->getTransactionManager();
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans) return false;
    {
        _pBox->upgradeForWrite();
        if (height < 0.0) _pBox->setPosition(_initOrigin + height * _zAxis);
        else _pBox->setPosition(_initOrigin);
        _pBox->setHeight(std::fabs(height));
    }
    if (wy::ErrorStatus::Ok == pTransMgr->endTransaction())
    {
        pTransMgr->mergeTransaction();
    }
    return true;
}

RectTransient::RectTransient()
{
    _geom = new osg::Geometry();
    _geom->setDataVariance(osg::Object::DYNAMIC);
    _geom->setUseDisplayList(false);
    _geom->setUseVertexBufferObjects(true);
    // 顶点数组
    _vertices = new osg::Vec3Array();
    _vertices->resize(4);
    (*_vertices)[0] = osg::Vec3(0.0f, 0.0f, 0.0f);
    (*_vertices)[1] = osg::Vec3(1.0f, 0.0f, 0.0f);
    (*_vertices)[2] = osg::Vec3(1.0f, 1.0f, 0.0f);
    (*_vertices)[3] = osg::Vec3(0.0f, 1.0f, 0.0f);
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
    indices->resize(8);
    (*indices)[0] = 0;
    (*indices)[1] = 1;
    (*indices)[2] = 1;
    (*indices)[3] = 2;
    (*indices)[4] = 2;
    (*indices)[5] = 3;
    (*indices)[6] = 3;
    (*indices)[7] = 0;
    // 绘制线
    _geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_LINES, indices->begin(), indices->end()));

    // 添加到根节点
    _root->addChild(_geom);
}

RectTransient::~RectTransient()
{
}

void RectTransient::update(const osg::Vec2& pnt1, const osg::Vec2& pnt2, double z)
{
    (*_vertices)[0].set(pnt1.x(), pnt1.y(), z);
    (*_vertices)[1].set(pnt2.x(), pnt1.y(), z);
    (*_vertices)[2].set(pnt2.x(), pnt2.y(), z);
    (*_vertices)[3].set(pnt1.x(), pnt2.y(), z);
    _vertices->dirty();
    _geom->dirtyBound();
}

void RectTransient::update(const osg::Vec3d& pnt1, const osg::Vec3d& pnt2)
{
    return this->update(osg::Vec2(pnt1.x(), pnt1.y()), osg::Vec2(pnt2.x(), pnt2.y()), pnt1.z());
}

void RectTransient::update(const wy3d::SketchPlane& workPln,
    const wy::Vector2& uv1, const wy::Vector2& uv2)
{
    (*_vertices)[0] = MathUtils::toVec3(workPln.value(uv1));
    (*_vertices)[1] = MathUtils::toVec3(workPln.value(uv2.x(), uv1.y()));
    (*_vertices)[2] = MathUtils::toVec3(workPln.value(uv2));
    (*_vertices)[3] = MathUtils::toVec3(workPln.value(uv1.x(), uv2.y()));
    _vertices->dirty();
    _geom->dirtyBound();
}
