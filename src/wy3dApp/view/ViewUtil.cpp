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

#include <wyVector3.h>
#include "ViewUtil.h"
#include <cassert>
#include <osgGA/TrackballManipulator>
#include "application/Application.h"
#include <wy3dApp/SketchEnvironment.h>

void ViewUtil::viewToISO(osgViewer::View* pView)
{
    osg::Quat rot(0.424708f, 0.17592f, 0.339851f, 0.820473f);
    osg::Vec3d lookDir = rot * osg::Vec3d(0, 0, -1);
    lookDir.normalize();
    osg::Vec3d up = rot * osg::Vec3d(0, 1, 0);
    up.normalize();
    viewTo(pView, lookDir, up);
}

void ViewUtil::viewToFront(osgViewer::View* pView)
{
    viewTo(pView, osg::Vec3d(0, 1, 0), osg::Vec3d(0, 0, 1));
}

void ViewUtil::viewToBack(osgViewer::View* pView)
{
    viewTo(pView, osg::Vec3d(0, -1, 0), osg::Vec3d(0, 0, 1));
}

void ViewUtil::viewToLeft(osgViewer::View* pView)
{
    viewTo(pView, osg::Vec3d(1, 0, 0), osg::Vec3d(0, 0, 1));
}

void ViewUtil::viewToRight(osgViewer::View* pView)
{
    viewTo(pView, osg::Vec3d(-1, 0, 0), osg::Vec3d(0, 0, 1));
}

void ViewUtil::viewToTop(osgViewer::View* pView)
{
    viewTo(pView, osg::Vec3d(0, 0, -1), osg::Vec3d(0, 1, 0));
}

void ViewUtil::viewToBottom(osgViewer::View* pView)
{
    viewTo(pView, osg::Vec3d(0, 0, 1), osg::Vec3d(0, -1, 0));
}

void ViewUtil::viewTo(osgViewer::View* pView, const osg::Vec3d& lookDir, const osg::Vec3d up)
{
    osg::Camera* camera = pView->getCamera();
    if (!camera) return;
    osgGA::TrackballManipulator* pCameraManipulator =
        dynamic_cast<osgGA::TrackballManipulator*>(pView->getCameraManipulator());
    assert(pCameraManipulator);

    osg::Vec3d eye;
    osg::Vec3d center;
    osg::Vec3d tempUp;
    pCameraManipulator->getTransformation(eye, center, tempUp);
    eye = center - lookDir * (eye - center).length();
    pCameraManipulator->setTransformation(eye, center, up);
}

void ViewUtil::viewToWorkingPlane(
    osgViewer::View* pView,
    const wy3d::Plane& workPln)
{
    osg::Camera* camera = pView->getCamera();
    if (!camera) return;
    osgGA::TrackballManipulator* pCameraManipulator =
        dynamic_cast<osgGA::TrackballManipulator*>(pView->getCameraManipulator());
    assert(pCameraManipulator);

    osg::Vec3d eye;
    osg::Vec3d center;
    osg::Vec3d up;
    pCameraManipulator->getTransformation(eye, center, up);
    double distance = (center - eye).length();

    // 新的中心点:原来的中心点投影到工作平面
    wy::Vector3 projPnt = workPln.project(wy::Vector3(center.x(), center.y(), center.z()));
    center.set(projPnt.x(), projPnt.y(), projPnt.z());
    // 新的摄像机位置
    wy::Vector3 normal = workPln.getNormal();
    eye = center + osg::Vec3d(normal.x(), normal.y(), normal.z()) * distance; // 平面法向为观察方向
    // 向上的方向为工作平面的Y轴方向
    wy::Vector3 yDir = workPln.getYDir();
    up.set(yDir.x(), yDir.y(), yDir.z());

    pCameraManipulator->setTransformation(eye, center, up);
}

// 计算正交投影下的视图最大外包围盒(在CameraManipulator3d.cpp中有相同的副本函数)
static osg::BoundingBox computeViewBoundingBoxInOrtho(osg::Camera* camera)
{
    assert(camera);
    osg::Matrix projection = camera->getProjectionMatrix(); // 投影矩阵
    osg::Matrix view = camera->getViewMatrix();             // 视图矩阵
    osg::Matrix invMat = osg::Matrix::inverse(projection * view); // 将NDC下的点转换到世界坐标系

    // 取NDC下正交投影视景体中间平面的四个点
    static osg::Vec3 ndcCorners[4] =
    {
        osg::Vec3(-1.0f, -1.0f, 0.0f),
        osg::Vec3(1.0f, -1.0f, 0.0f),
        osg::Vec3(-1.0f,  1.0f, 0.0f),
        osg::Vec3(1.0f,  1.0f, 0.0f)
    };

    osg::BoundingBox bb;
    for (int i = 0; i < 4; ++i)
    {
        osg::Vec3 worldCorner = ndcCorners[i] * invMat;
        bb.expandBy(worldCorner);
    }
    return bb;
}

