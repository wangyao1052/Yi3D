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

#ifndef WY3DAPP_SNAP_CONSTS_H
#define WY3DAPP_SNAP_CONSTS_H

#include <osg/Vec4>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SnapConsts
{
public:
    static inline float PickSize = 10; // 以鼠标为中心上下左右分别偏移的距离

    // 默认点大小和颜色
    // 端点+中点+中心点+切点+垂足
    static inline const float PointSize = 10.0f;
    static inline const osg::Vec4 PointColor = osg::Vec4(0.7f, 0.0f, 0.0f, 1.0f); // 红色

    // 点在曲线上的点大小和颜色
    static inline const float PointSizeOnSketchCurve = 8.0f;
    static inline const osg::Vec4 PointColorOnSketchCurve = osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f); // 白色
};

}

#endif // WY3DAPP_SNAP_CONSTS_H
