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

#include <geom/wy3dMatrix3.h>

NS_WY3D_GEOM_BEG

const Matrix3 Matrix3::kZero = Matrix3(
    0, 0, 0,
    0, 0, 0,
    0, 0, 0
);

const Matrix3 Matrix3::kIdentity = Matrix3(
    1, 0, 0,
    0, 1, 0,
    0, 0, 1
);

NS_WY3D_GEOM_END