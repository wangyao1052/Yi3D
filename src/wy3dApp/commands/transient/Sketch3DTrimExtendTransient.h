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

#ifndef WY3DAPP_SKETCH3D_TRIM_EXTEND_TRANSIENT_H
#define WY3DAPP_SKETCH3D_TRIM_EXTEND_TRANSIENT_H

#include <memory>

#include <wydbElementId.h>
#include <wy3dSketchCurve3D.h>

#include "GuiCmdTransient.h"

// The piece of a 3D sketch curve a trim or an extend would act on, drawn in the same orange as the
// 2D commands use. The range may run past the curve's own ends, which is exactly what an extension
// preview is, so the piece is evaluated directly rather than through SketchCurve3D::getPointAt.
class Sketch3DTrimExtendTransient : public GuiCmdTransient
{
public:
    Sketch3DTrimExtendTransient(const wy3d::SketchCurve3D* pCurve, double startParam, double endParam);
    virtual ~Sketch3DTrimExtendTransient();

    wydb::ElementId getId() const { return _id; }
    double getStartParam() const { return _startParam; }
    double getEndParam() const { return _endParam; }

private:
    wydb::ElementId _id;
    double _startParam;
    double _endParam;
};

typedef std::shared_ptr<Sketch3DTrimExtendTransient> Sketch3DTrimExtendTransientSPtr;

#endif // WY3DAPP_SKETCH3D_TRIM_EXTEND_TRANSIENT_H
