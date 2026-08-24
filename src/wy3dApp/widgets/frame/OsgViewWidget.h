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

#ifndef WY3DAPP_OSG_VIEW_WIDGET_H
#define WY3DAPP_OSG_VIEW_WIDGET_H

#include <QWidget>
#include <QRect>
#include <QTimer>
#include <QMenu>
#include <QPushButton>

#include <osgViewer/CompositeViewer>
#include <osgGA/TrackballManipulator>
#include "widgets/frame/ViewWidget.h"
#include "view/CameraManipulator3d.h"
#include <osgQOpenGL/osgQOpenGLWidget>
#include <osgViewer/View>
#include <osgViewer/Viewer>

class CameraManipulator;
class StateButton;

class OsgViewWidget : public ViewWidget
{
	Q_OBJECT

public:
	explicit OsgViewWidget(QWidget *parent = Q_NULLPTR);
	~OsgViewWidget();

    osgViewer::View* getOsgView()
    {
		return _pOsgGLWidget->getOsgViewer();
    }

	void setCursor(const QCursor& cursor) override;
	QRect getRenderAreaGlobalRect() const override;

protected slots:
	void initWindow();

private:
	void setNavigationCursor(NavCursorMode mode);

	osgQOpenGLWidget* _pOsgGLWidget;
	osg::ref_ptr<CameraManipulator3d> _pCameraManipulator;
	bool _navCursorActive;
	QCursor _cursorBeforeNav;
};

#endif // WY3DAPP_OSG_VIEW_WIDGET_H
