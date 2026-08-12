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

#ifndef WY3D_REVOLUTION_UTIL_H
#define WY3D_REVOLUTION_UTIL_H

#include <gp_Ax1.hxx>
#include <wy3dDefs.h>
#include <wy3dErrorCode.h>
#include <wydbElementId.h>

namespace wydb { class Database; }

NS_WY3D_BEG

// 从草图曲线 ID 计算旋转轴（用于旋转体和旋转曲面）
// 返回 ErrorCode + gp_Ax1；失败时 errorCode 非 NoError
std::pair<ErrorCode, gp_Ax1> computeRevolutionAxis(
    const wydb::Database* pDb,
    const wydb::ElementId& axisCurveId);

NS_WY3D_END

#endif // WY3D_REVOLUTION_UTIL_H
