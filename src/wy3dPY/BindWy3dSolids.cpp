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
#include <pybind11/stl.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSolid.h>
#include <wy3dExtrusion.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dRevolution.h>
#include <wy3dSweep.h>
#include <wy3dLoft.h>
#include <wy3dSketch.h>
#include <wy3dCurve.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dColor.h>

namespace py = pybind11;

void bindWy3dSolids(py::module_& m)
{
    py::class_ <wy3d::Solid, wy3d::Feature, std::unique_ptr<wy3d::Solid, py::nodelete >>(m, "Solid")
        .def("isCut", &wy3d::Solid::isCut)
        .def("setCut", &wy3d::Solid::setCut)
        .def("getColor", &wy3d::Solid::getColor)
        .def("setColor", &wy3d::Solid::setColor);

    py::enum_<wy3d::ExtrusionDirection>(m, "ExtrusionDirection")
        .value("OneSide", wy3d::ExtrusionDirection::OneSide)
        .value("Symmetric", wy3d::ExtrusionDirection::Symmetric)
        .def("__repr__", [](wy3d::ExtrusionDirection direction) {
            switch (direction) {
            case wy3d::ExtrusionDirection::OneSide: return "wy3d.ExtrusionDirection.OneSide";
            case wy3d::ExtrusionDirection::Symmetric: return "wy3d.ExtrusionDirection.Symmetric";
            default: return "wy3d.ExtrusionDirection.Unknown";
            }});

    py::class_<wy3d::Extrusion, wy3d::Solid, std::unique_ptr<wy3d::Extrusion, py::nodelete>>(m, "Extrusion")
        .def("getSketch", &wy3d::Extrusion::getSketch)
        .def("getDepth", &wy3d::Extrusion::getDepth)
        .def("setDepth", &wy3d::Extrusion::setDepth)
        .def("getStartOffset", &wy3d::Extrusion::getStartOffset)
        .def("setStartOffset", &wy3d::Extrusion::setStartOffset)
        .def("getDirection", &wy3d::Extrusion::getDirection)
        .def("setDirection", &wy3d::Extrusion::setDirection)

        .def_static("create",
            [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch, double depth) -> wy3d::Extrusion*
            {
                wy3d::Extrusion* pOutExtrusion = nullptr;
                wy::ErrorStatus status = wy3d::Extrusion::create(pTrans, pSketch, depth, pOutExtrusion);
                return pOutExtrusion;
            },
            py::arg("transaction"),
            py::arg("sketch"),
            py::arg("depth"),
            py::return_value_policy::reference)

        .def_static("create",
            [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch,
               wy3d::ExtrusionDirection direction, double depth) -> wy3d::Extrusion*
            {
                wy3d::Extrusion* pOutExtrusion = nullptr;
                wy::ErrorStatus status = wy3d::Extrusion::create(pTrans, pSketch, direction, depth, pOutExtrusion);
                return pOutExtrusion;
            },
            py::arg("transaction"),
            py::arg("sketch"),
            py::arg("direction"),
            py::arg("depth"),
            py::return_value_policy::reference)

        .def_static("createCut",
            [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch, double depth, wy3d::Solid* pSolidToCut) -> wy3d::Extrusion*
            {
                wy3d::Extrusion* pOutExtrusion = nullptr;
                wy::ErrorStatus status = wy3d::Extrusion::createCut(pTrans, pSketch, depth, pSolidToCut, pOutExtrusion);
                return pOutExtrusion;
            },
            py::arg("transaction"),
            py::arg("sketch"),
            py::arg("depth"),
            py::arg("solidToCut"),
            py::return_value_policy::reference)

        .def_static("createCut",
            [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch,
               wy3d::ExtrusionDirection direction, double depth, wy3d::Solid* pSolidToCut) -> wy3d::Extrusion*
            {
                wy3d::Extrusion* pOutExtrusion = nullptr;
                wy::ErrorStatus status = wy3d::Extrusion::createCut(pTrans, pSketch, direction, depth, pSolidToCut, pOutExtrusion);
                return pOutExtrusion;
            },
            py::arg("transaction"),
            py::arg("sketch"),
            py::arg("direction"),
            py::arg("depth"),
            py::arg("solidToCut"),
            py::return_value_policy::reference);

    py::class_<wy3d::ExtrudedSheet, wy3d::Feature, std::unique_ptr<wy3d::ExtrudedSheet, py::nodelete>>(m, "ExtrudedSheet")
        .def("getSketch", &wy3d::ExtrudedSheet::getSketch)
        .def("getDepth", &wy3d::ExtrudedSheet::getDepth)
        .def("setDepth", &wy3d::ExtrudedSheet::setDepth)
        .def("getStartOffset", &wy3d::ExtrudedSheet::getStartOffset)
        .def("setStartOffset", &wy3d::ExtrudedSheet::setStartOffset)
        .def("getDirection", &wy3d::ExtrudedSheet::getDirection)
        .def("setDirection", &wy3d::ExtrudedSheet::setDirection)

        .def_static("create",
            [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch, double depth) -> wy3d::ExtrudedSheet*
            {
                wy3d::ExtrudedSheet* pOutSheet = nullptr;
                wy::ErrorStatus status = wy3d::ExtrudedSheet::create(pTrans, pSketch, depth, pOutSheet);
                return pOutSheet;
            },
            py::arg("transaction"),
            py::arg("sketch"),
            py::arg("depth"),
            py::return_value_policy::reference)

        .def_static("create",
            [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch,
               wy3d::ExtrusionDirection direction, double depth) -> wy3d::ExtrudedSheet*
            {
                wy3d::ExtrudedSheet* pOutSheet = nullptr;
                wy::ErrorStatus status = wy3d::ExtrudedSheet::create(pTrans, pSketch, direction, depth, pOutSheet);
                return pOutSheet;
            },
            py::arg("transaction"),
            py::arg("sketch"),
            py::arg("direction"),
            py::arg("depth"),
            py::return_value_policy::reference);

    py::class_<wy3d::Revolution, wy3d::Solid, std::unique_ptr<wy3d::Revolution, py::nodelete>>(m, "Revolution")
        .def("getSketch", &wy3d::Revolution::getSketch)
        .def("getAxis", &wy3d::Revolution::getAxis)
        .def("setAxis", &wy3d::Revolution::setAxis)
        .def("getStartAngle", &wy3d::Revolution::getStartAngle)
        .def("setStartAngle", &wy3d::Revolution::setStartAngle)
        .def("getEndAngle", &wy3d::Revolution::getEndAngle)
        .def("setEndAngle", &wy3d::Revolution::setEndAngle)

        // 4 参数：自动找第一条中心线（向后兼容）
        .def_static("create",
            [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch, double startAngle, double endAngle) -> wy3d::Revolution*
            {
                if (!pTrans || !pSketch) return nullptr;
                wy3d::Revolution* pOutRevolution = nullptr;
                const wy3d::SketchCurve* pAxis(nullptr);
                for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
                {
                    const wydb::Element* pElem = pSketch->getDatabase()->getElement(iter.current());
                    const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pElem);
                    if (!pCenterLine) continue;
                    pAxis = pCenterLine;
                    break;
                }
                if (pAxis)
                {
                    wy::ErrorStatus status = wy3d::Revolution::create(pTrans, pSketch, pAxis, startAngle, endAngle, pOutRevolution);
                }
                return pOutRevolution;
            },
            py::arg("transaction"),
            py::arg("sketch"),
            py::arg("startAngle"),
            py::arg("endAngle"),
            py::return_value_policy::reference)

        // 5 参数：显式指定轴曲线
        .def_static("create",
            [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch, wy3d::SketchCurve* pAxis, double startAngle, double endAngle) -> wy3d::Revolution*
            {
                if (!pTrans || !pSketch || !pAxis) return nullptr;
                wy3d::Revolution* pOutRevolution = nullptr;
                wy::ErrorStatus status = wy3d::Revolution::create(pTrans, pSketch, pAxis, startAngle, endAngle, pOutRevolution);
                return pOutRevolution;
            },
            py::arg("transaction"),
            py::arg("sketch"),
            py::arg("axis"),
            py::arg("startAngle"),
            py::arg("endAngle"),
            py::return_value_policy::reference)

        // 5 参数：自动找第一条中心线（向后兼容）
        .def_static("createCut",
            [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch, double startAngle, double endAngle, wy3d::Solid* pSolidToCut) -> wy3d::Revolution*
            {
                if (!pTrans || !pSketch || !pSolidToCut) return nullptr;
                wy3d::Revolution* pOutRevolution = nullptr;
                const wy3d::SketchCurve* pAxis(nullptr);
                for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
                {
                    const wydb::Element* pElem = pSketch->getDatabase()->getElement(iter.current());
                    const wy3d::SketchCenterLine* pCenterLine = wy3d::SketchCenterLine::cast(pElem);
                    if (!pCenterLine) continue;
                    pAxis = pCenterLine;
                    break;
                }
                if (pAxis)
                {
                    wy::ErrorStatus status = wy3d::Revolution::createCut(pTrans, pSketch, pAxis, startAngle, endAngle, pSolidToCut, pOutRevolution);
                }
                return pOutRevolution;
            },
            py::arg("transaction"),
            py::arg("sketch"),
            py::arg("startAngle"),
            py::arg("endAngle"),
            py::arg("solidToCut"),
            py::return_value_policy::reference)

        // 6 参数：显式指定轴曲线
        .def_static("createCut",
            [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch, wy3d::SketchCurve* pAxis, double startAngle, double endAngle, wy3d::Solid* pSolidToCut) -> wy3d::Revolution*
            {
                if (!pTrans || !pSketch || !pAxis || !pSolidToCut) return nullptr;
                wy3d::Revolution* pOutRevolution = nullptr;
                wy::ErrorStatus status = wy3d::Revolution::createCut(pTrans, pSketch, pAxis, startAngle, endAngle, pSolidToCut, pOutRevolution);
                return pOutRevolution;
            },
            py::arg("transaction"),
            py::arg("sketch"),
            py::arg("axis"),
            py::arg("startAngle"),
            py::arg("endAngle"),
            py::arg("solidToCut"),
            py::return_value_policy::reference);

        py::class_<wy3d::Sweep, wy3d::Solid, std::unique_ptr<wy3d::Sweep, py::nodelete>>(m, "Sweep")
            .def("getPath", &wy3d::Sweep::getPath)
            .def("getProfile", &wy3d::Sweep::getProfile)

            .def_static("create",
                [](wydb::Transaction* pTrans, wy3d::Sketch* pPath, wy3d::Sketch* pProfile) -> wy3d::Sweep*
                {
                    if (!pTrans || !pPath || !pProfile) return nullptr;
                    wy3d::Sweep* pOutSweep = nullptr;
                    wy::ErrorStatus status = wy3d::Sweep::create(pTrans, pPath, pProfile, pOutSweep);
                    return pOutSweep;
                },
                py::arg("transaction"),
                py::arg("path"),
                py::arg("profile"),
                py::return_value_policy::reference)

            .def_static("create",
                [](wydb::Transaction* pTrans, wy3d::Curve* pPath, wy3d::Sketch* pProfile) -> wy3d::Sweep*
                {
                    if (!pTrans || !pPath || !pProfile) return nullptr;
                    wy3d::Sweep* pOutSweep = nullptr;
                    wy::ErrorStatus status = wy3d::Sweep::create(pTrans, pPath, pProfile, pOutSweep);
                    return pOutSweep;
                },
                py::arg("transaction"),
                py::arg("path"),
                py::arg("profile"),
                py::return_value_policy::reference)

            .def_static("createCut",
                [](wydb::Transaction* pTrans, wy3d::Sketch* pPath, wy3d::Sketch* pProfile, wy3d::Solid* pSolidToCut) -> wy3d::Sweep*
                {
                    if (!pTrans || !pPath || !pProfile || !pSolidToCut) return nullptr;
                    wy3d::Sweep* pOutSweep = nullptr;
                    wy::ErrorStatus status = wy3d::Sweep::createCut(pTrans, pPath, pProfile, pSolidToCut, pOutSweep);
                    return pOutSweep;
                },
                py::arg("transaction"),
                py::arg("path"),
                py::arg("profile"),
                py::arg("solidToCut"),
                py::return_value_policy::reference)

            .def_static("createCut",
                [](wydb::Transaction* pTrans, wy3d::Curve* pPath, wy3d::Sketch* pProfile, wy3d::Solid* pSolidToCut) -> wy3d::Sweep*
                {
                    if (!pTrans || !pPath || !pProfile || !pSolidToCut) return nullptr;
                    wy3d::Sweep* pOutSweep = nullptr;
                    wy::ErrorStatus status = wy3d::Sweep::createCut(pTrans, pPath, pProfile, pSolidToCut, pOutSweep);
                    return pOutSweep;
                },
                py::arg("transaction"),
                py::arg("path"),
                py::arg("profile"),
                py::arg("solidToCut"),
                py::return_value_policy::reference);

            py::class_<wy3d::Loft, wy3d::Solid, std::unique_ptr<wy3d::Loft, py::nodelete>>(m, "Loft")
                .def("getProfiles", &wy3d::Loft::getProfiles)

                .def_static("create",
                    [](wydb::Transaction* pTrans, const std::vector<wy3d::Sketch*>& profiles) -> wy3d::Loft*
                    {
                        if (!pTrans || profiles.empty()) return nullptr;
                        wy3d::Loft* pOutLoft = nullptr;
                        wy::ErrorStatus status = wy3d::Loft::create(pTrans, profiles, pOutLoft);
                        return pOutLoft;
                    },
                    py::arg("transaction"),
                    py::arg("profiles"),
                    py::return_value_policy::reference)

                .def_static("createCut",
                    [](wydb::Transaction* pTrans, const std::vector<wy3d::Sketch*>& profiles, wy3d::Solid* pSolidToCut) -> wy3d::Loft*
                    {
                        if (!pTrans || profiles.empty() || !pSolidToCut) return nullptr;
                        wy3d::Loft* pOutLoft = nullptr;
                        wy::ErrorStatus status = wy3d::Loft::createCut(pTrans, profiles, pSolidToCut, pOutLoft);
                        return pOutLoft;
                    },
                    py::arg("transaction"),
                    py::arg("profiles"),
                    py::arg("solidToCut"),
                    py::return_value_policy::reference);

}
