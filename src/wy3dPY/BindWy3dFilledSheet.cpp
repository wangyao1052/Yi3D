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

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dFilledSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketch3D.h>

namespace py = pybind11;

void bindWy3dFilledSheet(py::module_& m)
{
    py::class_<wy3d::FilledSheet, wy3d::Feature, std::unique_ptr<wy3d::FilledSheet, py::nodelete>>(m, "FilledSheet")
        .def("getBoundarySketch", &wy3d::FilledSheet::getBoundarySketch)
        .def("getChildren", &wy3d::FilledSheet::getChildren)
        .def("getConstraintSketches", &wy3d::FilledSheet::getConstraintSketches)
        // A constraint sketch is a whole sketch element, taken from the transaction for write
        .def("addConstraintSketch",
            [](wy3d::FilledSheet* pFilledSheet, wydb::Element* pConstraintSketch) -> bool
            {
                if (wy3d::Sketch* pSketch = wy3d::Sketch::cast(pConstraintSketch))
                {
                    return wy::ErrorStatus::Ok == pFilledSheet->addConstraintSketch(pSketch);
                }
                if (wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pConstraintSketch))
                {
                    return wy::ErrorStatus::Ok == pFilledSheet->addConstraintSketch(pSketch3D);
                }
                return false;
            },
            py::arg("constraintSketch"))
        .def("removeConstraintSketch",
            [](wy3d::FilledSheet* pFilledSheet, wydb::Element* pConstraintSketch) -> bool
            {
                if (wy3d::Sketch* pSketch = wy3d::Sketch::cast(pConstraintSketch))
                {
                    return wy::ErrorStatus::Ok == pFilledSheet->removeConstraintSketch(pSketch);
                }
                if (wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pConstraintSketch))
                {
                    return wy::ErrorStatus::Ok == pFilledSheet->removeConstraintSketch(pSketch3D);
                }
                return false;
            },
            py::arg("constraintSketch"))

        .def_static("create",
            [](wydb::Transaction* pTrans, wy3d::Sketch* pSketch) -> wy3d::FilledSheet*
            {
                wy3d::FilledSheet* pOutSheet = nullptr;
                wy::ErrorStatus status = wy3d::FilledSheet::create(pTrans, pSketch, pOutSheet);
                return pOutSheet;
            },
            py::arg("transaction"), py::arg("boundarySketch"),
            py::return_value_policy::reference)

        .def_static("create",
            [](wydb::Transaction* pTrans, wy3d::Sketch3D* pSketch3D) -> wy3d::FilledSheet*
            {
                wy3d::FilledSheet* pOutSheet = nullptr;
                wy::ErrorStatus status = wy3d::FilledSheet::create(pTrans, pSketch3D, pOutSheet);
                return pOutSheet;
            },
            py::arg("transaction"), py::arg("boundarySketch3d"),
            py::return_value_policy::reference);
}
