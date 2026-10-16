///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#ifndef WY3DAPP_SNAP2D_SYSTEM_H
#define WY3DAPP_SNAP2D_SYSTEM_H

#include <memory>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wy3dPlane.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace osgViewer { class View; }

namespace wy3dApp {

class Snap2dResult;
class SketchSnapContext;
class Snap2dSystem;
typedef std::shared_ptr<Snap2dResult> Snap2dResultSPtr;
typedef std::shared_ptr<SketchSnapContext> SketchSnapContextSPtr;
typedef std::shared_ptr<Snap2dSystem> Snap2dSystemSPtr;

// 2D snap system: plane-based snapping for interactive commands.
class WY3DAPP_EXPORT Snap2dSystem
{
public:
    // Create a snap system working on the given plane.
    static Snap2dSystemSPtr create(const wy3d::Plane& workPlane);

    // Partial update from database changes.
    virtual bool partiallyUpdate(const wydb::Database* pDb) = 0;

    // Snap a position; returns null when nothing is snapped.
    virtual Snap2dResultSPtr snap(
        double x, double y, osgViewer::View* pView,
        const wy::Vector2& pos, SketchSnapContextSPtr pContext) = 0;

    // Get the snap result.
    virtual Snap2dResultSPtr getSnapResult() const = 0;
    // Set the snap result.
    virtual void setSnapResult(Snap2dResultSPtr pSnapResult) = 0;
    // Clear the snap result.
    virtual void clearSnapResult() = 0;

protected:
    Snap2dSystem();
    virtual ~Snap2dSystem();
    Snap2dSystem(const Snap2dSystem&) = delete;
    Snap2dSystem& operator=(const Snap2dSystem&) = delete;
    Snap2dSystem(Snap2dSystem&&) = delete;
    Snap2dSystem& operator=(Snap2dSystem&&) = delete;
};

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_SYSTEM_H
