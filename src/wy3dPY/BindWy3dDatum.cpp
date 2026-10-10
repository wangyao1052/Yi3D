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
#include <wy3dDatumPlane.h>

namespace py = pybind11;

void bindWy3dDatum(py::module_& m)
{
    py::class_<wy3d::Datum, wy3d::Feature, std::unique_ptr<wy3d::Datum, py::nodelete>> datum_class(m, "Datum");

    py::class_<wy3d::DatumPlane, wy3d::Datum, std::unique_ptr<wy3d::DatumPlane, py::nodelete>>(m, "DatumPlane")
        .def("getPlane", &wy3d::DatumPlane::getPlane)
        .def("setPlane", &wy3d::DatumPlane::setPlane)
        .def("getName", &wy3d::DatumPlane::getName)
        .def("setName", &wy3d::DatumPlane::setName)

        .def_static("create",
            [](wydb::Transaction* pTrans, const wy3d::Plane& plane) -> wy3d::DatumPlane*
            {
                wy3d::DatumPlane* pDatumPlane = nullptr;
                wy::ErrorStatus status = wy3d::DatumPlane::create(pTrans, plane, pDatumPlane);
                return pDatumPlane;
            },
            py::arg("transaction"),
            py::arg("plane"),
            py::return_value_policy::reference);
    
    
    /*py::class_ <wy3d::Datum, wy3d::Solid, std::unique_ptr<wy3d::Boolean, py::nodelete >>(m, "Boolean")
        .def("", &wy3d::Boolean::getBooleanType)
        .def("", &wy3d::Boolean::getTarget)
        .def("", &wy3d::Boolean::getTools)
        .def("", &wy3d::Boolean::addTool)
        .def("", &wy3d::Boolean::cancelBoolean);

    py::enum_<wy3d::BooleanType>(m, "BooleanType")
        .value("Undefined", wy3d::BooleanType::Undefined)
        .value("Union", wy3d::BooleanType::Union)
        .value("Difference", wy3d::BooleanType::Difference)
        .value("Intersection", wy3d::BooleanType::Intersection)
        .def("__repr__", [](wy3d::BooleanType booleanType) {
        switch (booleanType) {
        case wy3d::BooleanType::Undefined: return "wy3d.BooleanType.Undefined";
        case wy3d::BooleanType::Union: return "wy3d.BooleanType.Union";
        case wy3d::BooleanType::Difference: return "wy3d.BooleanType.Difference";
        case wy3d::BooleanType::Intersection: return "wy3d.BooleanType.Intersection";
        default: return "wy3d.BooleanType.Unknown";
        }});

    py::class_ <wy3d::Boolean, wy3d::Solid, std::unique_ptr<wy3d::Boolean, py::nodelete >>(m, "Boolean")
        .def("", &wy3d::Boolean::getBooleanType)
        .def("", &wy3d::Boolean::getTarget)
        .def("", &wy3d::Boolean::getTools)
        .def("", &wy3d::Boolean::addTool)
        .def("", &wy3d::Boolean::cancelBoolean);

    py::class_<wy3d::Union, wy3d::Boolean, std::unique_ptr<wy3d::Union, py::nodelete>>(m, "Union")
        .def_static("create",
            [](wydb::Database* pDatabase, wy3d::Solid* pTarget, const std::vector<wy3d::Solid*>& tools) -> wy3d::Union*
            {
                wy3d::Union* pUnion = nullptr;
                wy::ErrorStatus status = wy3d::Union::create(pDatabase, pTarget, tools, pUnion);
                return pUnion;
            },
            py::arg("database"),
            py::arg("target"),
            py::arg("tools"),
            py::return_value_policy::reference);

    py::class_<wy3d::Intersection, wy3d::Boolean, std::unique_ptr<wy3d::Intersection, py::nodelete>>(m, "Intersection")
        .def_static("create",
            [](wydb::Database* pDatabase, wy3d::Solid* pTarget, const std::vector<wy3d::Solid*>& tools) -> wy3d::Intersection*
            {
                wy3d::Intersection* pIntersection = nullptr;
                wy::ErrorStatus status = wy3d::Intersection::create(pDatabase, pTarget, tools, pIntersection);
                return pIntersection;
            },
            py::arg("database"),
            py::arg("target"),
            py::arg("tools"),
            py::return_value_policy::reference);

    py::class_<wy3d::Difference, wy3d::Boolean, std::unique_ptr<wy3d::Difference, py::nodelete>>(m, "Difference")
        .def_static("create",
            [](wydb::Database* pDatabase, wy3d::Solid* pTarget, const std::vector<wy3d::Solid*>& tools) -> wy3d::Difference*
            {
                wy3d::Difference* pDifference = nullptr;
                wy::ErrorStatus status = wy3d::Difference::create(pDatabase, pTarget, tools, pDifference);
                return pDifference;
            },
            py::arg("database"),
            py::arg("target"),
            py::arg("tools"),
            py::return_value_policy::reference);*/

}