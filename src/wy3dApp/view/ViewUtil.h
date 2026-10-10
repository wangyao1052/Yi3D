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

#ifndef WY_VIEW_UTIL_H
#define WY_VIEW_UTIL_H

#include <osgViewer/View>
#include <wy3dPlane.h>
#include "view/OsgView.h"

class ViewUtil
{
public:
    // 视图方向
    static void viewToISO(osgViewer::View* pView);
    static void viewToFront(osgViewer::View* pView);
    static void viewToBack(osgViewer::View* pView);
    static void viewToLeft(osgViewer::View* pView);
    static void viewToRight(osgViewer::View* pView);
    static void viewToTop(osgViewer::View* pView);
    static void viewToBottom(osgViewer::View* pView);
    static void viewTo(osgViewer::View* pView, const osg::Vec3d& lookDir, const osg::Vec3d up);

    // 调整视图方向到垂直于工作平面
    static void viewToWorkingPlane(osgViewer::View* pView, const wy3d::Plane& workPln);

    // 获取观察方向
    static bool getLookDir(osgViewer::View* pView, osg::Vec3d& lookDir);

    // 充满视窗
    static void viewAll(osgViewer::View* pView, const osg::BoundingSphere& bdSphere);

    // 平行投影
    static void ortho(osgViewer::View* pView);

    // 透视显示
    static void perspective(osgViewer::View* pView);
};

#endif // WY_VIEW_UTIL_H