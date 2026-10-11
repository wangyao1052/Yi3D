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

#ifndef WY3DAPP_SKETCH_RECT_ARRAY_ELEMENTS_H
#define WY3DAPP_SKETCH_RECT_ARRAY_ELEMENTS_H

#include <vector>

#include <osg/PositionAttitudeTransform>
#include <osg/ref_ptr>

#include <wydbElementId.h>
#include <wy3dPlane.h>

#include <wy3dApp/GuiCommand.h>

class SketchRectArrayElements : public GuiCmdMakeElement
{
public:
    SketchRectArrayElements(GuiCommand* pGuiCmd, const wy3d::Plane& sketchPlane, wydb::ElementId sketchId);
    ~SketchRectArrayElements();

    bool init(const std::set<wydb::ElementId>& ids, unsigned int cols, unsigned int rows, double colSpacing, double rowSpacing);
    bool update(double colSpacing, double rowSpacing);
    bool perform(const std::set<wydb::ElementId>& ids, unsigned int cols, unsigned int rows, double colSpacing, double rowSpacing);

private:
    unsigned int _cols;
    unsigned int _rows;
    std::vector<osg::ref_ptr<osg::PositionAttitudeTransform>> _pats;
    wy3d::Plane _sketchPlane;
    wydb::ElementId _sketchId;
};

#endif // WY3DAPP_SKETCH_RECT_ARRAY_ELEMENTS_H
