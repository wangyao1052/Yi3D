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
#include <wy3dSolidModification.h>
#include <wy3dFillet.h>
#include <wy3dChamfer.h>
#include <wy3dShell.h>
#include <wy3dMirror.h>
#include <wy3dDraft.h>
#include <wy3dMove.h>
#include <wy3dRotate.h>
#include <wy3dSketchPlane.h>

namespace py = pybind11;

void bindWy3dSolidModifications(py::module_& m)
{
    // ========== SolidModification 基类 ==========
    py::class_<wy3d::SolidModification, wy3d::Feature, std::unique_ptr<wy3d::SolidModification, py::nodelete>>(
        m, "SolidModification")
        .def("getNewFaceIndices", &wy3d::SolidModification::getNewFaceIndices);

    // ========== Fillet 倒圆角 ==========
    py::class_<wy3d::Fillet, wy3d::SolidModification, std::unique_ptr<wy3d::Fillet, py::nodelete>>(
        m, "Fillet")
        .def("getRadius", &wy3d::Fillet::getRadius)
        .def("setRadius", &wy3d::Fillet::setRadius)
        .def("getEdges", &wy3d::Fillet::getEdges)
        .def("setEdges", &wy3d::Fillet::setEdges)
        .def("getFaces", &wy3d::Fillet::getFaces)
        .def("setFaces", &wy3d::Fillet::setFaces)

        .def_static("create",
            [](wydb::Transaction* pTrans,
               wy3d::Solid* pSolid,
               const std::vector<std::uint32_t>& faceIndices,
               const std::vector<std::uint32_t>& edgeIndices,
               double radius) -> wy3d::Fillet*
            {
                wy3d::Fillet* pOutFillet = nullptr;
                wy::ErrorStatus status = wy3d::Fillet::create(
                    pTrans, pSolid, faceIndices, edgeIndices, radius, pOutFillet);
                return pOutFillet;
            },
            py::arg("transaction"),
            py::arg("solid"),
            py::arg("faceIndices"),
            py::arg("edgeIndices"),
            py::arg("radius"),
            py::return_value_policy::reference);

    // ========== Chamfer 倒角 ==========
    py::class_<wy3d::Chamfer, wy3d::SolidModification, std::unique_ptr<wy3d::Chamfer, py::nodelete>>(
        m, "Chamfer")
        .def("getDistance", &wy3d::Chamfer::getDistance)
        .def("setDistance", &wy3d::Chamfer::setDistance)
        .def("getEdges", &wy3d::Chamfer::getEdges)
        .def("setEdges", &wy3d::Chamfer::setEdges)
        .def("getFaces", &wy3d::Chamfer::getFaces)
        .def("setFaces", &wy3d::Chamfer::setFaces)

        .def_static("create",
            [](wydb::Transaction* pTrans,
               wy3d::Solid* pSolid,
               const std::vector<std::uint32_t>& faceIndices,
               const std::vector<std::uint32_t>& edgeIndices,
               double distance) -> wy3d::Chamfer*
            {
                wy3d::Chamfer* pOutChamfer = nullptr;
                wy::ErrorStatus status = wy3d::Chamfer::create(
                    pTrans, pSolid, faceIndices, edgeIndices, distance, pOutChamfer);
                return pOutChamfer;
            },
            py::arg("transaction"),
            py::arg("solid"),
            py::arg("faceIndices"),
            py::arg("edgeIndices"),
            py::arg("distance"),
            py::return_value_policy::reference);

    // ========== ShellDirection 枚举 ==========
    py::enum_<wy3d::ShellDirection>(m, "ShellDirection")
        .value("Inward", wy3d::ShellDirection::Inward)
        .value("Outward", wy3d::ShellDirection::Outward)
        .def("__repr__", [](wy3d::ShellDirection dir) {
            switch (dir) {
            case wy3d::ShellDirection::Inward:  return "wy3d.ShellDirection.Inward";
            case wy3d::ShellDirection::Outward: return "wy3d.ShellDirection.Outward";
            default: return "wy3d.ShellDirection.Unknown";
            }});

    // ========== ShellJoinType 枚举 ==========
    py::enum_<wy3d::ShellJoinType>(m, "ShellJoinType")
        .value("Arc", wy3d::ShellJoinType::Arc)
        .value("Intersection", wy3d::ShellJoinType::Intersection)
        .def("__repr__", [](wy3d::ShellJoinType t) {
            switch (t) {
            case wy3d::ShellJoinType::Arc:          return "wy3d.ShellJoinType.Arc";
            case wy3d::ShellJoinType::Intersection: return "wy3d.ShellJoinType.Intersection";
            default: return "wy3d.ShellJoinType.Unknown";
            }});

    // ========== ShellOffsetMode 枚举 ==========
    py::enum_<wy3d::ShellOffsetMode>(m, "ShellOffsetMode")
        .value("Skin", wy3d::ShellOffsetMode::Skin)
        .value("Pipe", wy3d::ShellOffsetMode::Pipe)
        .value("RectoVerso", wy3d::ShellOffsetMode::RectoVerso)
        .def("__repr__", [](wy3d::ShellOffsetMode m) {
            switch (m) {
            case wy3d::ShellOffsetMode::Skin:       return "wy3d.ShellOffsetMode.Skin";
            case wy3d::ShellOffsetMode::Pipe:       return "wy3d.ShellOffsetMode.Pipe";
            case wy3d::ShellOffsetMode::RectoVerso: return "wy3d.ShellOffsetMode.RectoVerso";
            default: return "wy3d.ShellOffsetMode.Unknown";
            }});

    // ========== Shell 抽壳 ==========
    py::class_<wy3d::Shell, wy3d::SolidModification, std::unique_ptr<wy3d::Shell, py::nodelete>>(
        m, "Shell")
        .def("getThickness", &wy3d::Shell::getThickness)
        .def("setThickness", &wy3d::Shell::setThickness)
        .def("getDirection", &wy3d::Shell::getDirection)
        .def("setDirection", &wy3d::Shell::setDirection)
        .def("getFaces", &wy3d::Shell::getFaces)
        .def("setFaces", &wy3d::Shell::setFaces)
        .def("getJoinType", &wy3d::Shell::getJoinType)
        .def("setJoinType", &wy3d::Shell::setJoinType)
        .def("getOffsetMode", &wy3d::Shell::getOffsetMode)
        .def("setOffsetMode", &wy3d::Shell::setOffsetMode)
        .def("getIntersection", &wy3d::Shell::getIntersection)
        .def("setIntersection", &wy3d::Shell::setIntersection)

        .def_static("create",
            [](wydb::Transaction* pTrans,
               wy3d::Solid* pSolid,
               const std::vector<std::uint32_t>& faceIndices,
               double thickness,
               wy3d::ShellDirection direction) -> wy3d::Shell*
            {
                wy3d::Shell* pOutShell = nullptr;
                wy::ErrorStatus status = wy3d::Shell::create(
                    pTrans, pSolid, faceIndices, thickness, direction, pOutShell);
                return pOutShell;
            },
            py::arg("transaction"),
            py::arg("solid"),
            py::arg("faceIndices"),
            py::arg("thickness"),
            py::arg("direction"),
            py::return_value_policy::reference);

    // ========== Mirror 镜像 ==========
    py::class_<wy3d::Mirror, wy3d::SolidModification, std::unique_ptr<wy3d::Mirror, py::nodelete>>(
        m, "Mirror")
        .def("getSource", &wy3d::Mirror::getSource)
        .def("getPlane", &wy3d::Mirror::getPlane)
        .def("setPlane", &wy3d::Mirror::setPlane)
        .def_static("isValidSource", &wy3d::Mirror::isValidSource,
            py::arg("solid"))

        .def_static("create",
            [](wydb::Transaction* pTrans,
               wy3d::Solid* pOwner,
               const wy3d::Solid* pSource,
               const wy3d::SketchPlane& mirrorPlane) -> wy3d::Mirror*
            {
                wy3d::Mirror* pOutMirror = nullptr;
                wy::ErrorStatus status = wy3d::Mirror::create(
                    pTrans, pOwner, pSource, mirrorPlane, pOutMirror);
                return pOutMirror;
            },
            py::arg("transaction"),
            py::arg("owner"),
            py::arg("source"),
            py::arg("mirrorPlane"),
            py::return_value_policy::reference);

    // ========== Draft 拔模 ==========
    py::class_<wy3d::Draft, wy3d::SolidModification, std::unique_ptr<wy3d::Draft, py::nodelete>>(
        m, "Draft")
        .def("getAngle", &wy3d::Draft::getAngle)
        .def("setAngle", &wy3d::Draft::setAngle)
        .def("getNeutralFace", &wy3d::Draft::getNeutralFace)
        .def("setNeutralFace", &wy3d::Draft::setNeutralFace)
        .def("getFaces", &wy3d::Draft::getFaces)
        .def("setFaces", &wy3d::Draft::setFaces)

        .def_static("create",
            [](wydb::Transaction* pTrans,
               wy3d::Solid* pSolid,
               std::uint32_t neutralFaceIndex,
               const std::vector<std::uint32_t>& faceIndices,
               double angle) -> wy3d::Draft*
            {
                wy3d::Draft* pOutDraft = nullptr;
                wy::ErrorStatus status = wy3d::Draft::create(
                    pTrans, pSolid, neutralFaceIndex, faceIndices, angle, pOutDraft);
                return pOutDraft;
            },
            py::arg("transaction"),
            py::arg("solid"),
            py::arg("neutralFaceIndex"),
            py::arg("faceIndices"),
            py::arg("angle"),
            py::return_value_policy::reference);

    // ========== Move 移动面 ==========
    py::class_<wy3d::Move, wy3d::SolidModification, std::unique_ptr<wy3d::Move, py::nodelete>>(
        m, "Move")
        .def("getVector", &wy3d::Move::getVector)
        .def("setVector", &wy3d::Move::setVector)

        .def_static("create",
            [](wydb::Transaction* pTrans,
               wy3d::Solid* pSolid,
               const wy::Vector3& moveVector) -> wy3d::Move*
            {
                wy3d::Move* pOutMove = nullptr;
                wy::ErrorStatus status = wy3d::Move::create(
                    pTrans, pSolid, moveVector, pOutMove);
                return pOutMove;
            },
            py::arg("transaction"),
            py::arg("solid"),
            py::arg("moveVector"),
            py::return_value_policy::reference);

    // ========== Rotate 旋转面 ==========
    py::class_<wy3d::Rotate, wy3d::SolidModification, std::unique_ptr<wy3d::Rotate, py::nodelete>>(
        m, "Rotate")
        .def("getCenterPoint", &wy3d::Rotate::getCenterPoint)
        .def("setCenterPoint", &wy3d::Rotate::setCenterPoint)
        .def("getAxisDirection", &wy3d::Rotate::getAxisDirection)
        .def("setAxisDirection", &wy3d::Rotate::setAxisDirection)
        .def("getAngle", &wy3d::Rotate::getAngle)
        .def("setAngle", &wy3d::Rotate::setAngle)

        .def_static("create",
            [](wydb::Transaction* pTrans,
               wy3d::Solid* pSolid,
               const wy::Vector3& centerPoint,
               const wy::Vector3& axisDirection,
               double angle) -> wy3d::Rotate*
            {
                wy3d::Rotate* pOutRotate = nullptr;
                wy::ErrorStatus status = wy3d::Rotate::create(
                    pTrans, pSolid, centerPoint, axisDirection, angle, pOutRotate);
                return pOutRotate;
            },
            py::arg("transaction"),
            py::arg("solid"),
            py::arg("centerPoint"),
            py::arg("axisDirection"),
            py::arg("angle"),
            py::return_value_policy::reference);
}
