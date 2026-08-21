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

#ifndef WY3DAPP_SKETCH_UTIL_H
#define WY3DAPP_SKETCH_UTIL_H

#include <QString>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wy3dSketch.h>

class SketchUtil
{
public:
    // 是否有未使用的草图
    // 在进入拉伸&旋转&扫掠命令时,当没有未使用的草图时,需要提示用户并退出命令
    static bool hasUnusedSketch(const wydb::Database* pDb);

    // 获取草图的原点
    // 请确保传入的是草图的ID;如果失败会返回(0,0,0).
    static wy::Vector3 getSketchOrigin(const wydb::Database* pDb, const wydb::ElementId& sketchId);

    static bool isValidExtrusionProfile(const wy3d::Sketch& sketch, QString& error)
    {
        return isValidProfile(sketch, error);
    }

    static bool isValidRevolutionProfile(const wy3d::Sketch& sketch, QString& error);

    // 扫描体
    static bool isValidSweepPath(const wy3d::Sketch& sketch, QString& error);
    static bool isValidSweepProfile(const wy3d::Sketch& sketch, QString& error);

    // 放样体
    static bool isValidLoftProfile(const wy3d::Sketch& sketch, QString& error);

    // 螺旋线
    static bool isValidHelixProfile(const wy3d::Sketch& sketch, QString& error);

    static bool isValidProfileForPlanarSheet(const wy3d::Sketch& sketch, QString& error)
    {
        return isValidProfile(sketch, error);
    }

    static bool isValidProfileForExtrudedSheet(const wy3d::Sketch& sketch, QString& error)
    {
        return isValidProfileForSheet(sketch, error);
    }

    static bool isValidProfileForRevolvedSheet(const wy3d::Sketch& sketch, QString& error)
    {
        return isValidProfileForSheet(sketch, error);
    }

    static bool isValidProfileForSweptSheet(const wy3d::Sketch& sketch, QString& error)
    {
        return isValidProfileForSheet(sketch, error);
    }

    static bool isValidPathForSweptSheet(const wy3d::Sketch& sketch, QString& error)
    {
        return isValidSweepPath(sketch, error);
    }

    static bool isValidProfileForLoftedSheet(const wy3d::Sketch& sketch, QString& error);

private:
    static bool isValidProfile(const wy3d::Sketch& sketch, QString& error);
    static bool isValidProfileForSheet(const wy3d::Sketch& sketch, QString& error);

    static bool isValidPath(const wy3d::Sketch& sketch, QString& error);
};

#endif // WY3DAPP_SKETCH_UTIL_H