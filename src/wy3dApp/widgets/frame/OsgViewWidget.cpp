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

#include "OsgViewWidget.h"
#include "application/Application.h"
#include "application/Config.h"
#include "view/CameraManipulator3d.h"
#include "commands/OsgGuiEventDispatcher.h"
#include "widgets/CursorCenter.h"
#include "widgets/CursorType.h"

#include <QVBoxLayout>
#include <QMenu>
#include <QWindow>
#include <QPushButton>

#define _USE_MATH_DEFINES
#include <math.h>

#include <osg/Matrixd>
#include <osg/LineStipple>
#include <osg/AutoTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/Shape>
#include <osg/ShapeDrawable>
#include <osg/GLExtensions>
#include <osg/LightModel>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <osgGA/CameraManipulator>
#include <osgGA/OrbitManipulator>
#include <osgGA/MultiTouchTrackballManipulator>
#include <osgGA/TrackballManipulator>
#include <osgDB/ReadFile>

#include <vector>

OsgViewWidget::OsgViewWidget(QWidget *parent) : ViewWidget(parent), _pOsgGLWidget(nullptr), _navCursorActive(false)
{
	this->resize(400, 300);
	this->setMinimumWidth(400);
	this->setMinimumHeight(300);

    QVBoxLayout* pMainLayout = new QVBoxLayout();
    pMainLayout->setContentsMargins(0, 1, 1, 0);
    _pOsgGLWidget = new osgQOpenGLWidget(this);
    pMainLayout->addWidget(_pOsgGLWidget);
    this->setLayout(pMainLayout);

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setAlphaBufferSize(0);
    format.setSamples(8);
    _pOsgGLWidget->setFormat(format);
    _pOsgGLWidget->setMouseTracking(true);
    _pOsgGLWidget->setFocusPolicy(Qt::WheelFocus);

    connect(_pOsgGLWidget, SIGNAL(initialized()), this, SLOT(initWindow()));
}

OsgViewWidget::~OsgViewWidget()
{
    if (_pCameraManipulator)
    {
        _pCameraManipulator->setNavCursorCallback(nullptr);
    }
}

QRect OsgViewWidget::getRenderAreaGlobalRect() const
{
    if (!_pOsgGLWidget || !_pOsgGLWidget->isVisible())
    {
        return QRect();
    }

    return QRect(
        _pOsgGLWidget->mapToGlobal(QPoint(0, 0)),
        _pOsgGLWidget->size());
}

void OsgViewWidget::initWindow()
{
    osgViewer::View* mainView = _pOsgGLWidget->getOsgViewer();
    assert(mainView);
    osg::Camera* camera = mainView->getCamera();
    const osg::GraphicsContext::Traits* traits = camera->getGraphicsContext()->getTraits();
    camera->setClearColor(osg::Vec4(0.910f, 0.918f, 0.929f, 1.0f)); // 参照Creo的背景色

    camera->setViewport(new osg::Viewport(0, 0, traits->width, traits->height));
    camera->setDrawBuffer(GL_BACK); // set the draw and read buffers up for a double buffered.
    camera->setReadBuffer(GL_BACK); // window with rendering going to back buffer.
    camera->setProjectionMatrixAsPerspective(30.0f, static_cast<double>(traits->width) / static_cast<double>(traits->height), 0.1f, 5000.0f);

    // 摄像机操纵器(初始方位:东南轴等侧)
    CameraManipulator3d* pCameraManipulator = new CameraManipulator3d();
    _pCameraManipulator = pCameraManipulator;
    pCameraManipulator->setNavCursorCallback(
        [this](NavCursorMode mode) { setNavigationCursor(mode); });
    // added by wangyao 2025.07.28 {
    // 反转鼠标滚轮方向
    if (Application::instance().getConfig()->view.invertMouseWheelZoom)
    {
        pCameraManipulator->setWheelZoomFactor(-pCameraManipulator->getWheelZoomFactor());
    }
    // }
    osg::Quat rot(0.424708f, 0.17592f, 0.339851f, 0.820473f);
    osg::Vec3d lookDir = rot * osg::Vec3d(0, 0, -1);
    lookDir.normalize();
    osg::Vec3d up = rot * osg::Vec3d(0, 1, 0);
    up.normalize();
    osg::Vec3d center(0.0, 0.0, 0.0);
    double distance(200.0);
    osg::Vec3d eye = center - lookDir * distance;
    pCameraManipulator->setHomePosition(eye, center, up);
    mainView->setCameraManipulator(pCameraManipulator);

    // 统计渲染功能
    //mainView->addEventHandler(new osgViewer::StatsHandler());

    // 关闭细节裁剪
    osg::CullStack::CullingMode cullingMode = mainView->getCamera()->getCullingMode();
    cullingMode &= ~(osg::CullStack::SMALL_FEATURE_CULLING);
    mainView->getCamera()->setCullingMode(cullingMode);

    // Gui event dispatcher.
    mainView->addEventHandler(new OsgGuiEventDispatcher());
}

void OsgViewWidget::setCursor(const QCursor& cursor)
{
	if (_pOsgGLWidget)
	{
        _pOsgGLWidget->setCursor(cursor);
	}
}

void OsgViewWidget::setNavigationCursor(NavCursorMode mode)
{
	if (!_pOsgGLWidget)
	{
		return;
	}

	if (NavCursorMode::None == mode)
	{
		if (_navCursorActive)
		{
			_navCursorActive = false;
			_pOsgGLWidget->setCursor(_cursorBeforeNav);
		}
		return;
	}

	if (!_navCursorActive)
	{
		_navCursorActive = true;
		_cursorBeforeNav = _pOsgGLWidget->cursor();
	}

	CursorType cursorType = (NavCursorMode::Rotate == mode) ? CursorType::Rotate : CursorType::Pan;
	_pOsgGLWidget->setCursor(CursorCenter::instance().getCursor(cursorType));
}