void ViewUtil::viewAll(osgViewer::View* pView, const osg::BoundingSphere& bdSphere)
{
    osg::Camera* camera = pView->getCamera();
    if (!camera) return;
    osgGA::TrackballManipulator* pCameraManipulator =
        dynamic_cast<osgGA::TrackballManipulator*>(pView->getCameraManipulator());
    assert(pCameraManipulator);

    osg::Vec3d eye;
    osg::Vec3d center;
    osg::Vec3d up;
    pCameraManipulator->getTransformation(eye, center, up);
    osg::Vec3d lookDir = center - eye;
    lookDir.normalize();

    double radius = bdSphere.radius();
    if (radius <= 0) // 空场景
    {
        radius = 100.0;
    }

    double left, right, bottom, top, zNear, zFar, fov, aspect;
    // 平行投影
    if (camera->getProjectionMatrixAsOrtho(left, right, bottom, top, zNear, zFar))
    {
        double distance = (center - eye).length();
        // added by wangyao 2025.05.05 {
        // 草图环境下调整观察的中心点到草图平面
        wyap::Environment* pCurEnv = Application::instance().getEnvManager()->getActiveEnvironment();
        SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pCurEnv);
        if (pSketchEnv) // 如果当前在草图环境
        {
            const osg::Vec3d& sphereCenter = bdSphere.center();
            wy::Vector3 projPnt = pSketchEnv->getSketchPlane().project(
                wy::Vector3(sphereCenter.x(), sphereCenter.y(), sphereCenter.z()));
            center.set(projPnt.x(), projPnt.y(), projPnt.z());
        }
        else
        {
            center = bdSphere.center();
        }
        // }
        eye = center - lookDir * distance;
        pCameraManipulator->setTransformation(eye, center, up);

        double scaleX = (radius * 2.1) / (right - left);
        double scaleY = (radius * 2.1) / (top - bottom);
        double scale = scaleX > scaleY ? scaleX : scaleY;
        camera->setProjectionMatrixAsOrtho(left * scale, right * scale, bottom * scale, top * scale, zNear, zFar);
        // added by wangyao 2025.03.04 {
        // 调整zNear与zFar
        {
            osg::BoundingBox viewBBox = computeViewBoundingBoxInOrtho(camera);
            double viewRadius = viewBBox.radius();
            zNear = distance - viewRadius * 1.05;
            zFar = distance + viewRadius * 1.05;
            camera->setProjectionMatrixAsOrtho(left * scale, right * scale, bottom * scale, top * scale, zNear, zFar);
        }
        // }
    }
    else if (camera->getProjectionMatrixAsPerspective(fov, aspect, zNear, zFar))// 透视投影
    {
        double distance = radius / std::tan(osg::DegreesToRadians(fov) / 2);
        if (aspect < 1.0) // 视口宽度<视口高度
        {
            distance /= aspect;
        }
        // added by wangyao 2025.05.05 {
        // 草图环境下调整观察的中心点到草图平面
        wyap::Environment* pCurEnv = Application::instance().getEnvManager()->getActiveEnvironment();
        SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pCurEnv);
        if (pSketchEnv) // 如果当前在草图环境
        {
            const osg::Vec3d& sphereCenter = bdSphere.center();
            wy::Vector3 projPnt = pSketchEnv->getSketchPlane().project(
                wy::Vector3(sphereCenter.x(), sphereCenter.y(), sphereCenter.z()));
            center.set(projPnt.x(), projPnt.y(), projPnt.z());
        }
        else
        {
            center = bdSphere.center();
        }
        // }
        eye = center - lookDir * distance;
        pCameraManipulator->setTransformation(eye, center, up);
    }
}

void ViewUtil::ortho(osgViewer::View* pView)
{
    osg::Camera* camera = pView->getCamera();
    if (!camera) return;
    osgGA::TrackballManipulator* pCameraManipulator =
        dynamic_cast<osgGA::TrackballManipulator*>(pView->getCameraManipulator());
    assert(pCameraManipulator);
    double left, right, bottom, top, zNear, zFar;
    if (camera->getProjectionMatrixAsFrustum(left, right, bottom, top, zNear, zFar))
    {
        osg::Vec3d eye, center, up;
        pCameraManipulator->getTransformation(eye, center, up);
        double ratio = (center - eye).length() / zNear;
        // added by wangyao 2025.03.03 {
        // 正交投影时禁用自动计算远近裁剪面
        camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        // }
        camera->setProjectionMatrixAsOrtho(left * ratio, right * ratio, bottom * ratio, top * ratio, zNear, zFar);
    }
}

void ViewUtil::perspective(osgViewer::View* pView)
{
    osg::Camera* camera = pView->getCamera();
    if (!camera) return;
    osgGA::TrackballManipulator* pCameraManipulator =
        dynamic_cast<osgGA::TrackballManipulator*>(pView->getCameraManipulator());
    assert(pCameraManipulator);

    double left, right, bottom, top, zNear, zFar;
    if (camera->getProjectionMatrixAsOrtho(left, right, bottom, top, zNear, zFar))
    {
        osg::Vec3d eye, center, up;
        pCameraManipulator->getTransformation(eye, center, up);
        osg::Vec3d lookDir = eye - center;
        lookDir.normalize();
        double dist = top / tan(30.0f / 180.0f * osg::PI);
        eye = center + lookDir * dist;
        pCameraManipulator->setTransformation(eye, center, up);
        camera->setProjectionMatrixAsPerspective(30.0f, (right - left) / (top - bottom), /*zNear*/1.0f, /*zFar*/10000.0f);
        // added by wangyao 2025.03.04 {
        // 透视投影时自动计算远近裁剪面
        camera->setComputeNearFarMode(osg::CullSettings::COMPUTE_NEAR_FAR_USING_BOUNDING_VOLUMES);
        // }
    }
}

bool ViewUtil::getLookDir(osgViewer::View* pView, osg::Vec3d& lookDir)
{
    if (!pView) return false;
    osg::Camera* camera = pView->getCamera();
    if (!camera) return false;
    osgGA::TrackballManipulator* pCameraManipulator =
        dynamic_cast<osgGA::TrackballManipulator*>(pView->getCameraManipulator());
    if (!pCameraManipulator) return false;

    osg::Vec3d eye;
    osg::Vec3d center;
    osg::Vec3d up;
    pCameraManipulator->getTransformation(eye, center, up);
    lookDir = center - eye;
    lookDir.normalize();
    return true;
}