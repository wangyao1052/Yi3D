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

#ifndef WY3DAPP_SKETCH3D_DIRECTION_TRANSIENT_H
#define WY3DAPP_SKETCH3D_DIRECTION_TRANSIENT_H

#include <memory>

#include <wyVector3.h>

#include "GuiCmdTransient.h"

// An arrow rooted at a point and pointing along a direction. It is drawn at a size that does not
// change with the model or with the zoom, the way the gizmo arrows are: the question it answers -
// which way is this going to sweep - has to be just as readable on a 2 mm sketch as on a 2 m one.
//
// A bidirectional arrow is a single double headed one, centred on the root with an arm each way,
// which is how the flag reads: both sides, from the same place.
class Sketch3DDirectionTransient : public GuiCmdTransient
{
public:
    // A direction of no length draws nothing; anything else is normalized.
    Sketch3DDirectionTransient(const wy::Vector3& origin, const wy::Vector3& direction,
        bool bidirectional);

    virtual ~Sketch3DDirectionTransient();
};

typedef std::shared_ptr<Sketch3DDirectionTransient> Sketch3DDirectionTransientSPtr;

#endif // WY3DAPP_SKETCH3D_DIRECTION_TRANSIENT_H
