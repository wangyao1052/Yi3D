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

#ifndef WY3DAPP_MEASURE_UTIL_H
#define WY3DAPP_MEASURE_UTIL_H

#include <TopoDS_Shape.hxx>

#include <utility>

class MeasureUtil
{
public:
    // edgeIndex is 0-based, same ordering as TopoShapeUtil::getEdge
    static std::pair<bool, double> edgeLength(const TopoDS_Shape& shape, unsigned int edgeIndex);
    // faceIndex is 0-based, same ordering as TopoShapeUtil::getFace
    static std::pair<bool, double> faceArea(const TopoDS_Shape& shape, unsigned int faceIndex);
    // sum of all boundary wire lengths, including inner holes
    static std::pair<bool, double> facePerimeter(const TopoDS_Shape& shape, unsigned int faceIndex);
    static std::pair<bool, double> bodyVolume(const TopoDS_Shape& shape);
    static std::pair<bool, double> bodySurfaceArea(const TopoDS_Shape& shape);
};

#endif // WY3DAPP_MEASURE_UTIL_H
