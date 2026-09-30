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

#include <vector>

#include <osg/Array>
#include <osg/Camera>
#include <osg/MatrixTransform>
#include <osg/Vec3d>
#include <osg/Viewport>
#include <osg/observer_ptr>
#include <osgGA/GUIActionAdapter>
#include <osgGA/GUIEventAdapter>
#include <osgViewer/View>

// Navigation cube in the top right corner of the viewport (CrownCAD style).
//
// An overlay camera: screen space orthographic projection, pinned to the top right corner, never
// takes event focus, transparent to element picking. The body is a rounded box: the Minkowski sum
// of a box with a ball, so faces stay flat and the edges and corners are tangent continuous. It
// follows the camera; a click on a face gives the standard view, a click on an edge or corner
// region gives an oblique one.
class ViewCube : public osg::Camera
{
public:
    ViewCube(osgViewer::View* pView);

    // true if the event was consumed: the caller must stop dispatching it
    bool handleEvent(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa);

    // Drop the hover highlight. OSG 3.6 delivers no LEAVE event, so the viewport widget has
    // to report the cursor leaving on its own.
    void clearHover();

private:
    class CubeCullCallback;

    void buildCube();
    void buildLabels();

    // Per frame: pin the viewport to the top right corner and orient the cube
    void refresh();
    // Read the camera basis from the manipulator; false if unavailable
    bool computeBasis();
    // Apply the standard view of one facet
    void applyView(int facet);
    // Hit facet index, -1 when nothing is hit
    int pickFacet(float winX, float winY);
    // true if the hovered facet changed
    bool setHoverFacet(int facet);
    void refreshColors();

private:
    // The view dies before the scene (see viewport-on-demand-rendering)
    osg::observer_ptr<osgViewer::View> _pView;
    // Held here so the cull callback can update it in place instead of allocating every frame
    osg::ref_ptr<osg::Viewport> _pViewport;
    osg::ref_ptr<osg::MatrixTransform> _pCubeXform;
    osg::ref_ptr<osg::Vec3Array> _pVertices;
    osg::ref_ptr<osg::Vec4Array> _pFillColors;
    // Region of every vertex: a face (0..5), an edge (6..17) or a corner (18..25)
    std::vector<int> _vertexRegions;
    // Colours without the hover highlight, which refreshColors falls back on
    std::vector<osg::Vec4> _baseColors;
    // One colour array per label plate, so a hover can recolour a single plate
    std::vector<osg::ref_ptr<osg::Vec4Array>> _plateColors;

    // Camera basis, shared by the cull callback and picking
    osg::Vec3d _right;
    osg::Vec3d _up;
    osg::Vec3d _forward;

    int _hoverFacet = -1;
    int _pressedFacet = -1;
    bool _isPressed = false;
};
