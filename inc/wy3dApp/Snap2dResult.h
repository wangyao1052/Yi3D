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

#ifndef WY3DAPP_SNAP2D_RESULT_H
#define WY3DAPP_SNAP2D_RESULT_H

#include <memory>
#include <vector>
#include <wyVector2.h>
#include <wydbElementId.h>
#include <wy3dPlane.h>
#include <wy3dApp/Snap2dType.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

struct Snap2dHit
{
    Snap2dType type = Snap2dType::Undefined;

    // The element this hit refers to; null when there is none.
    wydb::ElementId element = wydb::ElementId::kNull;

    wy::Vector2 point = wy::Vector2::kZero;
};

class WY3DAPP_EXPORT Snap2dResult
{
public:
    virtual wy::Vector2 getPosition() const = 0;

    virtual const std::vector<Snap2dHit>& getHits() const = 0;

    virtual const wy3d::Plane& getWorkPlane() const = 0;

protected:
    Snap2dResult();
    virtual ~Snap2dResult();
    Snap2dResult(const Snap2dResult&) = delete;
    Snap2dResult& operator=(const Snap2dResult&) = delete;
    Snap2dResult(Snap2dResult&&) = delete;
    Snap2dResult& operator=(Snap2dResult&&) = delete;
};
typedef std::shared_ptr<Snap2dResult> Snap2dResultSPtr;

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_RESULT_H
