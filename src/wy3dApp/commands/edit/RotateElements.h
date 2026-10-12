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

#ifndef WY3DAPP_ROTATE_ELEMENTS_H
#define WY3DAPP_ROTATE_ELEMENTS_H

#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/ref_ptr>
#include <osg/Array>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapSelection.h>

#include <wy3dApp/GuiCommand.h>
#include <wy3dApp/GuiCmdTransient.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class RotateElements : public GuiCmdMakeElement
{
public:
    RotateElements(GuiCommand* pGuiCmd);
    ~RotateElements();

    bool init(const wyap::SelectionSet& ss,
        const wy::Vector3& centerPnt, const wy::Vector2& centerPnt2,
        const wy::Vector3& axisDir);
    bool update(double rotateAngle);
    bool perform(const wyap::SelectionSet& ss, double rotateAngle);

private:
    bool performImpl_Modeling(const std::set<wydb::ElementId>& ids, double rotateAngle);
    bool performImpl_Sketching(const std::set<wydb::ElementId>& ids, double rotateAngle);

private:
    wy::Vector2 _centerPnt2;
    wy::Vector3 _centerPnt;
    wy::Vector2 _axisDir2;
    wy::Vector3 _axisDir;
    osg::ref_ptr<osg::MatrixTransform> _matrixTransform;
};

class RotateTransient : public GuiCmdTransient
{
public:
    RotateTransient();
    ~RotateTransient();

    void show();
    void showBaseLine();
    void showRotateLine();
    void hide();
    void hideBaseLine();
    void hideRotateLine();
    void updateBaseLine(const wy::Vector3& pnt1, wy::Vector3 pnt2);
    void updateRotateLine(const wy::Vector3& pnt1, const wy::Vector3& pnt2);

private:
    void initGeom(osg::ref_ptr<osg::Geometry>& geom, osg::ref_ptr<osg::Vec3Array>& vertices);

private:
    osg::ref_ptr<osg::Geometry> _geomBaseLine;
    osg::ref_ptr<osg::Geometry> _geomRotateLine;
    osg::ref_ptr<osg::Vec3Array> _verticesBaseLine;
    osg::ref_ptr<osg::Vec3Array> _verticesRotateLine;
};

}

#endif // WY3DAPP_ROTATE_ELEMENTS_H
