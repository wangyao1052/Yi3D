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

#pragma once

#include <osgGA/TrackballManipulator>
#include <functional>

enum class NavCursorMode
{
    None = 0,
    Rotate = 1,
    Pan = 2,
};

class CameraManipulator3d : public osgGA::TrackballManipulator
{
public:
    CameraManipulator3d();

	virtual void setNode(osg::Node*) override;
	void setModelSize(double modelSize);
	void setNavCursorCallback(std::function<void(NavCursorMode)> callback);

protected:
	virtual bool handleMouseDrag(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &us);
	
	virtual bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us);

	//
	virtual bool performMovementLeftMouseButton(const double eventTimeDelta, const double dx, const double dy);
	virtual bool performMovementMiddleMouseButton(const double eventTimeDelta, const double dx, const double dy);
	virtual bool performMovementRightMouseButton(const double eventTimeDelta, const double dx, const double dy);
	virtual bool handleMouseWheel(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us);

	bool handleMouseWheelImpl_Perspective(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us);
	bool handleMouseWheelImpl_Ortho(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us);

private:
	void setNavCursor(NavCursorMode mode);
	void clearNavCursor();

	osg::Camera* _camera;
    double _maxNearFarDis;

	struct NavCursorData
	{
		std::function<void(NavCursorMode)> callback;
		bool isCursorActive;
		NavCursorMode mode;

		NavCursorData()
		{
			callback = nullptr;
			isCursorActive = false;
			mode = NavCursorMode::None;
		}
	} _navCursorData;
};