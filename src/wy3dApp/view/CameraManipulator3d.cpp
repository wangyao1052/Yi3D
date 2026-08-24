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

#include "CameraManipulator3d.h"
#include <cassert>
#include <Geom_Plane.hxx>
#include <Geom_Line.hxx>
#include <GeomAPI_IntCS.hxx>
#include "scene/RenderConst.h"
#include "scene/nodes/ElementNodeType.h"

CameraManipulator3d::CameraManipulator3d()
	: osgGA::TrackballManipulator(), _maxNearFarDis(1000.0)
{
	//this->_flags = UPDATE_MODEL_SIZE | PROCESS_MOUSE_WHEEL;
	this->setAllowThrow(false);
	//OrbitManipulator::_minimumDistanceFlagIndex = 0;
	//OrbitManipulator::_minimumDistance = 1;
}

void CameraManipulator3d::setNode(osg::Node* node)
{
	_node = node;

	// update model size
	if (_node.get())
	{
		const osg::BoundingSphere& boundingSphere = _node->getBound();
		_modelSize = boundingSphere.radius();
	}
	else
	{
		_modelSize = 0.;
	}

    if (_modelSize > 0.0)
    {
        _maxNearFarDis = 10 * _modelSize;
    }

	// compute home position
	if (getAutoComputeHomePosition())
		computeHomePosition(NULL, (_flags & COMPUTE_HOME_USING_BBOX) != 0);
}

void CameraManipulator3d::setModelSize(double modelSize)
{
	assert(modelSize > 0);
	_modelSize = modelSize;
    if (_modelSize > 0.0)
    {
        _maxNearFarDis = 10 * _modelSize;
    }
}

void CameraManipulator3d::setNavCursorCallback(std::function<void(NavCursorMode)> callback)
{
	_navCursorData.callback = callback;
}

void CameraManipulator3d::setNavCursor(NavCursorMode mode)
{
	_navCursorData.isCursorActive = true;

	if (_navCursorData.mode == mode) return;
	_navCursorData.mode = mode;
	if (_navCursorData.callback) _navCursorData.callback(_navCursorData.mode);
}

void CameraManipulator3d::clearNavCursor()
{
	_navCursorData.isCursorActive = false;
	_navCursorData.mode = NavCursorMode::None;
	if (_navCursorData.callback) _navCursorData.callback(NavCursorMode::None);
}

bool CameraManipulator3d::handleMouseDrag(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &us)
{
	if (ea.getButtonMask() == osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON)
	{
		return false;
	}
	else
	{
		return osgGA::TrackballManipulator::handleMouseDrag(ea, us);
	}
}

bool CameraManipulator3d::performMovementLeftMouseButton(const double eventTimeDelta, const double dx, const double dy)
{
	return true;
}

bool CameraManipulator3d::performMovementMiddleMouseButton(const double eventTimeDelta, const double dx, const double dy)
{
	// 中键+Shift:平移
	if (_ga_t0->getModKeyMask() & osgGA::GUIEventAdapter::MODKEY_SHIFT)
	{
		// 平行投影
		double left, right, bottom, top, zNear, zFar;
		if (_camera->getProjectionMatrixAsOrtho(left, right, bottom, top, zNear, zFar))
		{
			double viewDia = (right - left) > (top - bottom) ?
				right - left : top - bottom;
			float scale = -0.3f * viewDia * getThrowScale(eventTimeDelta);
			panModel(dx*scale, dy*scale);

			return true;
		}
		else // 透视投影
		{
			return OrbitManipulator::performMovementMiddleMouseButton(eventTimeDelta, dx, dy);
		}
	}
	else // 中键:旋转
	{
		return OrbitManipulator::performMovementLeftMouseButton(eventTimeDelta, dx, dy);
	}
}

bool CameraManipulator3d::performMovementRightMouseButton(const double eventTimeDelta, const double dx, const double dy)
{
	return true;
}

bool CameraManipulator3d::handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us)
{
    // added by wangyao 2025.04.13 {
    // OSG框架会默认处理空格键,在StandardManipulator中执行home(ea,aa);此处禁掉.
    if (ea.getKey() == osgGA::GUIEventAdapter::KEY_Space)
    {
        return false;
    }
    // }

	_camera = us.asView() ? us.asView()->getCamera() : nullptr;

	// Switch cursor during middle-button rotate/pan. Only PUSH/DRAG drive the
	// mode, since FRAME (from the viewer's own event queue) always lacks the
	// Shift mask; any event without the middle button clears the navigation cursor.
	const bool midButtonDown = (ea.getButtonMask() & osgGA::GUIEventAdapter::MIDDLE_MOUSE_BUTTON) != 0;
	osgGA::GUIEventAdapter::EventType eventType = ea.getEventType();
	if (midButtonDown && (osgGA::GUIEventAdapter::PUSH == eventType || osgGA::GUIEventAdapter::DRAG == eventType))
	{
		NavCursorMode mode = (ea.getModKeyMask() & osgGA::GUIEventAdapter::MODKEY_SHIFT) ?
			NavCursorMode::Pan : NavCursorMode::Rotate;
		this->setNavCursor(mode);
	}
	else if (_navCursorData.isCursorActive && !midButtonDown)
	{
		this->clearNavCursor();
	}

	return osgGA::StandardManipulator::handle(ea, us);
}

