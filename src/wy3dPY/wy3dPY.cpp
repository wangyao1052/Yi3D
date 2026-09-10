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

#include <pybind11/pybind11.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>

namespace py = pybind11;

extern void bindWyErrorStatus(py::module_& m);
extern void bindWy3d(py::module_& m);

extern void bindWyrxObject(py::module_& m);
extern void bindWydbElementId(py::module_& m);
extern void bindWydbParameter(py::module_& m);
extern void bindWydbElement(py::module_& m);

extern void bindWydbDatabase(py::module_& m);

extern void bindWydbTransaction(py::module_& m);

extern void bindWyapCmdManager(py::module_& m);
extern void bindWyapDocManager(py::module_& m);
extern void bindWyapDocument(py::module_& m);
extern void bindWyapSelection(py::module_& m);

extern void bindWy3dFeature(py::module_& m);
extern void bindWy3dSolids(py::module_& m);
extern void bindWy3dPrimitives(py::module_& m);
extern void bindWy3dBoolean(py::module_& m);
extern void bindWy3dSketch(py::module_& m);
extern void bindWy3dSketch3D(py::module_& m);
extern void bindWy3dFilledSheet(py::module_& m);
extern void bindWy3dDatum(py::module_& m);
extern void bindWy3dCurve(py::module_& m);
extern void bindWy3dSolidModifications(py::module_& m);
extern void bindWy3dPatterns(py::module_& m);
extern void bindWy3dColor(py::module_& m);
extern void bindWy3dImportedSolid(py::module_& m);

PYBIND11_MODULE(wy3d, m)
{
    bindWy3d(m);

    bindWyErrorStatus(m);
    bindWyrxObject(m);

    bindWydbElementId(m);

    bindWydbElement(m);
    bindWydbParameter(m);
    bindWydbDatabase(m);

    bindWydbTransaction(m);

    bindWyapCmdManager(m);
    bindWyapDocManager(m);
    bindWyapDocument(m);
    bindWyapSelection(m);

    bindWy3dFeature(m);
    bindWy3dSolids(m);
    bindWy3dPrimitives(m);
    bindWy3dBoolean(m);
    bindWy3dSketch(m);
    bindWy3dSketch3D(m);
    bindWy3dFilledSheet(m);
    bindWy3dDatum(m);
    bindWy3dCurve(m);
    bindWy3dSolidModifications(m);
    bindWy3dPatterns(m);
    bindWy3dColor(m);
    bindWy3dImportedSolid(m);
}
