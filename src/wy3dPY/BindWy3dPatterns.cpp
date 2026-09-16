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
#include <wy3dPattern.h>
#include <wy3dLinearPattern.h>
#include <wy3dCircularPattern.h>

namespace py = pybind11;

void bindWy3dPatterns(py::module_& m)
{
    // ========== Pattern 基类 ==========
    py::class_<wy3d::Pattern, wy3d::BodyModification, std::unique_ptr<wy3d::Pattern, py::nodelete>>(
        m, "Pattern")
        .def("getSource", &wy3d::Pattern::getSource)
        .def_static("isValidSource", &wy3d::Pattern::isValidSource,
            py::arg("solid"));

    // ========== LinearPattern 线性阵列 ==========
    py::class_<wy3d::LinearPattern, wy3d::Pattern, std::unique_ptr<wy3d::LinearPattern, py::nodelete>>(
        m, "LinearPattern")
        .def("getDirection1st", &wy3d::LinearPattern::getDirection1st)
        .def("setDirection1st", &wy3d::LinearPattern::setDirection1st)
        .def("getCount1st", &wy3d::LinearPattern::getCount1st)
        .def("setCount1st", &wy3d::LinearPattern::setCount1st)
        .def("getSpacing1st", &wy3d::LinearPattern::getSpacing1st)
        .def("setSpacing1st", &wy3d::LinearPattern::setSpacing1st)
        .def("getDirection2nd", &wy3d::LinearPattern::getDirection2nd)
        .def("setDirection2nd", &wy3d::LinearPattern::setDirection2nd)
        .def("getCount2nd", &wy3d::LinearPattern::getCount2nd)
        .def("setCount2nd", &wy3d::LinearPattern::setCount2nd)
        .def("getSpacing2nd", &wy3d::LinearPattern::getSpacing2nd)
        .def("setSpacing2nd", &wy3d::LinearPattern::setSpacing2nd)

        .def_static("create",
            [](wydb::Transaction* pTrans,
               wy3d::Solid* pOwner,
               const wy3d::Solid* pSource,
               const wy::Vector3& dir1st, std::uint32_t count1st, double spacing1st,
               const wy::Vector3& dir2nd, std::uint32_t count2nd, double spacing2nd) -> wy3d::LinearPattern*
            {
                wy3d::LinearPattern* pOutPattern = nullptr;
                wy::ErrorStatus status = wy3d::LinearPattern::create(
                    pTrans, pOwner, pSource,
                    dir1st, count1st, spacing1st,
                    dir2nd, count2nd, spacing2nd,
                    pOutPattern);
                return pOutPattern;
            },
            py::arg("transaction"),
            py::arg("owner"),
            py::arg("source"),
            py::arg("direction1st"),
            py::arg("count1st"),
            py::arg("spacing1st"),
            py::arg("direction2nd"),
            py::arg("count2nd"),
            py::arg("spacing2nd"),
            py::return_value_policy::reference);

    // ========== CircularPattern 圆周阵列 ==========
    py::class_<wy3d::CircularPattern, wy3d::Pattern, std::unique_ptr<wy3d::CircularPattern, py::nodelete>>(
        m, "CircularPattern")
        .def("getCenterPoint", &wy3d::CircularPattern::getCenterPoint)
        .def("setCenterPoint", &wy3d::CircularPattern::setCenterPoint)
        .def("getAxisDirection", &wy3d::CircularPattern::getAxisDirection)
        .def("setAxisDirection", &wy3d::CircularPattern::setAxisDirection)
        .def("getTotalAngle", &wy3d::CircularPattern::getTotalAngle)
        .def("setTotalAngle", &wy3d::CircularPattern::setTotalAngle)
        .def("getInstanceCount", &wy3d::CircularPattern::getInstanceCount)
        .def("setInstanceCount", &wy3d::CircularPattern::setInstanceCount)
        .def("isClockWise", &wy3d::CircularPattern::isClockWise)
        .def("setClockWise", &wy3d::CircularPattern::setClockWise)

        .def_static("create",
            [](wydb::Transaction* pTrans,
               wy3d::Solid* pOwner,
               const wy3d::Solid* pSource,
               const wy::Vector3& centerPoint,
               const wy::Vector3& axisDirection,
               double totalAngle,
               std::uint32_t instanceCount,
               bool isClockWise) -> wy3d::CircularPattern*
            {
                wy3d::CircularPattern* pOutPattern = nullptr;
                wy::ErrorStatus status = wy3d::CircularPattern::create(
                    pTrans, pOwner, pSource,
                    centerPoint, axisDirection,
                    totalAngle, instanceCount, isClockWise,
                    pOutPattern);
                return pOutPattern;
            },
            py::arg("transaction"),
            py::arg("owner"),
            py::arg("source"),
            py::arg("centerPoint"),
            py::arg("axisDirection"),
            py::arg("totalAngle"),
            py::arg("instanceCount"),
            py::arg("isClockWise"),
            py::return_value_policy::reference);
}