bool CameraManipulator3d::handleMouseWheel(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us)
{
	if (!_camera) return false;

	// 平行投影
	double left, right, bottom, top, zNear, zFar;
	if (_camera->getProjectionMatrixAsOrtho(left, right, bottom, top, zNear, zFar))
	{
		return handleMouseWheelImpl_Ortho(ea, us);
	}
	else // 透视投影
	{
		return handleMouseWheelImpl_Perspective(ea, us);
	}

	return true;
}

bool CameraManipulator3d::handleMouseWheelImpl_Perspective(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us)
{
	if (!_camera) return false;
	const osg::Viewport* viewport = _camera->getViewport();
	if (!viewport) return false;
	osg::Vec3 winCenter(
		viewport->x() + viewport->width() / 2.0f,
		viewport->y() + viewport->height() / 2.0f,
		0.0f);
	osg::Vec3 winCursor(ea.getX(), ea.getY(), 0.0f);

	//
	{
		osg::Matrix VPW = this->getInverseMatrix() * _camera->getProjectionMatrix() *
			_camera->getViewport()->computeWindowMatrix();
		osg::Matrix inverseVPW = osg::Matrix::inverse(VPW);
		osg::Vec3 center = winCenter * inverseVPW;
		osg::Vec3 cursor = winCursor * inverseVPW;

		gp_Pnt pntCenter(_center.x(), _center.y(), _center.z());
		osg::Vec3d eye = _center + _rotation * osg::Vec3d(0.0, 0.0, _distance);
		gp_Pnt pntEye(eye.x(), eye.y(), eye.z());
		gp_Pnt pntCursor(cursor.x(), cursor.y(), cursor.z());
		Handle(Geom_Line) line1 = new Geom_Line(pntEye, gp_Dir(gp_Vec(pntEye, pntCenter)));
		Handle(Geom_Line) line2 = new Geom_Line(pntEye, gp_Dir(gp_Vec(pntEye, pntCursor)));
		osg::Vec3d zaxis = _rotation * osg::Vec3d(0.0, 0.0, 1.0);
		Handle(Geom_Plane) plane = new Geom_Plane(pntCenter, gp_Dir(zaxis.x(), zaxis.y(), zaxis.z()));
		GeomAPI_IntCS intCS;
		intCS.Perform(line1, plane);
		gp_Pnt intPnt1 = intCS.Point(1);
		intCS.Perform(line2, plane);
		gp_Pnt intPnt2 = intCS.Point(1);
		osg::Vec3d panVec(intPnt2.X() - intPnt1.X(), intPnt2.Y() - intPnt1.Y(), intPnt2.Z() - intPnt1.Z());

		_center += panVec;
	}

	osgGA::GUIEventAdapter::ScrollingMotion sm = ea.getScrollingMotion();
	// handle centering
	if (_flags & osgGA::StandardManipulator::SET_CENTER_ON_WHEEL_FORWARD_MOVEMENT)
	{

		if (((sm == osgGA::GUIEventAdapter::SCROLL_DOWN && _wheelZoomFactor > 0.)) ||
			((sm == osgGA::GUIEventAdapter::SCROLL_UP   && _wheelZoomFactor < 0.)))
		{

			if (getAnimationTime() <= 0.)
			{
				// center by mouse intersection (no animation)
				setCenterByMousePointerIntersection(ea, us);
			}
			else
			{
				// start new animation only if there is no animation in progress
				if (!isAnimating())
					startAnimationByMousePointerIntersection(ea, us);

			}
		}
	}
	//
	switch (sm)
	{
		// mouse scroll up event
		case osgGA::GUIEventAdapter::SCROLL_UP:
		{
			// perform zoom
			zoomModel(_wheelZoomFactor, false);
			us.requestRedraw();
			us.requestContinuousUpdate(isAnimating() || _thrown);
			//return true;
		}
		break;

		// mouse scroll down event
		case osgGA::GUIEventAdapter::SCROLL_DOWN:
		{
			// perform zoom
			zoomModel(-_wheelZoomFactor, false);
			us.requestRedraw();
			us.requestContinuousUpdate(isAnimating() || _thrown);
			//return true;
		}
		break;

		// unhandled mouse scrolling motion
		default:
			break;
			//return false;
	}

	{
		osg::Matrix VPW = this->getInverseMatrix() * _camera->getProjectionMatrix() *
			_camera->getViewport()->computeWindowMatrix();
		osg::Matrix inverseVPW = osg::Matrix::inverse(VPW);
		osg::Vec3 center = winCenter * inverseVPW;
		osg::Vec3 cursor = winCursor * inverseVPW;

		gp_Pnt pntCenter(_center.x(), _center.y(), _center.z());
		osg::Vec3d eye = _center + _rotation * osg::Vec3d(0.0, 0.0, _distance);
		gp_Pnt pntEye(eye.x(), eye.y(), eye.z());
		gp_Pnt pntCursor(cursor.x(), cursor.y(), cursor.z());
		Handle(Geom_Line) line1 = new Geom_Line(pntEye, gp_Dir(gp_Vec(pntEye, pntCenter)));
		Handle(Geom_Line) line2 = new Geom_Line(pntEye, gp_Dir(gp_Vec(pntEye, pntCursor)));
		osg::Vec3d zaxis = _rotation * osg::Vec3d(0.0, 0.0, 1.0);
		Handle(Geom_Plane) plane = new Geom_Plane(pntCenter, gp_Dir(zaxis.x(), zaxis.y(), zaxis.z()));
		GeomAPI_IntCS intCS;
		intCS.Perform(line1, plane);
		gp_Pnt intPnt1 = intCS.Point(1);
		intCS.Perform(line2, plane);
		gp_Pnt intPnt2 = intCS.Point(1);
		osg::Vec3d panVec(intPnt2.X() - intPnt1.X(), intPnt2.Y() - intPnt1.Y(), intPnt2.Z() - intPnt1.Z());

		_center -= panVec;
	}
	
	return true;
}

