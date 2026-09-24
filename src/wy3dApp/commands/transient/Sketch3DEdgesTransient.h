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

#ifndef WY3DAPP_SKETCH3D_EDGES_TRANSIENT_H
#define WY3DAPP_SKETCH3D_EDGES_TRANSIENT_H

#include <memory>
#include <vector>

#include <TopoDS_Edge.hxx>

#include "GuiCmdTransient.h"

// A group of bare BRep edges, drawn in the same orange as the other previews. Its twin
// Sketch3DCurveTransient samples entities that already exist; this one is for the commands that
// have computed the geometry but have not turned it into entities yet - a projection or a section
// result, which is a TopoDS_Edge until the user confirms it.
class Sketch3DEdgesTransient : public GuiCmdTransient
{
public:
    explicit Sketch3DEdgesTransient(const std::vector<TopoDS_Edge>& edges);

    virtual ~Sketch3DEdgesTransient();

private:
    // Samples to a polyline and builds the geometry, appended under _root, so it can be called once
    // per edge. Deliberately a copy of Sketch3DCurveTransient::initGeom rather than a shared base
    // method: the two are the same drawing today, and keeping the newer one self contained leaves
    // the long standing preview class untouched. Change one, change the other.
    void addLineGeometry(const std::vector<wy::Vector3>& points, bool closed);
};

typedef std::shared_ptr<Sketch3DEdgesTransient> Sketch3DEdgesTransientSPtr;

#endif // WY3DAPP_SKETCH3D_EDGES_TRANSIENT_H