// added by wangyao 2025.03.03 由ChatGPT产生的函数 {
// 计算当前视图代表的最大包围盒
//osg::BoundingBox computeViewBoundingBox(osg::Camera* camera)
//{
//	// 获取摄像机的投影和视图矩阵
//	osg::Matrix projection = camera->getProjectionMatrix();
//	osg::Matrix view = camera->getViewMatrix();
//	// 合并后取逆矩阵，将NDC下的点转换到世界坐标系
//	osg::Matrix invMat = osg::Matrix::inverse(projection * view);
//
//	// NDC 下的 8 个角点
//	osg::Vec3 ndcCorners[8] = {
//		osg::Vec3(-1.0f, -1.0f, -1.0f), // near-bottom-left
//		osg::Vec3(1.0f, -1.0f, -1.0f), // near-bottom-right
//		osg::Vec3(-1.0f,  1.0f, -1.0f), // near-top-left
//		osg::Vec3(1.0f,  1.0f, -1.0f), // near-top-right
//		osg::Vec3(-1.0f, -1.0f,  1.0f), // far-bottom-left
//		osg::Vec3(1.0f, -1.0f,  1.0f), // far-bottom-right
//		osg::Vec3(-1.0f,  1.0f,  1.0f), // far-top-left
//		osg::Vec3(1.0f,  1.0f,  1.0f)  // far-top-right
//	};
//
//	osg::BoundingBox bb;
//	for (int i = 0; i < 8; ++i)
//	{
//		// 将归一化坐标转换为齐次坐标，再转换为世界坐标
//		osg::Vec3 worldCorner = ndcCorners[i] * invMat;
//		bb.expandBy(worldCorner);
//	}
//	return bb;
//}
// }

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
		osg::Vec3( 1.0f, -1.0f, 0.0f),
		osg::Vec3(-1.0f,  1.0f, 0.0f),
		osg::Vec3( 1.0f,  1.0f, 0.0f)
	};

	osg::BoundingBox bb;
	for (int i = 0; i < 4; ++i)
	{
		osg::Vec3 worldCorner = ndcCorners[i] * invMat;
		bb.expandBy(worldCorner);
	}
	return bb;
}

bool CameraManipulator3d::handleMouseWheelImpl_Ortho(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us)
{
	//
	if (!_camera) return false;
	const osg::Viewport* viewport = _camera->getViewport();
	if (!viewport) return false;
	osg::Vec3 winCenter(
		viewport->x() + viewport->width() / 2.0f,
		viewport->y() + viewport->height() / 2.0f,
		0.0f);
	osg::Vec3 winCursor(ea.getX(), ea.getY(), 0.0f);

	//
	{
		osg::Matrix PW = _camera->getProjectionMatrix() * _camera->getViewport()->computeWindowMatrix();
		osg::Matrix inversePW = osg::Matrix::inverse(PW);
		osg::Vec3 centerInView = winCenter * inversePW;
		osg::Vec3 cursorInView = winCursor * inversePW;
		osg::Vec3 moveVec = cursorInView - centerInView;
		panModel(moveVec.x(), moveVec.y());
	}

    // 获取摄像机信息
    osg::Vec3d eye;
    osg::Vec3d center;
    osg::Vec3d up;
    this->getTransformation(eye, center, up);

    // added by wangyao 2025.05.27 {
    // 在缩放视图时,根据Pick到的对象调整摄像机观察的中心点到Pick交点
    osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector = new osgUtil::LineSegmentIntersector(
        osgUtil::Intersector::WINDOW, ea.getX(), ea.getY());
    intersector->setIntersectionLimit(osgUtil::Intersector::IntersectionLimit::LIMIT_ONE_PER_DRAWABLE); // 优化性能
    osgUtil::IntersectionVisitor iv(intersector.get());
    iv.setTraversalMask(PICK_MASK);
    _camera->accept(iv);
    if (intersector->getIntersections().empty())
    {
        osgUtil::IntersectionVisitor iv(intersector.get());
        iv.setTraversalMask(~PICK_MASK);
        _camera->accept(iv);
    }
    const osgUtil::LineSegmentIntersector::Intersections& intersections = intersector->getIntersections();
    for (auto iter = intersections.cbegin(); iter != intersections.cend(); ++iter)
    {
        const osgUtil::LineSegmentIntersector::Intersection& intersection = *iter;
        osg::Vec3d lookVec = center - eye;
        osg::Vec3d lookDir = lookVec;
        lookDir.normalize();
        center = eye + lookDir * ((intersection.getWorldIntersectPoint() - eye) * lookDir);
        eye = center - lookVec;
        this->setTransformation(eye, center, up);
        break;
    }
    // }

	//
	double left, right, bottom, top, zNear, zFar;
	if (_camera->getProjectionMatrixAsOrtho(left, right, bottom, top, zNear, zFar))
	{
        // added by wangyao 2025.03.04 {
        // 计算出当前视图的场景大小,根据视图场景动态调整近远裁剪面
        osg::BoundingBox viewBBox = computeViewBoundingBoxInOrtho(_camera);
        osg::Vec3d eye, center, up;
        this->getTransformation(eye, center, up);
        double viewRadius = viewBBox.radius() * 1.6;
        double lookDistance = (center - eye).length();
        // }

		osgGA::GUIEventAdapter::ScrollingMotion sm = ea.getScrollingMotion();
		if (osgGA::GUIEventAdapter::SCROLL_UP == sm)
		{
            // added by wangyao 2025.05.05 {
            // 限制最大视图半径为1000万
			double scale = viewRadius <= 1e7 ? (1.0 + _wheelZoomFactor) : 1.0;
            // }
            // added by wangyao 2025.03.04 {
            // 动态调整zNear与zFar
            double delta = viewRadius * scale;
            if (delta > _maxNearFarDis) delta = _maxNearFarDis;
            zNear = lookDistance - delta;
            zFar = lookDistance + delta;
            // }

			_camera->setProjectionMatrixAsOrtho(left * scale, right * scale, bottom * scale, top * scale, zNear, zFar);
			us.requestRedraw();
			us.requestContinuousUpdate(isAnimating() || _thrown);
		}
		else
		{
            // added by wangyao 2025.05.05 {
            // 限制最小视图半径为1e-5
            double scale = viewRadius >= 1e-5 ? (1.0 - _wheelZoomFactor) : 1.0;
            // }
            // added by wangyao 2025.03.04 {
            // 动态调整zNear与zFar
            double delta = viewRadius * scale;
            if (delta > _maxNearFarDis) delta = _maxNearFarDis;
            zNear = lookDistance - delta;
            zFar = lookDistance + delta;
            // }
			_camera->setProjectionMatrixAsOrtho(left * scale, right * scale, bottom * scale, top * scale, zNear, zFar);
			us.requestRedraw();
			us.requestContinuousUpdate(isAnimating() || _thrown);
		}
	}

	//
	{
		osg::Matrix PW = _camera->getProjectionMatrix() * _camera->getViewport()->computeWindowMatrix();
		osg::Matrix inversePW = osg::Matrix::inverse(PW);
		osg::Vec3 centerInView = winCenter * inversePW;
		osg::Vec3 cursorInView = winCursor * inversePW;
		osg::Vec3 moveVec = cursorInView - centerInView;
		panModel(-moveVec.x(), -moveVec.y());
	}

	return true;
}